/* scene_engine.c — the scene upmix renderer in C (variant J): scene_engine_init / reset / set_mask /
 * clear_mask / frame / push / flush of scene_engine.h, the port of _engine() (per-frame algorithm) and
 * SceneStream (block driver) of tools/stereo-upmix-render/scene_upmix.py, on top of scene_engine_math.c
 * (helpers, tables, FFT) and scene_clusters.c (StageClusters).
 *
 * C99 (gnu99), float32 only, no FMA (host: -ffp-contract=off), no allocation after init, no other hook
 * file. The Python's order of operations is kept (numpy evaluates a * b * c * d left to right); every
 * generator local of _engine() that persists is a field of struct scene_state; the per-bin eigen
 * decompositions are evaluated bin by bin inside the output loop (the same pure per-element function, so
 * the values are identical) to keep the frame's stack scratch at ~16 KB.
 * Golden tests: tools/stereo-upmix-render/port/test_engine.py (stream_spatial.npz, stream_mask.npz).
 *
 * Mask timing (scene_engine_set_mask): the installed row applies to every frame computed after the call
 * until the next call. Frame t is computed as soon as t * SCENE_HOP input samples have been pushed, so
 * the Python's inst_mask[(t * H) // 512] is reproduced exactly when row r is installed after
 * 512 r - 256 and before 512 r samples have been pushed (test_engine.py splits its blocks there); a
 * caller that pushes 256-sample chunks installs row r before the chunk that starts at 512 r - 256.
 */
#include "scene_engine.h"   /* before <math.h>: the QNX 6.5 math.h sets GCC's _SIZE_T guard without the typedef */
#include "scene_clusters.h"
#include "scene_simd.h"
#include <math.h>
#include <string.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SCENE_ENGINE_NEON 1      /* window / overlap-add four samples per step; the scalar tail is the same arithmetic */
#else
#define SCENE_ENGINE_NEON 0
#endif

#define SCENE_PI 3.14159265358979323846f

/* ----------------------------------------------------------------------------- small helpers */
/* |atan2(im, re)| <= theta  <=>  re >= |C| cos(theta) (|C| = 0 counts as inside, like atan2(0, 0) = 0), squared
 * on the side where both have the sign of cos(theta): the group eligibility gate without atan2f. */
static int angle_within(float re, float im, float cos_t)
{
    const float r2 = re * re, m2 = re * re + im * im, c2 = cos_t * cos_t;
    if (cos_t >= 0.0f) return re >= 0.0f && r2 >= c2 * m2;
    return re >= 0.0f || r2 <= c2 * m2;
}
static int round_half_even(float x)                                             /* np.round, x >= 0 */
{
    int i = (int)x;
    float d = x - (float)i;
    if (d > 0.5f || (d == 0.5f && (i & 1))) i++;
    return i;
}

/* ----------------------------------------------------------------------------- init */
/* side-layer gain per bin (ambient gain, high-pass, optional low-pass): from init() and set_tuning() */
static void scene_side_shape(struct scene_state *s)
{
    const struct scene_config *cfg = &s->cfg;
    int k;

    for (k = 0; k < SCENE_BINS; k++) {
        float f = s->freqs[k];
        int lowmid = f >= 250.0f && f < 680.0f;
        s->GA[k] = (lowmid ? cfg->ambient_gain_lowmid : cfg->ambient_gain) * s->hp[k];
        if (cfg->side_lp_hz > 0.0f)
            s->GA[k] = s->GA[k] * (1.0f - scene_smoothstep(f, cfg->side_lp_hz, 1.4f * cfg->side_lp_hz));
    }
}

void scene_engine_set_tuning(struct scene_state *s, float alpha, float inst_alpha, float duck_depth, float side_lp_hz)
{
    s->cfg.alpha = alpha;
    s->cfg.inst_alpha = inst_alpha;
    s->cfg.duck_depth = duck_depth;
    if (s->cfg.side_lp_hz != side_lp_hz) {
        s->cfg.side_lp_hz = side_lp_hz;
        scene_side_shape(s);
    }
}

int scene_engine_init(struct scene_state *s, const struct scene_config *cfg)
{
    const int N = SCENE_NFFT, H = SCENE_HOP;
    float air_lin;
    int k, d;

    memset(s, 0, sizeof *s);
    s->cfg = *cfg;
    if (cfg->n_fft != N || cfg->hop != H || !cfg->erb_pool || (!cfg->cluster && cfg->unmix))
        return -1;
    s->F = SCENE_BINS;
    s->G = scene_erb_groups(SCENE_BINS, N, s->group, s->group_freq, s->group_count);
    if (s->G > SCENE_GROUPS_MAX) return -1;
    for (k = 0; k < SCENE_BINS; k++)
        s->freqs[k] = (float)k * (float)SCENE_SR / (float)N;                    /* exact in float32 */
    for (k = 0; k < N; k++)                                                     /* sine window of stft()/istft() */
        s->window[k] = sinf(SCENE_PI * ((float)k + 0.5f) / (float)N);

    s->a_s    = scene_ema_alpha(cfg->tau_stats_ms, H);
    s->a_fast = scene_ema_alpha(cfg->tau_fast_ms, H);
    s->a_dir  = scene_ema_alpha(cfg->tau_dir_ms, H);
    s->a_slow = scene_ema_alpha(cfg->tau_slow_ms, H);
    s->a_rel  = scene_ema_alpha(cfg->release_ms, H);
    s->a_env  = scene_ema_alpha(cfg->wide_env_tau_ms, H);
    s->a_iatt = scene_ema_alpha(cfg->inst_attack_ms, H);
    s->a_irel = scene_ema_alpha(cfg->inst_release_ms, H);
    s->a_long = cfg->inst_long_tau_s > 0.0f ? scene_ema_alpha(cfg->inst_long_tau_s * 1000.0f, H) : 0.0f;
    s->a_datt = scene_ema_alpha(cfg->duck_attack_ms, H);
    s->a_drel = scene_ema_alpha(cfg->duck_release_ms, H);
    s->a_anc_meas = scene_ema_alpha(1000.0f, H);
    s->a_anc_att  = scene_ema_alpha(300.0f, H);
    s->a_anc_rel  = scene_ema_alpha(2000.0f, H);
    s->theta_max = cfg->theta_max_deg * (SCENE_PI / 180.0f);                   /* np.deg2rad */
    s->cos_theta_max = cosf(s->theta_max);                                     /* the group gate compares cosines, not angles */

    /* band shaping */
    air_lin = expf((cfg->air_db / 20.0f) * 2.302585092994046f);                /* 10 ** (air_db / 20); the C has no "None" = off, air_db <= -200 is off enough */
    for (k = 0; k < SCENE_BINS; k++) {
        float f = s->freqs[k];
        s->hp[k] = scene_smoothstep(f, cfg->hp_hz, cfg->hp_hz + 60.0f);
        s->air[k] = air_lin * scene_smoothstep(f, 2000.0f, 2500.0f);
        s->same_sign[k] = f <= cfg->same_sign_below_hz;
        /* per-bin eligibility exponent: gamma_lo below the split when > 0, else gamma (1 = no sharpening) */
        s->gamma_vec[k] = cfg->inst_gamma_lo > 0.0f ? (f < cfg->inst_gamma_split_hz ? cfg->inst_gamma_lo : cfg->inst_gamma)
                                                    : cfg->inst_gamma;
    }
    scene_side_shape(s);
    if (cfg->decorrelate_ambient) {
        scene_phase_table(SCENE_BINS, (uint32_t)cfg->seed, +1.0f, s->phL);
        scene_phase_table(SCENE_BINS, (uint32_t)cfg->seed, -1.0f, s->phR);
    } else {
        for (k = 0; k < SCENE_BINS; k++) { s->phL[k].re = 1.0f; s->phL[k].im = 0.0f; s->phR[k] = s->phL[k]; }
    }
    /* keep the low band in phase (no scramble where the pair is same-sign) */
    for (k = 0; k < SCENE_BINS; k++)
        if (s->same_sign[k]) { s->phL[k].re = 1.0f; s->phL[k].im = 0.0f; s->phR[k] = s->phL[k]; }

    /* signed rear/front delay; Python round() is half-even, +0.5 truncation differs only at an exact .5 */
    d = (int)(fabsf(cfg->rear_delay_ms) * ((float)SCENE_SR / 1000.0f) + 0.5f);
    if (d > SCENE_DELAY_MAX) return -1;
    s->dlen = d;
    s->delay_rear = cfg->rear_delay_ms > 0.0f ? d : 0;
    s->delay_front = cfg->rear_delay_ms < 0.0f ? d : 0;
    s->blen = 0;
    if (cfg->bleed_cancel > 0.0f) {
        s->blen = (int)(cfg->bleed_cancel_offset_ms * ((float)SCENE_SR / 1000.0f) + 0.5f);
        if (s->blen < 0) s->blen = 0;
        if (s->blen > SCENE_DELAY_MAX) return -1;
    }
    scene_rfft1024(s->window, s->XL, s->fft_scratch);                          /* builds the FFT tables before the audio thread */
    scene_engine_reset_state(s);
    return 0;
}

