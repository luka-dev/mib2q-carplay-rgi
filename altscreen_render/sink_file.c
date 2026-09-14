/*
 * altscreen_render — host sink (Mac/desktop).
 *
 * Accounts every received Annex-B payload so the hook→forward→receiver path can
 * be exercised without the OMX/cluster hardware.  It writes nothing.
 *
 * Built with -DALTSCREEN_CAPTURE (`make CAPTURE=1`) it also appends each payload
 * to altscreen_capture.h264.  Payloads are start-code framed and arrive in decode
 * order (SPS/PPS first), so the file is a valid Annex-B elementary stream:
 *     ffplay altscreen_capture.h264      # or: ffmpeg -i ... out.mp4
 * That is a debugging tool, never a default: an unattended run must not grow a
 * file on disk for as long as a session lasts.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "sink.h"

#include <stdio.h>
#include <pthread.h>

static unsigned long g_bytes;
static unsigned long g_msgs;
static pthread_mutex_t g_ready_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_ready;

#ifdef ALTSCREEN_CAPTURE
static FILE *g_out;
#endif

int av_sink_open(void) {
#ifdef ALTSCREEN_CAPTURE
    const char *path = "altscreen_capture.h264";
    g_out = fopen(path, "wb");
    if (!g_out) { fprintf(stderr, "altscreen_render: cannot open %s\n", path); return -1; }
    fprintf(stderr, "altscreen_render: capturing Annex-B to %s\n", path);
#endif
    return 0;
}

int av_sink_frame(const uint8_t *annexb, uint32_t len, uint64_t received_ms) {
    (void)received_ms;
    if (!annexb || len == 0) return -1;
#ifdef ALTSCREEN_CAPTURE
    if (g_out) {
        if (fwrite(annexb, 1, len, g_out) != len) {
            fprintf(stderr, "altscreen_render: write error\n");
            return -1;
        }
        fflush(g_out);
    }
#endif
    g_bytes += len;
    g_msgs++;
    pthread_mutex_lock(&g_ready_lock);
    g_ready = 1;
    pthread_mutex_unlock(&g_ready_lock);
    if ((g_msgs & 0x3f) == 0)
        fprintf(stderr, "altscreen_render: %lu msgs, %lu bytes\n", g_msgs, g_bytes);
    return 0;
}

void av_sink_set_layout(int mode, int style, int rawSkin) {
    (void)mode; (void)style; (void)rawSkin;
}

/* The capture backend has no display swap; a successfully written payload is
 * its equivalent readiness edge for desktop protocol tests. */
int av_sink_video_ready(void) {
    int ready;
    pthread_mutex_lock(&g_ready_lock);
    ready = g_ready;
    pthread_mutex_unlock(&g_ready_lock);
    return ready;
}

void av_sink_session_end(void) {
    pthread_mutex_lock(&g_ready_lock);
    g_ready = 0;
    pthread_mutex_unlock(&g_ready_lock);
    fprintf(stderr, "altscreen_render: session end (%lu msgs, %lu bytes)\n", g_msgs, g_bytes);
}

void av_sink_close(void) {
#ifdef ALTSCREEN_CAPTURE
    if (g_out) { fclose(g_out); g_out = NULL; }
#endif
}
