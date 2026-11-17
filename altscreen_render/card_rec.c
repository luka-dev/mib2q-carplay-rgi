/*
 * CarPlay instruction-card recogniser: per-frame pipeline (see card_rec.h).
 *
 *   1 card gate   zone box is card black, bounded by non-black, bright (not a
 *                 fade: contrast and absolute white per input path), sparse,
 *                 the distance text has sharp edges and no ghost text near it
 *   2 layout      first text line = icon (leftmost group) + distance text; the
 *                 text starts at dist_x0 (a lost leading digit moves it) and
 *                 nothing light follows it (side light: a dimmed 'i' of 'mi')
 *   3-6           icon class (best <= ICON_D_MAX, every other output group and
 *                 every rotated / mirrored decoy MORE than ICON_MARGIN worse):
 *                 card_rec_match.c; icon sharpness / ghost / halo / mid-tone
 *                 gates and the ring exit number: card_rec_gates.c; distance
 *                 text: card_rec_text.c
 *   7 publish     STRAIGHT / ARRIVE (params.unpublished) are read, then REJECT
 *
 * Integer only; every rounding is the reference's floor((a + b/2) / b) on
 * non-negative operands.  Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define CARD_REC_INTERNAL
#include "card_rec.h"

#include <string.h>

typedef struct {
    const char *name;
    card_rec_class_t cls;
    int cp;
    card_rec_rb_dir_t dir;
} group_t;

static const group_t k_groups[] = {
    {"LEFT", CARD_REC_CLASS_LEFT_TURN, 1, CARD_REC_RB_NONE},
    {"RIGHT", CARD_REC_CLASS_RIGHT_TURN, 2, CARD_REC_RB_NONE},
    {"STRAIGHT", CARD_REC_CLASS_STRAIGHT, 3, CARD_REC_RB_NONE},
    {"KEEP_LEFT", CARD_REC_CLASS_KEEP_LEFT, 13, CARD_REC_RB_NONE},
    {"KEEP_RIGHT", CARD_REC_CLASS_KEEP_RIGHT, 14, CARD_REC_RB_NONE},
    {"ARRIVE", CARD_REC_CLASS_ARRIVE, 12, CARD_REC_RB_NONE},
    {"UTURN_L", CARD_REC_CLASS_UTURN_LEFT, 4, CARD_REC_RB_NONE},
    {"UTURN_R", CARD_REC_CLASS_UTURN_RIGHT, 4, CARD_REC_RB_NONE},
    {"RB_RING", CARD_REC_CLASS_ROUNDABOUT_NUMBERED, -1, CARD_REC_RB_NONE},
    {"RB_LEFT_R", CARD_REC_CLASS_ROUNDABOUT_LEFT, 6, CARD_REC_RB_LEFT},
    {"RB_LEFT_L", CARD_REC_CLASS_ROUNDABOUT_LEFT, 6, CARD_REC_RB_LEFT},
    {"RB_STRAIGHT_R", CARD_REC_CLASS_ROUNDABOUT_STRAIGHT, 6, CARD_REC_RB_STRAIGHT},
    {"RB_STRAIGHT_L", CARD_REC_CLASS_ROUNDABOUT_STRAIGHT, 6, CARD_REC_RB_STRAIGHT},
    {"RB_RIGHT_R", CARD_REC_CLASS_ROUNDABOUT_RIGHT, 6, CARD_REC_RB_RIGHT},
    {"RB_RIGHT_L", CARD_REC_CLASS_ROUNDABOUT_RIGHT, 6, CARD_REC_RB_RIGHT},
    {"RB_UTURN_R", CARD_REC_CLASS_ROUNDABOUT_UTURN, 19, CARD_REC_RB_UTURN},
    {"RB_UTURN_L", CARD_REC_CLASS_ROUNDABOUT_UTURN, 19, CARD_REC_RB_UTURN},
};
#define N_GROUPS ((int)(sizeof(k_groups) / sizeof(k_groups[0])))

/* zone safe boxes declared by the cluster for stream 111 (x, y, w, h) */
static const int k_zone_box[3][4] = {{330, 88, 780, 294}, {108, 88, 348, 294}, {540, 88, 360, 294}};

int card_rec_group_index(const char *name) {
    int i;
    for (i = 0; i < N_GROUPS; ++i)
        if (!strcmp(k_groups[i].name, name))
            return i;
    return -1;
}

int card_rec_group_count(void) { return N_GROUPS; }