void scene_engine_reset_state(struct scene_state *s)
{
    /* everything from the per-bin statistics to the scratch is state; the delay geometry sits in it */
    const int dlen = s->dlen, delay_rear = s->delay_rear, delay_front = s->delay_front, blen = s->blen;

    memset(s->PL, 0, sizeof *s - offsetof(struct scene_state, PL));
    if (s->cfg.cluster)
        scene_clusters_init(&s->clusters, &s->cfg, (float)SCENE_HOP / (float)SCENE_SR);
    /* streaming: the pad (n_fft zeros before the signal) is already in the ring; frame 0 covers it */
    s->in_head = SCENE_NFFT;
    s->in_count = SCENE_NFFT;
    s->next_frame = 0;
    s->dlen = dlen;
    s->delay_rear = delay_rear;
    s->delay_front = delay_front;
    s->blen = blen;
    s->anchor = 1.0f;
}

void scene_engine_reset(struct scene_state *s)
{
    s->pending_reset = 1;
}

/* ----------------------------------------------------------------------------- masks */
void scene_engine_clear_mask(struct scene_state *s)
{
    s->have_mask = 0;
    /* off = the smoothers of a stream that never had a mask: the next set_mask starts them from 0 */
    memset(s->e_inst, 0, sizeof s->e_inst);
    memset(s->e_long, 0, sizeof s->e_long);
    memset(s->d_prot, 0, sizeof s->d_prot);
}

void scene_engine_set_mask(struct scene_state *s, const float *elig, const float *prot, int mask_bins)
{
    int i;
    if ((!elig && !prot) || mask_bins < 1 || mask_bins > SCENE_MASK_BINS_MAX) {
        scene_engine_clear_mask(s);
        return;
    }
    if (mask_bins != s->mask_bins) {
        /* bin -> nearest mask bin: np.round(freqs / (SR / inst_mask_nfft)) clipped to the row, half-even
         * (on the 512-point grid every odd bin sits exactly on a .5) */
        float step = (float)SCENE_SR / (float)s->cfg.inst_mask_nfft;
        s->mask_bins = mask_bins;
        for (i = 0; i < SCENE_BINS; i++) {
            int m = round_half_even(s->freqs[i] / step);
            s->mbin[i] = m < mask_bins - 1 ? m : mask_bins - 1;
        }
    }
    for (i = 0; i < mask_bins; i++) {
        s->elig_row[i] = elig ? elig[i] : 1.0f - prot[i];
        s->prot_row[i] = prot ? prot[i] : 1.0f - elig[i];
    }
    s->have_mask = 1;
}

/* ----------------------------------------------------------------------------- one frame (_engine loop body)
 * Four bins per step on the scene_simd.h lanes (NEON, or the bit-identical scalar fallback): the three EMA
 * sets, the bounded slow pan, the holds, the output stage (long-averaged direction, unmix / transfer,
 * semantic stage, ambient pair). The per-frame SoA intermediates (PN, PS of the 43 ms statistics, the
 * membership outputs, the effective unmix pan) live in fft_scratch[0, 4N), 16-byte aligned: that block is
 * free between the forward and the inverse transforms of push_chunk and unused by the frame-level tests.
 * The 513th bin is a one-lane quad (n lanes loaded, n lanes stored). The group-level statistics (134 ERB
 * groups) go through the public array functions of scene_engine_math.c, which run the same kernels. */
#define SCENE_BINS4 ((SCENE_BINS + 3) & ~3)                       /* 516: bins rounded up to whole quads */
typedef char scene_scratch_fits[6 * SCENE_BINS4 + 4 <= 4 * SCENE_NFFT ? 1 : -1];

struct frame_scratch { float *PN, *PS, *w_member, *q_b, *a_scale, *pc_eff; };   /* [SCENE_BINS4] each */
struct frame_ctx { int unmix, inst_on, prot_on, gamma_on; float cold_gain, eps_bin; };

/* statistics: 43 ms (Wiener scalar, slow pan), 8 ms (fast pan), 300 ms (direction); *bmax folds max(fPL + fPR) */
static inline void stats_quad(struct scene_state *s, const scene_cpx *XL, const scene_cpx *XR, int k, int n, int first, scene_v4 *bmax)
{
    scene_v4 Lr, Li, Rr, Ri, iL, iR, iCr, iCi, fsum;
    v4_ld2_n((const float *)(XL + k), n, &Lr, &Li);
    v4_ld2_n((const float *)(XR + k), n, &Rr, &Ri);
    iL = v4_add(v4_mul(Lr, Lr), v4_mul(Li, Li));                /* |L|^2, |R|^2 */
    iR = v4_add(v4_mul(Rr, Rr), v4_mul(Ri, Ri));
    iCr = v4_add(v4_mul(Lr, Rr), v4_mul(Li, Ri));               /* conj(L) * R */
    iCi = v4_sub(v4_mul(Lr, Ri), v4_mul(Li, Rr));
    if (first) {                                                /* initialise from the first frame (no cold burst) */
        v4_st_n(s->PL + k, iL, n);  v4_st_n(s->PR + k, iR, n);  v4_st2_n((float *)(s->C + k), iCr, iCi, n);
        v4_st_n(s->fPL + k, iL, n); v4_st_n(s->fPR + k, iR, n); v4_st2_n((float *)(s->fC + k), iCr, iCi, n);
        v4_st_n(s->dPL + k, iL, n); v4_st_n(s->dPR + k, iR, n); v4_st2_n((float *)(s->dC + k), iCr, iCi, n);
        fsum = v4_add(iL, iR);
    } else {
        const scene_v4 as = v4_s(s->a_s), af = v4_s(s->a_fast), ad = v4_s(s->a_dir);
        scene_v4 pl, pr, cr, ci;
        pl = v4_ema(v4_ld_n(s->PL + k, n), as, iL);
        pr = v4_ema(v4_ld_n(s->PR + k, n), as, iR);
        v4_ld2_n((const float *)(s->C + k), n, &cr, &ci);
        cr = v4_ema(cr, as, iCr);
        ci = v4_ema(ci, as, iCi);
        v4_st_n(s->PL + k, pl, n); v4_st_n(s->PR + k, pr, n); v4_st2_n((float *)(s->C + k), cr, ci, n);
        pl = v4_ema(v4_ld_n(s->fPL + k, n), af, iL);
        pr = v4_ema(v4_ld_n(s->fPR + k, n), af, iR);
        v4_ld2_n((const float *)(s->fC + k), n, &cr, &ci);
        cr = v4_ema(cr, af, iCr);
        ci = v4_ema(ci, af, iCi);
        v4_st_n(s->fPL + k, pl, n); v4_st_n(s->fPR + k, pr, n); v4_st2_n((float *)(s->fC + k), cr, ci, n);
        fsum = v4_add(pl, pr);
        pl = v4_ema(v4_ld_n(s->dPL + k, n), ad, iL);
        pr = v4_ema(v4_ld_n(s->dPR + k, n), ad, iR);
        v4_ld2_n((const float *)(s->dC + k), n, &cr, &ci);
        cr = v4_ema(cr, ad, iCr);
        ci = v4_ema(ci, ad, iCi);
        v4_st_n(s->dPL + k, pl, n); v4_st_n(s->dPR + k, pr, n); v4_st2_n((float *)(s->dC + k), cr, ci, n);
    }
    *bmax = v4_sel(v4_gt(fsum, *bmax), fsum, *bmax);           /* exact max: order-free */
}

