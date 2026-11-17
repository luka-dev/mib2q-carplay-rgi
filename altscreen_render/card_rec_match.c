/*
 * CarPlay instruction-card recogniser: template matching (see card_rec.h).
 *
 *   icon      stroke bbox padded to a square, exact area average to 32 x 32,
 *             masked soft-Jaccard (per-mille) against every icon template; the
 *             margin is taken against every other-group template (size-gated
 *             or not), against D4 decoys (rotated / mirrored templates that
 *             mean another group), the ring's other side and its rotations
 *   glyphs    cut at the 50 % level, native-scale templates (+-1 px shifts,
 *             size and baseline gates); the best placement is kept so the
 *             matched templates can be pasted back (residual gates)
 *   (the distance reading itself is in card_rec_text.c)
 *
 * Integer only, no allocation on the frame path; ties reject.
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define CARD_REC_INTERNAL
#include "card_rec.h"

#include <string.h>

/* Exact area average of src (sw x sh) to dst (dw x dh), any ratio: output i
 * covers [i*sw, (i+1)*sw) and source k covers [k*dw, (k+1)*dw) on a common
 * grid; weights are the overlaps, one rounded division by sw*sh at the end. */
static void area_resample(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh) {
    int i, j, kx, ky;
    if (sw % dw == 0 && sh % dh == 0) {
        int bx = sw / dw, by = sh / dh, k = bx * by;
        for (j = 0; j < dh; ++j)
            for (i = 0; i < dw; ++i) {
                long sum = 0;
                for (ky = 0; ky < by; ++ky)
                    for (kx = 0; kx < bx; ++kx)
                        sum += src[(j * by + ky) * sw + i * bx + kx];
                dst[j * dw + i] = (uint8_t)((sum + k / 2) / k);
            }
        return;
    }
    for (j = 0; j < dh; ++j) {
        int ylo = j * sh, yhi = (j + 1) * sh;
        for (i = 0; i < dw; ++i) {
            int xlo = i * sw, xhi = (i + 1) * sw;
            long sum = 0, den = (long)sw * sh;
            for (ky = ylo / dh; ky <= (yhi - 1) / dh; ++ky) {
                int wy = cr_min(yhi, (ky + 1) * dh) - cr_max(ylo, ky * dh);
                long row = 0;
                if (wy <= 0)
                    continue;
                for (kx = xlo / dw; kx <= (xhi - 1) / dw; ++kx) {
                    int wx = cr_min(xhi, (kx + 1) * dw) - cr_max(xlo, kx * dw);
                    if (wx > 0)
                        row += (long)wx * src[ky * sw + kx];
                }
                sum += (long)wy * row;
            }
            dst[j * dw + i] = (uint8_t)((sum + den / 2) / den);
        }
    }
}

/* soft Jaccard distance with care mask: grey (optional) cells accept anything
 * up to template + OPT_TOL; 1000 - 1000 * sum(min) / sum(max) */
int card_rec_icon_distance(const uint8_t *o, const uint8_t *img, const uint8_t *care, int opt_tol) {
    uint32_t smn = 0, smx = 0;
    int i;
    for (i = 0; i < CARD_REC_NORM * CARD_REC_NORM; ++i) {
        int ov = o[i], te;
        if (!care[i])
            continue;
        te = care[i] == 2 ? img[i] : cr_min(ov, img[i] + opt_tol);
        smn += (uint32_t)cr_min(ov, te);
        smx += (uint32_t)cr_max(ov, te);
    }
    return smx ? 1000 - (int)((1000u * smn + smx / 2u) / smx) : 1000;
}

/* per-mille of grey-art cells seen clearly white (the class stroke differs) */
static int grey_white_pm(const uint8_t *o, const card_rec_icon_t *t, const card_rec_params_t *p) {
    int i, n = 0, v = 0;
    for (i = 0; i < CARD_REC_NORM * CARD_REC_NORM; ++i)
        if (t->care[i] == 1) {
            ++n;
            v += o[i] > t->img[i] + p->opt_tol && o[i] >= p->opt_white;
        }
    return n ? 1000 * v / n : 0;
}

/* D4 transform k (hflip, vflip, rot180, transpose, rot90cw, rot90ccw,
 * antitranspose): out[y][x] = in[src(y, x)].  scatter writes in[y][x] to
 * out[src(y, x)] instead: d(o, T(img), T(care)) == d(scatter(o), img, care),
 * so a decoy is scored without storing transformed templates. */
