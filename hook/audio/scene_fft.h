/* scene_fft.h — the complex FFT shared by the scene renderer (scene_engine_math.c, 1024 points) and the fine
 * mask net (scene_mask.c, 2048 points): Stockham autosort, radix-4 decimation in frequency plus one radix-2
 * stage when n = 2 * 4^a, split re/im float32 arrays, natural order in and out, out of place per stage
 * (ping-pong between the output and a scratch block), so there is no bit reversal anywhere.
 *
 * NEON (ARMv7 -mfpu=neon, the arm64 host) does four butterflies per step; the scalar fallback performs the
 * same operations in the same order, so every build computes the same bits. No fused multiply-add (separate
 * multiplies, one add or subtract), no doubles, no allocation: the caller owns the twiddle storage and the
 * buffers. Header-only (static inline) so both users stay standalone translation units.
 *
 * A complex array of n points is two float arrays: re[n] and im[n]. One stage with sub-length n and stride s
 * (m = n / 4; a_r = x[q + s (p + r m)] for p < m, q < s; W = exp(-2 pi i / n), conj W for the inverse):
 *   b0 = a0 + a2, b1 = a0 - a2, b2 = a1 + a3, d = a1 - a3, b3 = -i d (forward) / +i d (inverse)
 *   y[q + s (4p + 0)] = b0 + b2            y[q + s (4p + 1)] = (b1 + b3) W^p
 *   y[q + s (4p + 2)] = (b0 - b2) W^2p     y[q + s (4p + 3)] = (b1 - b3) W^3p
 * then n /= 4, s *= 4 on the swapped buffers; a final n = 2 stage is y[q] = x[q] + x[q + s], y[q + s] = x[q] - x[q + s].
 * The inverse is unscaled (the caller folds 1/N into its spectrum packing).
 */
#ifndef SCENE_FFT_H
#define SCENE_FFT_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SCENE_FFT_NEON 1
#else
#define SCENE_FFT_NEON 0
#endif

#define SCENE_FFT_STAGES_MAX 8

struct scene_fft_plan {
    int n;                              /* 4^a or 2 * 4^a, >= 16 */
    int n_r4;                           /* radix-4 stages */
    int has_r2;                         /* a final radix-2 stage (n = 2 * 4^a) */
    int off[SCENE_FFT_STAGES_MAX];      /* twiddles of radix-4 stage i start here: W^p, then W^2p, then W^3p, m each */
    const float *tw_re, *tw_im;         /* caller storage of n floats each (n - 1 or n - 2 are used) */
};

/* exp(-2 pi i k / n) for 0 <= k < n, n a multiple of 8: cosf/sinf see an argument of at most pi/4 (one
 * rounding of j * (2 pi / n)); the other octants follow by symmetry, so every entry is within an ulp or two. */
static inline void scene_fft_twiddle(int k, int n, float *re, float *im)
{
    const int n8 = n / 8;
    const int oct = k / n8, j = k - oct * n8;
    const float step = 6.283185307179586f / (float)n;            /* exact: n is a power of two */
    const float a = (float)((oct & 1) ? n8 - j : j) * step;
    const float c = cosf(a), s = sinf(a);
    float cr, sr;                                                  /* cos theta, sin theta, theta = 2 pi k / n */
    switch (oct) {
    case 0:  cr = c;  sr = s;  break;                              /* theta = a */
    case 1:  cr = s;  sr = c;  break;                              /* pi/2 - a */
    case 2:  cr = -s; sr = c;  break;                              /* pi/2 + a */
    case 3:  cr = -c; sr = s;  break;                              /* pi - a */
    case 4:  cr = -c; sr = -s; break;                              /* pi + a */
    case 5:  cr = -s; sr = -c; break;                              /* 3pi/2 - a */
    case 6:  cr = s;  sr = -c; break;                              /* 3pi/2 + a */
    default: cr = c;  sr = -s; break;                              /* 2pi - a */
    }
    *re = cr;
    *im = -sr;
}

