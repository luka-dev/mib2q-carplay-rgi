/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cockpit options menu, milestone 1 (owner 2026-09-29): drawn by the cluster player over the cockpit map.
 * Luka (com.sq5.aa.input.AaClusterMenu) owns the state and the wheel; it writes /tmp/sq5_cluster_menu, a
 * fixed 512-byte text record written in place:
 *   SQ5M1 <seq> <open> <sel> <edit> <up>\n
 *   <label>\t<value>\n ...           (one line per item)
 *   end <seq>\n                       (a torn read has a different or missing trailing seq -> ignored)
 * The player polls it every 100 ms, applies <up> as the live picture shift and, while <open>, draws the
 * panel after the frame conversion (the map keeps running underneath). Font: DejaVu Sans (vendor/tc_font.h,
 * the mirror's generated bitmaps). Header-only: included by player.c.
 */
#ifndef SQ5_MENU_H
#define SQ5_MENU_H
#include <stdio.h>
#include <string.h>
#include "tc_font.h"

#define MENU_MAX 6
typedef struct {
    int valid, seq, open, sel, edit, up, up_small, n;
    char label[MENU_MAX][40], value[MENU_MAX][24];
} menu_state;

static int menu_parse(const char *b, menu_state *m)
{
    int seq, open, sel, edit, up, up_small, seq2, nf;
    const char *p, *e;
    menu_state t;
    memset(&t, 0, sizeof(t));
    nf = sscanf(b, "SQ5M1 %d %d %d %d %d %d", &seq, &open, &sel, &edit, &up, &up_small);
    if (nf < 5) return 0;
    if (nf < 6) up_small = up;                    /* older record: one value for both views */
    e = strstr(b, "\nend ");
    if (!e || sscanf(e + 5, "%d", &seq2) != 1 || seq2 != seq) return 0;
    p = strchr(b, '\n');
    if (!p) return 0;
    p++;
    while (p <= e && t.n < MENU_MAX) {
        const char *nl = strchr(p, '\n'), *tab;
        size_t ll, vl;
        if (!nl || nl > e + 1) break;
        tab = memchr(p, '\t', (size_t)(nl - p));
        if (!tab) break;
        ll = (size_t)(tab - p); if (ll >= sizeof(t.label[0])) ll = sizeof(t.label[0]) - 1;
        vl = (size_t)(nl - tab - 1); if (vl >= sizeof(t.value[0])) vl = sizeof(t.value[0]) - 1;
        memcpy(t.label[t.n], p, ll); memcpy(t.value[t.n], tab + 1, vl);
        t.n++;
        p = nl + 1;
    }
    t.valid = 1; t.seq = seq; t.open = open; t.sel = sel; t.edit = edit; t.up = up; t.up_small = up_small;
    *m = t;
    return 1;
}

/* 1 = a new, complete record (different seq) was read into *m. */
static int menu_poll(const char *path, menu_state *m)
{
    char b[520] = {0};
    menu_state t;
    FILE *f = fopen(path, "r");
    size_t n;
    if (!f) return 0;
    n = fread(b, 1, sizeof(b) - 1, f);
    fclose(f);
    b[n] = 0;
    if (!menu_parse(b, &t) || (m->valid && t.seq == m->seq)) return 0;
    *m = t;
    return 1;
}

static const tcf_glyph *menu_glyph(const tcf_font *f, unsigned cp)
{
    int lo = 0, hi = f->nglyphs - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) return &f->glyphs[mid];
        if (f->glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
    }
    return cp != '?' ? menu_glyph(f, '?') : NULL;
}

static int menu_text_w(const tcf_font *f, const char *s)
{
    int w = 0;
    for (; *s; s++) { const tcf_glyph *g = menu_glyph(f, (unsigned char)*s); if (g) w += g->adv64; }
    return w / 64;
}

/* Blend colour (r,g,b) at alpha a (0..255) into the 4-byte pixel (BGRA when bgra, else RGBA). */
static void menu_blend(unsigned char *px, int r, int g, int b, int a, int bgra)
{
    int c0 = bgra ? b : r, c2 = bgra ? r : b;
    px[0] = (unsigned char)((px[0] * (255 - a) + c0 * a) / 255);
    px[1] = (unsigned char)((px[1] * (255 - a) + g * a) / 255);
    px[2] = (unsigned char)((px[2] * (255 - a) + c2 * a) / 255);
    px[3] = 255;
}

static void menu_fill(unsigned char *buf, int stride, int dw, int dh, int x0, int y0, int x1, int y1,
                      int r, int g, int b, int a, int bgra)
{
    int x, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dw) x1 = dw;
    if (y1 > dh) y1 = dh;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) menu_blend(buf + (size_t)y * stride + (size_t)x * 4, r, g, b, a, bgra);
}

static void menu_text(unsigned char *buf, int stride, int dw, int dh, int x, int base, const char *s,
                      const tcf_font *f, int r, int g, int b, int bgra)
{
    int pen = x * 64;
    for (; *s; s++) {
        const tcf_glyph *gl = menu_glyph(f, (unsigned char)*s);
        int gx, gy;
        if (!gl) continue;
        for (gy = 0; gy < gl->h; gy++) {
            int py = base + gl->y0 + gy;
            if (py < 0 || py >= dh) continue;
            for (gx = 0; gx < gl->w; gx++) {
                int px = pen / 64 + gl->x0 + gx, a = f->bits[gl->off + (unsigned)gy * gl->w + gx];
                if (a && px >= 0 && px < dw) menu_blend(buf + (size_t)py * stride + (size_t)px * 4, r, g, b, a, bgra);
            }
        }
        pen += gl->adv64;
    }
}

/* Panel geometry (window 1440x455): full view = centred between the dials, below the Audi top bar;
 * Sport view = inside the Sport window (window x 476..956). */
static void menu_draw(unsigned char *buf, int stride, int dw, int dh, int bgra, const menu_state *m, int small)
{
    const tcf_font *tf = &tcf_large, *rf = &tcf_small;
    int row = 44, pad = 16, w = small ? 440 : 520, x0 = small ? 496 : (dw - w) / 2, y0 = 48;
    int h = 52 + m->n * row + 10, i;
    if (!m->valid || !m->open) return;
    menu_fill(buf, stride, dw, dh, x0, y0, x0 + w, y0 + h, 8, 10, 14, 220, bgra);
    menu_fill(buf, stride, dw, dh, x0, y0, x0 + w, y0 + 3, 187, 10, 30, 255, bgra);          /* Audi red */
    menu_text(buf, stride, dw, dh, x0 + pad, y0 + 36, "Cockpit", tf, 255, 255, 255, bgra);
    for (i = 0; i < m->n; i++) {
        int ry = y0 + 52 + i * row, sel = i == m->sel, vw;
        if (sel) menu_fill(buf, stride, dw, dh, x0 + 6, ry, x0 + w - 6, ry + row - 6,
                           m->edit ? 187 : 60, m->edit ? 10 : 70, m->edit ? 30 : 84, 235, bgra);
        menu_text(buf, stride, dw, dh, x0 + pad, ry + 28, m->label[i], rf, 255, 255, 255, bgra);
        vw = menu_text_w(rf, m->value[i]);
        menu_text(buf, stride, dw, dh, x0 + w - pad - vw, ry + 28, m->value[i], rf, sel ? 255 : 180, sel ? 255 : 190,
                  sel ? 255 : 200, bgra);
    }
}
#endif