static int d4_src(int k, int y, int x) {
    const int n1 = CARD_REC_NORM - 1;
    switch (k) {
    case 0: return y * CARD_REC_NORM + n1 - x;
    case 1: return (n1 - y) * CARD_REC_NORM + x;
    case 2: return (n1 - y) * CARD_REC_NORM + n1 - x;
    case 3: return x * CARD_REC_NORM + y;
    case 4: return (n1 - x) * CARD_REC_NORM + y;
    case 5: return x * CARD_REC_NORM + n1 - y;
    default: return (n1 - x) * CARD_REC_NORM + n1 - y;
    }
}

void card_rec_d4(const uint8_t *src, uint8_t *dst, int k, int scatter) {
    int x, y;
    for (y = 0; y < CARD_REC_NORM; ++y)
        for (x = 0; x < CARD_REC_NORM; ++x) {
            int i = y * CARD_REC_NORM + x, j = d4_src(k, y, x);
            if (scatter)
                dst[j] = src[i];
            else
                dst[i] = src[j];
        }
}

/* Groups each transformed template is equivalent to (a real template of that
 * group within DECOY_EQ_D of it).  Load time, and again when OPT_TOL or
 * DECOY_EQ_D change: N x 7 x N masked Jaccards. */
void card_rec_build_decoys(card_rec_t *c) {
    uint8_t img[CARD_REC_NORM * CARD_REC_NORM];
    int i, j, k;
    for (i = 0; i < c->nicons; ++i)
        for (k = 0; k < CARD_REC_D4; ++k) {
            uint32_t gm = 0;
            card_rec_d4(c->icons[i].img, img, k, 0);
            for (j = 0; j < c->nicons; ++j)
                if (card_rec_icon_distance(img, c->icons[j].img, c->icons[j].care, c->p.opt_tol) <= c->p.decoy_eq_d)
                    gm |= 1u << c->icons[j].group;
            c->decoy_groups[i][k] = gm;
        }
    c->decoy_tol = c->p.opt_tol;
    c->decoy_eq = c->p.decoy_eq_d;
}

/* min distance of the norm against transform k of the templates in `use` */
static int transformed_min(card_rec_t *c, int k, const uint8_t *use, int cur) {
    int i;
    card_rec_d4(c->norm, c->dn, k, 1);
    for (i = 0; i < c->nicons; ++i)
        if (use[i])
            cur = cr_min(cur, card_rec_icon_distance(c->dn, c->icons[i].img, c->icons[i].care, c->p.opt_tol));
    return cur;
}

static card_rec_reason_t classify(card_rec_t *c, int iw, int ih, char *side, card_rec_result_t *out) {
    static const int k_rot[3] = {2, 4, 5};
    const card_rec_params_t *p = &c->p;
    int sc[CARD_REC_MAX_ICONS], raw[CARD_REC_MAX_ICONS], i, k, best = -1, second = 1000, same = -1, dm = 1001;
    uint8_t use[CARD_REC_MAX_ICONS];
    const card_rec_icon_t *bt;
    for (i = 0; i < c->nicons; ++i) {
        const card_rec_icon_t *t = &c->icons[i];
        int tw = cr_max(p->size_tol_min, t->exp_w * p->size_tol_pm / 1000);
        int th = cr_max(p->size_tol_min, t->exp_h * p->size_tol_pm / 1000);
        int gated = cr_abs(iw - t->exp_w) > tw || cr_abs(ih - t->exp_h) > th ||
                    grey_white_pm(c->norm, t, p) > p->opt_white_max_pm;
        raw[i] = card_rec_icon_distance(c->norm, t->img, t->care, p->opt_tol);
        sc[i] = gated ? 1000 : raw[i];
        if (best < 0 || sc[i] < sc[best])
            best = i;
    }
    bt = &c->icons[best];
    for (i = 0; i < c->nicons; ++i) {   /* raw distance: gated templates count too */
        if (c->icons[i].group != bt->group)
            second = cr_min(second, raw[i]);
        else if (c->icons[i].side != bt->side && (same < 0 || raw[i] < same))
            same = raw[i];
    }
    out->score = sc[best];
    out->margin = second - sc[best];
    out->icon_template = best;
    if (sc[best] > p->icon_d_max)
        return CARD_REC_R_ICON_UNKNOWN;
    if (out->margin <= p->icon_margin)
        return CARD_REC_R_ICON_AMBIGUOUS;
    if (card_rec_group_is_keep(bt->group) && sc[best] > p->keep_d_max)
        return CARD_REC_R_ICON_KEEP_UNSURE;
    if (c->decoy_tol != p->opt_tol || c->decoy_eq != p->decoy_eq_d)
        card_rec_build_decoys(c);
    for (k = 0; k < CARD_REC_D4; ++k) {  /* decoys that do not mean the best group */
        int any = 0;
        for (i = 0; i < c->nicons; ++i) {
            use[i] = !(c->decoy_groups[i][k] & (1u << bt->group));
            any |= use[i];
        }
        if (any)
            dm = transformed_min(c, k, use, dm);
    }
    if (dm <= 1000 && dm - sc[best] <= p->icon_margin)
        return CARD_REC_R_ICON_DECOY;
    if (same >= 0 && same - sc[best] <= p->side_margin)
        return CARD_REC_R_RING_SIDE_AMBIGUOUS;
    if (bt->has_hole) {                 /* ring art turned by 90 / 180 deg */
        for (i = 0; i < c->nicons; ++i)
            use[i] = (uint8_t)c->icons[i].has_hole;
        for (k = 0, dm = 1001; k < 3; ++k)
            dm = transformed_min(c, k_rot[k], use, dm);
        if (dm - sc[best] <= p->orient_margin)
            return CARD_REC_R_RING_ROTATED;
    }
    *side = bt->side;
    return CARD_REC_R_OK;
}

