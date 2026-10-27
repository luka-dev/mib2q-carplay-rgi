/* SPDX-License-Identifier: GPL-3.0-or-later
 * Turn-card panel for the cockpit map (run 106, owner: "our arrow box doesn't show the distance bars, the HUD
 * does"). In the VC's large map view the cockpit hides its own arrow tile (the one that draws the distance
 * number and bargraph) and luka shows its maneuver_render arrow (plane 98) as a card at the rect Luka writes to
 * /tmp/sq5_turncard (TurnCardFeed). The mirror used to draw the panel and text strip behind that card; the
 * cluster player replaces the mirror, so it draws them here, plus the approach bargraph:
 *   - dark panel over the card rect (98 is transparent around the arrow; the player advertises "tc1" in its
 *     ready line, so Luka leaves its opaque backing off),
 *   - a 16-segment bar column inside the card's right edge (level from the record's bar=, blink phases
 *     arrive as level 0/16 from Luka's blink ticks),
 *   - a text strip under the card: distance (large) and next street (small).
 * Record: "SQ5TC1 <seq>\nroute=<r> card=<c> x= y= w= h= [bar=<0..16> pm=<0..3>]\ndist=..\nstreet=..\nend=<seq>\n".
 * Header-only, included by player.c after menu.h (uses its fill/blend helpers and fonts).
 */
#ifndef SQ5_CARDPANEL_H
#define SQ5_CARDPANEL_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CP_PATH "/tmp/sq5_turncard"
#define CP_STALE_MS 6000u
#define CP_STRIP_H 44
#define CP_BARS 16

typedef struct {
    int valid, route, card, x, y, w, h, bar, pm;
    unsigned long seq;
    char dist[40], street[160];
} card_state;

static const char *cp_line(const char *p, const char *end, char *out, size_t cap)
{
    size_t n = 0;
    while (p < end && *p != '\n') { if (n + 1 < cap) out[n++] = *p; p++; }
    out[n] = 0;
    while (n > 0 && out[n - 1] == ' ') out[--n] = 0;
    return p < end ? p + 1 : end;
}

/* 1 = valid record parsed into *s */
static int card_parse(const char *buf, int len, card_state *s)
{
    card_state t;
    const char *p, *end = buf + len;
    char line[160];
    unsigned long seq2;
    memset(&t, 0, sizeof(t));
    if (len < 16 || strncmp(buf, "SQ5TC1 ", 7) != 0) return 0;
    p = cp_line(buf + 7, end, line, sizeof(line));
    if (sscanf(line, "%lu", &t.seq) != 1) return 0;
    p = cp_line(p, end, line, sizeof(line));
    if (sscanf(line, "route=%d card=%d x=%d y=%d w=%d h=%d bar=%d pm=%d", &t.route, &t.card, &t.x, &t.y, &t.w, &t.h,
               &t.bar, &t.pm) < 6) return 0;
    if (end - p < 5 || strncmp(p, "dist=", 5) != 0) return 0;
    p = cp_line(p + 5, end, t.dist, sizeof(t.dist));
    if (end - p < 7 || strncmp(p, "street=", 7) != 0) return 0;
    p = cp_line(p + 7, end, t.street, sizeof(t.street));
    if (end - p < 4 || strncmp(p, "end=", 4) != 0) return 0;
    cp_line(p + 4, end, line, sizeof(line));
    if (sscanf(line, "%lu", &seq2) != 1 || seq2 != t.seq) return 0;
    if (t.bar < 0) t.bar = 0;
    if (t.bar > CP_BARS) t.bar = CP_BARS;
    t.card = t.card && t.route && t.w > 0 && t.h > 0 && t.w <= 1440 && t.h <= 455;
    t.valid = 1;
    *s = t;
    return 1;
}

