/*
 * CarPlay instruction-card recogniser: thresholds and fail-closed gates (see
 * card_rec.h).
 *
 *   parameters   the tuned defaults and their ranges
 *   text ghost   isolated faint pixels around the distance line (crossfade or
 *                split transition: anti-aliasing always borders brighter ink)
 *   icon gates   edge concentration of the white (scored) art, ghost cells
 *                (bright where the 3x3-dilated template is empty), a halo of
 *                light around all-white art, mid-tone ratio vs the template
 *   ring number  exit digits anchored to the hole geometry (centre, cap
 *                height), confusable letters, unexplained ink in the hole
 * (the temporal publish rule: card_rec_stab.c)
 *
 * Integer only, no allocation.  Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define CARD_REC_INTERNAL
#include "card_rec.h"

#include <string.h>

/* ------------------------------------------------------------- parameters */
void card_rec_default_params(card_rec_params_t *p) {
    static const int white_min[] = {245, 228, 250}, sharp[] = {450, 450, 570}, mid[] = {280, 280, 240};
    static const int icon_sharp[] = {330, 330, 400};   /* car arrow 526..529 (gpu 610) */
    int i;
    memset(p, 0, sizeof(*p));
    for (i = 0; i < CARD_REC_PATH_COUNT; ++i) {
        p->white_min[i] = white_min[i];
        p->sharp_min_pm[i] = sharp[i];
        p->mid_max_pm[i] = mid[i];
        p->icon_sharp_min_pm[i] = icon_sharp[i];
    }
    p->band = 6; p->band_tol = 8; p->band_min_pm = 995; p->black_max = 40;
    p->ring = 2; p->ring_min_pm = 500; p->white_rank = 16; p->contrast_min = 200;
    p->cover_max_pm = 150; p->ink_min = 400;
    p->line_gap = 8; p->icon_h_min = 33; p->icon_h_max = 66; p->icon_w_min = 16; p->icon_w_max = 66;
    p->icon_x_min = 8; p->icon_x_max = 80; p->icon_gap = 16; p->icon_gap_max = 56;
    p->dist_gap_max = 24; p->dist_h_min = 14; p->dist_h_max = 36; p->dist_dy_max = 12;
    p->icon_d_max = 250; p->icon_margin = 100; p->side_margin = 30; p->keep_d_max = 180;
    p->decoy_eq_d = 250; p->orient_margin = 40;
    p->ghost_t = 16; p->ghost_tz = 8; p->ghost_max_pm = 6; p->halo_k = 10; p->halo_gap = 2; p->halo_max_pm = 10;
    p->faint_t = 16; p->faint_max = 64; p->text_ghost_max = 20; p->text_ghost_t = 32; p->text_res_max = 4;
    p->icon_mid_tol_pm = 160;
    p->opt_tol = 48; p->opt_white = 160; p->opt_white_max_pm = 150;
    p->size_tol_min = 3; p->size_tol_pm = 125;
    p->glyph_d_max = 300; p->glyph_margin = 80; p->glyph_size_tol = 3; p->glyph_off_tol = 3;
    p->space_min = 8; p->space_pm = 330;
    p->glyph_look_margin = 30;                  /* '~B' '~l' '~I' '~S': real glyphs >= 60 away */
    p->gap_min = 2; p->gap_max = 5; p->gap_one_min = 5; p->gap_one_max = 6;
    p->sep_d_max = 200; p->sep_peak_min = 232; p->sep_flat_max_pm = 910;   /* real 117 / 248 / 882 */
    p->dist_x0 = 106; p->dist_x0_tol = 4;       /* every car frame: 106 */
    p->side_light_gap = 3; p->side_light_t = 40;   /* real <= 31 */
    p->ring_digit_d_max = 300; p->ring_digit_margin = 80; p->ring_digit_h_min = 6; p->ring_digit_h_max = 24;
    p->ring_letter_margin = 30; p->ring_digit_ctr_tol = 2; p->ring_digit_cap_tol = 2; p->ring_res_max = 2;
    p->detect_any = 0;
    p->unpublished = 1u << CARD_REC_CLASS_STRAIGHT | 1u << CARD_REC_CLASS_ARRIVE;
}

static int in(int v, int lo, int hi) { return v >= lo && v <= hi; }

/* Ranges keep every index inside the static scratch and every product inside
 * int; a value outside them makes card_rec_recognise reject with BAD_ARGS. */