/* Icon at (s, stride) of iw x ih stretched px: pad to square, area average to
 * NORM x NORM, classify.  Sets score / margin / template on out. */
card_rec_reason_t card_rec_match_icon(card_rec_t *c, const uint8_t *s, int stride, int iw, int ih, char *side,
                                      card_rec_result_t *out) {
    int y, sq = cr_max(iw, ih);
    if (sq > CARD_REC_MAX_ICON)             /* only with loosened ICON_*_MAX params */
        return CARD_REC_R_ICON_SIZE;
    memset(c->sq, 0, (size_t)(sq * sq));
    for (y = 0; y < ih; ++y)
        memcpy(c->sq + (y + (sq - ih) / 2) * sq + (sq - iw) / 2, s + y * stride, (size_t)iw);
    area_resample(c->sq, sq, sq, c->norm, CARD_REC_NORM, CARD_REC_NORM);
    return classify(c, iw, ih, side, out);
}

/* ------------------------------------------------------------------ glyphs */
/* column runs (no gap) of ink at the 50 % level; returns the total count and
 * fills at most `max` boxes */
int card_rec_segment(const uint8_t *b, int stride, int bw, int bh, card_rec_gbox_t *g, int max) {
    int n = 0, x = 0, y;
    while (x < bw) {
        int a = x;
        for (y = 0; y < bh && b[y * stride + x] < T_INK; ++y)
            ;
        if (y == bh) {
            ++x;
            continue;
        }
        while (x < bw) {
            for (y = 0; y < bh && b[y * stride + x] < T_INK; ++y)
                ;
            if (y == bh)
                break;
            ++x;
        }
        if (n < max) {
            g[n].x0 = a;
            g[n].x1 = x - 1;
            cr_ink_rows(b, stride, a, x - 1, 0, bh - 1, &g[n].y0, &g[n].y1);
        }
        ++n;
    }
    return n;
}

int card_rec_baseline(const card_rec_gbox_t *g, int n) {
    int v[CARD_REC_MAX_GLYPHS], i, j;
    for (i = 0; i < n; ++i) {               /* insertion sort of the bottoms */
        int y = g[i].y1;
        for (j = i; j > 0 && v[j - 1] > y; --j)
            v[j] = v[j - 1];
        v[j] = y;
    }
    return v[n / 2];
}

/* best soft Jaccard of glyph (gw x gh) vs template over +-1 px, both
 * bottom-left aligned; sum(max) = sum(a) + sum(b) - sum(min).  The first
 * placement (dy-major) with the lowest distance wins. */
static int glyph_distance(const uint8_t *g, int stride, int gw, int gh, uint32_t gsum,
                          const card_rec_glyph_t *t, int *bdx, int *bdy) {
    int best = 1001, dx, dy, x, y;
    for (dy = -1; dy <= 1; ++dy)
        for (dx = -1; dx <= 1; ++dx) {
            uint32_t smn = 0, smx;
            int d;
            for (y = 0; y < gh; ++y) {
                int ty = y - gh + t->h - dy;
                if (ty < 0 || ty >= t->h)
                    continue;
                for (x = cr_max(0, dx); x < gw && x - dx < t->w; ++x)
                    smn += (uint32_t)cr_min(g[y * stride + x], t->px[ty * t->w + x - dx]);
            }
            smx = gsum + t->sum - smn;
            d = smx ? 1000 - (int)(((uint64_t)smn * 1000u + smx / 2u) / smx) : 1000;
            if (d < best) {
                best = d;
                *bdx = dx;
                *bdy = dy;
            }
        }
    return best;
}