/* Fills pl for n (16 <= n, 4^a or 2 * 4^a) using tw_re[n], tw_im[n] as storage. Returns 0, -1 on a bad n. */
static inline int scene_fft_plan_init(struct scene_fft_plan *pl, int n, float *tw_re, float *tw_im)
{
    int ni = n, off = 0;
    if (n < 16) return -1;
    pl->n = n; pl->n_r4 = 0; pl->has_r2 = 0;
    while (ni >= 4 && (ni & 3) == 0) {
        const int m = ni / 4;
        int p;
        if (pl->n_r4 >= SCENE_FFT_STAGES_MAX || off + 3 * m > n) return -1;
        pl->off[pl->n_r4++] = off;
        for (p = 0; p < m; p++) {
            if (ni >= 8) {
                scene_fft_twiddle(p, ni, tw_re + off + p, tw_im + off + p);
                scene_fft_twiddle(2 * p, ni, tw_re + off + m + p, tw_im + off + m + p);
                scene_fft_twiddle(3 * p, ni, tw_re + off + 2 * m + p, tw_im + off + 2 * m + p);
            } else {                                               /* n = 4: W^0 only (and the stage skips it) */
                tw_re[off + p] = tw_re[off + m + p] = tw_re[off + 2 * m + p] = 1.0f;
                tw_im[off + p] = tw_im[off + m + p] = tw_im[off + 2 * m + p] = 0.0f;
            }
        }
        off += 3 * m;
        ni /= 4;
    }
    if (ni == 2) pl->has_r2 = 1;
    else if (ni != 1) return -1;
    pl->tw_re = tw_re;
    pl->tw_im = tw_im;
    return 0;
}

/* The butterfly on scalars or on four lanes: b3 = -i d (forward) or +i d (inverse) costs no multiply and no
 * negation, it only swaps which of d's parts is added and which subtracted. */
#define SCENE_FFT_R4(T, ADD, SUB, inv, a0r, a0i, a1r, a1i, a2r, a2i, a3r, a3i, y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i) \
    do {                                                                                                      \
        T b0r_ = ADD(a0r, a2r), b0i_ = ADD(a0i, a2i), b1r_ = SUB(a0r, a2r), b1i_ = SUB(a0i, a2i);            \
        T b2r_ = ADD(a1r, a3r), b2i_ = ADD(a1i, a3i), dr_ = SUB(a1r, a3r), di_ = SUB(a1i, a3i);              \
        y0r = ADD(b0r_, b2r_); y0i = ADD(b0i_, b2i_); y2r = SUB(b0r_, b2r_); y2i = SUB(b0i_, b2i_);          \
        if (inv) { y1r = SUB(b1r_, di_); y1i = ADD(b1i_, dr_); y3r = ADD(b1r_, di_); y3i = SUB(b1i_, dr_); } \
        else     { y1r = ADD(b1r_, di_); y1i = SUB(b1i_, dr_); y3r = SUB(b1r_, di_); y3i = ADD(b1i_, dr_); } \
    } while (0)

/* y * w (forward) or y * conj(w) (inverse): four separate products, one add and one subtract. */
#define SCENE_FFT_CMUL(T, ADD, SUB, MUL, inv, yr, yi, wr, wi, outr, outi)                    \
    do {                                                                                     \
        T pr_ = MUL(yr, wr), pi_ = MUL(yi, wi), qr_ = MUL(yr, wi), qi_ = MUL(yi, wr);        \
        if (inv) { outr = ADD(pr_, pi_); outi = SUB(qi_, qr_); }                             \
        else     { outr = SUB(pr_, pi_); outi = ADD(qr_, qi_); }                             \
    } while (0)

/* NEON mid stages: the twiddle of this p is a (re, im) pair in one D register, multiplied in by lane (the
 * same IEEE products as the vector form, so the bits are the scalar path's); no broadcast registers, so
 * the butterfly's data keeps the 16 Q registers (the broadcast form spilled). */
#if SCENE_FFT_NEON
#define SCENE_FFT_CMUL_LANE(inv, yr, yi, w, outr, outi)                                                   \
    do {                                                                                                  \
        float32x4_t pr_ = vmulq_lane_f32(yr, w, 0), pi_ = vmulq_lane_f32(yi, w, 1);                       \
        float32x4_t qr_ = vmulq_lane_f32(yr, w, 1), qi_ = vmulq_lane_f32(yi, w, 0);                       \
        if (inv) { outr = vaddq_f32(pr_, pi_); outi = vsubq_f32(qi_, qr_); }                              \
        else     { outr = vsubq_f32(pr_, pi_); outi = vaddq_f32(qr_, qi_); }                              \
    } while (0)
