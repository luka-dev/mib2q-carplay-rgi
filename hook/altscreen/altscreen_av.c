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
#include "../../common/alt_timing_stats.h"
#include "../../common/alt_control_wire.h"

#include <sys/socket.h>
#include <sys/uio.h>
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

/* Bounded per-record send stall.  Lowered from 8000: a receiver stuck here holds g_transport.lock,
 * and a teardown/reap that joins it inherits this as latency on the RTSP path.  The
 * renderer's OMX still starts within this window; a genuinely dead link now fails ~1 s. */
#define AV_SEND_STALL_LIMIT_MS 1000L
#define AV_CONNECT_TIMEOUT_MS 250

/* Lock order: transport.lock -> owner.lock or forward.lock; neither owner
 * nor producer operations acquire transport.lock while holding forward.lock.
 * Each state object owns only the fields protected by its documented mutex. */
typedef struct {
    pthread_mutex_t lock;
    char host[64];
    int port, enabled, fd, need_config;
    uint32_t owner_gen, io_epoch;
    uint8_t *annexb;
    uint32_t annexb_cap;
    int wire_v2, status_supported, ack_pending;
    alt_control_stream_t control;
    uint8_t config[4096];
    uint32_t config_len;
} av_transport_state_t;
static av_transport_state_t g_transport = {.lock = PTHREAD_MUTEX_INITIALIZER,
                                           .host = ALTAV_DEFAULT_HOST,
                                           .port = ALTAV_DEFAULT_PORT,
                                           .enabled = 1,
                                           .fd = -1};
/* The reader's ownership snapshot never waits behind socket I/O. */
typedef struct {
    pthread_mutex_t lock;
    uint32_t generation;
} av_owner_state_t;
static av_owner_state_t g_owner = {.lock = PTHREAD_MUTEX_INITIALIZER};
static void av_async_reset(uint32_t gen);
static void av_async_cancel(uint32_t gen);
static void av_async_stop(void);
static void av_async_enable(int on);

void altav_init(const char *host, int port)
{
    pthread_mutex_lock(&g_transport.lock);
    if (host && host[0]) {
        strncpy(g_transport.host, host, sizeof(g_transport.host) - 1);
        g_transport.host[sizeof(g_transport.host) - 1] = '\0';
    }
    if (port > 0)
        g_transport.port = port;
    pthread_mutex_unlock(&g_transport.lock);
    LOG_INFO(LOG_MODULE, "forward target %s:%d", g_transport.host, g_transport.port);
}

void altav_set_enabled(int on)
{
    if (!on)
        av_async_enable(0);
    pthread_mutex_lock(&g_transport.lock);
    g_transport.enabled = on ? 1 : 0;
    if (!g_transport.enabled && g_transport.fd >= 0) {
        close(g_transport.fd);
        g_transport.fd = -1;
    }
    pthread_mutex_unlock(&g_transport.lock);
    if (on)
        av_async_enable(1);
}
int altav_is_enabled(void)
{
    int enabled;
    pthread_mutex_lock(&g_transport.lock);
    enabled = g_transport.enabled;
    pthread_mutex_unlock(&g_transport.lock);
    return enabled;
}

/* Make `gen` the transport owner only when it is newer than the committed owner.
 * An accepted claim drops the previous owner's connection and cached config so the
 * new owner starts clean (its first frame reconnects + replays SPS/PPS). */
int altav_claim(uint32_t gen)
{
    int accepted = 0;
    pthread_mutex_lock(&g_transport.lock);
    /* MONOTONIC: only a strictly newer generation may take ownership.  A slower older
     * SETUP that runs altav_claim() AFTER a newer one must not reclaim the transport
     * (that would make the newer receiver see itself superseded and exit).  g_alt_gen_counter
     * is monotonic under alt_lock, so a later SETUP always has a higher gen; no wrap over a
     * dio lifetime. */
    if (gen > g_transport.owner_gen) {
        if (g_transport.fd >= 0) {
            close(g_transport.fd);
            g_transport.fd = -1;
        }
        g_transport.need_config = 0;
        g_transport.io_epoch = 0;
        alt_control_reset(&g_transport.control);
        g_transport.config_len = 0;
        g_transport.owner_gen = gen;
        pthread_mutex_lock(&g_owner.lock);
        g_owner.generation = gen;
        pthread_mutex_unlock(&g_owner.lock);
        accepted = 1;
        LOG_INFO(LOG_MODULE, "transport owner -> gen=%u", (unsigned)gen);
    } else {
        LOG_INFO(LOG_MODULE, "transport claim gen=%u ignored (owner=%u newer)", (unsigned)gen,
                 (unsigned)g_transport.owner_gen);
    }
    pthread_mutex_unlock(&g_transport.lock);
    if (accepted)
        av_async_reset(gen);
    return accepted;
}