const char *card_rec_group_name(int g) { return g >= 0 && g < N_GROUPS ? k_groups[g].name : "-"; }

int card_rec_group_is_ring(int g) {
    return g >= 0 && g < N_GROUPS && k_groups[g].cls == CARD_REC_CLASS_ROUNDABOUT_NUMBERED;
}

int card_rec_group_is_keep(int g) {
    return g >= 0 && g < N_GROUPS &&
           (k_groups[g].cls == CARD_REC_CLASS_KEEP_LEFT || k_groups[g].cls == CARD_REC_CLASS_KEEP_RIGHT);
}

static int in(int v, int lo, int hi) { return v >= lo && v <= hi; }

typedef struct {
    const uint8_t *px;
    long stride;            /* |stride| <= MAX_STRIDE: y * stride fits a 32-bit long */
    int w, h;
} frame_t;

static int luma_at(const frame_t *f, int x, int y) {
    const uint8_t *q = f->px + (long)y * f->stride + 4L * x;
    return (77 * q[0] + 150 * q[1] + 29 * q[2]) >> 8;
}

/* ------------------------------------------------------------------ stage 1 */
/* 1 when (x, y0, w, h) is a card-black rectangle bounded by non-black; the
 * black level is the median of the BAND px inset band.  The caller bounds x,
 * y0 to [0, MAX_FRAME] and w, h to (0, MAX_BOX], so no sum below overflows. */
static int box_test(const card_rec_params_t *p, const frame_t *f, const int *box, int *black_out) {
    uint32_t hist[256], n = 0, cum = 0, ok = 0;
    int x = box[0], y0 = box[1], w = box[2], h = box[3], b = p->band, r = p->ring;
    int black, v, yy, xx, s;
    if (x - r < 0 || y0 - r < 0 || x + w + r > f->w || y0 + h + r > f->h)
        return 0;
    if (w > CARD_REC_MAX_BOX_W || h > CARD_REC_MAX_BOX_H || w <= 2 * b || h <= 2 * b)
        return 0;
    memset(hist, 0, sizeof(hist));
    for (yy = 0; yy < h; ++yy) {
        int full = yy < b || yy >= h - b;
        for (xx = 0; xx < w; ++xx) {
            if (!full && xx == b)
                xx = w - b;
            ++hist[luma_at(f, x + xx, y0 + yy)];
            ++n;
        }
    }
    for (black = 0; black < 255; ++black) {
        cum += hist[black];
        if (cum >= (n + 1u) / 2u)
            break;
    }
    if (black > p->black_max)
        return 0;
    for (v = black - p->band_tol; v <= black + p->band_tol; ++v)
        if (v >= 0 && v < 256)
            ok += hist[v];
    if ((int)(1000u * ok / n) < p->band_min_pm)
        return 0;
    for (s = 0; s < 4; ++s) {      /* top, bottom, left, right: RING px outside */
        int rx = s < 2 ? x : (s == 2 ? x - r : x + w), ry = s == 0 ? y0 - r : (s == 1 ? y0 + h : y0);
        int rw = s < 2 ? w : r, rh = s < 2 ? r : h;
        uint32_t nb = 0;
        for (yy = 0; yy < rh; ++yy)
            for (xx = 0; xx < rw; ++xx)
                nb += luma_at(f, rx + xx, ry + yy) > black + p->band_tol;
        if ((int)(1000u * nb / (uint32_t)(rw * rh)) < p->ring_min_pm)
            return 0;
    }
    *black_out = black;
    return 1;
}

/* value at sorted index k of a histogram */
static int order_stat(const uint32_t *hist, uint32_t k) {
    uint32_t cum = 0;
    int v;
    for (v = 0; v < 255; ++v) {
        cum += hist[v];
        if (cum > k)
            break;
    }
    return v;
}

/* longest run (first on ties) of indices with cnt >= min; 0 when none */
static int longest_run(const uint32_t *cnt, int n, uint32_t min, int *a, int *b) {
    int i, start = -1, found = 0;
    for (i = 0; i <= n; ++i) {
        if (i < n && cnt[i] >= min) {
            if (start < 0)
                start = i;
            continue;
        }
        if (start >= 0 && (!found || i - 1 - start > *b - *a)) {
            *a = start;
            *b = i - 1;
            found = 1;
        }
        start = -1;
    }
    return found;
}

/* Black-box auto-detect: rows / columns with many black px, longest run each
 * way, snapped to a known zone when every edge is within 3 px; a box that is
 * no zone rejects unless params.detect_any. */
