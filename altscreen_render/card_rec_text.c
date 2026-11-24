/*
 * CarPlay instruction-card recogniser: the distance text (see card_rec.h).
 *
 *   glyphs     cut at the 50 % level and matched against the readable labels
 *              (card_rec_match_glyph); a '~' lookalike label ('~B' for 8,
 *              '~l' for i) within glyph_look_margin of the reading rejects
 *   separator  '.' / ',' must be well matched (sep_d_max) and bright
 *              (sep_peak_min); '.' must not have a flat side: the larger
 *              end-row / end-column sum over the largest row / column sum
 *              <= sep_flat_max_pm (a square speck has a full-length edge)
 *   spacing    the gap between two number glyphs must be one the font makes:
 *              after '1' gap_one_min..gap_one_max, else gap_min..gap_max,
 *              one px less next to a separator, a kerned pair (kern[]) also
 *              at its own gap (a dropped separator whose gap was closed
 *              leaves a gap no real pair of glyphs has)
 *   residual   light the pasted templates do not explain
 *   grammar    number + unit, integer metres
 * Integer only, no allocation on the frame path; ties reject.
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define CARD_REC_INTERNAL
#include "card_rec.h"

#include <string.h>

typedef struct {        /* units the grammar accepts: label -> canonical unit */
    const char *label;
    card_rec_unit_t unit;
    long mm_per;        /* millimetres per unit (ft 305, yd 914, mi 1609344) */
    int int_only;
} unit_t;

static const unit_t k_units[] = {
    {"m", CARD_REC_UNIT_M, 1000L, 1}, {"km", CARD_REC_UNIT_KM, 1000000L, 0},
    {"ft", CARD_REC_UNIT_FT, 305L, 1}, {"yd", CARD_REC_UNIT_YD, 914L, 1},
    {"mi", CARD_REC_UNIT_MI, 1609344L, 0},
    {"\xd0\xbc", CARD_REC_UNIT_M, 1000L, 1},                 /* м  */
    {"\xd0\xba\xd0\xbc", CARD_REC_UNIT_KM, 1000000L, 0},     /* км */
};
#define N_UNITS ((int)(sizeof(k_units) / sizeof(k_units[0])))

const char *card_rec_unit_label(int i) { return i >= 0 && i < N_UNITS ? k_units[i].label : 0; }

/* '.' / ',' shape on the stretched block (glyph box g) */
static int sep_shape_ok(const card_rec_params_t *p, const uint8_t *b, int stride, const card_rec_gbox_t *g,
                        int dot, int d) {
    int w = g->x1 - g->x0 + 1, h = g->y1 - g->y0 + 1, x, y, peak = 0;
    long rmax = 0, cmax = 0, rend, cend, s;
    if (d > p->sep_d_max)
        return 0;
    b += g->y0 * stride + g->x0;
    for (y = 0, rend = 0; y < h; ++y) {
        for (x = 0, s = 0; x < w; ++x) {
            s += b[y * stride + x];
            peak = cr_max(peak, b[y * stride + x]);
        }
        rmax = s > rmax ? s : rmax;
        if ((y == 0 || y == h - 1) && s > rend)
            rend = s;
    }
    for (x = 0, cend = 0; x < w; ++x) {
        for (y = 0, s = 0; y < h; ++y)
            s += b[y * stride + x];
        cmax = s > cmax ? s : cmax;
        if ((x == 0 || x == w - 1) && s > cend)
            cend = s;
    }
    if (peak < p->sep_peak_min)
        return 0;
    return !dot || (1000L * rend / (rmax ? rmax : 1) <= p->sep_flat_max_pm &&
                    1000L * cend / (cmax ? cmax : 1) <= p->sep_flat_max_pm);
}

static int is_num(const char *lab) { return lab[0] && !lab[1] && strchr("0123456789.,", lab[0]); }