int altav_is_owner(uint32_t gen)
{
    int owner;
    pthread_mutex_lock(&g_owner.lock);
    owner = gen != 0 && (g_owner.generation == gen);
    pthread_mutex_unlock(&g_owner.lock);
    return owner;
}

/* Read a complete greeting within one absolute sub-deadline. Partial bytes
 * consumed before a legacy fallback stay in the idle parser's buffer, so a
 * late remainder cannot be mistaken for a new control record. Caller holds
 * g_transport.lock; the producer's queue mutex is never held while waiting. */
static int av_negotiate_locked(int fd, uint64_t transaction_deadline) {
    uint64_t now = alt_timing_now_us(), greeting_deadline;
    g_transport.wire_v2 = g_transport.status_supported = g_transport.ack_pending = 0;
    alt_control_reset(&g_transport.control);
    if (!now || now >= transaction_deadline)
        return 0;
    greeting_deadline = now + UINT64_C(40000);
    if (greeting_deadline > transaction_deadline)
        greeting_deadline = transaction_deadline;
    while (g_transport.control.used < ALT_WIRE_HELLO_SIZE) {
        struct pollfd p = {fd, POLLIN, 0};
        int ready;
        ssize_t got;
        now = alt_timing_now_us();
        if (!now || now >= greeting_deadline)
            return 0;
        ready = poll(&p, 1, (int)((greeting_deadline - now + 999u) / 1000u));
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready == 0)
            return 0;
        if (ready < 0 || (p.revents & (POLLERR | POLLNVAL)))
            return -1;
        /* The worker may have been descheduled while poll returned readable.
         * Do not negotiate bytes read after the greeting/transaction budget. */
        now = alt_timing_now_us();
        if (!now || now >= greeting_deadline)
            return 0;
        if (!(p.revents & (POLLIN | POLLHUP)))
            continue;
        got = recv(fd, g_transport.control.bytes + g_transport.control.used,
                   ALT_WIRE_HELLO_SIZE - g_transport.control.used, MSG_DONTWAIT);
        if (got < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
            continue;
        if (got <= 0)
            return -1;
        {
            alt_control_record_t record;
            int parsed = alt_control_commit(&g_transport.control, (size_t)got, &record);
            if (parsed < 0 || (g_transport.control.used >= 4 &&
                               alt_wire_u32(g_transport.control.bytes) != ALT_WIRE_HELLO))
                return -1;
            if (!parsed)
                continue;
            if (record.magic != ALT_WIRE_HELLO)
                return -1;
            now = alt_timing_now_us();
            if (!now || now >= greeting_deadline)
                return 0;
            g_transport.wire_v2 = 1;
            g_transport.status_supported = record.value == ALT_HELLO_STATUS_CAPABILITY;
            g_transport.ack_pending = g_transport.status_supported;
            return 1;
        }
    }

    return 1;
}

/* caller holds g_transport.lock */
static void av_connect_locked(uint64_t deadline_us) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int original_flags;
    int connected = 0;
    if (fd < 0) return;
    /* Only the forwarding worker performs this I/O. Nonblocking vector writes
     * retry EAGAIN within the shared transaction deadline. */
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;   /* 500 ms */
    int sndbuf = 512 * 1024;
    int nodelay = 1;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    /* Keep the historical delayed-ACK mitigation even with batched header/AU
     * writes. Partial writes and the old QNX io-pkt stack still need NODELAY. */
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)g_transport.port);
    inet_pton(AF_INET, g_transport.host, &sa.sin_addr);
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
        if (av_negotiate_locked(fd, deadline_us) < 0) {
            close(fd);
            alt_control_reset(&g_transport.control);
            return;
        }
        g_transport.fd = fd;
        g_transport.need_config = 1;
        LOG_INFO(LOG_MODULE, "connected to %s:%d (fd=%d)", g_transport.host, g_transport.port, fd);
        /* resend cached SPS/PPS first — the render's fresh decoder flags msg #1 as CODECCONFIG */
        /* Config replay is performed by av_send_locked() below. */
    } else {
        close(fd);
    }
}

/* Complete a two-vector record without duplicating the payload. One absolute
 * deadline survives partial headers, body writes, EINTR and EAGAIN. SIGPIPE is
 * ignored by the framework; QNX io-pkt does not support MSG_NOSIGNAL. */