static card_rec_reason_t detect_box(card_rec_t *c, const frame_t *f, int *box, card_rec_zone_kind_t *kind) {
    const card_rec_params_t *p = &c->p;
    uint32_t hist[256], n = (uint32_t)f->w * (uint32_t)f->h, k, rem;
    int x, y, lo, a, rr0 = 0, rr1 = 0, cr0 = 0, cr1 = 0, z, i;
    memset(hist, 0, sizeof(hist));
    for (y = 0; y < f->h; ++y)
        for (x = 0; x < f->w; ++x)
            ++hist[luma_at(f, x, y)];
    /* numpy percentile 1 ('linear'): index (n-1)/100, truncated interpolation */
    k = (n - 1u) / 100u;
    rem = (n - 1u) % 100u;
    a = order_stat(hist, k);
    lo = rem ? a + (int)((uint32_t)(order_stat(hist, k + 1u) - a) * rem / 100u) : a;
    if (lo > p->black_max)
        return CARD_REC_R_NO_BLACK_BOX;
    for (y = 0; y < f->h; ++y) {
        c->cnt[y] = 0;
        for (x = 0; x < f->w; ++x)
            c->cnt[y] += cr_abs(luma_at(f, x, y) - lo) <= p->band_tol;
    }
    if (!longest_run(c->cnt, f->h, 200u, &rr0, &rr1))
        return CARD_REC_R_NO_BLACK_BOX;
    for (x = 0; x < f->w; ++x) {
        c->cnt[x] = 0;
        for (y = rr0; y <= rr1; ++y)
            c->cnt[x] += cr_abs(luma_at(f, x, y) - lo) <= p->band_tol;
    }
    if (!longest_run(c->cnt, f->w, (uint32_t)(rr1 - rr0 + 1) / 2u, &cr0, &cr1))
        return CARD_REC_R_NO_BLACK_BOX;
    box[0] = cr0; box[1] = rr0; box[2] = cr1 - cr0 + 1; box[3] = rr1 - rr0 + 1;
    if (box[2] < 200 || box[3] < 150)
        return CARD_REC_R_BLACK_BOX_TOO_SMALL;
    for (z = 0; z < 3; ++z) {
        for (i = 0; i < 4 && cr_abs(box[i] - k_zone_box[z][i]) <= 3; ++i)
            ;
        if (i == 4) {
            memcpy(box, k_zone_box[z], sizeof(k_zone_box[z]));
            *kind = (card_rec_zone_kind_t)(CARD_REC_ZONE_FULLSCREEN + z);
            return CARD_REC_R_OK;
        }
    }
    *kind = CARD_REC_ZONE_BOX;
    return p->detect_any ? CARD_REC_R_OK : CARD_REC_R_DETECT_NOT_ZONE;
}

/* Edge concentration (per-mille) and mid-tone ratio of a stretched patch:
 * conc = 1000 * sum(g^2) / (255 * sum(g)) over forward differences; a step
 * spread over k px scores ~1000/k, so blur lowers it whatever the glyph. */
static void sharpness(const uint8_t *s, int stride, int x0, int y0, int x1, int y1, int *conc, int *midr) {
    uint64_t s1 = 0, s2 = 0;
    long ink = 0, mid = 0;
    int x, y;
    for (y = y0; y < y1; ++y)
        for (x = x0; x < x1; ++x) {
            int v = s[y * stride + x];
            if (x + 1 < x1) {
                int g = cr_abs(s[y * stride + x + 1] - v);
                s1 += (uint64_t)g;
                s2 += (uint64_t)(g * g);
            }
            if (y + 1 < y1) {
                int g = cr_abs(s[(y + 1) * stride + x] - v);
                s1 += (uint64_t)g;
                s2 += (uint64_t)(g * g);
            }
            ink += v >= T_INK;
            mid += v >= T_MID && v < T_INK;
        }
    *conc = s1 ? (int)(1000u * s2 / (255u * s1)) : 0;
    *midr = ink ? (int)(1000L * mid / ink) : 1000;
}

/* ------------------------------------------------------------------ stage 2 */
typedef struct {
    int l0, l1, ix0, iy0, ix1, iy1, d0, dy0, d1, dy1;
} layout_t;

