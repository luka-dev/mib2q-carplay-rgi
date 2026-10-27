/* scene_engine_math.c — pure helpers, tables and the 1024-point FFT of the scene renderer: the C port of
 * smoothstep(), ema_alpha(), decompose_frame(), centre_removed_pan(), erb_groups(), phase_table(), the
 * rfft/irfft inside stft()/istft() (on the shared kernel of scene_fft.h) and SceneConfig's defaults of
 * tools/stereo-upmix-render/scene_upmix.py.
 *
 * C99 (gnu99), float32 only, no FMA (host: -ffp-contract=off), no allocation, no other hook file.
 * Where the Python runs in float64 the ORDER of operations is kept; numpy broadcasting became loops.
 * Golden tests: tools/stereo-upmix-render/port/test_math.py (functions.npz).
 */
#include "scene_engine.h"   /* before <math.h>: the QNX 6.5 math.h sets GCC's _SIZE_T guard without the typedef */
#include "scene_fft.h"
#include "scene_simd.h"
#include <math.h>

#define SCENE_PI 3.14159265358979323846f

/* ----------------------------------------------------------------------------- config */
/* defaults = SceneConfig() of scene_upmix.py, variant J (rear_delay_ms 0, inst_gamma 1, inst_alpha 0.9,
 * duck_depth 0.25); front_follows "slow" -> front_follows_slow 1 */
void scene_config_defaults(struct scene_config *cfg)
{
    cfg->n_fft                  = 1024;
    cfg->hop                    = 256;
    cfg->tau_stats_ms           = 43.0f;     /* statistics EMA for the Wiener scalar and the pooled slow pan */
    cfg->tau_fast_ms            = 8.0f;      /* per-bin fast pan for the one-hop protection weight */
    cfg->tau_dir_ms             = 300.0f;    /* per-bin DIRECTION of the residual */
    cfg->erb_pool               = 1;
    cfg->pan_lo                 = 0.10f;     /* object map smoothstep(|p|, lo, hi) in energy pan */
    cfg->pan_hi                 = 0.35f;
    cfg->tau_slow_ms            = 400.0f;    /* slow pan for placement / permission */
    cfg->release_ms             = 2000.0f;   /* membership release */
    cfg->fast_protect           = 1;
    cfg->cluster                = 1;
    cfg->hist_tau_s             = 3.0f;
    cfg->pick_every_s           = 0.34f;
    cfg->admit_s                = 1.5f;
    cfg->admit_mass             = 0.08f;
    cfg->drop_mass              = 0.02f;
    cfg->drop_s                 = 30.0f;
    cfg->min_abs_pan            = 0.18f;
    cfg->peak_sep               = 0.25f;
    cfg->k_max                  = 3;
    cfg->centre_tau_s           = 2.0f;
    cfg->centre_slew_per_s      = 0.05f;
    cfg->ramp_s                 = 0.6f;
    cfg->sigma_min              = 0.08f;
    cfg->member_mass            = 1;
    cfg->sigma_max              = 0.25f;
    cfg->front_floor            = 0.20f;
    cfg->theta_max_deg          = 120.0f;    /* anti-phase cells never become objects */
    cfg->unmix                  = 1;
    cfg->unmix_lambda           = 0.05f;
    cfg->alpha                  = 0.7f;      /* transfer fraction of the coherent off-centre primary */
    cfg->energy_conserving      = 1;
    cfg->front_follows_slow     = 1;
    cfg->ambient_gain           = 1.0f;
    cfg->ambient_gain_lowmid    = 0.7f;      /* G_A in 250-680 Hz */
    cfg->surround               = 0.7f;
    cfg->air_db                 = -9.0f;     /* static air copy above 2.5 kHz */
    cfg->hp_hz                  = 190.0f;    /* nothing below; ramp to hp_hz + 60 */
    cfg->same_sign_below_hz     = 550.0f;
    cfg->side_lp_hz             = 7000.0f;
    cfg->decorrelate_ambient    = 1;
    cfg->rear_delay_ms          = 0.0f;      /* variant J: the J525 rear doors trail the front ~5.2 ms by themselves */
    cfg->bleed_cancel           = 0.0f;
    cfg->bleed_cancel_offset_ms = 0.0f;
    cfg->front_keep_db          = 0.0f;      /* front anchor off; > 0 = the most the transfers may take from the front, dB */
    cfg->wide_pair              = 0;
    cfg->wide_pair_alpha        = 0.5f;
    cfg->wide_env_tau_ms        = 300.0f;
    cfg->duck_depth             = 0.25f;
    cfg->inst_alpha             = 0.9f;
    cfg->inst_attack_ms         = 300.0f;    /* eligibility rises slowly (an instrument earns the rear) */
    cfg->inst_release_ms        = 30.0f;     /* and drops at once (a vocal entering the cell wins immediately) */
    cfg->inst_mask_nfft         = 512;
    cfg->inst_mask_hop          = 512;
    cfg->inst_gamma             = 1.0f;
    cfg->inst_thresh            = 0.0f;
    cfg->inst_gamma_lo          = 0.0f;
    cfg->inst_gamma_split_hz    = 700.0f;
    cfg->memory_boost           = 0.75f;
    cfg->memory_s               = 120.0f;
    cfg->inst_long_tau_s        = 0.0f;
    cfg->inst_long_lo           = 0.15f;
    cfg->inst_long_hi           = 0.40f;
    cfg->duck_from_mask         = 1;
    cfg->duck_attack_ms         = 30.0f;     /* protection rises at once */
    cfg->duck_release_ms        = 300.0f;    /* and lets go slowly */
    cfg->seed                   = 7;
}