/* energy-weighted slow pan per bin with a BOUNDED weight: one loud hop (cross terms of two coherent sources
 * at one frequency) must not move the estimate faster than tau_slow allows. Also leaves the 43 ms P_N / P_S
 * of the quad in the scratch for the output stage. */
static inline void slowpan_quad(struct scene_state *s, const struct frame_scratch *sc, int k, int n, float eps_bin)
{
    const scene_v4 pl = v4_ld_n(s->PL + k, n), pr = v4_ld_n(s->PR + k, n), a_slow = v4_s(s->a_slow);
    scene_v4 cr, ci, pn, Rt, p_cr, e_cr, pbn, pbd, wgt;
    scene_m4 valid;
    v4_ld2_n((const float *)(s->C + k), n, &cr, &ci);
    v4_decompose_pn(pl, pr, cr, ci, &pn, &Rt);
    v4_st(sc->PN + k, pn);                                      /* padded scratch: whole quads */
    v4_st(sc->PS + k, Rt);
    v4_centre_removed_pan(pl, pr, cr, eps_bin, v4_mul(v4_s(1.5f), pn), &p_cr, &e_cr, &valid);
    pbn = v4_ld_n(s->pb_num + k, n);
    pbd = v4_ld_n(s->pb_den + k, n);
    wgt = v4_min(e_cr, v4_add(v4_mul(v4_s(2.0f), pbd), v4_s(eps_bin)));
    pbn = v4_ema(pbn, a_slow, v4_mul(wgt, p_cr));
    pbd = v4_ema(pbd, a_slow, wgt);
    v4_st_n(s->pb_num + k, pbn, n);
    v4_st_n(s->pb_den + k, pbd, n);
    v4_st_n(s->p_slow_bin + k, v4_div(pbn, v4_add(pbd, v4_s(1e-12f))), n);
}

/* permission opens with the dwell/ramp, releases slowly (protection is algebraic, §13.2-2); the place's
 * image, alpha scale and pan are held together with the membership; the effective unmix pan of the bin is
 * its own slow pan where it is a member (fresh), the held place pan otherwise */
static inline void holds_quad(struct scene_state *s, const struct frame_scratch *sc, int k, int n)
{
    const scene_v4 wm = v4_ld(sc->w_member + k), qb = v4_ld(sc->q_b + k), as = v4_ld(sc->a_scale + k);
    const scene_v4 lpc = v4_ld_n(s->clusters.last_pc + k, n), psb = v4_ld_n(s->p_slow_bin + k, n);
    scene_v4 wh = v4_ld_n(s->w_hold + k, n), qh = v4_ld_n(s->q_hold + k, n);
    scene_v4 ah = v4_ld_n(s->a_hold + k, n), pch = v4_ld_n(s->pc_hold + k, n);
    const scene_m4 fresh = v4_ge(wm, wh);
    wh = v4_sel(fresh, wm, v4_ema(wh, v4_s(s->a_rel), wm));
    qh = v4_sel(fresh, qb, qh);
    ah = v4_sel(fresh, as, ah);
    pch = v4_sel(fresh, lpc, pch);
    v4_st_n(s->w_hold + k, wh, n);
    v4_st_n(s->q_hold + k, qh, n);
    v4_st_n(s->a_hold + k, ah, n);
    v4_st_n(s->pc_hold + k, pch, n);
    v4_st(sc->pc_eff + k, v4_sel(fresh, v4_add(v4_mul(wh, psb), v4_mul(v4_sub(v4_s(1.0f), wh), pch)), pch));
}

/* no clustering: the holds carry this frame's per-bin permission, image and alpha scale (1) */
static inline void nocluster_quad(struct scene_state *s, const struct frame_scratch *sc, int k, int n)
{
    const struct scene_config *cfg = &s->cfg;
    const scene_v4 p = v4_ld_n(s->p_slow_bin + k, n), ap = v4_abs(p);
    v4_st_n(s->w_hold + k, v4_smoothstep(ap, cfg->pan_lo, cfg->pan_hi), n);
    v4_st_n(s->q_hold + k, v4_mul(v4_sign(p), v4_add(v4_s(0.78f), v4_mul(v4_s(0.17f), v4_smoothstep(ap, 0.05f, 0.92f)))), n);
    v4_st_n(s->a_hold + k, v4_s(1.0f), n);
    v4_st(sc->pc_eff + k, v4_ld_n(s->pc_hold + k, n));           /* fresh = 0 (unused: the unmix needs the clusters) */
}

/* e ** gamma lane by lane (QNX 6.5 libm: exp/log instead of powf); gamma 1 = no sharpening, the default */
static inline scene_v4 pow_lanes(scene_v4 e, const float *gamma, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        const float g = gamma[i];
        if (g != 1.0f) {
            const float x = v4_lane(e, i);
            e = v4_setlane(e, i, x > 0.0f ? expf(g * logf(x)) : 0.0f);
        }
    }
    return e;
}

/* per-bin output: the direction (and the fast protection weight) must be the bin's own, otherwise a centred
 * drum bin inside a guitar-dominated group is projected onto the guitar's direction and transferred behind.
 * Direction for the residual from the long statistics (tau_dir): ambience exists only relative to an
 * AVERAGED direction (an instantaneous direction makes every bin rank-one). */