/* the one extra gap the font makes for a kerned / tracked pair (CoreText-laid-out
 * real formats: '74 km', '17 km', '150 m', '1,5 km'); per pair - a wider range let
 * a closed-up lost '.' ('1.2 km' -> '12 km') and an added speck ('1.9 km') through */
static const struct { char a, b, gap; } kern[] = {
    {'7', '4', 1}, {'4', '7', 1}, {'4', '4', 1}, {'1', '0', 4}, {'1', '4', 4}, {'1', '7', 4}, {'1', '5', 7},
    {'1', ',', 3}};

/* gap between number glyphs a | b against the font spacing */
static int gap_ok(const card_rec_params_t *p, char a, char b, int gap) {
    int lo = a == '1' ? p->gap_one_min : p->gap_min, hi = a == '1' ? p->gap_one_max : p->gap_max;
    size_t i;
    if (a == '.' || a == ',' || b == '.' || b == ',')
        --lo;
    if (gap >= lo && gap <= hi)
        return 1;
    for (i = 0; i < sizeof(kern) / sizeof(kern[0]); ++i)
        if (kern[i].a == a && kern[i].b == b && kern[i].gap == gap)
            return 1;
    return 0;
}

card_rec_reason_t card_rec_read_distance(card_rec_t *c, const uint8_t *b, int stride, int bw, int bh,
                                         card_rec_result_t *out) {
    static const card_rec_reason_t fail[] = {CARD_REC_R_OK, CARD_REC_R_DISTANCE_NO_TEMPLATE_FITS,
                                             CARD_REC_R_DISTANCE_GLYPH_UNKNOWN,
                                             CARD_REC_R_DISTANCE_GLYPH_AMBIGUOUS};
    const card_rec_params_t *p = &c->p;
    card_rec_gbox_t g[CARD_REC_MAX_GLYPHS];
    card_rec_gmatch_t gm, lm;
    char text[CARD_REC_TEXT_MAX];
    const char *sp, *num;
    const unit_t *u = 0;
    int n = card_rec_segment(b, stride, bw, bh, g, CARD_REC_MAX_GLYPHS), i, x, y, base, top = bh, space;
    int nlen, nsep = 0, sep = -1, ndig = 0, bad = 0, iplen, fplen, ip = 0, fp = 0, res = 0;
    long v100;
    int64_t metres;
    if (n < 2 || n > CARD_REC_MAX_GLYPHS)          /* 2: only a zero, "0 m" (below) */
        return CARD_REC_R_DISTANCE_GLYPH_COUNT;
    for (i = 0; i < n; ++i) {
        if (g[i].y1 - g[i].y0 + 1 > CARD_REC_MAX_GLYPH_H || g[i].x1 - g[i].x0 + 1 > CARD_REC_MAX_GLYPH_W)
            return CARD_REC_R_DISTANCE_GLYPH_TOO_BIG;
        top = cr_min(top, g[i].y0);
    }
    base = card_rec_baseline(g, n);
    space = cr_max(p->space_min, (base - top + 1) * p->space_pm / 1000);
    text[0] = 0;
    memset(c->expl, 0, (size_t)bw * (size_t)bh);   /* bh <= ICON_H_MAX <= MAX_ICON */
    for (i = 0; i < n; ++i) {
        int m = card_rec_match_glyph(&c->glyphs, p, b, stride, &g[i], base, CARD_REC_LABELS_ALL, p->glyph_d_max,
                                     p->glyph_margin, &gm);
        const char *lab;
        size_t tl;
        if (i && g[i].x0 - g[i - 1].x1 - 1 >= space)
            strcat(text, " ");
        tl = strlen(text);
        if (m != CARD_REC_MATCH_OK)
            return fail[m];
        lab = c->glyphs.labels[gm.label];
        if (card_rec_match_glyph(&c->glyphs, p, b, stride, &g[i], base, CARD_REC_LABELS_REJECT, 1000, -1, &lm) ==
                CARD_REC_MATCH_OK && lm.d - gm.d <= p->glyph_look_margin)
            return CARD_REC_R_DISTANCE_GLYPH_LOOKALIKE;
        card_rec_paste(c->expl, bw, bh, g[i].x0, g[i].y1, &gm);
        if ((gm.label == c->glyphs.dot || gm.label == c->glyphs.comma) &&
            !sep_shape_ok(p, b, stride, &g[i], gm.label == c->glyphs.dot, gm.d))
            return CARD_REC_R_DISTANCE_SEPARATOR_SHAPE;
        if (i && !strchr(text, ' ') && tl && is_num(text + tl - 1) && is_num(lab) &&
            !gap_ok(p, text[tl - 1], lab[0], g[i].x0 - g[i - 1].x1 - 1))
            return CARD_REC_R_DISTANCE_GLYPH_SPACING;
        strcat(text, lab);                          /* 12 x (7 + 1) + 1 < TEXT_MAX */
    }
    /* light no matched template explains: a crossfaded distance, an extra mark */
    for (y = 0; y < bh; ++y)
        for (x = 0; x < bw; ++x)
            res += b[y * stride + x] >= p->text_ghost_t && cr_dil3(c->expl, bw, bw, bh, x, y) <= p->ghost_tz;
    if (res > p->text_res_max)
        return CARD_REC_R_DISTANCE_RESIDUAL;
    sp = strchr(text, ' ');
    if (!sp || strchr(sp + 1, ' '))
        return CARD_REC_R_DISTANCE_GRAMMAR;
    for (i = 0; i < N_UNITS; ++i)
        if (!strcmp(sp + 1, k_units[i].label))
            u = &k_units[i];
    if (!u)
        return CARD_REC_R_DISTANCE_UNIT_UNKNOWN;
    num = text;
    nlen = (int)(sp - text);
    for (i = 0; i < nlen; ++i) {
        if (num[i] == '.' || num[i] == ',') {
            if (!nsep++)
                sep = i;
        } else if (num[i] >= '0' && num[i] <= '9') {
            ++ndig;
        } else {
            bad = 1;
        }
    }
    if (bad || !ndig || nsep > 1)
        return CARD_REC_R_DISTANCE_NUMBER_GRAMMAR;
    iplen = nsep ? sep : nlen;
    fplen = nsep ? nlen - sep - 1 : 0;
    if (nsep ? (u->int_only || !iplen || fplen < 1 || fplen > 2 || iplen > 3) : iplen > 4)
        return CARD_REC_R_DISTANCE_NUMBER_GRAMMAR;
    if (iplen > 1 && num[0] == '0')
        return CARD_REC_R_DISTANCE_NUMBER_GRAMMAR;
    for (i = 0; i < iplen; ++i)
        ip = ip * 10 + (num[i] - '0');
    for (i = 0; i < fplen; ++i)
        fp = fp * 10 + (num[sep + 1 + i] - '0');
    v100 = (long)ip * 100 + (fplen ? fp * (fplen == 1 ? 10 : 1) : 0);
    metres = ((int64_t)v100 * u->mm_per + 50000) / 100000;
    /* '0 m' is shown at the maneuver point; a decimal zero never is */
    if ((v100 == 0 && fplen) || metres > 2000000)
        return CARD_REC_R_DISTANCE_IMPLAUSIBLE;
    if (n == 2 && v100)             /* '3 m': off every ladder, '3 mi' with an occluded 'i' */
        return CARD_REC_R_DISTANCE_GLYPH_COUNT;
    out->distance_value_x100 = (int)v100;
    out->distance_value_x10 = v100 % 10 ? -1 : (int)(v100 / 10);
    out->distance_unit = u->unit;
    out->distance_m = (int)metres;
    memcpy(out->distance_text, text, sizeof(text));
    return CARD_REC_R_OK;
}