static int av_write_vectors_locked(struct iovec v[2], uint64_t deadline) {
    unsigned index = 0;
    while (index < 2) {
        uint64_t now = alt_timing_now_us();
        ssize_t n;
        if (!now || now >= deadline)
            return -1;
        n = writev(g_transport.fd, v + index, (int)(2 - index));
        if (n > 0) {
            size_t remaining = v[index].iov_len + (index == 0 ? v[1].iov_len : 0);
            size_t used = (size_t)n;
            if (used > remaining)
                return -1;
            while (index < 2 && used >= v[index].iov_len) {
                used -= v[index].iov_len;
                ++index;
            }
            if (index < 2) {
                v[index].iov_base = (uint8_t *)v[index].iov_base + used;
                v[index].iov_len -= used;
            }
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd pfd = {g_transport.fd, POLLOUT, 0};
            int ready;
            now = alt_timing_now_us();
            if (!now || now >= deadline)
                return -1;
            ready = poll(&pfd, 1, (int)((deadline - now + 999u) / 1000u));
            if (ready < 0 && errno == EINTR)
                continue;
            if (ready > 0 && (pfd.revents & POLLOUT) &&
                !(pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))
                continue;
        }
        return -1;
    }
    return 0;
}
static int av_send_record_locked(const uint8_t *buf, uint32_t len, const alt_frame_meta_t *meta,
                                 uint64_t deadline) {
    uint8_t header[ALT_WIRE_HEADER_SIZE];
    uint32_t size = 4;
    struct iovec v[2];
    if (g_transport.wire_v2 && meta) {
        alt_wire_encode(header, len, meta);
        size = sizeof(header);
    } else
        alt_wire_put32(header, len);
    v[0].iov_base = header;
    v[0].iov_len = size;
    v[1].iov_base = (void *)buf;
    v[1].iov_len = len;
    return av_write_vectors_locked(v, deadline);
}
/* One deadline covers connect, greeting, config replay, header and body. */
static int av_send_locked(const uint8_t *buf, uint32_t len, const alt_frame_meta_t *meta) {
    uint64_t now = alt_timing_now_us(), deadline;
    int opened = 0;
    if (!now)
        return -1;
    deadline = now + (uint64_t)AV_SEND_STALL_LIMIT_MS * 1000u;
    if (g_transport.fd < 0) {
        av_connect_locked(deadline);
        if (g_transport.fd >= 0)
            opened = 1;
    }
    if (g_transport.fd < 0)
        return -1;
    if (!meta) {
        g_transport.wire_v2 = 0;
        g_transport.status_supported = 0;
        g_transport.ack_pending = 0;
    } /* legacy API remains legacy even with a new renderer */
    if (g_transport.ack_pending) {
        uint8_t ack[ALT_WIRE_ACK_SIZE];
        struct iovec v[2];
        alt_control_record_t record = {ALT_WIRE_ACK, g_transport.owner_gen,
                                       ALT_CONTROL_ACK_CAPABILITY};
        if (!alt_control_encode(ack, sizeof(ack), &record))
            goto fail;
        v[0].iov_base = ack;
        v[0].iov_len = sizeof(ack);
        v[1].iov_base = NULL;
        v[1].iov_len = 0;
        if (av_write_vectors_locked(v, deadline) != 0)
            goto fail;
        g_transport.ack_pending = 0;
    }
    if (g_transport.need_config && g_transport.config_len) {
        alt_frame_meta_t config;
        memset(&config, 0, sizeof(config));
        config.version = ALT_WIRE_VERSION;
        config.generation = g_transport.owner_gen;
        config.flags = ALT_WIRE_CONFIG;
        if (av_send_record_locked(g_transport.config, g_transport.config_len, &config, deadline) !=
            0)
            goto fail;
        g_transport.need_config = 0;
        if (buf == g_transport.config && len == g_transport.config_len)
            return opened;
    }
    if (av_send_record_locked(buf, len, meta, deadline) != 0)
        goto fail;
    return opened;
fail:
    close(g_transport.fd);
    g_transport.fd = -1;
    g_transport.need_config = 0;
    return -1;
}

/* Cache the Annex-B SPS/PPS. The phone can repeat an identical avcC during
 * startup; feeding it to a running Qualcomm AVC decoder can reset references
 * immediately after its first picture. A genuine codec change needs a fresh
 * decoder and IDR, so end the old link rather than reconfigure it in place. */
