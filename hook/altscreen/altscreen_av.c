/*
 * altscreen_av — AltScreen H.264 forward (hook side). See altscreen_av.h.
 * Ported from mhi3-altscreen libaltscreen_hook_ultra.c forward path; adapted to
 * MHI2Q (ARM32 QNX), no __thread, lazy-connect to the decoder process.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "../framework/common.h"
#include "../framework/logging.h"
#include "altscreen_av.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>  /* TCP_NODELAY on the renderer link */
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

DEFINE_LOG_MODULE(altscreen_av);

/* QNX 6.5 exposes MSG_NOSIGNAL in its headers but io-pkt returns ENOSYS when
 * it is passed to send(). bus_init() ignores SIGPIPE process-wide, so socket
 * sends in this hook must use flags=0. */
#define ALTAV_SEND_FLAGS 0
typedef char altav_send_flags_must_be_zero[(ALTAV_SEND_FLAGS == 0) ? 1 : -1];

/* Bounded per-record send stall.  Lowered from 8000: a receiver stuck here holds g_lock,
 * and a teardown/reap that joins it inherits this as latency on the RTSP path.  The
 * renderer's OMX still starts within this window; a genuinely dead link now fails ~1 s. */
#define AV_SEND_STALL_LIMIT_MS 1000L
#define AV_CONNECT_TIMEOUT_MS 250

static char            g_host[64] = ALTAV_DEFAULT_HOST;
static int             g_port     = ALTAV_DEFAULT_PORT;
static int             g_enabled  = 1;
static int             g_fd       = -1;
static int             g_need_config = 0;
static pthread_mutex_t g_lock     = PTHREAD_MUTEX_INITIALIZER;
/* The generation that currently owns the single renderer transport (g_fd/g_cfg).
 * Guarded by g_lock; 0 = unclaimed. Only the owner may connect/config/forward/close. */
static uint32_t        g_owner_gen = 0;
/* Cached Annex-B SPS/PPS.  The renderer marks the FIRST message on each connection as
 * OMX_BUFFERFLAG_CODECCONFIG, so on every (re)connect we must resend the config BEFORE any frame —
 * otherwise a reconnect mid-stream sends a video frame first, the decoder takes it as config and
 * reports OMX_ErrorFormatNotDetected → black.  (Set via altav_set_config on each avcC.) */
static uint8_t         g_cfg[4096];
static uint32_t        g_cfg_len = 0;

void altav_init(const char *host, int port)
{
    pthread_mutex_lock(&g_lock);
    if (host && host[0]) {
        strncpy(g_host, host, sizeof(g_host) - 1);
        g_host[sizeof(g_host) - 1] = '\0';
    }
    if (port > 0) g_port = port;
    pthread_mutex_unlock(&g_lock);
    LOG_INFO(LOG_MODULE, "forward target %s:%d", g_host, g_port);
}

void altav_set_enabled(int on)
{
    pthread_mutex_lock(&g_lock);
    g_enabled = on ? 1 : 0;
    if (!g_enabled && g_fd >= 0) { close(g_fd); g_fd = -1; }
    pthread_mutex_unlock(&g_lock);
}
int altav_is_enabled(void)
{
    int enabled;
    pthread_mutex_lock(&g_lock);
    enabled = g_enabled;
    pthread_mutex_unlock(&g_lock);
    return enabled;
}

/* Make `gen` the transport owner only when it is newer than the committed owner.
 * An accepted claim drops the previous owner's connection and cached config so the
 * new owner starts clean (its first frame reconnects + replays SPS/PPS). */
int altav_claim(uint32_t gen)
{
    int accepted = 0;
    pthread_mutex_lock(&g_lock);
    /* MONOTONIC: only a strictly newer generation may take ownership.  A slower older
     * SETUP that runs altav_claim() AFTER a newer one must not reclaim the transport
     * (that would make the newer receiver see itself superseded and exit).  g_alt_gen_counter
     * is monotonic under alt_lock, so a later SETUP always has a higher gen; no wrap over a
     * dio lifetime. */
    if (gen > g_owner_gen) {
        if (g_fd >= 0) { close(g_fd); g_fd = -1; }
        g_need_config = 0;
        g_cfg_len = 0;
        g_owner_gen = gen;
        accepted = 1;
        LOG_INFO(LOG_MODULE, "transport owner -> gen=%u", (unsigned)gen);
    } else {
        LOG_INFO(LOG_MODULE, "transport claim gen=%u ignored (owner=%u newer)",
                 (unsigned)gen, (unsigned)g_owner_gen);
    }
    pthread_mutex_unlock(&g_lock);
    return accepted;
}

int altav_is_owner(uint32_t gen)
{
    int owner;
    pthread_mutex_lock(&g_lock);
    owner = (g_owner_gen == gen);
    pthread_mutex_unlock(&g_lock);
    return owner;
}