/* ----------------------------------------------------------------------------- helpers */
/* atan2 within ~2e-7 rad and sin/cos of x in [-pi, pi] within ~1.5e-7: lane 0 of the four-lane kernels of
 * scene_simd.h (v4_atan2 / v4_sincos), which the engine runs four bins at a time; one implementation, so the
 * scalar entry points of the fixtures and the quads can never disagree. */
float scene_fast_atan2f(float y, float x)
{
    return v4_lane0(v4_atan2(v4_s(y), v4_s(x)));
}

void scene_fast_sincosf(float x, float *sn, float *cs)
{
    scene_v4 s_, c_;
    v4_sincos(v4_s(x), &s_, &c_);
    *sn = v4_lane0(s_);
    *cs = v4_lane0(c_);
}

float scene_smoothstep(float x, float lo, float hi)
{
    float d = hi - lo;
    if (d < 1e-9f) d = 1e-9f;                 /* max(hi - lo, 1e-9) */
    float t = (x - lo) / d;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

float scene_ema_alpha(float tau_ms, int hop)
{
    if (tau_ms <= 0.0f) return 1.0f;
    /* 1 - exp(-hop / sr / (tau_ms / 1000)); -expm1f keeps the 2 s constants exact in float32
     * (1.0f - expf() loses 3 digits there); expm1f is in the QNX 6.5 libm */
    return -expm1f(-(float)hop / (float)SCENE_SR / (tau_ms / 1000.0f));
}

/* Closed-form eigen decomposition (section 3). p = (P_b - P_a) / (P_a + P_b + eps).
 * Any output pointer may be NULL (the Python callers discard most of the seven). Four cells per step on
 * the scene_simd.h kernels (v4_decompose_pn / _ab / _theta); a quad's inputs are read before any of its
 * outputs are written, so an output may alias an input. P_N = (T - Rt) / 2 in the Python; a coherent
 * hard-panned bin has T == Rt to 7 digits, so in float32 that difference is noise (b off by up to 40 % on
 * the stream fixtures, seen on Rs). T^2 - Rt^2 = 4 (PL PR - |C|^2), so the identical quantity
 * 2 (PL PR - |C|^2) / (T + Rt) keeps the 1e-7 accuracy of its inputs (test_engine.py: Rs error 1e-4 -> 1e-6).
 * Regularised direction: as the primary vanishes (pure ambience, P_S << P_N) the direction tends to the
 * CENTRE, so the residual becomes the plain side signal (L - R)/sqrt2 instead of vanishing. */
static inline void decompose_quad(const float *PL, const float *PR, const scene_cpx *C, int i, int m,
                                  float *PN, float *PS, float *a, float *b, float *theta, float *Pa, float *Pb)
{
    const scene_v4 pl = v4_ld_n(PL + i, m), pr = v4_ld_n(PR + i, m);
    scene_v4 cr, ci, pn, Rt;
    v4_ld2_n((const float *)(C + i), m, &cr, &ci);
    v4_decompose_pn(pl, pr, cr, ci, &pn, &Rt);
    if (PN) v4_st_n(PN + i, pn, m);
    if (PS) v4_st_n(PS + i, Rt, m);
    if (a || b) {                                                 /* the engine asks for a, b in one of its calls per bin */
        scene_v4 va, vb;
        v4_decompose_ab(pl, pr, pn, &va, &vb);
        if (a) v4_st_n(a + i, va, m);
        if (b) v4_st_n(b + i, vb, m);
    }
    if (theta) v4_st_n(theta + i, v4_decompose_theta(cr, ci, Rt, pn), m);   /* phase only where a primary exists */
    if (Pa) v4_st_n(Pa + i, v4_max0(v4_sub(pl, pn)), m);
    if (Pb) v4_st_n(Pb + i, v4_max0(v4_sub(pr, pn)), m);
}

void scene_decompose_frame(const float *PL, const float *PR, const scene_cpx *C, int n,
                           float *PN, float *PS, float *a, float *b, float *theta, float *Pa, float *Pb)
{
    int i;
    for (i = 0; i + 4 <= n; i += 4) decompose_quad(PL, PR, C, i, 4, PN, PS, a, b, theta, Pa, Pb);   /* whole quads: plain loads */
    if (i < n) decompose_quad(PL, PR, C, i, n - i, PN, PS, a, b, theta, Pa, Pb);                /* the tail: n - i lanes */
}

/* Pan of the OFF-CENTRE content of a cell, immune to a centred source of any level.
 *
 * Model L = a·s + m', R = b·s + m' (+ symmetric ambience A):  num = P_L − Re C = a·S·(a−b) + A and
 * den = Re C − P_R = b·S·(a−b) − A, so num + den = S·(a² − b²) is free of both the centre and the
 * ambience (its sign is the side, its size the place energy), and a/b = num/den when ambience is
 * small.  Ambience pushes den below zero for a hard-panned source, so the sign test tolerates
 * 1.5·P_N (the eigen ambience estimate) and the clamped ratio biases a mild pan OUTWARD, never
 * toward the centre (the safe error: less transfer, never a vocal leak).
 * PN may be NULL (tol = 0, the Python PN=None); p, e, valid may be NULL. Four cells per step
 * (v4_centre_removed_pan of scene_simd.h). */
static inline void pan_quad(const float *PL, const float *PR, const scene_cpx *C, int i, int m, float eps,
                            const float *PN, float *p, float *e, int *valid)
{
    const scene_v4 pl = v4_ld_n(PL + i, m), pr = v4_ld_n(PR + i, m);
    const scene_v4 tol = PN ? v4_mul(v4_s(1.5f), v4_ld_n(PN + i, m)) : v4_zero();
    scene_v4 cr, ci, vp, ve;
    scene_m4 vv;
    v4_ld2_n((const float *)(C + i), m, &cr, &ci);
    v4_centre_removed_pan(pl, pr, cr, eps, tol, &vp, &ve, &vv);
    if (p) v4_st_n(p + i, vp, m);
    if (e) v4_st_n(e + i, ve, m);
    if (valid) m4_st_flags_n(valid + i, vv, m);
}

void scene_centre_removed_pan(const float *PL, const float *PR, const scene_cpx *C, int n, float eps,
                              const float *PN, float *p, float *e, int *valid)
{
    int i;
    for (i = 0; i + 4 <= n; i += 4) pan_quad(PL, PR, C, i, 4, eps, PN, p, e, valid);
    if (i < n) pan_quad(PL, PR, C, i, n - i, eps, PN, p, e, valid);
}

/* Bin -> group index; group width grows with frequency (~2 bins < 500 Hz, 3 to 2 kHz, 4 above).
 * Returns the number of groups (134 for 513/1024). group_freq (mean bin frequency, the Python
 * bincount(group, weights=freqs) / max(bincount(group), 1)) and group_count (bincount(group)) may be
 * NULL; both are written only for the first SCENE_GROUPS_MAX groups (the caller checks the count).
 * freqs[k] = k * SR / n_fft is exact in float32 (48000 = 2^7 * 375), as the engine's freqs table must be. */
int scene_erb_groups(int n_bins, int n_fft, int *group, float *group_freq, int *group_count)
{
    int scale = n_fft >= 1024 ? n_fft / 1024 : 1;
    int g = 0, k = 0;
    while (k < n_bins) {
        float f = (float)k * (float)SCENE_SR / (float)n_fft;
        int w = (f < 500.0f ? 2 : f < 2000.0f ? 3 : 4) * scale;
        if (w < 1) w = 1;
        int cnt = 0;
        float fsum = 0.0f;
        for (int j = k; j < k + w && j < n_bins; j++) {
            group[j] = g;
            fsum += (float)j * (float)SCENE_SR / (float)n_fft;
            cnt++;
        }
        if (g < SCENE_GROUPS_MAX) {
            if (group_count) group_count[g] = cnt;
            if (group_freq) group_freq[g] = fsum / (float)(cnt > 0 ? cnt : 1);
        }
        k += w;
        g++;
    }
    return g;
}

/* Static per-bin phase for Ls/Rs decorrelation of the ambient layer (free in the STFT domain).
 * A static per-bin phase: the per-bin MSC stays 1 (LTI), what drops is the band-limited
 * interaural correlation the ear uses; measure it as IACC over ERB bands, not Welch MSC.
 * ponytail: xorshift32 instead of numpy's PCG64 stream — any static uniform phase does the job, so the
 * values differ from phase_table(); the stream fixture test loads the Python table (phL/phR) instead. */
void scene_phase_table(int n_bins, uint32_t seed, float sign, scene_cpx *out)
{
    uint32_t s = seed * 2654435761u + 0x9E3779B9u;
    if (!s) s = 1u;                                   /* xorshift32 must not start at 0 */
    for (int k = 0; k < n_bins; k++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        float phi = (float)(s >> 8) * (2.0f * SCENE_PI / 16777216.0f) - SCENE_PI;   /* uniform [-pi, pi) */
        out[k].re = cosf(sign * phi);                 /* exp(1j * sign * phi) */
        out[k].im = sinf(sign * phi);
    }
}

/* ----------------------------------------------------------------------------- FFT
 * 1024-point transforms on the shared Stockham radix-4 kernel (scene_fft.h: NEON four-wide, no bit reversal,
 * natural order in and out). The engine packs the two channels of a frame into one complex transform
 * (z = a + i b) and recovers two real spectra, and packs two Hermitian spectra into one inverse transform
 * and reads two real signals from its real and imaginary parts: three complex FFTs per hop instead of six.
 * The twiddle plan is static, built on the first call (identical values if two threads race to build it).
 *
 * scratch[4 * SCENE_NFFT]: [0, 2N) work block, [2N, 3N) and [3N, 4N) the input block of the inverse pair
 * (consumed by its first stage, so it may also hold the forward pair's input). */
static struct scene_fft_plan fft_plan;
static float fft_tw_re[SCENE_NFFT], fft_tw_im[SCENE_NFFT];
static int   fft_ready;

static void fft_tables(void)
{
    scene_fft_plan_init(&fft_plan, SCENE_NFFT, fft_tw_re, fft_tw_im);   /* 1024 = 4^5: cannot fail */
    fft_ready = 1;
}

/* Single real input: the complex transform of (x, 0), bins 0..512 read off directly. */
void scene_rfft1024(const float *x, scene_cpx *X, float *scratch)
{
    const int N = SCENE_NFFT;
    float *zr = scratch + 2 * N, *zi = scratch + 3 * N;
    int k;
    if (!fft_ready) fft_tables();
    for (k = 0; k < N; k++) { zr[k] = x[k]; zi[k] = 0.0f; }
    scene_fft_forward(&fft_plan, zr, zi, scratch, scratch + N, scratch + 2 * N);
    for (k = 0; k < SCENE_BINS; k++) {
        X[k].re = scratch[k];
        X[k].im = scratch[N + k];
    }
}

/* Single real output: the Hermitian extension X[N-k] = conj(X[k]), scaled by 1/N (numpy irfft), through the
 * inverse transform; the real part is the signal. The imaginary parts of bins 0 and 512 only reach the
 * discarded imaginary output, so they are dropped here as numpy's c2r drops them. */
void scene_irfft1024(const scene_cpx *X, float *x, float *scratch)
{
    const int N = SCENE_NFFT;
    const float inv_n = 1.0f / (float)N;
    float *zr = scratch + 2 * N, *zi = scratch + 3 * N;
    int k;
    if (!fft_ready) fft_tables();
    zr[0] = X[0].re * inv_n;             zi[0] = 0.0f;
    zr[N / 2] = X[N / 2].re * inv_n;     zi[N / 2] = 0.0f;
    for (k = 1; k < N / 2; k++) {
        const float re = X[k].re * inv_n, im = X[k].im * inv_n;
        zr[k] = re;     zi[k] = im;
        zr[N - k] = re; zi[N - k] = -im;
    }
    scene_fft_inverse(&fft_plan, zr, zi, scratch, scratch + N, scratch + 2 * N);
    for (k = 0; k < N; k++) x[k] = scratch[k];
}

/* Two real inputs in one complex transform: z = xa + i xb, Z = FFT(z), then
 * XA[k] = (Z[k] + conj Z[N-k]) / 2 and XB[k] = (Z[k] - conj Z[N-k]) / 2i for k = 0..512 (Z[N] = Z[0]).
 * xa/xb may live in scratch[2N, 4N) (the forward's second target, consumed by its first stage). */
void scene_rfft1024_pair(const float *xa, const float *xb, scene_cpx *XA, scene_cpx *XB, float *scratch)
{
    const int N = SCENE_NFFT;
    const float *zr = scratch, *zi = scratch + N;
    int k;
    if (!fft_ready) fft_tables();
    scene_fft_forward(&fft_plan, xa, xb, scratch, scratch + N, scratch + 2 * N);
    XA[0].re = zr[0];         XA[0].im = 0.0f;
    XB[0].re = zi[0];         XB[0].im = 0.0f;
    XA[N / 2].re = zr[N / 2]; XA[N / 2].im = 0.0f;
    XB[N / 2].re = zi[N / 2]; XB[N / 2].im = 0.0f;
    {
        const scene_v4 half = v4_s(0.5f);
        for (k = 1; k + 4 <= N / 2; k += 4) {                       /* bins k..k+3 against N-k..N-k-3 (reversed) */
            const scene_v4 ar = v4_ld(zr + k), ai = v4_ld(zi + k);
            const scene_v4 br = v4_ldrev(zr + N - k - 3), bi = v4_ldrev(zi + N - k - 3);
            v4_st2((float *)(XA + k), v4_mul(half, v4_add(ar, br)), v4_mul(half, v4_sub(ai, bi)));
            v4_st2((float *)(XB + k), v4_mul(half, v4_add(ai, bi)), v4_mul(half, v4_sub(br, ar)));
        }
    }
    for (; k < N / 2; k++) {                                        /* the last three bins */
        const float ar = zr[k], ai = zi[k], br = zr[N - k], bi = zi[N - k];
        XA[k].re = 0.5f * (ar + br); XA[k].im = 0.5f * (ai - bi);
        XB[k].re = 0.5f * (ai + bi); XB[k].im = 0.5f * (br - ar);
    }
}

/* Two real outputs from one inverse transform: Z = A_full + i B_full (Hermitian extensions, bins 0 and 512
 * real as in scene_irfft1024, 1/N folded in), z = IFFT(Z), xa = Re z, xb = Im z. xa/xb must not overlap
 * scratch[0, 4N) (the inverse's first target is xa/xb themselves, its second the work block). */
void scene_irfft1024_pair(const scene_cpx *XA, const scene_cpx *XB, float *xa, float *xb, float *scratch)
{
    const int N = SCENE_NFFT;
    const float inv_n = 1.0f / (float)N;
    float *zr = scratch + 2 * N, *zi = scratch + 3 * N;
    int k;
    if (!fft_ready) fft_tables();
    zr[0] = XA[0].re * inv_n;         zi[0] = XB[0].re * inv_n;
    zr[N / 2] = XA[N / 2].re * inv_n; zi[N / 2] = XB[N / 2].re * inv_n;
    {
        const scene_v4 vin = v4_s(inv_n);
        for (k = 1; k + 4 <= N / 2; k += 4) {                       /* bins k..k+3, mirrored into N-k..N-k-3 */
            scene_v4 ar, ai, br, bi;
            v4_ld2((const float *)(XA + k), &ar, &ai);
            v4_ld2((const float *)(XB + k), &br, &bi);
            ar = v4_mul(ar, vin); ai = v4_mul(ai, vin); br = v4_mul(br, vin); bi = v4_mul(bi, vin);
            v4_st(zr + k, v4_sub(ar, bi));              v4_st(zi + k, v4_add(ai, br));              /* A + iB */
            v4_strev(zr + N - k - 3, v4_add(ar, bi));   v4_strev(zi + N - k - 3, v4_sub(br, ai));   /* conj A + i conj B */
        }
    }
    for (; k < N / 2; k++) {                                        /* the last three bins */
        const float ar = XA[k].re * inv_n, ai = XA[k].im * inv_n, br = XB[k].re * inv_n, bi = XB[k].im * inv_n;
        zr[k] = ar - bi;     zi[k] = ai + br;
        zr[N - k] = ar + bi; zi[N - k] = br - ai;
    }
    scene_fft_inverse(&fft_plan, zr, zi, xa, xb, scratch);
}