static inline void output_quad(struct scene_state *s, const struct frame_scratch *sc, const struct frame_ctx *fc,
                               const scene_cpx *XL, const scene_cpx *XR, const float *wide, int k, int n)
{
    const struct scene_config *cfg = &s->cfg;
    const scene_v4 one = v4_s(1.0f), half = v4_s(0.5f), zero = v4_zero(), cold = v4_s(fc->cold_gain);
    const scene_v4 hp = v4_ld_n(s->hp + k, n);
    scene_v4 Lr, Li, Rr, Ri, dpl, dpr, dcr, dci, pn, Rt, ab, bb, th, pnb, psb, W, sn, cs;
    scene_v4 emr, emi, tr, ti, ur, ui, w_perm, q, alpha_b, w_bin, w_front, D, omD, a_r, a_f, g_r, cL, cR;
    scene_v4 objLr, objLi, objRr, objRi, subLr, subLi, subRr, subRi, instLr, instLi, instRr, instRi;
    scene_v4 ambLr, ambLi, ambRr, ambRi, phLr, phLi, phRr, phRi, air, GA, rearLr, rearLi, rearRr, rearRi, t1, t2, br, bi;

    v4_ld2_n((const float *)(XL + k), n, &Lr, &Li);
    v4_ld2_n((const float *)(XR + k), n, &Rr, &Ri);
    /* long-averaged direction of the bin itself (a, b, theta) */
    dpl = v4_ld_n(s->dPL + k, n);
    dpr = v4_ld_n(s->dPR + k, n);
    v4_ld2_n((const float *)(s->dC + k), n, &dcr, &dci);
    v4_decompose_pn(dpl, dpr, dcr, dci, &pn, &Rt);
    v4_decompose_ab(dpl, dpr, pn, &ab, &bb);
    th = v4_decompose_theta(dcr, dci, Rt, pn);
    /* 43 ms statistics: the Wiener scalar, object term only */
    pnb = v4_ld(sc->PN + k);
    psb = v4_ld(sc->PS + k);
    W = v4_div(psb, v4_add(v4_add(psb, pnb), v4_s(1e-12f)));
    v4_sincos(th, &sn, &cs);                                    /* e_m = exp(-i th) = (cs, -sn); e_p = exp(+i th) = (cs, sn) */
    emr = v4_mul(cs, ab);                                       /* e_m * ab */
    emi = v4_mul(v4_neg(sn), ab);
    v4_cmul(emr, emi, Rr, Ri, &tr, &ti);
    ur = v4_sub(v4_mul(Lr, bb), tr);                            /* orthogonal residual u = bb L - ab e_m R (no primary) */
    ui = v4_sub(v4_mul(Li, bb), ti);

    w_perm = v4_ld_n(s->w_hold + k, n);
    q = v4_ld_n(s->q_hold + k, n);
    alpha_b = v4_mul(v4_s(cfg->alpha), v4_ld_n(s->a_hold + k, n));
    if (fc->unmix) {
        w_bin = w_perm;                                         /* the centre is removed algebraically; no pan gate needed */
    } else {
        /* fast per-bin pan (tau_fast) for the one-hop protection weight only (its only consumer) */
        const scene_v4 fpl = v4_ld_n(s->fPL + k, n), fpr = v4_ld_n(s->fPR + k, n);
        scene_v4 fcr, fci, fpn, fRt, thf, fPa, fPb, p_bin, w_fast;
        v4_ld2_n((const float *)(s->fC + k), n, &fcr, &fci);
        v4_decompose_pn(fpl, fpr, fcr, fci, &fpn, &fRt);
        thf = v4_decompose_theta(fcr, fci, fRt, fpn);
        fPa = v4_max0(v4_sub(fpl, fpn));
        fPb = v4_max0(v4_sub(fpr, fpn));
        p_bin = v4_div(v4_sub(fPb, fPa), v4_add(v4_add(fPa, fPb), v4_s(fc->eps_bin)));
        w_fast = v4_sel(v4_gt(v4_abs(thf), v4_s(s->theta_max)), zero, v4_smoothstep(v4_abs(p_bin), cfg->pan_lo, cfg->pan_hi));
        w_bin = cfg->fast_protect ? v4_min(w_fast, w_perm) : w_perm;
    }
    w_front = cfg->front_follows_slow ? w_perm : w_bin;
    D = zero;
    if (fc->prot_on) {                                          /* duck D = duck_depth * protect (vocal/bass/drum-dominant cells) */
        const scene_v4 p_now = v4_clip01(v4_gather_n(s->prot_row, s->mbin + k, n));
        scene_v4 dp = v4_ld_n(s->d_prot + k, n);
        dp = v4_ema(dp, v4_sel(v4_gt(p_now, dp), v4_s(s->a_datt), v4_s(s->a_drel)), p_now);
        v4_st_n(s->d_prot + k, dp, n);
        D = v4_mul(v4_s(cfg->duck_depth), dp);
    }
    omD = v4_sub(one, D);
    a_r = v4_mul(v4_mul(v4_mul(alpha_b, w_bin), omD), cold);
    a_f = v4_mul(v4_mul(v4_mul(alpha_b, w_front), omD), cold);
    g_r = cfg->energy_conserving ? v4_conserving(a_r) : a_r;   /* rear gain sqrt(1 - (1 - a)^2) (13.2 item 3) */
    /* object: widened constant-power image per group, coherent, undecorrelated */
    cL = v4_sqrt(v4_mul(v4_sub(one, q), half));
    cR = v4_sqrt(v4_mul(v4_add(one, q), half));
    if (fc->unmix) {
        /* Exact two-source unmix of the bin against the centre: [L;R] = [a_c;b_c]*s + [1;1]/sqrt2 * m.
         * A centred source gives s = 0 sample-exactly for ANY place; the rank-one projection could not. */
        scene_v4 pc = v4_ld(sc->pc_eff + k), a_c, b_c, det, inv, sr, si, e_bin, e_s, scl, objr, obji;
        scene_m4 big;
        pc = v4_sel(v4_gt(v4_abs(pc), v4_s(0.9f)), v4_sign(pc), pc);
        a_c = v4_sqrt(v4_clip01(v4_mul(v4_sub(one, pc), half)));
        b_c = v4_sqrt(v4_clip01(v4_mul(v4_add(one, pc), half)));
        det = v4_sub(a_c, b_c);
        big = v4_gt(v4_abs(det), half);                         /* 1/det, or the Tikhonov floor det/(det^2 + lambda) near the centre: one division */
        inv = v4_div(v4_sel(big, one, det), v4_sel(big, det, v4_add(v4_mul(det, det), v4_s(cfg->unmix_lambda))));
        sr = v4_mul(v4_sub(Lr, Rr), inv);
        si = v4_mul(v4_sub(Li, Ri), inv);
        /* bounded: the place's content can never exceed the bin's own energy */
        e_bin = v4_add(v4_add(v4_mul(Lr, Lr), v4_mul(Li, Li)), v4_add(v4_mul(Rr, Rr), v4_mul(Ri, Ri)));
        e_s = v4_add(v4_add(v4_mul(sr, sr), v4_mul(si, si)), v4_s(1e-18f));
        scl = v4_min(one, v4_sqrt(v4_div(e_bin, e_s)));
        sr = v4_mul(sr, scl);
        si = v4_mul(si, scl);
        objr = v4_mul(v4_mul(sr, g_r), hp);                     /* g_r * s_obj * hp */
        obji = v4_mul(v4_mul(si, g_r), hp);
        objLr = v4_mul(objr, cL); objLi = v4_mul(obji, cL);
        objRr = v4_mul(objr, cR); objRi = v4_mul(obji, cR);
        t1 = v4_mul(sr, a_f);                                   /* leaves the front at the place's pan */
        t2 = v4_mul(si, a_f);
        subLr = v4_mul(v4_mul(t1, a_c), hp); subLi = v4_mul(v4_mul(t2, a_c), hp);
        subRr = v4_mul(v4_mul(t1, b_c), hp); subRi = v4_mul(v4_mul(t2, b_c), hp);
    } else {
        scene_v4 yr, yi, gw, aw, objr, obji, t_r, t_i, xr, xi;
        v4_cmul(v4_mul(cs, bb), v4_mul(v4_neg(sn), bb), Rr, Ri, &tr, &ti);   /* (e_m bb) R */
        yr = v4_add(v4_mul(Lr, ab), tr);                        /* projection on the bin's primary direction */
        yi = v4_add(v4_mul(Li, ab), ti);
        gw = v4_mul(g_r, W);
        aw = v4_mul(a_f, W);
        objr = v4_mul(v4_mul(yr, gw), hp);                      /* g_r * W * y * hp */
        obji = v4_mul(v4_mul(yi, gw), hp);
        t_r = v4_mul(yr, aw);                                   /* a_f * W * y */
        t_i = v4_mul(yi, aw);
        objLr = v4_mul(objr, cL); objLi = v4_mul(obji, cL);
        v4_cmul(v4_mul(objr, cR), v4_mul(obji, cR), cs, sn, &objRr, &objRi);
        subLr = v4_mul(v4_mul(t_r, ab), hp);                    /* leaves the front at the primary's own pan */
        subLi = v4_mul(v4_mul(t_i, ab), hp);
        v4_cmul(v4_mul(t_r, bb), v4_mul(t_i, bb), cs, sn, &xr, &xi);
        subRr = v4_mul(xr, hp);
        subRi = v4_mul(xi, hp);
    }
    /* semantic stage: the instrument part of what is still in front (external eligibility mask), at the cell's own pan */
    instLr = instLi = instRr = instRi = zero;
    if (fc->inst_on) {
        scene_v4 e_now = v4_clip01(v4_gather_n(s->elig_row, s->mbin + k, n)), ei, a_e, g_e, oLr, oLi, oRr, oRi;
        if (cfg->inst_thresh > 0.0f)
            e_now = v4_clip01(v4_div(v4_sub(e_now, v4_s(cfg->inst_thresh)), v4_s(1.0f - cfg->inst_thresh)));
        if (fc->gamma_on)
            e_now = pow_lanes(e_now, s->gamma_vec + k, n);
        if (cfg->inst_long_tau_s > 0.0f) {                      /* long-term evidence gate */
            const scene_v4 el = v4_ema(v4_ld_n(s->e_long + k, n), v4_s(s->a_long), e_now);
            v4_st_n(s->e_long + k, el, n);
            e_now = v4_mul(e_now, v4_smoothstep(el, cfg->inst_long_lo, cfg->inst_long_hi));
        }
        ei = v4_ld_n(s->e_inst + k, n);
        ei = v4_ema(ei, v4_sel(v4_gt(e_now, ei), v4_s(s->a_iatt), v4_s(s->a_irel)), e_now);
        v4_st_n(s->e_inst + k, ei, n);
        /* the object is the masked part O = e * front; the conserving gains come from the constant fraction,
         * NOT from alpha*e (sqrt(1-(1-a e)^2) ~ sqrt(2 a e) would amplify every small eligibility) */
        a_e = v4_mul(v4_mul(v4_mul(v4_s(cfg->inst_alpha), omD), cold), hp);
        g_e = cfg->energy_conserving ? v4_conserving(a_e) : a_e;
        oLr = v4_mul(v4_sub(Lr, subLr), ei); oLi = v4_mul(v4_sub(Li, subLi), ei);
        oRr = v4_mul(v4_sub(Rr, subRr), ei); oRi = v4_mul(v4_sub(Ri, subRi), ei);
        instLr = v4_mul(oLr, g_e); instLi = v4_mul(oLi, g_e);
        instRr = v4_mul(oRr, g_e); instRi = v4_mul(oRi, g_e);
        subLr = v4_add(subLr, v4_mul(oLr, a_e)); subLi = v4_add(subLi, v4_mul(oLi, a_e));
        subRr = v4_add(subRr, v4_mul(oRr, a_e)); subRi = v4_add(subRi, v4_mul(oRi, a_e));
    }
    /* ambient: the residual as the room, decorrelated between Ls and Rs */
    GA = v4_ld_n(s->GA + k, n);
    air = v4_ld_n(s->air + k, n);
    v4_ld2_n((const float *)(s->phL + k), n, &phLr, &phLi);
    v4_ld2_n((const float *)(s->phR + k), n, &phRr, &phRi);
    t1 = v4_mul(GA, bb);                                        /* GA * bb * u * (1 - D) */
    ambLr = v4_mul(v4_mul(ur, t1), omD);
    ambLi = v4_mul(v4_mul(ui, t1), omD);
    t2 = v4_mul(GA, v4_neg(ab));                                /* GA * (-ab) * exp(1j th) * u * (1 - D) */
    v4_cmul(v4_mul(cs, t2), v4_mul(sn, t2), ur, ui, &ambRr, &ambRi);
    ambRr = v4_mul(ambRr, omD);
    ambRi = v4_mul(ambRi, omD);
    {
        const scene_m4 ss = m4_flags_n(s->same_sign + k, n);      /* same sign below 550 Hz */
        ambRr = v4_sel(ss, v4_neg(ambRr), ambRr);
        ambRi = v4_sel(ss, v4_neg(ambRi), ambRi);
    }
    /* rear = obj + inst + ((amb + air L) ph) surround cold */
    v4_cmul(ambLr, ambLi, phLr, phLi, &t1, &t2);
    v4_cmul(v4_mul(Lr, air), v4_mul(Li, air), phLr, phLi, &br, &bi);
    rearLr = v4_add(v4_add(objLr, instLr), v4_mul(v4_mul(v4_add(t1, br), v4_s(cfg->surround)), cold));
    rearLi = v4_add(v4_add(objLi, instLi), v4_mul(v4_mul(v4_add(t2, bi), v4_s(cfg->surround)), cold));
    v4_cmul(ambRr, ambRi, phRr, phRi, &t1, &t2);
    v4_cmul(v4_mul(Rr, air), v4_mul(Ri, air), phRr, phRi, &br, &bi);
    rearRr = v4_add(v4_add(objRr, instRr), v4_mul(v4_mul(v4_add(t1, br), v4_s(cfg->surround)), cold));
    rearRi = v4_add(v4_add(objRi, instRi), v4_mul(v4_mul(v4_add(t2, bi), v4_s(cfg->surround)), cold));
    if (cfg->wide_pair) {
        const scene_v4 w_prot = fc->prot_on ? v4_sub(one, v4_ld_n(s->d_prot + k, n)) : omD;
        const scene_v4 wg = v4_mul(v4_mul(v4_mul(v4_mul(v4_s(cfg->wide_pair_alpha), v4_gather_n(wide, s->group + k, n)), hp), cold), w_prot);
        const scene_v4 gr = v4_conserving(wg);
        const scene_v4 remLr = v4_sub(Lr, subLr), remLi = v4_sub(Li, subLi);
        const scene_v4 remRr = v4_sub(Rr, subRr), remRi = v4_sub(Ri, subRi);
        rearLr = v4_add(rearLr, v4_mul(remLr, gr)); rearLi = v4_add(rearLi, v4_mul(remLi, gr));
        rearRr = v4_add(rearRr, v4_mul(remRr, gr)); rearRi = v4_add(rearRi, v4_mul(remRi, gr));
        subLr = v4_add(subLr, v4_mul(remLr, wg)); subLi = v4_add(subLi, v4_mul(remLi, wg));
        subRr = v4_add(subRr, v4_mul(remRr, wg)); subRi = v4_add(subRi, v4_mul(remRi, wg));
    }
    if (cfg->bleed_cancel > 0.0f && s->blen == 0) {
        const scene_v4 bc = v4_mul(v4_s(cfg->bleed_cancel), hp);
        rearLr = v4_sub(rearLr, v4_mul(v4_sub(Lr, subLr), bc));
        rearLi = v4_sub(rearLi, v4_mul(v4_sub(Li, subLi), bc));
        rearRr = v4_sub(rearRr, v4_mul(v4_sub(Rr, subRr), bc));
        rearRi = v4_sub(rearRi, v4_mul(v4_sub(Ri, subRi), bc));
    }
    v4_st2_n((float *)(s->subL + k), subLr, subLi, n);
    v4_st2_n((float *)(s->subR + k), subRr, subRi, n);
    v4_st2_n((float *)(s->rearL + k), rearLr, rearLi, n);
    v4_st2_n((float *)(s->rearR + k), rearRr, rearRi, n);
}

