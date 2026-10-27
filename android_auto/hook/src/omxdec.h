/* SPDX-License-Identifier: GPL-3.0-or-later
 * Hardware H.264 decode for the cluster player (run 127: software 1080p decode used both MSM8x60 cores,
 * ~29 fps decoded but only 2.5-6 fps reached the cockpit). Qualcomm OpenMAX IL decoder, verified by
 * src/omx_probe.c on the car (1080p at ~42 fps, output 64x32-tiled NV12, src/detile.h).
 * QNX only; the host build uses the FFmpeg path.
 */
#ifndef SQ5_OMXDEC_H
#define SQ5_OMXDEC_H
struct AVFrame;
/* 0 = decoder running. Loads libOmxCore.so; failures are logged and the caller keeps FFmpeg. */
int omxdec_open(unsigned width, unsigned height);
/* One phone packet (Annex-B, one access unit or the codec config). 0 ok, -1 = decoder failed (use FFmpeg). */
int omxdec_feed(const unsigned char *data, unsigned size);
/* Presenter: newest decoded frame into f (planar YUV 4:2:0, own buffers). 1 = new frame, 0 = none. */
/* only rows [y0,y1) x columns [x0,x1); split (may be NULL) runs fn(arg, a, b) over the rows on both cores */
typedef void (*omx_rows_fn)(void *arg, int ya, int yb);
typedef void (*omx_split_fn)(omx_rows_fn fn, void *arg, int ya, int yb, int align);
int omxdec_take(struct AVFrame *f, int y0, int y1, int x0, int x1, omx_split_fn split);
/* Frames output so far; -1 once the component reported an error. */
int omxdec_frames(void);
int omxdec_failed(void);
void omxdec_close(void);
#endif
