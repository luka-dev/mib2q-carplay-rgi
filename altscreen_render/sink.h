/*
 * altscreen_render — video sink interface.
 *
 * The receiver hands each Annex-B payload (in arrival order: SPS/PPS header
 * first, then frames) to the active sink.  Two implementations:
 *   - sink_file.c     : append the raw stream to a .h264 file (Mac/debug; the
 *                       whole hook→renderer wire is provable with ffplay).
 *   - sink_omx_qnx.c  : QNX — OMX AVC decode → GL texture → present on the
 *                       instrument-cluster surface (the real target).
 */
#ifndef ALTSCREEN_RENDER_SINK_H
#define ALTSCREEN_RENDER_SINK_H

#include <stdint.h>

/* Called once at startup.  Returns 0 on success, <0 on fatal init failure. */
int  av_sink_open(void);

/* One Annex-B payload (start-code framed).  Called from the receive loop. */
void av_sink_frame(const uint8_t *annexb, uint32_t len);

/* True only after the current session has produced and successfully presented
 * at least one decoded frame.  A TCP connection or decoder start is not visual
 * readiness: Java uses this edge to leave the stock cluster context. */
int  av_sink_video_ready(void);

/* Select the Audi safe area used by the startup loader. */
void av_sink_set_view_area(int mode);   /* 0=fullscreen, 1=smallscreen */

/* Called when the client disconnects (session end) — flush/reset decoder so the
 * next session starts clean.  The process keeps running to accept the next one. */
void av_sink_session_end(void);

/* Called once before process exit. */
void av_sink_close(void);

#endif /* ALTSCREEN_RENDER_SINK_H */