static void av_set_config(uint32_t gen, const uint8_t *annexb_cfg, uint32_t len) {
    if (!annexb_cfg || len == 0 || len > sizeof(g_transport.config))
        return;
    pthread_mutex_lock(&g_transport.lock);
    if (g_transport.owner_gen == gen) { /* a superseded receiver must not replace the new config */
        if (g_transport.config_len != len || memcmp(g_transport.config, annexb_cfg, len) != 0) {
            int had_link = g_transport.fd >= 0;
            memcpy(g_transport.config, annexb_cfg, len);
            g_transport.config_len = len;
            if (had_link) {
                close(g_transport.fd);
                g_transport.fd = -1;
                g_transport.need_config = 0;
                LOG_INFO(LOG_MODULE,
                         "codec config changed gen=%u; reconnect on next IDR",
                         (unsigned)gen);
            }
        }
    }
    pthread_mutex_unlock(&g_transport.lock);
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

void altav_shutdown_if_owner(uint32_t gen)
{
    av_async_cancel(gen);
    pthread_mutex_lock(&g_transport.lock);
    if (g_transport.owner_gen == gen) {
        if (g_transport.fd >= 0) {
            close(g_transport.fd);
            g_transport.fd = -1;
        }
        g_transport.need_config = 0;
    }
    pthread_mutex_unlock(&g_transport.lock);
}

void altav_shutdown(void)
{
    av_async_stop();
    pthread_mutex_lock(&g_transport.lock);
    if (g_transport.fd >= 0) {
        close(g_transport.fd);
        g_transport.fd = -1;
    }
    g_transport.need_config = 0;
    free(g_transport.annexb);
    g_transport.annexb = NULL;
    g_transport.annexb_cap = 0;
    pthread_mutex_unlock(&g_transport.lock);
}

/* The reader never takes g_transport.lock (the socket writer's lock). Queue storage is
 * bounded and reused. An overflow discards the entire dependent chain, fences
 * the in-flight epoch and resumes only at config + IDR, not an arbitrary P-frame. */
#define AV_QUEUE_ITEMS 32u
#define AV_QUEUE_BYTES (8u * 1024u * 1024u)
#define AV_POOL_BYTES (4u * 1024u * 1024u)
typedef struct av_pending {
    struct av_pending *next;
    uint8_t *data;
    uint32_t cap, len, gen, epoch;
    uint8_t ls, annexb, idr;
    uint32_t annexb_len;
    uint64_t queued_us;
    alt_frame_meta_t meta;
} av_pending_t;
/* All queue, pool, condition and recovery fields use forward.lock. */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t default_cond, *cond;
#if !defined(__APPLE__)
    pthread_cond_t monotonic_cond;
    int cond_ready;
#endif
    pthread_t thread;
    av_pending_t *head, *tail, *pool;
    uint32_t generation, epoch, failure;
    unsigned count, pool_count;
    uint32_t bytes, pool_bytes;
    int started, stop, need_idr, busy, open, enabled;
    alt_recovery_policy_t recovery;
    uint8_t config[4096];
    uint32_t config_len;
    void (*failed_cb)(uint32_t, void *);
    void *cb_context;
} av_forward_state_t;
static av_forward_state_t g_forward = {.lock = PTHREAD_MUTEX_INITIALIZER,
                                       .default_cond = PTHREAD_COND_INITIALIZER,
                                       .cond = &g_forward.default_cond,
                                       .need_idr = 1,
                                       .enabled = 1};

static void av_pending_release_locked(av_pending_t *p) {
    if (g_forward.pool_count < AV_QUEUE_ITEMS && p->cap <= AV_POOL_BYTES - g_forward.pool_bytes) {
        p->next = g_forward.pool;
        g_forward.pool = p;
        ++g_forward.pool_count;
        g_forward.pool_bytes += p->cap;
    } else {
        free(p->data);
        free(p);
    }
}
static void av_pending_clear_locked(void) {
    while (g_forward.head) {
        av_pending_t *p = g_forward.head;
        g_forward.head = p->next;
        av_pending_release_locked(p);
    }
    g_forward.tail = NULL;
    g_forward.count = g_forward.bytes = 0;
}
static void av_async_reset(uint32_t gen) {
    pthread_mutex_lock(&g_forward.lock);
    if (gen > g_forward.generation) {
        g_forward.generation = gen;
        alt_recovery_reset(&g_forward.recovery, gen);
        g_forward.open = 1;
        ++g_forward.epoch;
        g_forward.failure = 0;
        g_forward.need_idr = 1;
        g_forward.config_len = 0;
        av_pending_clear_locked();
    }
    pthread_mutex_unlock(&g_forward.lock);
}
static void av_async_cancel(uint32_t gen) {
    pthread_mutex_lock(&g_forward.lock);
    if (gen == g_forward.generation) {
        g_forward.open = 0;
        ++g_forward.epoch;
        g_forward.need_idr = 1;
        av_pending_clear_locked();
    }
    pthread_mutex_unlock(&g_forward.lock);
}
static void av_async_enable(int on) {
    pthread_mutex_lock(&g_forward.lock);
    g_forward.enabled = on ? 1 : 0;
    if (!on) {
        ++g_forward.epoch;
        g_forward.need_idr = 1;
        av_pending_clear_locked();
    }
    pthread_mutex_unlock(&g_forward.lock);
}
static void av_transport_reset(uint32_t gen) {
    pthread_mutex_lock(&g_transport.lock);
    if (gen == g_transport.owner_gen) {
        if (g_transport.fd >= 0)
            close(g_transport.fd);
        g_transport.fd = -1;
        g_transport.need_config = 0;
    }
    pthread_mutex_unlock(&g_transport.lock);
}
/* State mutex held. Install a separate monotonic condition once, before the
 * first worker starts. The default condition stays valid if initialization
 * fails or shutdown is called before start; no live condition is destroyed. */