static card_rec_reason_t layout(card_rec_t *c, const uint8_t *s, int w, int h, layout_t *L) {
    const card_rec_params_t *p = &c->p;
    int x, y, l0 = -1, l1 = -1, ng = 0, ga = 0, gb = 0, run, iw, ih;
    memset(L, 0, sizeof(*L));
    for (y = 0; y < h; ++y) {               /* first line: ink rows, gaps <= LINE_GAP */
        for (x = 0; x < w && s[y * w + x] < T_INK; ++x)
            ;
        if (x == w)
            continue;
        if (l0 < 0)
            l0 = y;
        else if (y - l1 - 1 > p->line_gap)
            break;
        l1 = y;
    }
    if (l0 < 0)
        return CARD_REC_R_NO_CONTENT;
    if (l1 - l0 + 1 < p->icon_h_min || l1 - l0 + 1 > p->icon_h_max)
        return CARD_REC_R_FIRST_LINE_HEIGHT;
    for (x = 0; x < w; ++x) {               /* column groups of line 1, gap < ICON_GAP */
        for (y = l0; y <= l1 && s[y * w + x] < T_INK; ++y)
            ;
        c->flag[x] = y <= l1;
        if (!c->flag[x])
            continue;
        if (ng && x - gb - 1 <= p->icon_gap - 1) {
            gb = x;
            continue;
        }
        if (ng == 1) {
            L->ix0 = ga;
            L->ix1 = gb;
            L->d0 = x;
        }
        ++ng;
        ga = gb = x;
    }
    if (ng < 2)
        return CARD_REC_R_NO_DISTANCE_TEXT;
    L->d1 = gb;
    if (L->ix0 < p->icon_x_min || L->ix0 > p->icon_x_max)
        return CARD_REC_R_ICON_NOT_AT_LEFT;
    if (L->d0 - L->ix1 - 1 > p->icon_gap_max)
        return CARD_REC_R_ICON_TEXT_GAP;
    for (x = L->d0, run = 0; x <= L->d1; ++x) {
        run = c->flag[x] ? 0 : run + 1;
        if (run > p->dist_gap_max)
            return CARD_REC_R_LINE1_EXTRA_CONTENT;
    }
    cr_ink_rows(s, w, L->ix0, L->ix1, l0, l1, &L->iy0, &L->iy1);
    iw = L->ix1 - L->ix0 + 1;
    ih = L->iy1 - L->iy0 + 1;
    if (iw < p->icon_w_min || iw > p->icon_w_max || ih < p->icon_h_min || ih > p->icon_h_max)
        return CARD_REC_R_ICON_SIZE;
    cr_ink_rows(s, w, L->d0, L->d1, l0, l1, &L->dy0, &L->dy1);
    if (L->dy1 - L->dy0 + 1 < p->dist_h_min || L->dy1 - L->dy0 + 1 > p->dist_h_max)
        return CARD_REC_R_DISTANCE_TEXT_HEIGHT;
    if (cr_abs((L->dy0 + L->dy1) - (L->iy0 + L->iy1)) > 2 * p->dist_dy_max)
        return CARD_REC_R_DISTANCE_NOT_BESIDE_ICON;
    L->l0 = l0;
    L->l1 = l1;
    return CARD_REC_R_OK;
}