int card_rec_params_valid(const card_rec_params_t *p) {
    const int W = CARD_REC_MAX_BOX_W, H = CARD_REC_MAX_BOX_H, I = CARD_REC_MAX_ICON, PM = 1000;
    int i, ok = p != 0;
    for (i = 0; ok && i < CARD_REC_PATH_COUNT; ++i)
        ok = in(p->white_min[i], 0, 255) && in(p->sharp_min_pm[i], 0, PM) && in(p->mid_max_pm[i], 0, PM) &&
             in(p->icon_sharp_min_pm[i], 0, PM);
    return ok && in(p->band, 1, 64) && in(p->band_tol, 0, 255) && in(p->band_min_pm, 0, PM) &&
           in(p->black_max, 0, 255) && in(p->ring, 1, 64) && in(p->ring_min_pm, 0, PM) &&
           in(p->white_rank, 1, W * H) && in(p->contrast_min, 0, 255) && in(p->cover_max_pm, 0, PM) &&
           in(p->ink_min, 0, W * H) && in(p->line_gap, 0, H) && in(p->icon_h_min, 1, p->icon_h_max) &&
           in(p->icon_h_max, 1, I) && in(p->icon_w_min, 1, p->icon_w_max) && in(p->icon_w_max, 1, I) &&
           in(p->icon_x_min, 0, W) && in(p->icon_x_max, 0, W) && in(p->icon_gap, 0, W) &&
           in(p->icon_gap_max, 0, W) && in(p->dist_gap_max, 0, W) && in(p->dist_h_min, 0, p->dist_h_max) &&
           in(p->dist_h_max, 0, H) && in(p->dist_dy_max, 0, H) && in(p->icon_d_max, 0, PM) &&
           in(p->icon_margin, 0, PM) && in(p->side_margin, 0, PM) && in(p->keep_d_max, 0, PM) &&
           in(p->decoy_eq_d, 0, PM) && in(p->orient_margin, 0, PM) && in(p->ghost_t, 0, 256) &&
           in(p->ghost_tz, 0, 255) && in(p->ghost_max_pm, 0, PM) && in(p->halo_k, 0, W) &&
           in(p->halo_gap, 0, W) && in(p->halo_max_pm, 0, PM) && in(p->faint_t, 0, 256) &&
           in(p->faint_max, 0, 256) && in(p->text_ghost_max, 0, W * H) && in(p->text_ghost_t, 0, 256) &&
           in(p->text_res_max, 0, W * I) && in(p->icon_mid_tol_pm, 0, PM * PM) && in(p->opt_tol, 0, 255) &&
           in(p->opt_white, 0, 256) && in(p->opt_white_max_pm, 0, PM) && in(p->size_tol_min, 0, I) &&
           in(p->size_tol_pm, 0, PM) && in(p->glyph_d_max, 0, PM) && in(p->glyph_margin, 0, PM) &&
           in(p->glyph_size_tol, 0, 128) && in(p->glyph_off_tol, 0, 128) && in(p->space_min, 0, W) &&
           in(p->space_pm, 0, PM) && in(p->glyph_look_margin, 0, PM) && in(p->gap_min, 0, p->gap_max) &&
           in(p->gap_max, 0, W) && in(p->gap_one_min, 0, p->gap_one_max) && in(p->gap_one_max, 0, W) &&
           in(p->sep_d_max, 0, PM) && in(p->sep_peak_min, 0, 256) && in(p->sep_flat_max_pm, 0, PM) &&
           in(p->dist_x0, 0, W) && in(p->dist_x0_tol, 0, W) && in(p->side_light_gap, 0, W) &&
           in(p->side_light_t, 0, 256) && in(p->ring_digit_d_max, 0, PM) && in(p->ring_digit_margin, 0, PM) &&
           in(p->ring_digit_h_min, 0, p->ring_digit_h_max) && in(p->ring_digit_h_max, 0, I) &&
           in(p->ring_letter_margin, 0, PM) && in(p->ring_digit_ctr_tol, 0, I) &&
           in(p->ring_digit_cap_tol, 0, I) && in(p->ring_res_max, 0, I * I) && in(p->detect_any, 0, 1) &&
           !(p->unpublished & ~((1u << CARD_REC_CLASS_COUNT) - 1u));
}

/* ------------------------------------------------------------- text ghost */
/* pixels ft <= v < fmax of the w x h region whose 5x5 neighbourhood (clipped
 * to the region) holds nothing >= fmax */
int card_rec_faint_count(const uint8_t *s, int stride, int w, int h, int ft, int fmax) {
    int x, y, i, j, n = 0;
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x) {
            int v = s[y * stride + x], iso = 1;
            if (v < ft || v >= fmax)
                continue;
            for (j = cr_max(0, y - 2); j <= cr_min(h - 1, y + 2) && iso; ++j)
                for (i = cr_max(0, x - 2); i <= cr_min(w - 1, x + 2) && iso; ++i)
                    iso = s[j * stride + i] < fmax;
            n += iso;
        }
    return n;
}