#endif

#define SCENE_FFT_SADD(a, b) ((a) + (b))
#define SCENE_FFT_SSUB(a, b) ((a) - (b))
#define SCENE_FFT_SMUL(a, b) ((a) * (b))

#if defined(__GNUC__)
#define SCENE_FFT_INLINE static inline __attribute__((always_inline))
#else
#define SCENE_FFT_INLINE static inline
#endif

/* First stage (s = 1): a_r = x[p + r m], y[4p + j]. Four p per step, the outputs interleaved by vst4. */
SCENE_FFT_INLINE void scene_fft_r4_first(const struct scene_fft_plan *pl, int inv,
                                         const float *xr, const float *xi, float *yr, float *yi)
{
    const int m = pl->n / 4;
    const float *w1r = pl->tw_re + pl->off[0], *w1i = pl->tw_im + pl->off[0];
    const float *w2r = w1r + m, *w2i = w1i + m, *w3r = w1r + 2 * m, *w3i = w1i + 2 * m;
    int p = 0;
#if SCENE_FFT_NEON
    for (; p + 4 <= m; p += 4) {
        const float32x4_t a0r = vld1q_f32(xr + p), a0i = vld1q_f32(xi + p);
        const float32x4_t a1r = vld1q_f32(xr + p + m), a1i = vld1q_f32(xi + p + m);
        const float32x4_t a2r = vld1q_f32(xr + p + 2 * m), a2i = vld1q_f32(xi + p + 2 * m);
        const float32x4_t a3r = vld1q_f32(xr + p + 3 * m), a3i = vld1q_f32(xi + p + 3 * m);
        const float32x4_t v1r = vld1q_f32(w1r + p), v1i = vld1q_f32(w1i + p);
        const float32x4_t v2r = vld1q_f32(w2r + p), v2i = vld1q_f32(w2i + p);
        const float32x4_t v3r = vld1q_f32(w3r + p), v3i = vld1q_f32(w3i + p);
        float32x4_t y1r, y1i, y2r, y2i, y3r, y3i;
        float32x4x4_t outr, outi;
        SCENE_FFT_R4(float32x4_t, vaddq_f32, vsubq_f32, inv, a0r, a0i, a1r, a1i, a2r, a2i, a3r, a3i,
                     outr.val[0], outi.val[0], y1r, y1i, y2r, y2i, y3r, y3i);
        SCENE_FFT_CMUL(float32x4_t, vaddq_f32, vsubq_f32, vmulq_f32, inv, y1r, y1i, v1r, v1i, outr.val[1], outi.val[1]);
        SCENE_FFT_CMUL(float32x4_t, vaddq_f32, vsubq_f32, vmulq_f32, inv, y2r, y2i, v2r, v2i, outr.val[2], outi.val[2]);
        SCENE_FFT_CMUL(float32x4_t, vaddq_f32, vsubq_f32, vmulq_f32, inv, y3r, y3i, v3r, v3i, outr.val[3], outi.val[3]);
        vst4q_f32(yr + 4 * p, outr);
        vst4q_f32(yi + 4 * p, outi);
    }
#endif
    for (; p < m; p++) {
        const float a0r = xr[p], a0i = xi[p], a1r = xr[p + m], a1i = xi[p + m];
        const float a2r = xr[p + 2 * m], a2i = xi[p + 2 * m], a3r = xr[p + 3 * m], a3i = xi[p + 3 * m];
        float y1r, y1i, y2r, y2i, y3r, y3i;
        SCENE_FFT_R4(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, inv, a0r, a0i, a1r, a1i, a2r, a2i, a3r, a3i,
                     yr[4 * p], yi[4 * p], y1r, y1i, y2r, y2i, y3r, y3i);
        SCENE_FFT_CMUL(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, SCENE_FFT_SMUL, inv, y1r, y1i, w1r[p], w1i[p], yr[4 * p + 1], yi[4 * p + 1]);
        SCENE_FFT_CMUL(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, SCENE_FFT_SMUL, inv, y2r, y2i, w2r[p], w2i[p], yr[4 * p + 2], yi[4 * p + 2]);
        SCENE_FFT_CMUL(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, SCENE_FFT_SMUL, inv, y3r, y3i, w3r[p], w3i[p], yr[4 * p + 3], yi[4 * p + 3]);
    }
}