/* ------------------------------------------------------------------ driver */
static card_rec_reason_t run(card_rec_t *c, const frame_t *f, const card_rec_zone_t *zone, card_rec_result_t *out) {
    const card_rec_params_t *p = &c->p;
    card_rec_zone_kind_t kind = zone ? zone->kind : CARD_REC_ZONE_AUTO, ck[3];
    int cand[3][4], nc = 0, hits = 0, hit = 0, black = 0, blk = 0, i, x, y, w, h, white = -1, d;
    int b = p->band, exit_n = 0, conc, midr, iw, ih, bb[4];
    uint32_t hist[256], cum = 0;
    long cover = 0, ink = 0;
    char side = '-';
    uint8_t *px = c->box;
    const group_t *g;
    layout_t L;
    card_rec_reason_t r;
    if (kind == CARD_REC_ZONE_AUTO) {
        for (nc = 0; nc < 3; ++nc) {
            memcpy(cand[nc], k_zone_box[nc], sizeof(cand[nc]));
            ck[nc] = (card_rec_zone_kind_t)(CARD_REC_ZONE_FULLSCREEN + nc);
        }
    } else if (kind >= CARD_REC_ZONE_FULLSCREEN && kind <= CARD_REC_ZONE_CLASSICSCREEN) {
        memcpy(cand[0], k_zone_box[kind - CARD_REC_ZONE_FULLSCREEN], sizeof(cand[0]));
        ck[nc++] = kind;
    } else if (kind == CARD_REC_ZONE_BOX) {
        cand[0][0] = zone->x; cand[0][1] = zone->y; cand[0][2] = zone->w; cand[0][3] = zone->h;
        ck[nc++] = kind;
    } else {
        r = detect_box(c, f, cand[0], &ck[0]);
        if (r != CARD_REC_R_OK)
            return r;
        nc = 1;
    }
    for (i = 0; i < nc; ++i)
        if (box_test(p, f, cand[i], &blk)) {
            if (!hits++) {
                hit = i;
                black = blk;
            }
        }
    if (hits != 1)
        return hits ? CARD_REC_R_CARD_BOX_MULTI : CARD_REC_R_NO_CARD_BOX;
    out->zone = ck[hit];
    out->box_x = cand[hit][0]; out->box_y = cand[hit][1];
    w = out->box_w = cand[hit][2];
    h = out->box_h = cand[hit][3];
    memset(hist, 0, sizeof(hist));
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x) {
            px[y * w + x] = (uint8_t)luma_at(f, out->box_x + x, out->box_y + y);
            if (y >= b && y < h - b && x >= b && x < w - b)
                ++hist[px[y * w + x]];
        }
    for (i = 255; i >= 0; --i) {            /* white = WHITE_RANK-th brightest */
        cum += hist[i];
        if (cum >= (uint32_t)p->white_rank) {
            white = i;
            break;
        }
    }
    out->black = black;
    out->white = white;
    if (white - black < p->contrast_min || white < p->white_min[c->path])
        return CARD_REC_R_DIM_CONTENT;
    d = cr_max(1, white - black);
    for (i = 0; i < w * h; ++i) {           /* stretch [black, white] -> [0, 255] */
        int v = (px[i] - black) * 255 + d / 2;
        px[i] = (uint8_t)(v < 0 ? 0 : cr_min(255, v / d));
    }
    for (y = b; y < h - b; ++y)
        for (x = b; x < w - b; ++x) {
            cover += px[y * w + x] >= T_ANY;
            ink += px[y * w + x] >= T_INK;
        }
    out->cover_pm = (int)(1000L * cover / ((long)(w - 2 * b) * (h - 2 * b)));
    out->ink = (int)ink;
    if (out->cover_pm > p->cover_max_pm)
        return CARD_REC_R_MOSTLY_NON_BLACK;
    if (ink < p->ink_min)
        return CARD_REC_R_TOO_LITTLE_CONTENT;
    r = layout(c, px, w, h, &L);
    if (r != CARD_REC_R_OK)
        return r;
    if (cr_abs(L.d0 - p->dist_x0) > p->dist_x0_tol)   /* lost / dimmed leading glyph */
        return CARD_REC_R_DISTANCE_NOT_AT_ANCHOR;
    sharpness(px, w, cr_max(0, L.d0 - 2), cr_max(0, L.dy0 - 2), cr_min(w, L.d1 + 3), cr_min(h, L.dy1 + 3),
              &conc, &midr);
    out->sharp_pm = conc;
    out->mid_pm = midr;
    if (conc < p->sharp_min_pm[c->path] || midr > p->mid_max_pm[c->path])
        return CARD_REC_R_SOFT_EDGES;
    /* isolated faint px around the distance line: ghost of other text */
    y = cr_max(0, L.dy0 - 6);
    x = cr_max(0, L.d0 - 6);
    if (card_rec_faint_count(px + y * w + x, w, (w - b) - x, cr_min(h, L.dy1 + 7) - y, p->faint_t, p->faint_max) >
        p->text_ghost_max)
        return CARD_REC_R_TEXT_GHOST;
    for (y = L.dy0, d = 0; y <= L.dy1; ++y)        /* light right of the distance: a dimmed 'i' */
        for (x = L.d1 + p->side_light_gap; x < w - b; ++x)
            d = cr_max(d, px[y * w + x]);
    if (d >= p->side_light_t)
        return CARD_REC_R_DISTANCE_SIDE_LIGHT;
    iw = L.ix1 - L.ix0 + 1;
    ih = L.iy1 - L.iy0 + 1;
    r = card_rec_match_icon(c, px + L.iy0 * w + L.ix0, w, iw, ih, &side, out);
    if (r != CARD_REC_R_OK)
        return r;
    bb[0] = L.ix0; bb[1] = L.iy0; bb[2] = L.ix1; bb[3] = L.iy1;
    r = card_rec_icon_gates(c, px, w, h, bb, L.d0, &c->icons[out->icon_template]);
    if (r != CARD_REC_R_OK)
        return r;
    if (c->icons[out->icon_template].has_hole) {
        r = card_rec_read_ring(c, px + L.iy0 * w + L.ix0, w, iw, ih, &c->icons[out->icon_template], &exit_n);
        if (r != CARD_REC_R_OK)
            return r;
    }
    r = card_rec_read_distance(c, px + L.l0 * w + L.d0, w, L.d1 - L.d0 + 1, L.l1 - L.l0 + 1, out);
    if (r != CARD_REC_R_OK)
        return r;
    g = &k_groups[c->icons[out->icon_template].group];
    out->cls = g->cls;
    out->cp_maneuver_type = exit_n ? 27 + exit_n : g->cp;
    out->exit_number = exit_n;
    out->roundabout_direction = g->dir;
    out->traffic_side = side == '-' ? 0 : side;
    /* read completely, never published: card_rec_recognise moves it to internal */
    return p->unpublished & (1u << g->cls) ? CARD_REC_R_CLASS_NOT_PUBLISHED : CARD_REC_R_OK;
}