/* mid-tone cells (T_MID..T_INK-1) per ink cell, per-mille, over scored cells */
static int mid_ratio_pm(const uint8_t *a, const uint8_t *care) {
    int i, ink = 0, mid = 0;
    for (i = 0; i < CARD_REC_NORM * CARD_REC_NORM; ++i)
        if (care[i] == 2) {
            ink += a[i] >= T_INK;
            mid += a[i] >= T_MID && a[i] < T_INK;
        }
    return ink ? 1000 * mid / ink : 1000;
}

/* Checks of the classified icon against its own template.  s: stretched box
 * (bw x bh), bb: stroke bbox x0, y0, x1, y1, d0: first distance column. */
card_rec_reason_t card_rec_icon_gates(card_rec_t *c, const uint8_t *s, int bw, int bh, const int *bb,
                                      int d0, const card_rec_icon_t *t) {
    const card_rec_params_t *p = &c->p;
    int iw = bb[2] - bb[0] + 1, ih = bb[3] - bb[1] + 1, sq = cr_max(iw, ih), x, y, cnt = 0, n2 = 0;
    int cx[CARD_REC_MAX_ICON], cy[CARD_REC_MAX_ICON];
    const uint8_t *crop = s + bb[1] * bw + bb[0];
    uint64_t s1 = 0, s2 = 0;
    for (x = 0; x < iw; ++x)            /* native px -> care cell, as the pad-to-square */
        cx[x] = (x + (sq - iw) / 2) * CARD_REC_NORM / sq;
    for (y = 0; y < ih; ++y)
        cy[y] = (y + (sq - ih) / 2) * CARD_REC_NORM / sq;
#define CARE2(yy, xx) (t->care[cy[yy] * CARD_REC_NORM + cx[xx]] == 2)
    for (y = 0; y < ih; ++y)            /* edge concentration over scored pairs only */
        for (x = 0; x < iw; ++x) {
            int v = crop[y * bw + x], g;
            if (!CARE2(y, x))
                continue;
            if (x + 1 < iw && CARE2(y, x + 1)) {
                g = cr_abs(crop[y * bw + x + 1] - v);
                s1 += (uint64_t)g;
                s2 += (uint64_t)(g * g);
            }
            if (y + 1 < ih && CARE2(y + 1, x)) {
                g = cr_abs(crop[(y + 1) * bw + x] - v);
                s1 += (uint64_t)g;
                s2 += (uint64_t)(g * g);
            }
        }
#undef CARE2
    if ((s1 ? (int)(1000u * s2 / (255u * s1)) : 0) < p->icon_sharp_min_pm[c->path])
        return CARD_REC_R_ICON_SOFT_EDGES;
    for (y = 0; y < CARD_REC_NORM; ++y)  /* ghost: bright where the template is empty */
        for (x = 0; x < CARD_REC_NORM; ++x) {
            int i = y * CARD_REC_NORM + x;
            n2 += t->care[i] == 2;
            cnt += t->care[i] && c->norm[i] >= p->ghost_t &&
                   cr_dil3(t->img, CARD_REC_NORM, CARD_REC_NORM, CARD_REC_NORM, x, y) <= p->ghost_tz;
        }
    if (1000 * cnt / cr_max(1, n2) > p->ghost_max_pm)
        return CARD_REC_R_ICON_GHOST;
    if (!t->has_care) {                 /* all-white art: no light around its bbox */
        int b = p->band, k = p->halo_k, gap = p->halo_gap, hc = 0, ha = 0;
        int ox0 = cr_max(b, bb[0] - k), oy0 = cr_max(b, bb[1] - k);
        int ox1 = cr_min(cr_min(bw - b - 1, bb[2] + k), d0 - 1 - gap), oy1 = cr_min(bh - b - 1, bb[3] + k);
        for (y = oy0; y <= oy1; ++y)
            for (x = ox0; x <= ox1; ++x) {
                if (x >= bb[0] - gap && x <= bb[2] + gap && y >= bb[1] - gap && y <= bb[3] + gap)
                    continue;
                ++ha;
                hc += s[y * bw + x] >= p->ghost_t;
            }
        if (ha && 1000 * hc / ha > p->halo_max_pm)
            return CARD_REC_R_ICON_GHOST_HALO;
    }
    if (mid_ratio_pm(c->norm, t->care) > mid_ratio_pm(t->img, t->care) + p->icon_mid_tol_pm)
        return CARD_REC_R_ICON_MID_TONES;
    return CARD_REC_R_OK;
}