/* Front anchor: a sparse mix of only off-centre and wide parts (no vocal, bass or drums the mask
 * protects) would otherwise move most of the song to the rear and leave the front thin.  The front
 * is X - a S, S = this frame's subtraction at full transfer (sub / cold_gain), a = the anchor gain.
 * Its 1 s energy Ex - 2a Rxs + a^2 Es must stay >= keep Ex (keep = 10^(-front_keep_db/10)): the
 * largest such a is the smaller root of the quadratic.  No feedback through the measurement, so it
 * cannot hunt; the gain follows that bound down in 0.3 s and comes back in 2 s.  It scales every
 * transfer and the ambient layer together (cold_gain), so the scene keeps its shape, only quieter. */
static void front_anchor(struct scene_state *s, const scene_cpx *XL, const scene_cpx *XR, float cold_gain)
{
    const float keep = expf(-s->cfg.front_keep_db * (2.302585092994046f / 10.0f));   /* 10 ** (-db / 10) */
    const float inv = 1.0f / (cold_gain > 1e-3f ? cold_gain : 1e-3f);
    float ex = 0.0f, rxs = 0.0f, es = 0.0f, a_max, disc, target;
    int k;

    for (k = 0; k < s->F; k++) {
        float slr, sli, srr, sri;
        if (s->freqs[k] < s->cfg.hp_hz || s->freqs[k] > 6000.0f) continue;
        slr = s->subL[k].re * inv; sli = s->subL[k].im * inv;
        srr = s->subR[k].re * inv; sri = s->subR[k].im * inv;
        ex  += XL[k].re * XL[k].re + XL[k].im * XL[k].im + XR[k].re * XR[k].re + XR[k].im * XR[k].im;
        rxs += XL[k].re * slr + XL[k].im * sli + XR[k].re * srr + XR[k].im * sri;
        es  += slr * slr + sli * sli + srr * srr + sri * sri;
    }
    s->anc_ex  += s->a_anc_meas * (ex - s->anc_ex);
    s->anc_rxs += s->a_anc_meas * (rxs - s->anc_rxs);
    s->anc_es  += s->a_anc_meas * (es - s->anc_es);
    /* a^2 Es - 2a Rxs + (1 - keep) Ex <= 0 */
    disc = s->anc_rxs * s->anc_rxs - s->anc_es * (1.0f - keep) * s->anc_ex;
    if (s->anc_es <= 1e-12f || disc < 0.0f) a_max = 1.0f;     /* never takes that much at any a */
    else a_max = (s->anc_rxs - sqrtf(disc)) / s->anc_es;
    target = a_max < 0.0f ? 0.0f : (a_max > 1.0f ? 1.0f : a_max);
    s->anchor += (target < s->anchor ? s->a_anc_att : s->a_anc_rel) * (target - s->anchor);
}