/* caller holds g_lock */
static void av_connect_locked(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int original_flags;
    int connected = 0;
    if (fd < 0) return;
    /* Bounded send: this is the hook-owned 111 receiver thread, not stock
     * libairplay's main-screen thread.  The renderer starts OMX synchronously
     * after reading the 33-byte config, so its next read can legitimately be
     * delayed beyond one SO_SNDTIMEO interval.  av_send_all_locked() retries
     * EAGAIN for a bounded total instead of tearing down a healthy stream. */
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;   /* 500 ms */
    int sndbuf = 512 * 1024;
    int nodelay = 1;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    /* Every frame is written as two records, [u32 BE len] then the access unit.
     * With Nagle on, the 4-byte header ships at once and leaves data unacked, so
     * the access unit -- one sub-MSS segment on loopback -- is held until that
     * header is acknowledged.  The renderer never writes back on this socket, so
     * nothing piggybacks the ACK and it waits out the delayed-ACK timer.  That
     * timer is 200 ms, which is exactly the arrival gap the renderer reports
     * (max 140-204 ms) while decode and present stay under 3 ms. */
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)g_port);
    inet_pton(AF_INET, g_host, &sa.sin_addr);
    original_flags = fcntl(fd, F_GETFL, 0);
    if (original_flags < 0 || fcntl(fd, F_SETFL, original_flags | O_NONBLOCK) != 0) {
        close(fd);
        return;
    }
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
        connected = 1;
    } else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
        struct pollfd pfd;
        int socket_error = 0;
        socklen_t socket_error_len = sizeof(socket_error);
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        if (poll(&pfd, 1, AV_CONNECT_TIMEOUT_MS) > 0 &&
            (pfd.revents & POLLOUT) &&
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error,
                       &socket_error_len) == 0 && socket_error == 0)
            connected = 1;
    }
    /* Keep O_NONBLOCK: the send loop owns the total deadline and POLLOUT wait.
     * Restoring blocking mode here lets one send outlive that deadline. */
    if (connected) {
        g_fd = fd;
        g_need_config = 1;
        LOG_INFO(LOG_MODULE, "connected to %s:%d (fd=%d)", g_host, g_port, fd);
        /* resend cached SPS/PPS first — the render's fresh decoder flags msg #1 as CODECCONFIG */
        /* Config replay is performed by av_send_locked() below. */
    } else {
        close(fd);
    }
}

/* TCP send() may legally return short even on loopback. Complete each protocol
 * record while retaining the 500 ms socket timeout; transient back-pressure is
 * retried for at most AV_SEND_STALL_LIMIT_MS before the connection is dropped. */