/* ------------------------------------------------------------- ring number */
card_rec_reason_t card_rec_read_ring(card_rec_t *c, const uint8_t *crop, int cs, int w, int h,
                                     const card_rec_icon_t *t, int *exit_n) {
    static const card_rec_reason_t fail[] = {CARD_REC_R_OK, CARD_REC_R_RING_DIGIT_NO_TEMPLATE_FITS,
                                             CARD_REC_R_RING_DIGIT_GLYPH_UNKNOWN,
                                             CARD_REC_R_RING_DIGIT_GLYPH_AMBIGUOUS};
    const card_rec_params_t *p = &c->p;
    int cx = (w * t->hole_cx_pm + 500) / 1000, cy = (h * t->hole_cy_pm + 500) / 1000;
    int r = (w * t->hole_r_pm + 500) / 1000, x, y, bx0 = w, bx1 = -1, by0 = h, by1 = -1, n, i, cap, tol2;
    int value = 0, res = 0;
    char text[2 * CARD_REC_LABEL_MAX + 1];
    card_rec_gbox_t g[3];
    card_rec_gmatch_t dm, lm;
    const uint8_t *blk;
    if (!c->ring.count)
        return CARD_REC_R_RING_NO_DIGIT_TEMPLATES;
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x) {
            int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d2 <= r * r && d2 > (r - 2) * (r - 2) && crop[y * cs + x] >= T_INK)
                return CARD_REC_R_RING_DIGIT_TOUCHES_RING;
        }
    for (y = 0; y < h; ++y)                 /* hole disc only */
        for (x = 0; x < w; ++x) {
            int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            uint8_t v = d2 <= r * r ? crop[y * cs + x] : 0;
            c->sub[y * w + x] = v;
            if (v >= T_INK) {
                bx0 = cr_min(bx0, x); bx1 = cr_max(bx1, x);
                by0 = cr_min(by0, y); by1 = cr_max(by1, y);
            }
        }
    if (bx1 < 0)
        return CARD_REC_R_RING_NO_EXIT_NUMBER;
    /* the number sits on the hole centre at the cap height of this ring size */
    cap = (c->ring_cap * w + t->exp_w / 2) / t->exp_w;
    tol2 = 2 * p->ring_digit_ctr_tol;
    if (cr_abs(bx0 + bx1 - 2 * cx) > tol2 || cr_abs(by0 + by1 - 2 * cy) > tol2)
        return CARD_REC_R_RING_DIGIT_OFF_CENTRE;
    if (cr_abs(by1 - by0 + 1 - cap) > p->ring_digit_cap_tol)
        return CARD_REC_R_RING_DIGIT_HEIGHT;
    blk = c->sub + by0 * w + bx0;
    n = card_rec_segment(blk, w, bx1 - bx0 + 1, by1 - by0 + 1, g, 3);
    if (n < 1 || n > 2)
        return CARD_REC_R_RING_DIGIT_COUNT;
    text[0] = 0;
    memset(c->expl, 0, (size_t)(w * h));
    for (i = 0; i < n; ++i) {
        int gh = g[i].y1 - g[i].y0 + 1, base = n > 1 ? card_rec_baseline(g, n) : g[0].y1, m;
        if (gh < p->ring_digit_h_min || gh > p->ring_digit_h_max)
            return CARD_REC_R_RING_DIGIT_SIZE;
        m = card_rec_match_glyph(&c->ring, p, blk, w, &g[i], base, CARD_REC_LABELS_DIGITS, p->ring_digit_d_max,
                                 p->ring_digit_margin, &dm);
        if (m != CARD_REC_MATCH_OK)
            return fail[m];
        if (c->ring.nletters) {         /* every confusable letter MORE than the margin worse */
            card_rec_match_glyph(&c->ring, p, blk, w, &g[i], base, CARD_REC_LABELS_LETTERS, 1000, -1, &lm);
            if (lm.d - dm.d <= p->ring_letter_margin)
                return CARD_REC_R_RING_DIGIT_LETTER;
        }
        card_rec_paste(c->expl, w, h, bx0 + g[i].x0, by0 + g[i].y1, &dm);
        strcat(text, c->ring.labels[dm.label]);     /* 2 labels < sizeof(text) */
    }
    /* light in the inner disc the matched digits do not explain: a ghost
     * number, a descender or second row, any extra mark */
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x)
            res += (x - cx) * (x - cx) + (y - cy) * (y - cy) <= (r - 2) * (r - 2) &&
                   crop[y * cs + x] >= p->text_ghost_t && cr_dil3(c->expl, w, w, h, x, y) <= p->ghost_tz;
    if (res > p->ring_res_max)
        return CARD_REC_R_RING_DIGIT_RESIDUAL;
    for (i = 0; text[i]; ++i)
        value = cr_min(1000, value * 10 + (text[i] - '0'));
    if (text[0] == '0' || value < 1 || value > 19)
        return CARD_REC_R_RING_EXIT_OUT_OF_RANGE;
    *exit_n = value;
    return CARD_REC_R_OK;
}