/* Best label (filter: readable, digit, non-digit or '~' labels) with size /
 * baseline-offset gates; '.' and ',' are one class for the margin, which is
 * taken to the best label outside the winner's class and must be MORE than
 * margin_min.  m gets the best label, distance and placement whenever any
 * template fits. */
int card_rec_match_glyph(const card_rec_glyph_set_t *set, const card_rec_params_t *p, const uint8_t *img,
                         int stride, const card_rec_gbox_t *g, int base, int filter, int d_max, int margin_min,
                         card_rec_gmatch_t *m) {
    int dl[CARD_REC_MAX_LABELS], px[CARD_REC_MAX_LABELS], py[CARD_REC_MAX_LABELS];
    const card_rec_glyph_t *bt[CARD_REC_MAX_LABELS];
    int i, x, y, best = -1, second = 1000, sep, dx = 0, dy = 0;
    int w = g->x1 - g->x0 + 1, h = g->y1 - g->y0 + 1, top = g->y0 - base, bot = g->y1 - base;
    uint32_t gsum = 0;
    img += g->y0 * stride + g->x0;
    for (i = 0; i < set->nlabels; ++i)
        dl[i] = 1001;
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x)
            gsum += img[y * stride + x];
    for (i = 0; i < set->count; ++i) {
        const card_rec_glyph_t *t = &set->t[i];
        int d;
        int rej = set->labels[t->label][0] == '~';
        if ((filter == CARD_REC_LABELS_DIGITS && !set->is_digit[t->label]) ||
            (filter == CARD_REC_LABELS_LETTERS && set->is_digit[t->label]) ||
            (filter == CARD_REC_LABELS_ALL && rej) || (filter == CARD_REC_LABELS_REJECT && !rej))
            continue;
        if (cr_abs(t->h - h) > p->glyph_size_tol || cr_abs(t->w - w) > p->glyph_size_tol ||
            cr_abs(t->top - top) > p->glyph_off_tol || cr_abs(t->bottom - bot) > p->glyph_off_tol)
            continue;
        d = glyph_distance(img, stride, w, h, gsum, t, &dx, &dy);
        if (d < dl[t->label]) {
            dl[t->label] = d;
            bt[t->label] = t;
            px[t->label] = dx;
            py[t->label] = dy;
        }
    }
    for (i = 0; i < set->nlabels; ++i)
        if (dl[i] <= 1000 && (best < 0 || dl[i] < dl[best]))
            best = i;
    memset(m, 0, sizeof(*m));
    m->label = best;
    m->d = 1000;
    if (best < 0)
        return CARD_REC_MATCH_NO_FIT;
    m->d = dl[best];
    m->t = bt[best];
    m->dx = px[best];
    m->dy = py[best];
    sep = best == set->dot || best == set->comma;
    for (i = 0; i < set->nlabels; ++i)
        if (i != best && dl[i] <= 1000 && !(sep && (i == set->dot || i == set->comma)))
            second = cr_min(second, dl[i]);
    if (dl[best] > d_max)
        return CARD_REC_MATCH_UNKNOWN;
    if (second - dl[best] <= margin_min)
        return CARD_REC_MATCH_AMBIGUOUS;
    return CARD_REC_MATCH_OK;
}

/* max-paste the matched template into ex (ew x eh): its bottom row on row
 * y1 + dy and its left column on x0 + dx (the best placement) */
void card_rec_paste(uint8_t *ex, int ew, int eh, int x0, int y1, const card_rec_gmatch_t *m) {
    const card_rec_glyph_t *t = m->t;
    int top = y1 + 1 - t->h + m->dy, left = x0 + m->dx, tx, ty;
    for (ty = cr_max(0, -top); ty < t->h && top + ty < eh; ++ty)
        for (tx = cr_max(0, -left); tx < t->w && left + tx < ew; ++tx) {
            uint8_t *q = ex + (top + ty) * ew + left + tx;
            *q = (uint8_t)cr_max(*q, t->px[ty * t->w + tx]);
        }
}