static int av_send_all_locked(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t off = 0;
    struct timespec started, now;

    if (clock_gettime(CLOCK_MONOTONIC, &started) != 0)
        return -1;
    while (off < len) {
        long elapsed_ms;
        struct pollfd pfd;
        int ready;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
        elapsed_ms = (long)(now.tv_sec - started.tv_sec) * 1000L +
                     (long)(now.tv_nsec - started.tv_nsec) / 1000000L;
        if (elapsed_ms >= AV_SEND_STALL_LIMIT_MS) return -1;
        ssize_t n = send(g_fd, p + off, len - off, ALTAV_SEND_FLAGS);
        if (n > 0) { off += (uint32_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            /* The connect socket stays nonblocking. SO_SNDTIMEO therefore
             * cannot pace retries: wait for writability instead of spinning
             * for a second with the transport/teardown lock held. */
            pfd.fd = g_fd; pfd.events = POLLOUT; pfd.revents = 0;
            ready = poll(&pfd, 1, (int)(AV_SEND_STALL_LIMIT_MS - elapsed_ms));
            if (ready < 0 && errno == EINTR) continue;
            if (ready > 0 && (pfd.revents & POLLOUT) &&
                !(pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) continue;
        }
        LOG_ERROR(LOG_MODULE,
                  "send failed fd=%d off=%u/%u ret=%d errno=%d",
                  g_fd, off, len, (int)n, errno);
        return -1;
    }
    return 0;
}

/* caller holds g_lock; sends [u32 BE len][buf]. drops fd on error. */
static int av_send_locked(const uint8_t *buf, uint32_t len)
{
    int opened = 0;
    if (g_fd < 0) {
        av_connect_locked();
        if (g_fd >= 0) opened = 1;
    }
    if (g_fd < 0) return -1;

    /* A fresh connection must begin with CODECCONFIG.  Send it here rather than
     * from av_connect_locked so all writes use the same short-write-safe path. */
    if (g_need_config && g_cfg_len) {
        uint32_t cfg_be = htonl(g_cfg_len);
        if (av_send_all_locked(&cfg_be, 4) != 0 ||
            av_send_all_locked(g_cfg, g_cfg_len) != 0) {
            close(g_fd); g_fd = -1; g_need_config = 0;
            return -1;
        }
        g_need_config = 0;
        LOG_INFO(LOG_MODULE, "replayed SPS/PPS config (%u bytes) on new connection", g_cfg_len);
        /* altav_set_config() passes g_cfg itself.  It has just been sent as the
         * mandatory first record, so do not duplicate it. */
        if (buf == g_cfg && len == g_cfg_len) return opened;
    }
    uint32_t be = htonl(len);
    if (av_send_all_locked(&be, 4) != 0 ||
        av_send_all_locked(buf, len) != 0) {
        close(g_fd);
        g_fd = -1;
        g_need_config = 0;
        return -1;
    }
    return opened;
}

/* Cache the Annex-B SPS/PPS.  Do not create the renderer connection merely for
 * config: the first IDR-safe access unit opens it and atomically replays config
 * first.  A mid-stream codec change is still sent to an existing decoder. */
void altav_set_config(uint32_t gen, const uint8_t *annexb_cfg, uint32_t len)
{
    if (!annexb_cfg || len == 0 || len > sizeof(g_cfg)) return;
    pthread_mutex_lock(&g_lock);
    if (g_owner_gen == gen) {                 /* a superseded receiver must not replace the new config */
        memcpy(g_cfg, annexb_cfg, len);
        g_cfg_len = len;
        if (g_enabled && g_fd >= 0)
            (void)av_send_locked(g_cfg, g_cfg_len);
    }
    pthread_mutex_unlock(&g_lock);
}

/* Convert AVCC (length_size-byte BE length prefixes) → Annex-B (00 00 00 01
 * start codes).  All arithmetic uses subtraction form so hostile length fields
 * cannot wrap: `nal > inlen - i` never underflows (loop keeps i <= inlen).
 * Reject the whole access unit if any length is malformed; forwarding a prefix
 * loses decoder references and conceals the need for a new IDR.
 * `out == NULL` = measure only. Returns the exact
 * Annex-B byte count (identical for measure and write passes → exact alloc). */
static uint32_t avcc_to_annexb(const uint8_t *in, uint32_t inlen, uint8_t ls,
                               uint8_t *out, uint32_t outcap, int *is_idr)
{
    uint32_t i = 0, o = 0;
    int idr = 0;
    if (is_idr) *is_idr = 0;
    if (ls < 1 || ls > 4) return 0;
    while (inlen - i >= ls) {                            /* i <= inlen invariant */
        uint32_t nal = 0, k;
        for (k = 0; k < ls; k++) nal = (nal << 8) | in[i + k];
        i += ls;                                         /* i <= inlen (checked above) */
        if (nal == 0 || nal > inlen - i) return 0;
        if (nal > 0xFFFFFFFFu - 4 || o > 0xFFFFFFFFu - 4 - nal) return 0;
        if ((in[i] & 0x1f) == 5) idr = 1;
        if (out) {
            if (o + 4 + nal > outcap) return 0;
            out[o] = 0; out[o + 1] = 0; out[o + 2] = 0; out[o + 3] = 1;
            memcpy(out + o + 4, in + i, nal);
        }
        o += 4 + nal;
        i += nal;
    }
    if (i != inlen) return 0;
    if (is_idr) *is_idr = idr;
    return o;
}

int altav_forward_avcc_frame(uint32_t gen, const uint8_t *avcc, uint32_t len, uint8_t ls)
{
    int result = -1;
    int is_idr = 0;
    if (!avcc || len == 0) return -1;
    uint32_t need = avcc_to_annexb(avcc, len, ls, NULL, 0, &is_idr);
    if (!need) return -1;
    uint8_t *ab = (uint8_t *)malloc(need);
    if (!ab) return -1;
    uint32_t n = avcc_to_annexb(avcc, len, ls, ab, need, NULL);
    if (n) {
        pthread_mutex_lock(&g_lock);
        /* Ownership checked HERE under g_lock, not before the call: a supersede between
         * a caller-side check and this send would otherwise write into the new owner's fd. */
        /* Every fresh connection needs config AND an IDR, regardless of which
         * prior send closed it (including a config-only send or runtime off). */
        if (g_enabled && g_owner_gen == gen && g_cfg_len &&
            (g_fd >= 0 || is_idr))
            result = av_send_locked(ab, n);
        pthread_mutex_unlock(&g_lock);
    }
    free(ab);
    return result;
}

void altav_shutdown_if_owner(uint32_t gen)
{
    pthread_mutex_lock(&g_lock);
    if (g_owner_gen == gen) {
        if (g_fd >= 0) { close(g_fd); g_fd = -1; }
        g_need_config = 0;
    }
    pthread_mutex_unlock(&g_lock);
}

void altav_shutdown(void)
{
    pthread_mutex_lock(&g_lock);
    if (g_fd >= 0) { close(g_fd); g_fd = -1; }
    g_need_config = 0;
    pthread_mutex_unlock(&g_lock);
}
