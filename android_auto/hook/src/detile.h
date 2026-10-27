/* SPDX-License-Identifier: GPL-3.0-or-later
 * Qualcomm 64x32-tiled NV12 (OMX colour 0x7f000004, the MHI2Q hardware decoder's output) -> planar YUV 4:2:0.
 * Layout = VLC modules/codec/omxil/qcom.c (qcom_convert / tile_pos), verified on the car's own decoded frame
 * with galhook/detile/qcom_detile.py (run 117): 64x32 tiles of 2048 bytes, tile columns padded to an even
 * count, Z-flipped tile order, chroma plane at the luma size rounded up to 8192.
 * Pure C (no OpenMAX), so the host tests can check it against that reference.
 */
#ifndef SQ5_DETILE_H
#define SQ5_DETILE_H
#include <stddef.h>
#include <string.h>

#define DT_TW 64
#define DT_TH 32
#define DT_TSIZE (DT_TW * DT_TH)

static inline size_t dt_tile_pos(int x, int y, int w, int h)
{
    size_t flim = (size_t)x + (size_t)(y & ~1) * (size_t)w;
    if (y & 1) flim += (size_t)((x & ~3) + 2);
    else if ((h & 1) == 0 || y != h - 1) flim += (size_t)((x + 2) & ~3);
    return flim;
}

/* Bytes the tiled frame needs (the decoder's buffers are at least this big). */
static inline size_t dt_frame_bytes(int width, int height)
{
    int tw = (width - 1) / DT_TW + 1, twa = (tw + 1) & ~1;
    int thl = (height - 1) / DT_TH + 1, thc = (height / 2 - 1) / DT_TH + 1;
    size_t luma = (size_t)twa * (size_t)thl * DT_TSIZE;
    if (luma % (4 * DT_TSIZE)) luma = (luma / (4 * DT_TSIZE) + 1) * (4 * DT_TSIZE);
    return luma + (size_t)twa * (size_t)thc * DT_TSIZE;
}

/* Rows y0..y1-1 and columns x0..x1-1 (widened to whole 64-px tiles) of the tiled frame -> Y / U / V planes;
 * nothing outside that window is read or written. Run 135: the decoder's buffers read slowly (~26 ms for
 * 808 rows) -> callers read only the rows/columns they show, chroma is read a word (2 UV pairs) at a time. */
static inline void dt_detile(const unsigned char *src, size_t src_len, int width, int height, int y0, int y1,
                      int x0, int x1,
                      unsigned char *Y, int ys, unsigned char *U, int us, unsigned char *V, int vs)
{
    int tw = (width - 1) / DT_TW + 1, twa = (tw + 1) & ~1;
    int thl = (height - 1) / DT_TH + 1, thc = (height / 2 - 1) / DT_TH + 1;
    size_t luma = (size_t)twa * (size_t)thl * DT_TSIZE;
    int tx, ty, r;
    if (luma % (4 * DT_TSIZE)) luma = (luma / (4 * DT_TSIZE) + 1) * (4 * DT_TSIZE);
    if (y0 < 0) y0 = 0;
    if (y1 > height) y1 = height;
    y0 &= ~1;
    if (x0 < 0) x0 = 0;
    if (x1 > width) x1 = width;
    for (ty = y0 / DT_TH; ty < thl && ty * DT_TH < y1; ty++) {
        int r0 = ty * DT_TH < y0 ? y0 - ty * DT_TH : 0, r1 = DT_TH;
        if (ty * DT_TH + r1 > y1) r1 = y1 - ty * DT_TH;
        for (tx = x0 / DT_TW; tx < tw && tx * DT_TW < x1; tx++) {
            int cw = width - tx * DT_TW < DT_TW ? width - tx * DT_TW : DT_TW;
            size_t lo = dt_tile_pos(tx, ty, twa, thl) * DT_TSIZE;
            size_t co = luma + dt_tile_pos(tx, ty / 2, twa, thc) * DT_TSIZE + ((ty & 1) ? DT_TSIZE / 2 : 0);
            if (lo + DT_TSIZE <= src_len)
                for (r = r0; r < r1; r++)
                    memcpy(Y + (size_t)(ty * DT_TH + r) * (size_t)ys + (size_t)tx * DT_TW, src + lo + (size_t)r * DT_TW, (size_t)cw);
            if (co + DT_TSIZE / 2 <= src_len)
                for (r = r0 / 2; r < (r1 + 1) / 2; r++) {
                    const unsigned char *s = src + co + (size_t)r * DT_TW;
                    unsigned char *u = U + (size_t)(ty * DT_TH / 2 + r) * (size_t)us + (size_t)tx * (DT_TW / 2);
                    unsigned char *v = V + (size_t)(ty * DT_TH / 2 + r) * (size_t)vs + (size_t)tx * (DT_TW / 2);
                    int k;
                    if (ty * DT_TH / 2 + r >= (height + 1) / 2) break;
                    if (cw == DT_TW) {                  /* whole tile: 16 aligned words = 32 UV pairs */
                        const unsigned int *s32 = (const unsigned int *)(const void *)s;
                        for (k = 0; k < DT_TW / 4; k++) {
                            unsigned int w = s32[k];
                            u[2 * k] = (unsigned char)w; v[2 * k] = (unsigned char)(w >> 8);
                            u[2 * k + 1] = (unsigned char)(w >> 16); v[2 * k + 1] = (unsigned char)(w >> 24);
                        }
                    } else
                        for (k = 0; k < cw / 2; k++) { u[k] = s[2 * k]; v[k] = s[2 * k + 1]; }
                }
        }
    }
}
#endif