void scene_engine_frame(struct scene_state *s, const scene_cpx *XL, const scene_cpx *XR, int t)
{
    const struct scene_config *cfg = &s->cfg;
    const int F = s->F, G = s->G;
    const int use_clusters = cfg->cluster != 0;
    struct frame_ctx fc;
    struct frame_scratch sc;
    float gPL[SCENE_GROUPS_MAX], gPR[SCENE_GROUPS_MAX], gdPL[SCENE_GROUPS_MAX], gdPR[SCENE_GROUPS_MAX];
    float gPN[SCENE_GROUPS_MAX], gPS[SCENE_GROUPS_MAX], p[SCENE_GROUPS_MAX], e_g[SCENE_GROUPS_MAX], wide[SCENE_GROUPS_MAX];
    scene_cpx gC[SCENE_GROUPS_MAX], gdC[SCENE_GROUPS_MAX];
    scene_v4 bmax;
    float blockmax, eps_abs, m;
    int k, g;

    fc.unmix = cfg->unmix && use_clusters;
    fc.inst_on = s->have_mask;
    fc.prot_on = s->have_mask && cfg->duck_from_mask;
    fc.gamma_on = cfg->inst_gamma != 1.0f || cfg->inst_gamma_lo > 0.0f;   /* some gamma_vec[k] != 1 */
    fc.cold_gain = t + 1 >= SCENE_RAMP_HOPS ? 1.0f : (float)(t + 1) / (float)SCENE_RAMP_HOPS;   /* min(1, (t+1)/ramp_hops) */
    fc.cold_gain *= s->anchor;                                  /* 1 unless the front anchor holds the transfers back */
    sc.PN = (float *)(((uintptr_t)s->fft_scratch + 15u) & ~(uintptr_t)15u);
    sc.PS = sc.PN + SCENE_BINS4;
    sc.w_member = sc.PS + SCENE_BINS4;
    sc.q_b = sc.w_member + SCENE_BINS4;
    sc.a_scale = sc.q_b + SCENE_BINS4;
    sc.pc_eff = sc.a_scale + SCENE_BINS4;

    s->t = t;
    if (s->pending_reset) {                                     /* track change (Now Playing): song-level state only */
        if (use_clusters) scene_clusters_reset(&s->clusters);
        memset(s->e_long, 0, sizeof s->e_long);
        s->pending_reset = 0;
    }

    /* statistics; blockmax = max(max(fPL + fPR), 1e-12) */
    bmax = v4_s(1e-12f);
    for (k = 0; k + 4 <= F; k += 4) stats_quad(s, XL, XR, k, 4, t == 0, &bmax);
    if (k < F) stats_quad(s, XL, XR, k, F - k, t == 0, &bmax);
    blockmax = v4_hmax(bmax);
    fc.eps_bin = 1e-4f * blockmax;

    for (k = 0; k + 4 <= F; k += 4) slowpan_quad(s, &sc, k, 4, fc.eps_bin);
    if (k < F) slowpan_quad(s, &sc, k, F - k, fc.eps_bin);

    /* pooled statistics per group (bincount order = bin order): the slow pan, the histogram, the membership.
     * A group is a contiguous run of bins (scene_erb_groups), so the eight sums run bin by bin per group
     * with the accumulators in registers: the same additions in the same order as the scatter form. */
    for (g = 0, k = 0; g < G; ) {
        if (g + 4 <= G && s->group_count[g] == 4 && s->group_count[g + 1] == 4 &&
            s->group_count[g + 2] == 4 && s->group_count[g + 3] == 4) {
            /* four groups of four bins (every group above 2 kHz): one 4x4 transpose per array puts the
             * j-th bin of each group in row j, then ((row0 + row1) + row2) + row3 is the scalar order */
            scene_v4 a0, a1, a2, a3, b0, b1, b2, b3;
            v4_ld2((const float *)(s->C + k), &a0, &b0); v4_ld2((const float *)(s->C + k + 4), &a1, &b1);
            v4_ld2((const float *)(s->C + k + 8), &a2, &b2); v4_ld2((const float *)(s->C + k + 12), &a3, &b3);
            v4_transpose(&a0, &a1, &a2, &a3); v4_transpose(&b0, &b1, &b2, &b3);
            v4_st2((float *)(gC + g), v4_add(v4_add(v4_add(a0, a1), a2), a3), v4_add(v4_add(v4_add(b0, b1), b2), b3));
            v4_ld2((const float *)(s->dC + k), &a0, &b0); v4_ld2((const float *)(s->dC + k + 4), &a1, &b1);
            v4_ld2((const float *)(s->dC + k + 8), &a2, &b2); v4_ld2((const float *)(s->dC + k + 12), &a3, &b3);
            v4_transpose(&a0, &a1, &a2, &a3); v4_transpose(&b0, &b1, &b2, &b3);
            v4_st2((float *)(gdC + g), v4_add(v4_add(v4_add(a0, a1), a2), a3), v4_add(v4_add(v4_add(b0, b1), b2), b3));
            a0 = v4_ld(s->PL + k); a1 = v4_ld(s->PL + k + 4); a2 = v4_ld(s->PL + k + 8); a3 = v4_ld(s->PL + k + 12);
            b0 = v4_ld(s->PR + k); b1 = v4_ld(s->PR + k + 4); b2 = v4_ld(s->PR + k + 8); b3 = v4_ld(s->PR + k + 12);
            v4_transpose(&a0, &a1, &a2, &a3); v4_transpose(&b0, &b1, &b2, &b3);
            v4_st(gPL + g, v4_add(v4_add(v4_add(a0, a1), a2), a3)); v4_st(gPR + g, v4_add(v4_add(v4_add(b0, b1), b2), b3));
            a0 = v4_ld(s->dPL + k); a1 = v4_ld(s->dPL + k + 4); a2 = v4_ld(s->dPL + k + 8); a3 = v4_ld(s->dPL + k + 12);
            b0 = v4_ld(s->dPR + k); b1 = v4_ld(s->dPR + k + 4); b2 = v4_ld(s->dPR + k + 8); b3 = v4_ld(s->dPR + k + 12);
            v4_transpose(&a0, &a1, &a2, &a3); v4_transpose(&b0, &b1, &b2, &b3);
            v4_st(gdPL + g, v4_add(v4_add(v4_add(a0, a1), a2), a3)); v4_st(gdPR + g, v4_add(v4_add(v4_add(b0, b1), b2), b3));
            g += 4; k += 16;
        } else {                                                /* the narrow low groups (2 and 3 bins) */
            const int end = k + s->group_count[g];
            float pl = 0.0f, pr = 0.0f, cre = 0.0f, cim = 0.0f, dpl = 0.0f, dpr = 0.0f, dcr = 0.0f, dci = 0.0f;
            for (; k < end; k++) {
                pl += s->PL[k]; pr += s->PR[k]; cre += s->C[k].re; cim += s->C[k].im;
                dpl += s->dPL[k]; dpr += s->dPR[k]; dcr += s->dC[k].re; dci += s->dC[k].im;
            }
            gPL[g] = pl; gPR[g] = pr; gC[g].re = cre; gC[g].im = cim;
            gdPL[g] = dpl; gdPR[g] = dpr; gdC[g].re = dcr; gdC[g].im = dci;
            g++;
        }
    }
    scene_decompose_frame(gPL, gPR, gC, G, gPN, gPS, NULL, NULL, NULL, NULL, NULL);
    m = 1e-12f;
    for (g = 0; g < G; g++) if (gPL[g] + gPR[g] > m) m = gPL[g] + gPR[g];
    eps_abs = 1e-4f * m;
    scene_centre_removed_pan(gPL, gPR, gC, G, eps_abs, gPN, p, e_g, NULL);
    for (g = 0; g < G; g++) {
        float wg = e_g[g] < 2.0f * s->ps_den[g] + eps_abs ? e_g[g] : 2.0f * s->ps_den[g] + eps_abs;
        s->ps_num[g] += s->a_slow * (wg * p[g] - s->ps_num[g]);
        s->ps_den[g] += s->a_slow * (wg - s->ps_den[g]);
        s->p_slow[g] = s->ps_num[g] / (s->ps_den[g] + 1e-12f);
    }
    /* (the Python's per-group w_fast and the per-bin fPN are computed and never used: not ported) */

    if (use_clusters) {
        /* eligibility: a valid centre-removed place pan on the long statistics (double-tracks and
         * ambience are invalid: their L/R difference is not a panned source), no anti-phase */
        float dPN[SCENE_GROUPS_MAX], dp[SCENE_GROUPS_MAX], de[SCENE_GROUPS_MAX];
        int dv[SCENE_GROUPS_MAX], ok[SCENE_GROUPS_MAX];
        scene_decompose_frame(gdPL, gdPR, gdC, G, dPN, NULL, NULL, NULL, NULL, NULL, NULL);
        scene_centre_removed_pan(gdPL, gdPR, gdC, G, eps_abs, dPN, dp, de, dv);
        for (g = 0; g < G; g++)
            ok[g] = dv[g] && de[g] > 1e-3f * blockmax && angle_within(gdC[g].re, gdC[g].im, s->cos_theta_max)
                    && g > 0 && s->group_freq[g] >= cfg->hp_hz && s->group_freq[g] <= 6000.0f;
        scene_clusters_update(&s->clusters, p, e_g, ok, G);
        /* membership per BIN from the bin's own slow pan: a bin shared by a panned source and a
         * centred one sits between the place and the centre and gets ~no membership (stays front) */
        scene_clusters_membership(&s->clusters, s->p_slow_bin, s->pb_den, F, sc.w_member, sc.q_b, sc.a_scale);
        for (k = 0; k + 4 <= F; k += 4) holds_quad(s, &sc, k, 4);
        if (k < F) holds_quad(s, &sc, k, F - k);
    } else {
        for (k = 0; k + 4 <= F; k += 4) nocluster_quad(s, &sc, k, 4);
        if (k < F) nocluster_quad(s, &sc, k, F - k);
    }

    /* wide uncorrelated pairs (13.2 item 10): envelope-correlated, incoherent, near-centre */
    if (cfg->wide_pair) {
        float eL[SCENE_GROUPS_MAX], eR[SCENE_GROUPS_MAX];
        for (g = 0; g < G; g++) eL[g] = eR[g] = 0.0f;
        for (k = 0; k < F; k++) {
            eL[s->group[k]] += XL[k].re * XL[k].re + XL[k].im * XL[k].im;
            eR[s->group[k]] += XR[k].re * XR[k].re + XR[k].im * XR[k].im;
        }
        for (g = 0; g < G; g++) {
            float vL, vR, corr, coh;
            s->envL[g] += s->a_env * (eL[g] - s->envL[g]); s->envR[g] += s->a_env * (eR[g] - s->envR[g]);
            s->envLR[g] += s->a_env * (eL[g] * eR[g] - s->envLR[g]);
            s->envLL[g] += s->a_env * (eL[g] * eL[g] - s->envLL[g]);
            s->envRR[g] += s->a_env * (eR[g] * eR[g] - s->envRR[g]);
            vL = s->envLL[g] - s->envL[g] * s->envL[g]; if (vL < 0.0f) vL = 0.0f;
            vR = s->envRR[g] - s->envR[g] * s->envR[g]; if (vR < 0.0f) vR = 0.0f;
            corr = (s->envLR[g] - s->envL[g] * s->envR[g]) / (sqrtf(vL * vR) + 1e-12f);
            coh = gPS[g] / (gPS[g] + gPN[g] + 1e-12f);
            wide[g] = scene_smoothstep(corr, 0.5f, 0.8f) * (1.0f - scene_smoothstep(coh, 0.35f, 0.6f))
                      * (1.0f - scene_smoothstep(fabsf(s->p_slow[g]), 0.2f, 0.4f));
        }
    }

    for (k = 0; k + 4 <= F; k += 4) output_quad(s, &sc, &fc, XL, XR, wide, k, 4);
    if (k < F) output_quad(s, &sc, &fc, XL, XR, wide, k, F - k);
    if (cfg->front_keep_db > 0.0f) front_anchor(s, XL, XR, fc.cold_gain);
}