/* Later stages (s >= 4): one twiddle triple per p, four q per step. notw = the n = 4 stage (W^0 only). */
SCENE_FFT_INLINE void scene_fft_r4_mid(const struct scene_fft_plan *pl, int stage, int n, int s, int inv, int notw,
                                       const float *xr, const float *xi, float *yr, float *yi)
{
    const int m = n / 4, sm = s * m;
    const float *twr = pl->tw_re + pl->off[stage], *twi = pl->tw_im + pl->off[stage];
    int p, q;
    for (p = 0; p < m; p++) {
        const float *xb_r = xr + s * p, *xb_i = xi + s * p;    /* a_r at + r * sm */
        float *yb_r = yr + 4 * s * p, *yb_i = yi + 4 * s * p;  /* y_j at + j * s */
        const float w1r = twr[p], w1i = twi[p], w2r = twr[m + p], w2i = twi[m + p], w3r = twr[2 * m + p], w3i = twi[2 * m + p];
        q = 0;
#if SCENE_FFT_NEON
        {
            const float32x2_t v1 = vld1_lane_f32(twi + p, vld1_dup_f32(twr + p), 1);               /* {w1r, w1i} */
            const float32x2_t v2 = vld1_lane_f32(twi + m + p, vld1_dup_f32(twr + m + p), 1);
            const float32x2_t v3 = vld1_lane_f32(twi + 2 * m + p, vld1_dup_f32(twr + 2 * m + p), 1);
            for (; q + 4 <= s; q += 4) {
                const float32x4_t a0r = vld1q_f32(xb_r + q), a0i = vld1q_f32(xb_i + q);
                const float32x4_t a1r = vld1q_f32(xb_r + q + sm), a1i = vld1q_f32(xb_i + q + sm);
                const float32x4_t a2r = vld1q_f32(xb_r + q + 2 * sm), a2i = vld1q_f32(xb_i + q + 2 * sm);
                const float32x4_t a3r = vld1q_f32(xb_r + q + 3 * sm), a3i = vld1q_f32(xb_i + q + 3 * sm);
                float32x4_t y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i;
                SCENE_FFT_R4(float32x4_t, vaddq_f32, vsubq_f32, inv, a0r, a0i, a1r, a1i, a2r, a2i, a3r, a3i,
                             y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i);
                vst1q_f32(yb_r + q, y0r);
                vst1q_f32(yb_i + q, y0i);
                if (notw) {
                    vst1q_f32(yb_r + q + s, y1r);         vst1q_f32(yb_i + q + s, y1i);
                    vst1q_f32(yb_r + q + 2 * s, y2r);     vst1q_f32(yb_i + q + 2 * s, y2i);
                    vst1q_f32(yb_r + q + 3 * s, y3r);     vst1q_f32(yb_i + q + 3 * s, y3i);
                } else {
                    float32x4_t tr, ti;
                    SCENE_FFT_CMUL_LANE(inv, y1r, y1i, v1, tr, ti);
                    vst1q_f32(yb_r + q + s, tr);          vst1q_f32(yb_i + q + s, ti);
                    SCENE_FFT_CMUL_LANE(inv, y2r, y2i, v2, tr, ti);
                    vst1q_f32(yb_r + q + 2 * s, tr);      vst1q_f32(yb_i + q + 2 * s, ti);
                    SCENE_FFT_CMUL_LANE(inv, y3r, y3i, v3, tr, ti);
                    vst1q_f32(yb_r + q + 3 * s, tr);      vst1q_f32(yb_i + q + 3 * s, ti);
                }
            }
        }
#endif
        for (; q < s; q++) {
            const float a0r = xb_r[q], a0i = xb_i[q], a1r = xb_r[q + sm], a1i = xb_i[q + sm];
            const float a2r = xb_r[q + 2 * sm], a2i = xb_i[q + 2 * sm], a3r = xb_r[q + 3 * sm], a3i = xb_i[q + 3 * sm];
            float y1r, y1i, y2r, y2i, y3r, y3i;
            SCENE_FFT_R4(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, inv, a0r, a0i, a1r, a1i, a2r, a2i, a3r, a3i,
                         yb_r[q], yb_i[q], y1r, y1i, y2r, y2i, y3r, y3i);
            if (notw) {
                yb_r[q + s] = y1r;     yb_i[q + s] = y1i;
                yb_r[q + 2 * s] = y2r; yb_i[q + 2 * s] = y2i;
                yb_r[q + 3 * s] = y3r; yb_i[q + 3 * s] = y3i;
            } else {
                SCENE_FFT_CMUL(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, SCENE_FFT_SMUL, inv, y1r, y1i, w1r, w1i, yb_r[q + s], yb_i[q + s]);
                SCENE_FFT_CMUL(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, SCENE_FFT_SMUL, inv, y2r, y2i, w2r, w2i, yb_r[q + 2 * s], yb_i[q + 2 * s]);
                SCENE_FFT_CMUL(float, SCENE_FFT_SADD, SCENE_FFT_SSUB, SCENE_FFT_SMUL, inv, y3r, y3i, w3r, w3i, yb_r[q + 3 * s], yb_i[q + 3 * s]);
            }
        }
    }
}

