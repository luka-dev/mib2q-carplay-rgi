/*
 * altscreen_render — video sink interface.
 *
 * The receiver hands each Annex-B payload (in arrival order: SPS/PPS header
 * first, then frames) to the active sink.  Two implementations:
 *   - sink_file.c     : append the raw stream to a .h264 file (Mac/debug; the
 *                       whole hook→renderer wire is provable with ffplay).
 *   - sink_omx_qnx.c  : QNX — OMX AVC decode → GL texture → present on the
 *                       instrument-cluster surface (the real target).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_RENDER_SINK_H
#define ALTSCREEN_RENDER_SINK_H

#include <stdint.h>
#include "../common/alt_video_wire.h"

/* Called once at startup.  Returns 0 on success, <0 on fatal init failure. */
int  av_sink_open(void);

/* One Annex-B payload from the decoder worker. Returns 0 when accepted, <0
 * when decoder continuity was lost. The caller must end this connection so
 * the hook reconnects with config + a fresh IDR, never another delta frame.
 * received_ms is local CLOCK_MONOTONIC at socket receipt, before queue/startup
 * delays. Zero means unavailable; it never changes decoder order. */
int av_sink_frame(const uint8_t *annexb, uint32_t len, uint64_t received_ms);
int av_sink_frame_ex(const uint8_t *annexb, uint32_t len, uint64_t received_ms,
                     const alt_frame_meta_t *meta);

/* True only after the current session has produced and successfully presented
 * at least one decoded frame.  A TCP connection or decoder start is not visual
 * readiness: Java uses this edge to leave the stock cluster context. Called
 * by the control thread; the implementation must synchronize the snapshot
 * with its decode callbacks and suppress readiness during teardown. */
int  av_sink_video_ready(void);

/* Select the startup-loader geometry.  style: 0=Sport,
 * 1=Classic, 255=unknown (preserve the current/default Sport geometry).
 * rawSkin is diagnostic only and comes from stock LastmodeStorage. */
void av_sink_set_layout(int mode, int style, int rawSkin);

/* Called when the client disconnects (session end) — flush/reset decoder so the
 * next session starts clean.  The process keeps running to accept the next one. */
void av_sink_session_end(void);

/* Called once before process exit. */
void av_sink_close(void);

#endif /* ALTSCREEN_RENDER_SINK_H */
