/* scene_clusters.c — StageClusters of tools/stereo-upmix-render/scene_upmix.py in C (variant J).
 *
 * Slow pan clustering: the rear stage is a small set of PLACES (13.2 item 2). Float32 only, no
 * allocation, no libm beyond expf/sqrtf/fabsf, no dependency on the rest of the hook. The Python's
 * order of operations is kept wherever a rounding could cross one of its thresholds; the Python's
 * unbounded lists (clusters, song memory) are the fixed arrays of scene_engine.h, see the two
 * ponytail notes for what happens when they are full. */
#include <math.h>
#include <string.h>

#include "scene_clusters.h"
#include "scene_simd.h"

/* ----------------------------------------------------------------------------- local helpers
 * Copies of the Python helpers (scene_engine.c has scene_smoothstep/scene_ema_alpha; this file
 * stays linkable on its own, so it carries its own four-line versions). */
static float smoothstep_f(float x, float lo, float hi)
{
    float d = hi - lo;
    float t = (x - lo) / (d > 1e-9f ? d : 1e-9f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static float ema_alpha_f(float tau_ms, int hop)
{
    if (tau_ms <= 0.0f) return 1.0f;
    return 1.0f - expf(-(float)hop / (float)SCENE_SR / (tau_ms / 1000.0f));
}

static float sign_f(float x) { return x > 0.0f ? 1.0f : (x < 0.0f ? -1.0f : 0.0f); }

/* np.linspace(-1, 1, 22): edges[i] = i * (2/21) + (-1), the last one exactly the stop value */
static float hist_edge(int i)
{
    return i == SCENE_HIST_BINS ? 1.0f : (float)i * (2.0f / (float)SCENE_HIST_BINS) + (-1.0f);
}

/* np.histogram bin of v: edges[i] <= v < edges[i+1], the last bin inclusive on the right; values
 * outside [-1, 1] (and NaN) are dropped. The index guess is corrected against the edges exactly
 * like numpy's uniform-bin fix-up, so a value sitting on an edge lands where the reference puts it. */
static int hist_bin(float v)
{
    int i;
    if (!(v >= -1.0f && v <= 1.0f)) return -1;
    i = (int)((v + 1.0f) * ((float)SCENE_HIST_BINS / 2.0f));
    if (i > SCENE_HIST_BINS - 1) i = SCENE_HIST_BINS - 1;
    if (i > 0 && v < hist_edge(i)) i--;
    else if (i < SCENE_HIST_BINS - 1 && v >= hist_edge(i + 1)) i++;
    return i;
}

/* ----------------------------------------------------------------------------- song memory
 * _remember / _remembered: places admitted earlier in this song (re-admitted after admit_s*(1-boost)) */
static void remember(struct scene_clusters *c, float centre)
{
    int i, s;
    for (i = 0; i < c->n_memory; i++) {
        struct scene_memory *m = &c->memory[i];
        if (fabsf(m->centre - centre) < 0.15f) {
            m->centre += 0.1f * (centre - m->centre);
            m->last_s = c->song_s;
            return;
        }
    }
    if (c->n_memory < SCENE_MEMORY_MAX) {
        s = c->n_memory++;
    } else {
        /* ponytail: the Python list is unbounded; when the 16 slots are full the stalest entry (the
         * one a track change would have cleared first) is overwritten. Entries older than memory_s
         * are dead weight anyway, so this only ever differs from the reference on a 16-place song. */
        s = 0;
        for (i = 1; i < SCENE_MEMORY_MAX; i++)
            if (c->memory[i].last_s < c->memory[s].last_s) s = i;
    }
    c->memory[s].centre = centre;
    c->memory[s].last_s = c->song_s;
}

static int remembered(const struct scene_clusters *c, float centre)
{
    int i;
    for (i = 0; i < c->n_memory; i++)
        if (fabsf(c->memory[i].centre - centre) < 0.15f && c->song_s - c->memory[i].last_s <= c->cfg.memory_s)
            return 1;
    return 0;
}

/* ----------------------------------------------------------------------------- _shadow
 * True when the candidate's bins are already owned by an admitted place (overlap > 0.5) */
static int shadow(const struct scene_clusters *c, int ci)
{
    const struct scene_place *cand = &c->c[ci];
    float den2c = 2.0f * (cand->sigma * cand->sigma);
    int i, j;
    if (!c->have_last) return 0;
    for (j = 0; j < c->n; j++) {
        const struct scene_place *o = &c->c[j];
        float den2o, num = 0.0f, den = 0.0f, ov;
        if (j == ci || !o->admitted || o->dying) continue;
        den2o = 2.0f * (o->sigma * o->sigma);
        for (i = 0; i < SCENE_BINS; i++) {
            float dc = c->last_p[i] - cand->centre, dd = c->last_p[i] - o->centre;
            float mc = expf(-(dc * dc) / den2c), mo = expf(-(dd * dd) / den2o);
            num += (mc < mo ? mc : mo) * c->last_e[i];
            den += mc * c->last_e[i];
        }
        ov = num / (den + 1e-12f);
        if (ov > 0.5f) return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------- _pick
 * Peaks of the pan histogram -> matching against the places -> new places -> merges -> k_max */
struct peak { float mass, cen, sig; };

/* tuple order of the Python sort: (mass, cen, sig) */
static int peak_less(const struct peak *a, const struct peak *b)
{
    if (a->mass != b->mass) return a->mass < b->mass;
    if (a->cen != b->cen) return a->cen < b->cen;
    return a->sig < b->sig;
}

static void pick(struct scene_clusters *c)
{
    const struct scene_config *cfg = &c->cfg;
    struct peak peaks[SCENE_HIST_BINS], chosen[SCENE_HIST_BINS], pend[SCENE_HIST_BINS];
    float pend_seen0[SCENE_HIST_BINS];
    int off[SCENE_HIST_BINS], matched[SCENE_PLACES_MAX], idx[SCENE_PLACES_MAX];
    float total = 0.0f, total_off = 0.0f;
    int i, j, np = 0, nc = 0, nl = 0, npend = 0, no_stage;

    for (i = 0; i < SCENE_HIST_BINS; i++) total += c->hist[i];
    total += 1e-12f;
    /* The centre (lead, bass, drum core) holds most of the mass of any real mix; a rear place is
     * judged against the OFF-CENTRE mass, and there is no rear stage at all when that is < 2 %. */
    for (i = 0; i < SCENE_HIST_BINS; i++) {
        off[i] = fabsf(c->centres_h[i]) >= cfg->min_abs_pan;
        if (off[i]) total_off += c->hist[i];
    }
    no_stage = total_off < 0.02f * total;              /* Python: total_off = inf, every mass 0 */
    for (i = 0; i < SCENE_HIST_BINS && !no_stage; i++) {
        float left, right, hs = 0.0f, hcs = 0.0f, v = 0.0f, mass, cen, sig;
        int lo, hi;
        if (c->hist[i] <= 0.0f || !off[i]) continue;
        left = i > 0 ? c->hist[i - 1] : 0.0f;
        right = i < SCENE_HIST_BINS - 1 ? c->hist[i + 1] : 0.0f;
        if (!(c->hist[i] >= left && c->hist[i] >= right)) continue;
        lo = i > 0 ? i - 1 : 0;                        /* slice(max(0, i - 1), i + 2) */
        hi = i < SCENE_HIST_BINS - 1 ? i + 1 : SCENE_HIST_BINS - 1;
        for (j = lo; j <= hi; j++) hs += c->hist[j];
        mass = hs / total_off;
        if (!(mass >= cfg->admit_mass * 0.5f)) continue;
        for (j = lo; j <= hi; j++) hcs += c->hist[j] * c->centres_h[j];
        cen = hcs / (hs + 1e-12f);
        if (fabsf(cen) > 0.9f)                         /* the outer bins are centred at +-0.952 */
            cen = sign_f(cen);
        for (j = lo; j <= hi; j++) {
            float d = c->centres_h[j] - cen;
            v += d * d * c->hist[j];
        }
        sig = sqrtf(v / (hs + 1e-12f));
        if (sig < cfg->sigma_min) sig = cfg->sigma_min;
        if (sig > cfg->sigma_max) sig = cfg->sigma_max;
        peaks[np].mass = mass; peaks[np].cen = cen; peaks[np].sig = sig; np++;
    }
    /* peaks.sort(reverse=True): descending, stable */
    for (i = 1; i < np; i++) {
        struct peak t = peaks[i];
        for (j = i; j > 0 && peak_less(&peaks[j - 1], &t); j--) peaks[j] = peaks[j - 1];
        peaks[j] = t;
    }
    if (c->silent)                                     /* a decayed histogram keeps its shape: no peaks in silence */
        np = 0;
    for (i = 0; i < np; i++) {
        for (j = 0; j < nc; j++)
            if (!(fabsf(peaks[i].cen - chosen[j].cen) >= cfg->peak_sep)) break;
        if (j == nc) chosen[nc++] = peaks[i];
        if (nc >= cfg->k_max) break;
    }

    memset(matched, 0, sizeof matched);
    for (i = 0; i < c->n; i++) c->c[i].present = 0;
    for (i = 0; i < nc; i++) {
        float mass = chosen[i].mass, cen = chosen[i].cen, sig = chosen[i].sig, best = 0.0f;
        int bi = -1;
        for (j = 0; j < c->n; j++) {
            float d;
            if (matched[j] || c->c[j].dying) continue;
            d = fabsf(c->c[j].centre - cen);
            if (d < 0.15f && (bi < 0 || d < best)) { best = d; bi = j; }
        }
        if (bi >= 0) {
            struct scene_place *k = &c->c[bi];
            float nw = k->centre + c->a_centre * (cen - k->centre);
            float slew = cfg->centre_slew_per_s * c->dt_pick, thr;
            matched[bi] = 1;
            if (nw < k->centre - slew) nw = k->centre - slew;
            if (nw > k->centre + slew) nw = k->centre + slew;
            k->centre = nw;
            k->sigma += c->a_centre * (sig - k->sigma);
            k->mass = mass;
            thr = k->admitted ? cfg->drop_mass : cfg->admit_mass;
            k->present = mass >= thr;
            if (k->present) {
                k->absent_s = 0.0f;
                k->seen_s += c->dt_pick;
            } else {
                k->absent_s += c->dt_pick;
                if (!k->admitted) k->seen_s = 0.0f;    /* dwell must be continuous */
            }
        } else if (mass >= cfg->admit_mass) {
            /* a new cluster counts as matched in its own pick (its dwell starts now); Python appends it
             * here, which a later peak of this pick can neither match nor see, so it is added below */
            pend[npend] = chosen[i];
            pend_seen0[npend++] = c->dt_pick + (remembered(c, cen) ? cfg->admit_s * cfg->memory_boost : 0.0f);
        }
    }
    for (i = 0; i < c->n; i++) {
        if (matched[i]) continue;
        c->c[i].absent_s += c->dt_pick;
        if (!c->c[i].admitted) c->c[i].seen_s = 0.0f;
    }
    for (i = 0; i < npend; i++) {
        struct scene_place *k;
        int slot;
        /* The new cluster sorts behind every existing one (gain 0, youngest), so the merge below marks it
         * dying at once when any existing cluster is within 0.15, and the end of this update() deletes
         * it: it never does anything. Not creating it is exact (chosen peaks are peak_sep apart). */
        for (j = 0; j < c->n; j++)
            if (fabsf(c->c[j].centre - pend[i].cen) < 0.15f) break;
        if (j < c->n) continue;
        slot = c->n;
        if (slot == SCENE_PLACES_MAX) {
            /* ponytail: the Python list is unbounded; with all 8 slots taken the oldest ghost (gain 0,
             * never admitted, not matched in this pick: inert for membership and shadow) gives way,
             * else the candidate is dropped. Needs 9 places all 0.15 apart to happen. */
            for (slot = 0; slot < c->n; slot++)
                if (!matched[slot] && !c->c[slot].admitted && c->c[slot].gain <= 0.0f) break;
            if (slot == c->n) continue;
        } else {
            c->n++;
        }
        k = &c->c[slot];
        k->centre = pend[i].cen; k->sigma = pend[i].sig; k->mass = pend[i].mass; k->seen_s = pend_seen0[i];
        k->absent_s = 0.0f; k->gain = 0.0f; k->admitted = 0; k->present = 1; k->dying = 0;
        matched[slot] = 1;
    }
    /* merge: a younger cluster within 0.15 of an older one fades out (no one-pick step);
     * clusters.sort(key=-gain) is stable, so equal gains keep their order */
    for (i = 1; i < c->n; i++) {
        struct scene_place t = c->c[i];
        for (j = i; j > 0 && c->c[j - 1].gain < t.gain; j--) c->c[j] = c->c[j - 1];
        c->c[j] = t;
    }
    for (i = 0; i < c->n; i++) {
        if (c->c[i].dying) continue;
        for (j = 0; j < i; j++)                        /* every earlier cluster, dying ones included */
            if (!(fabsf(c->c[i].centre - c->c[j].centre) >= 0.15f)) { c->c[i].dying = 1; break; }
    }
    /* K <= k_max admitted places: the weakest beyond that fade (stable sort by mass, descending) */
    for (i = 0; i < c->n; i++)
        if (c->c[i].admitted && !c->c[i].dying) idx[nl++] = i;
    for (i = 1; i < nl; i++) {
        int t = idx[i];
        for (j = i; j > 0 && c->c[idx[j - 1]].mass < c->c[t].mass; j--) idx[j] = idx[j - 1];
        idx[j] = t;
    }
    for (i = cfg->k_max; i < nl; i++) c->c[idx[i]].dying = 1;
}

/* ----------------------------------------------------------------------------- public
 */
void scene_clusters_init(struct scene_clusters *c, const struct scene_config *cfg, float frame_s)
{
    int i;
    memset(c, 0, sizeof *c);
    c->cfg = *cfg;
    c->frame_s = frame_s;
    for (i = 0; i < SCENE_HIST_BINS; i++)              /* 0.5 * (edges[:-1] + edges[1:]) */
        c->centres_h[i] = 0.5f * (hist_edge(i) + hist_edge(i + 1));
    c->a_hist = ema_alpha_f(cfg->hist_tau_s * 1000.0f, cfg->hop);
    /* Python round() is half-even; +0.5 truncation differs only at an exact .5 (0.34 s / 5.33 ms = 63.75) */
    c->pick_every = (int)(cfg->pick_every_s / frame_s + 0.5f);
    if (c->pick_every < 1) c->pick_every = 1;
    c->dt_pick = (float)c->pick_every * frame_s;
    c->a_centre = ema_alpha_f(cfg->centre_tau_s * 1000.0f, cfg->hop * c->pick_every);   /* per PICK */
    /* tot_ref: slow maximum of the eligible primary power: silence = 30 dB below it (no peaks, places go absent) */
}

void scene_clusters_update(struct scene_clusters *c, const float *p, const float *ps, const int *ok, int G)
{
    const struct scene_config *cfg = &c->cfg;
    float h[SCENE_HIST_BINS], tot = 0.0f, decayed;
    const float dt = c->frame_s;
    int g, i, j;

    memset(h, 0, sizeof h);
    for (g = 0; g < G; g++) {
        if (!ok[g]) continue;
        i = hist_bin(p[g]);
        if (i >= 0) h[i] += ps[g];
        tot += ps[g];
    }
    for (i = 0; i < SCENE_HIST_BINS; i++) c->hist[i] += c->a_hist * (h[i] - c->hist[i]);
    decayed = c->tot_ref * (1.0f - c->frame_s / 60.0f);
    c->tot_ref = tot > decayed ? tot : decayed;
    c->silent = tot < 1e-3f * c->tot_ref;
    c->frame += 1;
    c->song_s += dt;
    if (c->frame % c->pick_every == 0) pick(c);
    for (i = 0; i < c->n; i++) {
        struct scene_place *k = &c->c[i];
        float target, step;
        int alive;
        if (!k->admitted && k->seen_s >= cfg->admit_s) {
            if (shadow(c, i)) k->dying = 1;            /* the same instrument seen through shared cells: not a new place */
            else k->admitted = 1;
        }
        if (k->admitted && !k->dying && k->present) remember(c, k->centre);
        alive = k->admitted && !k->dying && k->absent_s < cfg->drop_s;
        target = alive ? 1.0f : 0.0f;
        step = target > k->gain ? dt / cfg->ramp_s : dt / 1.5f;
        k->gain += sign_f(target - k->gain) * step;
        if (k->gain < 0.0f) k->gain = 0.0f;
        if (k->gain > 1.0f) k->gain = 1.0f;
    }
    for (i = 0, j = 0; i < c->n; i++) {
        const struct scene_place *k = &c->c[i];
        if (k->gain <= 0.0f && (k->dying || k->absent_s >= cfg->drop_s)) continue;
        if (j != i) c->c[j] = *k;
        j++;
    }
    c->n = j;
}

void scene_clusters_membership(struct scene_clusters *c, const float *p_slow, const float *e_slow, int n,
                               float *w, float *q, float *a_scale)
{
    const struct scene_config *cfg = &c->cfg;
    /* per-place constants of the live places; four bins per step (scene_simd.h), the places inside */
    float den2[SCENE_PLACES_MAX], qk[SCENE_PLACES_MAX], ak[SCENE_PLACES_MAX];
    int live[SCENE_PLACES_MAX], nl = 0, i, j;
    const scene_v4 zero = v4_zero(), tiny = v4_s(1e-12f);

    if (n > SCENE_BINS) n = SCENE_BINS;
    for (j = 0; j < c->n; j++) {
        const struct scene_place *k = &c->c[j];
        if (k->gain <= 0.0f) continue;
        den2[nl] = 2.0f * (k->sigma * k->sigma);
        qk[nl] = sign_f(k->centre) * (0.78f + 0.17f * smoothstep_f(fabsf(k->centre), 0.05f, 0.92f));
        ak[nl] = smoothstep_f(fabsf(k->centre), cfg->min_abs_pan, 0.45f);
        live[nl++] = j;
    }
    for (i = 0; i < n; i += 4) {
        const int m = n - i < 4 ? n - i : 4;
        const scene_v4 p = v4_ld_n(p_slow + i, m);
        scene_v4 num = zero, q_acc = zero, a_acc = zero, pc_acc = zero, nq;
        scene_v4 den = v4_s(cfg->front_floor);                 /* flat front floor (no Gaussian) */
        scene_m4 big;
        for (j = 0; j < nl; j++) {
            const struct scene_place *k = &c->c[live[j]];
            const scene_v4 d = v4_sub(p, v4_s(k->centre));
            const scene_v4 d2 = v4_div(v4_mul(d, d), v4_s(den2[j]));
            scene_v4 mm = v4_sel(v4_lt(d2, v4_s(3.125f)), v4_exp_neg(d2), zero);   /* clipped at 2.5 sigma */
            scene_v4 g;
            if (cfg->member_mass) mm = v4_mul(v4_s(k->mass), mm);
            den = v4_add(den, mm);
            g = v4_mul(v4_s(k->gain), mm);
            num = v4_add(num, g);
            q_acc = v4_add(q_acc, v4_mul(g, v4_s(qk[j])));
            a_acc = v4_add(a_acc, v4_mul(g, v4_s(ak[j])));
            pc_acc = v4_add(pc_acc, v4_mul(g, v4_s(k->centre)));
        }
        big = v4_gt(num, tiny);
        nq = v4_add(num, tiny);
        v4_st_n(w + i, v4_div(num, v4_add(den, tiny)), m);
        v4_st_n(q + i, v4_sel(big, v4_div(q_acc, nq), zero), m);
        v4_st_n(a_scale + i, v4_sel(big, v4_div(a_acc, nq), zero), m);
        v4_st_n(c->last_pc + i, v4_sel(big, v4_div(pc_acc, nq), zero), m);
        v4_st_n(c->last_p + i, p, m);
        v4_st_n(c->last_e + i, e_slow ? v4_ld_n(e_slow + i, m) : v4_s(1.0f), m);
    }
    for (i = n; i < SCENE_BINS; i++) {                 /* unused tail: weight 0 keeps the shadow sums exact */
        c->last_p[i] = 0.0f; c->last_e[i] = 0.0f; c->last_pc[i] = 0.0f;
    }
    c->have_last = 1;
}

void scene_clusters_reset(struct scene_clusters *c)
{
    int i;
    for (i = 0; i < c->n; i++) c->c[i].dying = 1;      /* every place fades out over 1.5 s (the dying ramp) */
    memset(c->hist, 0, sizeof c->hist);
    c->n_memory = 0;
    c->song_s = 0.0f;
}

int scene_clusters_n_live(const struct scene_clusters *c)
{
    int i, n = 0;
    for (i = 0; i < c->n; i++) n += c->c[i].gain > 0.0f;
    return n;
}
