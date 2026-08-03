/*
 * altscreen_av — AltScreen H.264 forward (hook side). See altscreen_av.h.
 * Ported from mhi3-altscreen libaltscreen_hook_ultra.c forward path; adapted to
 * MHI2Q (ARM32 QNX), no __thread, lazy-connect to the decoder process.
 */
#include "../framework/common.h"
#include "../framework/logging.h"
#include "altscreen_av.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

DEFINE_LOG_MODULE(altscreen_av);

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static char            g_host[64] = ALTAV_DEFAULT_HOST;
static int             g_port     = ALTAV_DEFAULT_PORT;
static int             g_enabled  = 1;
static int             g_fd       = -1;
static pthread_mutex_t g_lock     = PTHREAD_MUTEX_INITIALIZER;
/* Cached Annex-B SPS/PPS.  The renderer marks the FIRST message on each connection as
 * OMX_BUFFERFLAG_CODECCONFIG, so on every (re)connect we must resend the config BEFORE any frame —
 * otherwise a reconnect mid-stream sends a video frame first, the decoder takes it as config and
 * reports OMX_ErrorFormatNotDetected → black.  (Set via altav_set_config on each avcC.) */
static uint8_t         g_cfg[512];
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

void altav_set_enabled(int on) { g_enabled = on ? 1 : 0; }
int  altav_is_enabled(void)    { return g_enabled; }

/* caller holds g_lock */
static void av_connect_locked(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    /* Bounded send: a hung/slow decoder must never block the libairplay frame
     * thread (which would also stall teardown).  On timeout send() returns short
     * → av_send_locked drops the fd.  Localhost connect never blocks. */
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100000;   /* 100 ms */
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)g_port);
    inet_pton(AF_INET, g_host, &sa.sin_addr);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
        g_fd = fd;
        LOG_INFO(LOG_MODULE, "connected to %s:%d (fd=%d)", g_host, g_port, fd);
        /* resend cached SPS/PPS first — the render's fresh decoder flags msg #1 as CODECCONFIG */
        if (g_cfg_len) {
            uint32_t be = htonl(g_cfg_len);
            if (send(g_fd, &be, 4, MSG_NOSIGNAL) != 4 ||
                send(g_fd, g_cfg, g_cfg_len, MSG_NOSIGNAL) != (ssize_t)g_cfg_len) {
                close(g_fd); g_fd = -1;      /* config send failed → drop, next frame retries */
            } else {
                LOG_INFO(LOG_MODULE, "replayed SPS/PPS config (%u bytes) on new connection", g_cfg_len);
            }
        }
    } else {
        close(fd);
    }
}

/* caller holds g_lock; sends [u32 BE len][buf]. drops fd on error. */
static void av_send_locked(const uint8_t *buf, uint32_t len)
{
    if (g_fd < 0) av_connect_locked();
    if (g_fd < 0) return;
    uint32_t be = htonl(len);
    ssize_t r1 = send(g_fd, &be, 4, MSG_NOSIGNAL);
    ssize_t r2 = (r1 == 4) ? send(g_fd, buf, len, MSG_NOSIGNAL) : -1;
    if (r1 != 4 || r2 != (ssize_t)len) {
        close(g_fd);
        g_fd = -1;
    }
}

void altav_forward_frame(const uint8_t *avcc_data, uint32_t len)
{
    if (!g_enabled || !avcc_data || len == 0) return;
    pthread_mutex_lock(&g_lock);
    av_send_locked(avcc_data, len);
    pthread_mutex_unlock(&g_lock);
}

/* Cache the Annex-B SPS/PPS AND send it now.  Cached copy is replayed on every reconnect
 * (av_connect_locked) so the render's decoder always gets config before frames. */
void altav_set_config(const uint8_t *annexb_cfg, uint32_t len)
{
    if (!g_enabled || !annexb_cfg || len == 0 || len > sizeof(g_cfg)) return;
    pthread_mutex_lock(&g_lock);
    memcpy(g_cfg, annexb_cfg, len);
    g_cfg_len = len;
    av_send_locked(annexb_cfg, len);
    pthread_mutex_unlock(&g_lock);
}

/* Convert AVCC (length_size-byte BE length prefixes) → Annex-B (00 00 00 01
 * start codes).  All arithmetic uses subtraction form so hostile length fields
 * cannot wrap: `nal > inlen - i` never underflows (loop keeps i <= inlen).
 * Lenient like stock (stops at the first malformed NAL, forwarding the clean
 * prefix — never over-reads).  `out == NULL` = measure only.  Returns the exact
 * Annex-B byte count (identical for measure and write passes → exact alloc). */
static uint32_t avcc_to_annexb(const uint8_t *in, uint32_t inlen, uint8_t ls,
                               uint8_t *out, uint32_t outcap)
{
    uint32_t i = 0, o = 0;
    if (ls < 1 || ls > 4) ls = 4;
    while (inlen - i >= ls) {                            /* i <= inlen invariant */
        uint32_t nal = 0, k;
        for (k = 0; k < ls; k++) nal = (nal << 8) | in[i + k];
        i += ls;                                         /* i <= inlen (checked above) */
        if (nal == 0 || nal > inlen - i) break;          /* truncated / garbage → stop */
        if (o > 0xFFFFFFFFu - 4 - nal) break;            /* output size overflow */
        if (out) {
            if (o + 4 + nal > outcap) break;             /* never happens on the write pass */
            out[o] = 0; out[o + 1] = 0; out[o + 2] = 0; out[o + 3] = 1;
            memcpy(out + o + 4, in + i, nal);
        }
        o += 4 + nal;
        i += nal;
    }
    return o;
}

void altav_forward_avcc_frame(const uint8_t *avcc, uint32_t len, uint8_t ls)
{
    if (!g_enabled || !avcc || len == 0) return;
    uint32_t need = avcc_to_annexb(avcc, len, ls, NULL, 0);   /* pass 1: exact size */
    if (!need) return;
    uint8_t *ab = (uint8_t *)malloc(need);
    if (!ab) return;
    uint32_t n = avcc_to_annexb(avcc, len, ls, ab, need);     /* pass 2: fill */
    if (n) altav_forward_frame(ab, n);
    free(ab);
}

void altav_shutdown(void)
{
    pthread_mutex_lock(&g_lock);
    if (g_fd >= 0) { close(g_fd); g_fd = -1; }
    pthread_mutex_unlock(&g_lock);
}