/* ----------------------------------------------------------------------------- block driver (SceneStream) */
/* one sample through a ring delay line: out = line[pos]; line[pos] = in; pos = (pos + 1) % len */
static void delay2(float line[][2], int *pos, int len, float *l, float *r)
{
    float ol = line[*pos][0], orr = line[*pos][1];
    line[*pos][0] = *l; line[*pos][1] = *r;
    *pos = *pos + 1 == len ? 0 : *pos + 1;
    *l = ol; *r = orr;
}

/* frame[0..N) of the ring from pad index base, windowed: out_l/out_r (split L/R for the FFT pair); the
 * ring wraps at most once, so two contiguous runs (NEON on the runs, the same multiply per sample as scalar) */
static void window_frame(const struct scene_state *s, int base, float *out_l, float *out_r)
{
    const int N = SCENE_NFFT, M = SCENE_RING - 1;
    int n = 0;
    while (n < N) {
        const int idx = (base + n) & M;
        int run = SCENE_RING - idx;
        if (run > N - n) run = N - n;
        {
            const float *in = &s->inbuf[idx][0], *w = s->window + n;
            float *ol = out_l + n, *orr = out_r + n;
            int i = 0;
#if SCENE_ENGINE_NEON
            for (; i + 4 <= run; i += 4) {
                const float32x4x2_t lr = vld2q_f32(in + 2 * i);
                const float32x4_t wv = vld1q_f32(w + i);
                vst1q_f32(ol + i, vmulq_f32(lr.val[0], wv));
                vst1q_f32(orr + i, vmulq_f32(lr.val[1], wv));
            }
#endif
            for (; i < run; i++) {
                ol[i] = in[2 * i] * w[i];
                orr[i] = in[2 * i + 1] * w[i];
            }
        }
        n += run;
    }
}

/* overlap-add of the four synthesis signals (subL, subR, rearL, rearR, windowed) into the ring from pad index base */
static void ola_frame(struct scene_state *s, int base, const float *a0, const float *b0, const float *a1, const float *b1)
{
    const int N = SCENE_NFFT, M = SCENE_RING - 1;
    int n = 0;
    while (n < N) {
        const int idx = (base + n) & M;
        int run = SCENE_RING - idx;
        if (run > N - n) run = N - n;
        {
            float *o = &s->ola[idx][0];
            const float *w = s->window + n;
            int i = 0;
#if SCENE_ENGINE_NEON
            for (; i + 4 <= run; i += 4) {
                const float32x4_t wv = vld1q_f32(w + i);
                float32x4x4_t acc = vld4q_f32(o + 4 * i);          /* channel c of four samples in val[c] */
                acc.val[0] = vaddq_f32(acc.val[0], vmulq_f32(vld1q_f32(a0 + n + i), wv));
                acc.val[1] = vaddq_f32(acc.val[1], vmulq_f32(vld1q_f32(b0 + n + i), wv));
                acc.val[2] = vaddq_f32(acc.val[2], vmulq_f32(vld1q_f32(a1 + n + i), wv));
                acc.val[3] = vaddq_f32(acc.val[3], vmulq_f32(vld1q_f32(b1 + n + i), wv));
                vst4q_f32(o + 4 * i, acc);
            }
#endif
            for (; i < run; i++) {
                o[4 * i + 0] += a0[n + i] * w[i];
                o[4 * i + 1] += b0[n + i] * w[i];
                o[4 * i + 2] += a1[n + i] * w[i];
                o[4 * i + 3] += b1[n + i] * w[i];
            }
        }
        n += run;
    }
}

