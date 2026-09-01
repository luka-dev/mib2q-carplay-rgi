/*
 * altscreen_render — file-capture sink (Mac/debug).
 *
 * Appends every received Annex-B payload to a .h264 file.  Because each payload
 * is start-code framed and they arrive in decode order (SPS/PPS first), the file
 * is a valid Annex-B elementary stream:
 *     ffplay altscreen_capture.h264      # or: ffmpeg -i ... out.mp4
 * This proves the entire hook→forward→receiver path end-to-end without the
 * OMX/cluster hardware.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "sink.h"

#include <stdio.h>

static FILE *g_out;
static unsigned long g_bytes;
static unsigned long g_msgs;

int av_sink_open(void) {
    const char *path = "altscreen_capture.h264";
    g_out = fopen(path, "wb");
    if (!g_out) { fprintf(stderr, "altscreen_render: cannot open %s\n", path); return -1; }
    fprintf(stderr, "altscreen_render: capturing Annex-B to %s\n", path);
    return 0;
}

void av_sink_frame(const uint8_t *annexb, uint32_t len) {
    if (!g_out || !annexb || len == 0) return;
    if (fwrite(annexb, 1, len, g_out) != len) {
        fprintf(stderr, "altscreen_render: write error\n");
        return;
    }
    fflush(g_out);
    g_bytes += len;
    g_msgs++;
    if ((g_msgs & 0x3f) == 0)
        fprintf(stderr, "altscreen_render: %lu msgs, %lu bytes\n", g_msgs, g_bytes);
}

void av_sink_set_layout(int mode, int style, int rawSkin) {
    (void)mode; (void)style; (void)rawSkin;
}

/* The capture backend has no display swap; a successfully written payload is
 * its equivalent readiness edge for desktop protocol tests. */
int av_sink_video_ready(void) { return g_msgs != 0; }

void av_sink_session_end(void) {
    fprintf(stderr, "altscreen_render: session end (%lu msgs, %lu bytes)\n", g_msgs, g_bytes);
}

void av_sink_close(void) {
    if (g_out) { fclose(g_out); g_out = NULL; }
}