/* Poll: 1 = the drawn state changed. Staleness (Luka heartbeat is 2 s) hides the panel. */
static int card_poll(const char *path, card_state *s, uint64_t now, uint64_t *last_change)
{
    char b[520];
    card_state t;
    size_t n;
    FILE *f = fopen(path, "r");
    if (f) {
        n = fread(b, 1, sizeof(b) - 1, f);
        fclose(f);
        b[n] = 0;
        if (card_parse(b, (int)n, &t)) {
            if (!s->valid || t.seq != s->seq) {
                int changed = !s->valid || t.card != s->card || t.x != s->x || t.y != s->y || t.w != s->w || t.h != s->h ||
                              t.bar != s->bar || strcmp(t.dist, s->dist) || strcmp(t.street, s->street);
                *s = t;
                *last_change = now;
                return changed;
            }
        }
    }
    if (s->valid && s->card && now - *last_change > CP_STALE_MS) { s->card = 0; return 1; }
    return 0;
}

/* UTF-8 text (Latin-1 range covers the distance fractions); stops at max_x. */
static void cp_text(unsigned char *buf, int stride, int dw, int dh, int x, int base, const char *s, int max_x,
                    const tcf_font *f, int r, int g, int b, int bgra)
{
    int pen = x * 64;
    const unsigned char *u = (const unsigned char *)s;
    while (*u) {
        unsigned cp = *u++;
        const tcf_glyph *gl;
        int gx, gy;
        if (cp >= 0xC0 && (*u & 0xC0) == 0x80) {
            if (cp < 0xE0) cp = ((cp & 0x1F) << 6) | (*u++ & 0x3F);
            else { while ((*u & 0xC0) == 0x80) u++; cp = '?'; }
        }
        gl = menu_glyph(f, cp);
        if (!gl) continue;
        if ((pen + gl->adv64) / 64 > max_x) break;
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

/* Run 108: the card is now the stock tile's crop (210x153) at the stock tile's place, so the frame is drawn
 * like the VC's own tile: a header strip with the distance on top, the arrow, the bar column on the right. */
#define CP_HEAD_H 40
#define CP_PAD 10
#define CP_BAR_W 12
static void card_draw(unsigned char *buf, int stride, int dw, int dh, int bgra, const card_state *s)
{
    int x, y, w, h, i, bx, top, bot, seg, sy, x0, x1, y0, y1;
    if (!s->valid || !s->card) return;
    x = s->x; y = s->y; w = s->w; h = s->h;
    x0 = x - CP_PAD; x1 = x + w + CP_PAD + CP_BAR_W + CP_PAD; y0 = y - CP_HEAD_H; y1 = y + h + CP_PAD;
    /* tile body behind the arrow (98 is transparent around it) and a slightly lighter header */
    menu_fill(buf, stride, dw, dh, x0, y0, x1, y1, 22, 24, 28, 235, bgra);
    menu_fill(buf, stride, dw, dh, x0, y0, x1, y0 + CP_HEAD_H - 4, 38, 40, 44, 245, bgra);
    menu_fill(buf, stride, dw, dh, x0, y0, x0 + 2, y1, 110, 112, 116, 255, bgra);        /* grey frame edges */
    menu_fill(buf, stride, dw, dh, x1 - 2, y0, x1, y1, 110, 112, 116, 255, bgra);
    /* header: dot + distance (large), next street (small) if it fits */
    if (s->dist[0]) {
        int tx = x0 + 14, dwid;
        menu_fill(buf, stride, dw, dh, tx, y0 + 14, tx + 10, y0 + 24, 70, 190, 240, 255, bgra);
        tx += 18;
        cp_text(buf, stride, dw, dh, tx, y0 + 30, s->dist, x1 - 8, &tcf_large, 255, 255, 255, bgra);
        dwid = menu_text_w(&tcf_large, s->dist) + 12;
        if (s->street[0])
            cp_text(buf, stride, dw, dh, tx + dwid, y0 + 29, s->street, x1 - 10, &tcf_small, 200, 204, 210, bgra);
    }
    /* bar column: 16 segments bottom-up right of the arrow, like the VC tile */
    bx = x + w + CP_PAD / 2; top = y + 6; bot = y + h - 6;
    seg = (bot - top) / CP_BARS;
    for (i = 0; i < CP_BARS; i++) {
        int on = i < s->bar;
        sy = bot - (i + 1) * seg;
        menu_fill(buf, stride, dw, dh, bx, sy + 1, bx + CP_BAR_W, sy + seg - 1,
                  on ? 90 : 48, on ? 200 : 54, on ? 240 : 62, 255, bgra);
    }
}
#endif