/* The hop's H final samples from pad index base, four per step on each contiguous run of the ring (it
 * wraps at most once): front = in - ola / 2, rear = ola / 2, six channels out, the ring entries zeroed
 * behind. Only for the plain path (no delay line, no bleed cancel: variant J) with room for the whole hop;
 * push_chunk's scalar loop covers the rest. The division by two is exact, so the lane multiply by 0.5 and
 * the scalar tail's / 2.0f are the same bits. */
static void emit_hop(struct scene_state *s, int base, float *out)
{
    const int H = SCENE_HOP, M = SCENE_RING - 1;
    const scene_v4 half = v4_s(0.5f), zero = v4_zero();
    int n = 0;
    while (n < H) {
        const int idx = (base + n) & M;
        int run = SCENE_RING - idx, i = 0;
        if (run > H - n) run = H - n;
        for (; i + 4 <= run; i += 4) {
            float *o = out + 6 * (n + i), *ol = &s->ola[idx + i][0];
            scene_v4 inl, inr, o0, o1, o2, o3;
            v4_ld2(&s->inbuf[idx + i][0], &inl, &inr);
            v4_ld4(ol, &o0, &o1, &o2, &o3);
            v4_st_stride(o, v4_sub(inl, v4_mul(o0, half)), 6);
            v4_st_stride(o + 1, v4_sub(inr, v4_mul(o1, half)), 6);
            v4_st_stride(o + 2, zero, 6);
            v4_st_stride(o + 3, zero, 6);
            v4_st_stride(o + 4, v4_mul(o2, half), 6);
            v4_st_stride(o + 5, v4_mul(o3, half), 6);
            v4_st(ol, zero); v4_st(ol + 4, zero); v4_st(ol + 8, zero); v4_st(ol + 12, zero);
        }
        for (; i < run; i++) {                                      /* the run's odd samples (the ring index is any) */
            float *o = out + 6 * (n + i), *ol = &s->ola[idx + i][0];
            o[0] = s->inbuf[idx + i][0] - ol[0] / 2.0f;
            o[1] = s->inbuf[idx + i][1] - ol[1] / 2.0f;
            o[2] = 0.0f; o[3] = 0.0f;
            o[4] = ol[2] / 2.0f; o[5] = ol[3] / 2.0f;
            ol[0] = ol[1] = ol[2] = ol[3] = 0.0f;
        }
        n += run;
    }
}

static size_t push_chunk(struct scene_state *s, const float *in, int frames, float *out, size_t out_capacity)
{
    const int N = SCENE_NFFT, H = SCENE_HOP, M = SCENE_RING - 1;      /* ring size is a power of two */
    const struct scene_config *cfg = &s->cfg;
    float *tin = s->fft_scratch + 2 * N, *tsub = s->fft_scratch + 4 * N, *trear = s->fft_scratch + 6 * N;   /* time-domain pairs: in [2N,4N), outs [4N,6N) [6N,8N) */
    size_t n_out = 0;
    int i, n;

    for (i = 0; i < frames; i++) {                                  /* pad-index space, modulo the ring */
        s->inbuf[s->in_head][0] = in[2 * i];
        s->inbuf[s->in_head][1] = in[2 * i + 1];
        s->in_head = (s->in_head + 1) & M;
        s->in_count++;
    }
    /* frame t needs pad[tH, tH + N): the oldest unconsumed sample is its first one */
    while (s->in_count >= N) {
        const int t = s->next_frame;
        const int base = (s->in_head - s->in_count) & M;
        /* analysis: both channels of the windowed frame through one complex FFT */
        window_frame(s, base, tin, tin + N);
        scene_rfft1024_pair(tin, tin + N, s->XL, s->XR, s->fft_scratch);
        scene_engine_frame(s, s->XL, s->XR, t);
        /* synthesis: (subL, subR) and (rearL, rearR) each through one inverse, irfft * window, overlap-add */
        scene_irfft1024_pair(s->subL, s->subR, tsub, tsub + N, s->fft_scratch);
        scene_irfft1024_pair(s->rearL, s->rearR, trear, trear + N, s->fft_scratch);
        ola_frame(s, base, tsub, tsub + N, trear, trear + N);
        /* pad indices [tH, (t+1)H) are final now (the last covering frame is t); the window-power sum
         * of the sine window at hop N/4 is the constant 2. The first N/H hops are the pad: not emitted. */
        if (t >= N / H && !s->delay_rear && !s->delay_front && !(cfg->bleed_cancel > 0.0f && s->blen > 0) &&
            n_out + (size_t)H <= out_capacity) {
            emit_hop(s, base, out + 6 * n_out);
            n_out += (size_t)H;
        } else
        for (n = 0; n < H; n++) {
            const int idx = (base + n) & M;
            if (t >= N / H) {
                float fl = s->inbuf[idx][0] - s->ola[idx][0] / 2.0f;
                float fr = s->inbuf[idx][1] - s->ola[idx][1] / 2.0f;
                float rl = s->ola[idx][2] / 2.0f, rr = s->ola[idx][3] / 2.0f;
                if (s->delay_rear)       delay2(s->dline, &s->dpos, s->dlen, &rl, &rr);
                else if (s->delay_front) delay2(s->dline, &s->dpos, s->dlen, &fl, &fr);
                if (cfg->bleed_cancel > 0.0f && s->blen > 0) {      /* delayed front->rear bleed cancel (0-offset is applied in output_quad above hp_hz) */
                    float bl = fl, br = fr;
                    delay2(s->bline, &s->bpos, s->blen, &bl, &br);
                    rl = rl - cfg->bleed_cancel * bl; rr = rr - cfg->bleed_cancel * br;
                }
                if (n_out < out_capacity) {                         /* ponytail: a full output buffer drops the sample; out_capacity >= frames + SCENE_HOP never does */
                    float *o = out + 6 * n_out;
                    o[0] = fl; o[1] = fr; o[2] = 0.0f; o[3] = 0.0f; o[4] = rl; o[5] = rr;
                    n_out++;
                }
            }
            s->ola[idx][0] = s->ola[idx][1] = s->ola[idx][2] = s->ola[idx][3] = 0.0f;
        }
        s->in_count -= H;
        s->next_frame = t + 1;
    }
    return n_out;
}

size_t scene_engine_push(struct scene_state *s, const float *in, size_t frames, float *out, size_t out_capacity)
{
    size_t n_out = 0;
    while (frames > 0) {                                            /* chunks of SCENE_MAX_BLOCK keep the ring from lapping itself */
        int n = frames > SCENE_MAX_BLOCK ? SCENE_MAX_BLOCK : (int)frames;
        n_out += push_chunk(s, in, n, out + 6 * n_out, out_capacity - n_out);
        in += 2 * n;
        frames -= (size_t)n;
    }
    return n_out;
}

size_t scene_engine_flush(struct scene_state *s, float *out, size_t out_capacity)
{
    static const float zeros[2 * SCENE_HOP];
    size_t n_out = 0;
    int i;
    for (i = 0; i < SCENE_NFFT / SCENE_HOP; i++)
        n_out += push_chunk(s, zeros, SCENE_HOP, out + 6 * n_out, out_capacity - n_out);
    return n_out;
}

/* ----------------------------------------------------------------------------- test hook
 * scene_phase_table() is a different random stream than numpy's PCG64 (any static uniform phase does
 * the job), so the golden stream comparison loads the Python's raw phase_table() tables (functions.npz
 * phL/phR) and re-applies the engine's same-sign override. Test only; not in scene_engine.h. */
void scene_engine_test_set_phase(struct scene_state *s, const scene_cpx *phL, const scene_cpx *phR)
{
    int k;
    for (k = 0; k < SCENE_BINS; k++) {
        s->phL[k] = phL[k]; s->phR[k] = phR[k];
        if (s->same_sign[k]) { s->phL[k].re = 1.0f; s->phL[k].im = 0.0f; s->phR[k] = s->phL[k]; }
    }
}