static int zone_ok(const card_rec_zone_t *z) {
    if (!z)
        return 1;
    if ((int)z->kind < CARD_REC_ZONE_AUTO || z->kind > CARD_REC_ZONE_DETECT)
        return 0;
    return z->kind != CARD_REC_ZONE_BOX ||
           (in(z->x, 0, CARD_REC_MAX_FRAME) && in(z->y, 0, CARD_REC_MAX_FRAME) &&
            in(z->w, 1, CARD_REC_MAX_BOX_W) && in(z->h, 1, CARD_REC_MAX_BOX_H));
}

int card_rec_recognise(card_rec_t *c, const uint8_t *rgba, int stride, int width, int height,
                       const card_rec_zone_t *zone, card_rec_result_t *out) {
    frame_t f;
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    out->cp_maneuver_type = out->distance_value_x10 = out->distance_value_x100 = out->distance_m = -1;
    out->score = out->margin = out->icon_template = -1;
    out->black = out->white = out->cover_pm = out->ink = out->sharp_pm = out->mid_pm = -1;
    out->internal.cp_maneuver_type = out->internal.distance_value_x100 = out->internal.distance_m = -1;
    if (!c) {
        out->reason = CARD_REC_R_DISABLED;
        return 0;
    }
    /* stride first without negating it: -INT_MIN is undefined */
    if (!rgba || width <= 0 || height <= 0 || width > CARD_REC_MAX_FRAME || height > CARD_REC_MAX_FRAME ||
        stride < -CARD_REC_MAX_STRIDE || stride > CARD_REC_MAX_STRIDE || (stride < 0 ? -stride : stride) < 4 * width ||
        !zone_ok(zone) || !card_rec_params_valid(&c->p) || (int)c->path < 0 || c->path >= CARD_REC_PATH_COUNT) {
        out->reason = CARD_REC_R_BAD_ARGS;
        return 0;
    }
    f.px = rgba;
    f.stride = stride;
    f.w = width;
    f.h = height;
    out->reason = run(c, &f, zone, out);
    out->accept = out->reason == CARD_REC_R_OK;
    if (out->reason == CARD_REC_R_CLASS_NOT_PUBLISHED) {   /* withheld: stabiliser history only */
        card_rec_reading_t *w = &out->internal;
        w->cls = out->cls;
        w->cp_maneuver_type = out->cp_maneuver_type;
        w->exit_number = out->exit_number;
        w->traffic_side = out->traffic_side;
        w->distance_value_x100 = out->distance_value_x100;
        w->distance_m = out->distance_m;
        w->distance_unit = out->distance_unit;
        memcpy(w->distance_text, out->distance_text, sizeof(w->distance_text));
    }
    if (!out->accept) {                     /* nothing of a rejected card leaks out */
        out->cls = CARD_REC_CLASS_NONE;
        out->cp_maneuver_type = -1;
        out->exit_number = 0;
        out->traffic_side = 0;
        out->roundabout_direction = CARD_REC_RB_NONE;
        out->distance_value_x10 = out->distance_value_x100 = out->distance_m = -1;
        out->distance_unit = CARD_REC_UNIT_NONE;
        out->distance_text[0] = 0;
    }
    return out->accept;
}