static int av_async_condition_init_locked(void) {
#if defined(__APPLE__)
    return 0; /* Darwin provides a relative timed wait instead of setclock. */
#else
    pthread_condattr_t attr;
    int rc;
    if (g_forward.cond_ready)
        return 0;
    rc = pthread_condattr_init(&attr);
    if (rc)
        return rc;
    rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (!rc)
        rc = pthread_cond_init(&g_forward.monotonic_cond, &attr);
    pthread_condattr_destroy(&attr);
    if (!rc) {
        g_forward.cond = &g_forward.monotonic_cond;
        g_forward.cond_ready = 1;
    }
    return rc;
#endif
}
static void av_async_wait_locked(long ms) {
    struct timespec deadline;
#if defined(__APPLE__)
    deadline.tv_sec = ms / 1000;
    deadline.tv_nsec = (ms % 1000) * 1000000L;
    (void)pthread_cond_timedwait_relative_np(g_forward.cond, &g_forward.lock, &deadline);
#else
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        /* Clock failure must not spin under the state mutex. Producer/stop
         * signals can still wake the worker while its timing clock is absent. */
        (void)pthread_cond_wait(g_forward.cond, &g_forward.lock);
        return;
    }
    deadline.tv_sec += ms / 1000;
    deadline.tv_nsec += (ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    (void)pthread_cond_timedwait(g_forward.cond, &g_forward.lock, &deadline);
#endif
}
/* Queue mutex held. All failures join one episode, rather than resetting the
 * keyframe budget whenever a fresh IDR fails. */
static void av_fail_locked(uint32_t gen, int transport) {
    ++g_forward.epoch;
    g_forward.failure = gen;
    g_forward.need_idr = 1;
    av_pending_clear_locked();
    alt_recovery_fault(&g_forward.recovery, alt_timing_now_us(), transport);
}
static void av_request_recovery(uint32_t gen) {
    void (*cb)(uint32_t, void *) = NULL;
    void *context = NULL;
    pthread_mutex_lock(&g_forward.lock);
    if (gen && gen == g_forward.generation && g_forward.open && g_forward.enabled &&
        !g_forward.stop && g_forward.recovery.active && g_forward.failed_cb &&
        alt_recovery_request(&g_forward.recovery, gen, ALT_KEYFRAME_RECOVERY,
                             alt_timing_now_us())) {
        cb = g_forward.failed_cb;
        context = g_forward.cb_context;
    }
    pthread_mutex_unlock(&g_forward.lock);
    if (cb)
        cb(gen, context);
}
/* This worker owns socket I/O. The queue/reader is never held during polling.
 * Drain late HELLO and framed presentation status, including fragmented records;
 * only EOF/error means loss. Silence is a valid static scene. */