/* The final n = 2 stage (s = N / 2): twiddle 1, pure add/subtract. */
SCENE_FFT_INLINE void scene_fft_r2_last(int s, const float *xr, const float *xi, float *yr, float *yi)
{
    int q = 0;
#if SCENE_FFT_NEON
    for (; q + 4 <= s; q += 4) {
        const float32x4_t ar = vld1q_f32(xr + q), ai = vld1q_f32(xi + q);
        const float32x4_t br = vld1q_f32(xr + q + s), bi = vld1q_f32(xi + q + s);
        vst1q_f32(yr + q, vaddq_f32(ar, br));     vst1q_f32(yi + q, vaddq_f32(ai, bi));
        vst1q_f32(yr + q + s, vsubq_f32(ar, br)); vst1q_f32(yi + q + s, vsubq_f32(ai, bi));
    }
#endif
    for (; q < s; q++) {
        const float ar = xr[q], ai = xi[q], br = xr[q + s], bi = xi[q + s];
        yr[q] = ar + br;     yi[q] = ai + bi;
        yr[q + s] = ar - br; yi[q + s] = ai - bi;
    }
}

/* in (re, im) -> out (re, im) through the stages, ping-ponging between out and scratch[2n] so that the last
 * stage lands in out. in must not alias the FIRST target (out when the stage count is odd, e.g. 1024 with
 * five stages; scratch when it is even, e.g. 2048 with six); it may alias the second (it is consumed by then). */
SCENE_FFT_INLINE void scene_fft_exec(const struct scene_fft_plan *pl, int inv, const float *in_re, const float *in_im,
                                     float *out_re, float *out_im, float *scratch)
{
    const int N = pl->n, total = pl->n_r4 + pl->has_r2;
    const float *xr = in_re, *xi = in_im;
    float *yr, *yi, *zr, *zi, *t;
    int n = N, s = 1, i;
    if (total & 1) { yr = out_re; yi = out_im; zr = scratch; zi = scratch + N; }
    else           { yr = scratch; yi = scratch + N; zr = out_re; zi = out_im; }
    for (i = 0; i < pl->n_r4; i++) {
        if (s == 1)          scene_fft_r4_first(pl, inv, xr, xi, yr, yi);
        else if (n == 4)     scene_fft_r4_mid(pl, i, n, s, inv, 1, xr, xi, yr, yi);
        else                 scene_fft_r4_mid(pl, i, n, s, inv, 0, xr, xi, yr, yi);
        xr = yr; xi = yi;
        t = yr; yr = zr; zr = t;
        t = yi; yi = zi; zi = t;
        n >>= 2; s <<= 2;
    }
    if (pl->has_r2) scene_fft_r2_last(s, xr, xi, yr, yi);
}

static inline void scene_fft_forward(const struct scene_fft_plan *pl, const float *in_re, const float *in_im,
                                     float *out_re, float *out_im, float *scratch)
{
    scene_fft_exec(pl, 0, in_re, in_im, out_re, out_im, scratch);
}

static inline void scene_fft_inverse(const struct scene_fft_plan *pl, const float *in_re, const float *in_im,
                                     float *out_re, float *out_im, float *scratch)
{
    scene_fft_exec(pl, 1, in_re, in_im, out_re, out_im, scratch);
}

#endif