static void av_idle_tick(uint32_t gen, uint32_t epoch) {
    int lost = 0;
    unsigned messages = 0;
    pthread_mutex_lock(&g_transport.lock);
    if (gen == g_transport.owner_gen && g_transport.fd >= 0) {
        if (g_transport.io_epoch != epoch) {
            close(g_transport.fd);
            g_transport.fd = -1;
            alt_control_reset(&g_transport.control);
        } else {
            struct pollfd p = {g_transport.fd, POLLIN, 0};
            int ready = poll(&p, 1, 0);
            if (ready > 0 && (p.revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))) {
                while (messages < 8 && !lost) {
                    unsigned needed = alt_control_needed(&g_transport.control);
                    alt_control_record_t record;
                    ssize_t n;
                    int parsed;
                    if (!needed || (g_transport.control.used >= 4 &&
                                    alt_wire_u32(g_transport.control.bytes) != ALT_WIRE_HELLO &&
                                    alt_wire_u32(g_transport.control.bytes) != ALT_WIRE_STATUS)) {
                        lost = 1;
                        break;
                    }
                    n = recv(g_transport.fd, g_transport.control.bytes + g_transport.control.used,
                             needed, MSG_DONTWAIT);
                    if (n == 0) {
                        lost = 1;
                        break;
                    }
                    if (n < 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                            lost = 1;
                        break;
                    }
                    parsed = alt_control_commit(&g_transport.control, (size_t)n, &record);
                    if (parsed < 0 ||
                        (g_transport.control.used >= 4 &&
                         alt_wire_u32(g_transport.control.bytes) != ALT_WIRE_HELLO &&
                         alt_wire_u32(g_transport.control.bytes) != ALT_WIRE_STATUS)) {
                        lost = 1;
                        break;
                    }
                    if (!parsed)
                        continue;
                    if (record.magic == ALT_WIRE_STATUS) {
                        if (!g_transport.status_supported || record.generation != gen)
                            lost = 1;
                        else {
                            pthread_mutex_lock(&g_forward.lock);
                            if (gen == g_forward.generation && epoch == g_forward.epoch &&
                                g_forward.open)
                                alt_recovery_health(&g_forward.recovery, alt_timing_now_us(),
                                                    (int)record.value);
                            pthread_mutex_unlock(&g_forward.lock);
                        }
                    } else if (record.magic != ALT_WIRE_HELLO)
                        lost = 1;
                    ++messages;
                }
            }
        }
        if (lost) {
            close(g_transport.fd);
            g_transport.fd = -1;
            alt_control_reset(&g_transport.control);
        }
    }
    pthread_mutex_unlock(&g_transport.lock);
    if (lost) {
        pthread_mutex_lock(&g_forward.lock);
        if (gen == g_forward.generation && epoch == g_forward.epoch && g_forward.open &&
            g_forward.enabled && !g_forward.stop)
            av_fail_locked(gen, 1);
        pthread_mutex_unlock(&g_forward.lock);
    }
    av_request_recovery(gen);
}
static int av_forward_prepared(av_pending_t *p) {
    int result = -1;
    const uint8_t *bytes = p->data;
    uint32_t size = p->len;
    pthread_mutex_lock(&g_transport.lock);
    pthread_mutex_lock(&g_forward.lock);
    {
        int current = p->gen == g_forward.generation && p->epoch == g_forward.epoch &&
                      g_forward.open && g_forward.enabled && !g_forward.stop;
        pthread_mutex_unlock(&g_forward.lock);
        if (!current) {
            pthread_mutex_unlock(&g_transport.lock);
            return -1;
        }
    }
    if (g_transport.owner_gen == p->gen && g_transport.enabled && g_transport.config_len &&
        (g_transport.fd >= 0 || p->idr)) {
        if (!p->annexb) {
            size = p->annexb_len;
            if (size > g_transport.annexb_cap) {
                uint8_t *grown = (uint8_t *)realloc(g_transport.annexb, size);
                if (grown) {
                    g_transport.annexb = grown;
                    g_transport.annexb_cap = size;
                }
            }
            if (size > g_transport.annexb_cap ||
                avcc_to_annexb(p->data, p->len, p->ls, g_transport.annexb, g_transport.annexb_cap,
                               NULL) != size)
                goto done;
            bytes = g_transport.annexb;
        }
        g_transport.io_epoch = p->epoch;
        result = av_send_locked(bytes, size, &p->meta);
        if (result >= 0 && !g_transport.status_supported) {
            pthread_mutex_lock(&g_forward.lock);
            if (p->gen == g_forward.generation && p->epoch == g_forward.epoch) {
                g_forward.recovery.active = 0;
                g_forward.recovery.requests = g_forward.recovery.connect_failures = 0;
                g_forward.recovery.request_due_us = g_forward.recovery.connect_due_us = 0;
            }
            pthread_mutex_unlock(&g_forward.lock);
        }
    }
done:
    pthread_mutex_unlock(&g_transport.lock);
    return result;
}
static void *av_forward_worker(void *unused) {
    uint32_t previous_epoch = 0;
    unsigned attempts = 0, delivered = 0;
    alt_timing_stat_t queue_wait = {0}, work = {0};
    (void)unused;
    for (;;) {
        av_pending_t *p = NULL;
        uint8_t config[4096];
        uint32_t config_len, gen, epoch;
        uint64_t started;
        int rc;
        pthread_mutex_lock(&g_forward.lock);
        if (!g_forward.stop &&
            (!g_forward.head || alt_timing_now_us() < g_forward.recovery.connect_due_us)) {
            av_async_wait_locked(100);
        }
        if (g_forward.stop) {
            pthread_mutex_unlock(&g_forward.lock);
            return NULL;
        }
        gen = g_forward.generation;
        epoch = g_forward.epoch;
        if (g_forward.head && alt_timing_now_us() >= g_forward.recovery.connect_due_us) {
            p = g_forward.head;
            g_forward.head = p->next;
            if (!g_forward.head)
                g_forward.tail = NULL;
            g_forward.busy = 1;
            --g_forward.count;
            g_forward.bytes -= p->cap;
            config_len = g_forward.config_len;
            memcpy(config, g_forward.config, config_len);
        } else
            config_len = 0;
        pthread_mutex_unlock(&g_forward.lock);
        if (!p) {
            av_idle_tick(gen, epoch);
            continue;
        }
        if (previous_epoch != p->epoch) {
            av_transport_reset(p->gen);
            previous_epoch = p->epoch;
            attempts = delivered = 0;
            memset(&queue_wait, 0, sizeof(queue_wait));
            memset(&work, 0, sizeof(work));
        }
        av_idle_tick(p->gen, p->epoch);
        started = alt_timing_now_us();
        alt_timing_elapsed(&queue_wait, p->queued_us, started);
        av_set_config(p->gen, config, config_len);
        rc = av_forward_prepared(p);
        alt_timing_elapsed(&work, started, alt_timing_now_us());
        ++attempts;
        if (rc >= 0)
            ++delivered;
        if (attempts >= 300u) {
            if (ENABLE_ALTSCREEN_PACE_TRACE || log_verbose())
                log_write(
                    LOG_LEVEL_WARN, "altscreen",
                    "forward pace gen=%u attempted=%u sent=%u queue_p95_us=%llu work_p95_us=%llu",
                    p->gen, attempts, delivered,
                    (unsigned long long)alt_timing_percentile(&queue_wait, 95),
                    (unsigned long long)alt_timing_percentile(&work, 95));
            attempts = delivered = 0;
            memset(&queue_wait, 0, sizeof(queue_wait));
            memset(&work, 0, sizeof(work));
        }
        pthread_mutex_lock(&g_forward.lock);
        gen = p->gen;
        if (rc < 0 && p->gen == g_forward.generation && p->epoch == g_forward.epoch &&
            !g_forward.stop && g_forward.open && g_forward.enabled)
            av_fail_locked(gen, 1);
        av_pending_release_locked(p);
        g_forward.busy = 0;
        pthread_cond_broadcast(g_forward.cond);
        pthread_mutex_unlock(&g_forward.lock);
        av_request_recovery(gen);
    }
}
int altav_async_start(void (*failed)(uint32_t, void *), void *context) {
    int rc = 0;
    pthread_mutex_lock(&g_forward.lock);
    if (!g_forward.started) {
        g_forward.stop = 0;
        g_forward.failed_cb = failed;
        g_forward.cb_context = context;
        rc = av_async_condition_init_locked();
        if (!rc)
            rc = pthread_create(&g_forward.thread, NULL, av_forward_worker, NULL);
        if (!rc)
            g_forward.started = 1;
    }
    pthread_mutex_unlock(&g_forward.lock);
    return rc ? -1 : 0;
}
int altav_enqueue_config(uint32_t gen, const uint8_t *config, uint32_t len) {
    int rc = -1;
    if (!config || !len || len > sizeof(g_forward.config))
        return -1;
    pthread_mutex_lock(&g_forward.lock);
    if (g_forward.started && !g_forward.stop && g_forward.open && g_forward.enabled &&
        gen == g_forward.generation) {
        if (len != g_forward.config_len || memcmp(config, g_forward.config, len)) {
            if (g_forward.config_len)
                alt_recovery_fault(&g_forward.recovery, alt_timing_now_us(), 0);
            memcpy(g_forward.config, config, len);
            g_forward.config_len = len;
            ++g_forward.epoch;
            g_forward.need_idr = 1;
            av_pending_clear_locked();
            pthread_cond_signal(g_forward.cond);
        }
        rc = 0;
    }
    pthread_mutex_unlock(&g_forward.lock);
    return rc;
}
int altav_enqueue_owned(uint32_t gen, uint8_t **data, uint32_t *capacity,
                        const alt_avcc_frame_t *info, const alt_frame_meta_t *meta) {
    av_pending_t *p = NULL;
    int rc = -1;
    if (!data || !*data || !capacity || !info || !info->input_len || info->input_len > *capacity ||
        *capacity > ALT_WIRE_MAX_PAYLOAD || !info->annexb_len ||
        info->annexb_len > ALT_WIRE_MAX_PAYLOAD || info->length_size < 1 || info->length_size > 4 ||
        !meta || !meta->sequence)
        return -1;
    pthread_mutex_lock(&g_forward.lock);
    if (!g_forward.started || g_forward.stop || !g_forward.open || !g_forward.enabled ||
        gen != g_forward.generation || !g_forward.config_len)
        goto out;
    if (g_forward.count >= AV_QUEUE_ITEMS || *capacity > AV_QUEUE_BYTES - g_forward.bytes)
        goto loss;
    if (g_forward.need_idr && !info->idr)
        goto out;
    p = g_forward.pool;
    if (p) {
        g_forward.pool = p->next;
        --g_forward.pool_count;
        g_forward.pool_bytes -= p->cap;
    } else
        p = (av_pending_t *)calloc(1, sizeof(*p));
    if (!p)
        goto loss;
    {
        uint8_t *spare = p->data;
        uint32_t spare_cap = p->cap;
        p->data = *data;
        p->cap = *capacity;
        *data = spare;
        *capacity = spare_cap;
    }
    p->len = info->input_len;
    p->ls = info->length_size;
    p->annexb = info->annexb;
    p->idr = info->idr;
    p->annexb_len = info->annexb_len;
    p->gen = gen;
    p->epoch = g_forward.epoch;
    p->meta = *meta;
    p->meta.version = ALT_WIRE_VERSION;
    p->meta.generation = gen;
    p->meta.flags = info->idr ? ALT_WIRE_IDR : 0;
    p->queued_us = alt_timing_now_us();
    p->next = NULL;
    if (g_forward.tail)
        g_forward.tail->next = p;
    else
        g_forward.head = p;
    g_forward.tail = p;
    ++g_forward.count;
    g_forward.bytes += p->cap;
    g_forward.need_idr = 0;
    pthread_cond_signal(g_forward.cond);
    rc = 0;
    goto out;
loss:
    av_fail_locked(gen, 0);
    pthread_cond_signal(g_forward.cond);
out:
    pthread_mutex_unlock(&g_forward.lock);
    return rc;
}
int altav_keyframe_permit(uint32_t gen, unsigned kind) {
    int permit = 0;
    pthread_mutex_lock(&g_forward.lock);
    if (gen == g_forward.generation && g_forward.open && g_forward.enabled && !g_forward.stop)
        permit = alt_recovery_request(&g_forward.recovery, gen, kind, alt_timing_now_us());
    pthread_cond_signal(g_forward.cond);
    pthread_mutex_unlock(&g_forward.lock);
    return permit;
}
void altav_note_input_loss(uint32_t gen) {
    pthread_mutex_lock(&g_forward.lock);
    if (gen == g_forward.generation && g_forward.open && g_forward.enabled && !g_forward.stop) {
        av_fail_locked(gen, 0);
        pthread_cond_signal(g_forward.cond);
    }
    pthread_mutex_unlock(&g_forward.lock);
}
int altav_take_failure(uint32_t gen) {
    int failed;
    pthread_mutex_lock(&g_forward.lock);
    failed = gen != 0 && g_forward.failure == gen && gen == g_forward.generation;
    if (failed)
        g_forward.failure = 0;
    pthread_mutex_unlock(&g_forward.lock);
    return failed;
}
static void av_async_stop(void) {
    int started;
    pthread_mutex_lock(&g_forward.lock);
    g_forward.stop = 1;
    g_forward.open = 0;
    av_pending_clear_locked();
    started = g_forward.started;
    pthread_cond_broadcast(g_forward.cond);
    pthread_mutex_unlock(&g_forward.lock);
    /* Nonblocking socket sends have one bounded transaction deadline. */
    if (started)
        pthread_join(g_forward.thread, NULL);
    pthread_mutex_lock(&g_forward.lock);
    g_forward.started = 0;
    while (g_forward.pool) {
        av_pending_t *p = g_forward.pool;
        g_forward.pool = p->next;
        free(p->data);
        free(p);
    }
    g_forward.pool_count = g_forward.pool_bytes = 0;
    pthread_mutex_unlock(&g_forward.lock);
}
