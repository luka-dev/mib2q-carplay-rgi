/* scene_mask.c — the fine mask net (mask_net.py ConvNet / conv2048) as a streaming float32 C runtime.
 * See scene_mask.h for the contract. Golden test: tools/stereo-upmix-render/port/test_mask.py.
 *
 * C99 (gnu99), float32 only, no FMA, no allocation, no other hook file (scene_fft.h is header-only). Every
 * kernel is written once on a four-lane vector type `v4`: NEON (ARMv7 -mfpu=neon, the arm64 host) or a
 * scalar struct of four floats that performs the same operations in the same order, so the two builds compute
 * the same bits on the host (-ffp-contract=off). Per control (QEMU -icount, GCC 8.5 -O2) the scalar libm port
 * cost 19.4 M instructions; this file is measured by tools/stereo-upmix-render/bench/scene_mask_bench.c.
 *
 * Arithmetic that is not torch's: tanh / sigmoid are the retired hssr_mask.c's clamped [7/6] rational
 * (tag upmix-universal-last-2026-10-09; |err| < 1e-4), the
 * log features a mantissa series (|err| < 1e-6 in natural-log units, standardised and clamped to +-6 after), and
 * on ARMv7 every reciprocal is VRECPE refined by two Newton steps (<= 2 ulp; the host divides exactly). The
 * goldens gate the result at 1e-3 per layer (test_mask.py section B).
 *
 * Kernel shape (GCC 8.5 for ARMv7): bins are the vector lanes, a tile is 4 or 8 output channels of 4 or 8
 * bins held in registers, and each weight reaches the multiply-accumulate as a lane of a two-float VFP
 * load (vldr with an immediate offset from the tile's contiguous weight block). A weight loaded as a
 * four-lane vector costs this compiler two register copies per use (the by-scalar VMLA wants d0-d15).
 */
/* GCC's first scheduling pass (on by default for ARM at -O2) hoists the weight loads of a tile above the
 * multiply-accumulates that consume them, runs out of NEON registers and spills: -4 % instructions per control
 * without it (QEMU, 1.425 -> 1.367 M). The pragma precedes every include so the whole unit shares one option
 * set and the always_inline helpers keep inlining. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("no-schedule-insns")
#endif
#include "scene_mask.h"   /* before <math.h>: the QNX 6.5 math.h sets GCC's _SIZE_T guard without the typedef */
#include "scene_fft.h"
#include <math.h>
#include <string.h>

#define SMASK_PI 3.14159265358979323846f
#define SMASK_TANH_LIMIT 4.97f     /* where the rational reaches 1 - 1e-5 (hssr_mask.c's SPA_MASK_TANH_LIMIT) */

typedef char smask_weights_size_check[sizeof(struct scene_mask_weights) == 20 + 4 * SMASK_NPARAM ? 1 : -1];
typedef char smask_cpx_size_check[sizeof(smask_cpx) == 2 * sizeof(float) ? 1 : -1];
typedef char smask_debug_room_check[2 * 2 * SMASK_NFFT >= SMASK_CH * SMASK_BINS ? 1 : -1];
typedef char smask_row_check[SMASK_PAD + 264 <= SMASK_ROW && 264 <= SMASK_OROW ? 1 : -1];   /* 8-bin steps to bin 263 */

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SMASK_NEON 1
#else
#define SMASK_NEON 0
#endif

/* the stages stay separate functions so the QEMU function profile (bench/qemu_fnprof_plugin.c) attributes
 * their instructions; six calls per control cost nothing */
#if defined(__GNUC__)
#define SMASK_STAGE static __attribute__((noinline))
#else
#define SMASK_STAGE static
#endif

/* ----------------------------------------------------------------------------- the four-lane layer
 * V4_MLA(a, b, c) = a + b * c with the product rounded first (NEON VMLA is unfused on ARMv7; the host build is
 * -ffp-contract=off). V4_MLA4W(a0..a3, x, w) = a_n + x * w[n] for the four floats at w. */
#if SMASK_NEON
typedef float32x4_t v4;
SCENE_FFT_INLINE v4   V4_LD(const float *p)        { return vld1q_f32(p); }
SCENE_FFT_INLINE void V4_ST(float *p, v4 a)        { vst1q_f32(p, a); }
SCENE_FFT_INLINE v4   V4_DUP(float s)              { return vdupq_n_f32(s); }
SCENE_FFT_INLINE v4   V4_ADD(v4 a, v4 b)           { return vaddq_f32(a, b); }
SCENE_FFT_INLINE v4   V4_SUB(v4 a, v4 b)           { return vsubq_f32(a, b); }
SCENE_FFT_INLINE v4   V4_MUL(v4 a, v4 b)           { return vmulq_f32(a, b); }
SCENE_FFT_INLINE v4   V4_MLA(v4 a, v4 b, v4 c)     { return vmlaq_f32(a, b, c); }
SCENE_FFT_INLINE v4   V4_MIN(v4 a, v4 b)           { return vminq_f32(a, b); }
SCENE_FFT_INLINE v4   V4_MAX(v4 a, v4 b)           { return vmaxq_f32(a, b); }
SCENE_FFT_INLINE v4   V4_EXT1(v4 a, v4 b)          { return vextq_f32(a, b, 1); }   /* a[1..3], b[0] */
SCENE_FFT_INLINE v4   V4_EXT2(v4 a, v4 b)          { return vextq_f32(a, b, 2); }
SCENE_FFT_INLINE v4   V4_EXT3(v4 a, v4 b)          { return vextq_f32(a, b, 3); }
SCENE_FFT_INLINE v4   V4_REV(v4 a)                 { v4 r = vrev64q_f32(a); return vextq_f32(r, r, 2); }
/* a weight pair as a plain 64-bit load (VFP vldr with an immediate offset; vld1_f32 would cost an address
 * add per pair and GCC 8 copies a four-lane weight vector into d0-d15 before every by-scalar VMLA) */
typedef float32x2_t __attribute__((may_alias)) smask_wpair;
#define V4_MLA4W(a0, a1, a2, a3, x, w) do {                                              \
        const smask_wpair lo_ = *(const smask_wpair *)(w);                               \
        const smask_wpair hi_ = *(const smask_wpair *)((w) + 2); v4 x_ = (x);            \
        a0 = vmlaq_lane_f32(a0, x_, lo_, 0); a1 = vmlaq_lane_f32(a1, x_, lo_, 1);        \
        a2 = vmlaq_lane_f32(a2, x_, hi_, 0); a3 = vmlaq_lane_f32(a3, x_, hi_, 1); } while (0)
SCENE_FFT_INLINE void V4_LD2(const float *p, v4 *a, v4 *b)
{
    float32x4x2_t t = vld2q_f32(p);
    *a = t.val[0]; *b = t.val[1];
}
SCENE_FFT_INLINE void V4_ST2(float *p, v4 a, v4 b)
{
    float32x4x2_t t;
    t.val[0] = a; t.val[1] = b;
    vst2q_f32(p, t);
}
SCENE_FFT_INLINE void V4_LD4(const float *p, v4 *a, v4 *b, v4 *c, v4 *d)
{
    float32x4x4_t t = vld4q_f32(p);
    *a = t.val[0]; *b = t.val[1]; *c = t.val[2]; *d = t.val[3];
}
/* x = m 2^e for a positive normal x: e as a float, m in [1, 2) */
SCENE_FFT_INLINE v4 V4_EXPONENT(v4 x)
{
    return vcvtq_f32_s32(vsubq_s32(vreinterpretq_s32_u32(vshrq_n_u32(vreinterpretq_u32_f32(x), 23)), vdupq_n_s32(127)));
}
SCENE_FFT_INLINE v4 V4_MANTISSA(v4 x)
{
    return vreinterpretq_f32_u32(vorrq_u32(vandq_u32(vreinterpretq_u32_f32(x), vdupq_n_u32(0x007fffffu)),
                                           vdupq_n_u32(0x3f800000u)));
}
#if defined(__aarch64__)
SCENE_FFT_INLINE v4 V4_DIV(v4 a, v4 b)  { return vdivq_f32(a, b); }      /* exact: equals the scalar fallback */
SCENE_FFT_INLINE v4 V4_SQRT(v4 a)       { return vsqrtq_f32(a); }
#else
/* ARMv7 has no vector divide: the estimate plus two Newton steps (hssr_mask.c's spa_mask_tanh4 form), <= 2 ulp.
 * The square root (257 vectors per control) goes through VFP exactly. */
SCENE_FFT_INLINE v4 V4_DIV(v4 a, v4 b)
{
    v4 r = vrecpeq_f32(b);
    r = vmulq_f32(r, vrecpsq_f32(b, r));
    r = vmulq_f32(r, vrecpsq_f32(b, r));
    return vmulq_f32(a, r);
}
SCENE_FFT_INLINE v4 V4_SQRT(v4 a)
{
    float t[4] SMASK_ALIGN16;
    vst1q_f32(t, a);
    t[0] = sqrtf(t[0]); t[1] = sqrtf(t[1]); t[2] = sqrtf(t[2]); t[3] = sqrtf(t[3]);
    return vld1q_f32(t);
}
#endif
#else  /* the scalar fallback: lane by lane, the same operations in the same order */
typedef struct { float s[4]; } v4;
#define V4_LANES(expr) do { int i_; for (i_ = 0; i_ < 4; i_++) { expr; } } while (0)
SCENE_FFT_INLINE v4   V4_LD(const float *p)        { v4 r; V4_LANES(r.s[i_] = p[i_]); return r; }
SCENE_FFT_INLINE void V4_ST(float *p, v4 a)        { V4_LANES(p[i_] = a.s[i_]); }
SCENE_FFT_INLINE v4   V4_DUP(float s)              { v4 r; V4_LANES(r.s[i_] = s); return r; }
SCENE_FFT_INLINE v4   V4_ADD(v4 a, v4 b)           { v4 r; V4_LANES(r.s[i_] = a.s[i_] + b.s[i_]); return r; }
SCENE_FFT_INLINE v4   V4_SUB(v4 a, v4 b)           { v4 r; V4_LANES(r.s[i_] = a.s[i_] - b.s[i_]); return r; }
SCENE_FFT_INLINE v4   V4_MUL(v4 a, v4 b)           { v4 r; V4_LANES(r.s[i_] = a.s[i_] * b.s[i_]); return r; }
SCENE_FFT_INLINE v4   V4_MLA(v4 a, v4 b, v4 c)     { v4 r; V4_LANES(r.s[i_] = a.s[i_] + b.s[i_] * c.s[i_]); return r; }
SCENE_FFT_INLINE v4   V4_MIN(v4 a, v4 b)           { v4 r; V4_LANES(r.s[i_] = a.s[i_] < b.s[i_] ? a.s[i_] : b.s[i_]); return r; }
SCENE_FFT_INLINE v4   V4_MAX(v4 a, v4 b)           { v4 r; V4_LANES(r.s[i_] = a.s[i_] > b.s[i_] ? a.s[i_] : b.s[i_]); return r; }
SCENE_FFT_INLINE v4   V4_EXT1(v4 a, v4 b)          { v4 r; V4_LANES(r.s[i_] = i_ < 3 ? a.s[i_ + 1] : b.s[i_ - 3]); return r; }
SCENE_FFT_INLINE v4   V4_EXT2(v4 a, v4 b)          { v4 r; V4_LANES(r.s[i_] = i_ < 2 ? a.s[i_ + 2] : b.s[i_ - 2]); return r; }
SCENE_FFT_INLINE v4   V4_EXT3(v4 a, v4 b)          { v4 r; V4_LANES(r.s[i_] = i_ < 1 ? a.s[i_ + 3] : b.s[i_ - 1]); return r; }
SCENE_FFT_INLINE v4   V4_REV(v4 a)                 { v4 r; V4_LANES(r.s[i_] = a.s[3 - i_]); return r; }
SCENE_FFT_INLINE v4   V4_MLA_N(v4 a, v4 b, float s){ v4 r; V4_LANES(r.s[i_] = a.s[i_] + b.s[i_] * s); return r; }
#define V4_MLA4W(a0, a1, a2, a3, x, w) do { const float *w_ = (w); v4 x_ = (x);           \
        a0 = V4_MLA_N(a0, x_, w_[0]); a1 = V4_MLA_N(a1, x_, w_[1]);                       \
        a2 = V4_MLA_N(a2, x_, w_[2]); a3 = V4_MLA_N(a3, x_, w_[3]); } while (0)
SCENE_FFT_INLINE void V4_LD2(const float *p, v4 *a, v4 *b)          { V4_LANES(a->s[i_] = p[2 * i_]; b->s[i_] = p[2 * i_ + 1]); }
SCENE_FFT_INLINE void V4_ST2(float *p, v4 a, v4 b)                  { V4_LANES(p[2 * i_] = a.s[i_]; p[2 * i_ + 1] = b.s[i_]); }
SCENE_FFT_INLINE void V4_LD4(const float *p, v4 *a, v4 *b, v4 *c, v4 *d)
{
    V4_LANES(a->s[i_] = p[4 * i_]; b->s[i_] = p[4 * i_ + 1]; c->s[i_] = p[4 * i_ + 2]; d->s[i_] = p[4 * i_ + 3]);
}
SCENE_FFT_INLINE v4 V4_EXPONENT(v4 x)
{
    v4 r;
    V4_LANES({ union { float f; uint32_t u; } b; b.f = x.s[i_]; r.s[i_] = (float)((int)(b.u >> 23) - 127); });
    return r;
}
SCENE_FFT_INLINE v4 V4_MANTISSA(v4 x)
{
    v4 r;
    V4_LANES({ union { float f; uint32_t u; } b; b.f = x.s[i_]; b.u = (b.u & 0x007fffffu) | 0x3f800000u; r.s[i_] = b.f; });
    return r;
}
SCENE_FFT_INLINE v4 V4_DIV(v4 a, v4 b)  { v4 r; V4_LANES(r.s[i_] = a.s[i_] / b.s[i_]); return r; }
SCENE_FFT_INLINE v4 V4_SQRT(v4 a)       { v4 r; V4_LANES(r.s[i_] = sqrtf(a.s[i_])); return r; }
#endif

/* eight output rows: the eight floats at w */
#define V4_MLA8W(a0, a1, a2, a3, a4, a5, a6, a7, x, w) do { v4 xx_ = (x); const float *ww_ = (w); \
        V4_MLA4W(a0, a1, a2, a3, xx_, ww_); V4_MLA4W(a4, a5, a6, a7, xx_, ww_ + 4); } while (0)

/* tanh: x (135135 + 17325 x^2 + 378 x^4 + x^6) / (135135 + 62370 x^2 + 3150 x^4 + 28 x^6), |x| clamped to
 * 4.97 (hssr_mask.c spa_mask_tanh_fast / spa_mask_tanh4): |err| < 1e-4, |result| < 1. */
SCENE_FFT_INLINE v4 v4_tanh(v4 x)
{
    v4 x2, p, q;
    x = V4_MIN(V4_MAX(x, V4_DUP(-SMASK_TANH_LIMIT)), V4_DUP(SMASK_TANH_LIMIT));
    x2 = V4_MUL(x, x);
    p = V4_ADD(V4_DUP(378.0f), x2);
    p = V4_MLA(V4_DUP(17325.0f), x2, p);
    p = V4_MLA(V4_DUP(135135.0f), x2, p);
    p = V4_MUL(x, p);
    q = V4_MLA(V4_DUP(3150.0f), x2, V4_DUP(28.0f));
    q = V4_MLA(V4_DUP(62370.0f), x2, q);
    q = V4_MLA(V4_DUP(135135.0f), x2, q);
    return V4_DIV(p, q);
}

/* sigmoid(x) = (1 + tanh(x / 2)) / 2 */
SCENE_FFT_INLINE v4 v4_sigmoid(v4 x)
{
    return V4_MLA(V4_DUP(0.5f), v4_tanh(V4_MUL(x, V4_DUP(0.5f))), V4_DUP(0.5f));
}

/* ln x for a positive normal x = m 2^e: e ln 2 + 2 t (1 + t^2/3 + t^4/5 + t^6/7 + t^8/9), t = (m-1)/(m+1) in
 * [0, 1/3) (hssr_mask.c spa_mask_log_fast without its sqrt-2 fold): |err| < 1.1e-6. */
SCENE_FFT_INLINE v4 v4_log(v4 x)
{
    const v4 one = V4_DUP(1.0f);
    v4 e = V4_EXPONENT(x), m = V4_MANTISSA(x);
    v4 t = V4_DIV(V4_SUB(m, one), V4_ADD(m, one)), t2 = V4_MUL(t, t), p;
    p = V4_MLA(V4_DUP(0.142857143f), t2, V4_DUP(0.111111111f));
    p = V4_MLA(V4_DUP(0.2f), t2, p);
    p = V4_MLA(V4_DUP(0.333333333f), t2, p);
    p = V4_MLA(one, t2, p);
    return V4_MLA(V4_MUL(e, V4_DUP(0.693147181f)), V4_MUL(t, V4_DUP(2.0f)), p);
}

/* ----------------------------------------------------------------------------- tables
 * The 2048-point FFT plan (scene_fft.h: Stockham radix-4 + one radix-2 stage, NEON, no bit reversal) and
 * torch.hann_window(2048) (periodic). Built by scene_mask_init(). */
static struct scene_fft_plan fft_plan;
static float fft_tw_re[SMASK_NFFT], fft_tw_im[SMASK_NFFT];
static float hann[SMASK_NFFT] SMASK_ALIGN16;
static int   tables_ready;

static void tables(void)
{
    scene_fft_plan_init(&fft_plan, SMASK_NFFT, fft_tw_re, fft_tw_im);   /* 2048 = 2 * 4^5: cannot fail */
    for (int n = 0; n < SMASK_NFFT; n++)
        hann[n] = 0.5f - 0.5f * cosf((float)n * (2.0f * SMASK_PI / (float)SMASK_NFFT));
    tables_ready = 1;
}

/* ----------------------------------------------------------------------------- API */
size_t scene_mask_size(void) { return sizeof(struct scene_mask); }

void scene_mask_reset(struct scene_mask *m)
{
    memset(m->hist, 0, sizeof m->hist);
    memset(m->h, 0, sizeof m->h);
    m->calls = 0;
}

int scene_mask_init(struct scene_mask *m, const struct scene_mask_weights *w)
{
    int c, i, k, g, l, n;
    if (memcmp(w->magic, SMASK_MAGIC, 8) != 0 || w->ch != SMASK_CH || w->hidden != SMASK_HID || w->n_out != SMASK_OUT)
        return -1;
    if (!tables_ready) tables();
    memset(m, 0, sizeof *m);
    for (c = 0; c < SMASK_CH; c++) {
        for (i = 0; i < SMASK_FEAT; i++)
            for (k = 0; k < SMASK_K1; k++) m->c1_wt[c / 4][i * SMASK_K1 + k][c % 4] = w->c1_w[c][i][k];
        for (i = 0; i < SMASK_CH; i++)
            for (k = 0; k < SMASK_K2; k++) {
                m->c2_wt[c / 8][i * SMASK_K2 + k][c % 8] = w->c2_w[c][i][k];
                m->c3_wt[c / 8][i * SMASK_K2 + k][c % 8] = w->c3_w[c][i][k];
            }
    }
    memcpy(m->c1_b, w->c1_b, sizeof m->c1_b);
    memcpy(m->c2_b, w->c2_b, sizeof m->c2_b);
    memcpy(m->c3_b, w->c3_b, sizeof m->c3_b);
    for (g = 0; g < 3 * SMASK_HID; g++) {
        for (i = 0; i < SMASK_CH; i++)  m->gru_wt[g / 4][i][g % 4] = w->gru_w_ih[g][i];
        for (l = 0; l < SMASK_HID; l++) m->gru_ut[g / 4][l][g % 4] = w->gru_w_hh[g][l];
        m->gru_b[g] = g < 2 * SMASK_HID ? w->gru_b_ih[g] + w->gru_b_hh[g] : w->gru_b_ih[g];
    }
    for (l = 0; l < SMASK_HID; l++) m->gru_bn[l] = w->gru_b_hh[2 * SMASK_HID + l];
    for (n = 0; n < SMASK_OUT; n++) {
        for (i = 0; i < SMASK_CH + SMASK_HID; i++) m->head_wt[i][n] = w->head_w[n][i];
        m->head_b[n] = w->head_b[n];
        m->elig_w[n] = w->elig_w[n];
    }
    for (i = 0; i < SMASK_FEAT; i++) {
        m->feat_mean[i] = w->feat_mean[i];
        m->feat_istd[i] = 1.0f / w->feat_std[i];
    }
    m->has_calib = w->has_calib; m->calib_a = w->calib_a; m->calib_b = w->calib_b;
    return 0;
}

const float *scene_mask_debug(struct scene_mask *m, int what)
{
    float *d = m->fft[0];                         /* dead after the control: 8192 floats >= 24 * 257 */
    const float *src = NULL;
    int rows = 0, stride = 0, width = SMASK_BINS, r;
    switch (what) {
    case SMASK_DBG_L:      return &m->L[0].re;
    case SMASK_DBG_R:      return &m->R[0].re;
    case SMASK_DBG_ELIG:   return m->elig;
    case SMASK_DBG_PROT:   return m->prot;
    case SMASK_DBG_FEAT:   src = &m->feat[0][SMASK_PAD]; rows = SMASK_FEAT; stride = SMASK_FROW; width = SMASK_FBINS; break;
    case SMASK_DBG_H1:     src = &m->h1[0][SMASK_PAD];   rows = SMASK_CH;   stride = SMASK_ROW;  break;
    case SMASK_DBG_H2:     src = &m->h2[0][SMASK_PAD];   rows = SMASK_CH;   stride = SMASK_ROW;  break;
    case SMASK_DBG_H3:     src = &m->h3[0][SMASK_PAD];   rows = SMASK_CH;   stride = SMASK_ROW;  break;
    case SMASK_DBG_GRU:    src = &m->h[0][SMASK_PAD];    rows = SMASK_HID;  stride = SMASK_ROW;  break;
    case SMASK_DBG_LOGITS: src = &m->logits[0][0];       rows = SMASK_OUT;  stride = SMASK_OROW; break;
    case SMASK_DBG_POST:   src = &m->post[0][0];         rows = SMASK_OUT;  stride = SMASK_OROW; break;
    default:               return NULL;
    }
    for (r = 0; r < rows; r++) memcpy(d + r * width, src + r * stride, sizeof(float) * (size_t)width);
    return d;
}

/* ----------------------------------------------------------------------------- stages */
/* (1) fine analysis: one complex FFT carries both channels (L in re, R in im); the windowed input goes to
 * fft[0] (consumed by the first stage, which writes fft[1]), the six stages land back in fft[0]; then
 * L = (Z[k] + conj Z[N-k]) / 2, R = (Z[k] - conj Z[N-k]) / 2i for k = 0..1024 (Z[N] = Z[0]). */
SMASK_STAGE void smask_analysis(struct scene_mask *m)
{
    float *zr = m->fft[0], *zi = m->fft[0] + SMASK_NFFT;
    int n, k;
    for (n = 0; n < SMASK_NFFT; n += 4) {
        v4 l, r, w = V4_LD(hann + n);
        V4_LD2(&m->hist[n][0], &l, &r);
        V4_ST(zr + n, V4_MUL(w, l));
        V4_ST(zi + n, V4_MUL(w, r));
    }
    scene_fft_forward(&fft_plan, zr, zi, zr, zi, m->fft[1]);
    {
        const float ar = zr[0], ai = zi[0], br = zr[0], bi = -zi[0];      /* bin 0 mirrors itself */
        m->L[0].re = 0.5f * (ar + br); m->L[0].im = 0.5f * (ai + bi);
        m->R[0].re = 0.5f * (ai - bi); m->R[0].im = -0.5f * (ar - br);
    }
    for (k = 1; k < SMASK_FBINS; k += 4) {                               /* k .. k+3 against N-k-3 .. N-k reversed */
        const v4 half = V4_DUP(0.5f), mhalf = V4_DUP(-0.5f);
        v4 ar = V4_LD(zr + k), ai = V4_LD(zi + k);
        v4 br = V4_REV(V4_LD(zr + SMASK_NFFT - k - 3)), zb = V4_REV(V4_LD(zi + SMASK_NFFT - k - 3));
        V4_ST2(&m->L[k].re, V4_MUL(half, V4_ADD(ar, br)), V4_MUL(half, V4_SUB(ai, zb)));
        V4_ST2(&m->R[k].re, V4_MUL(half, V4_ADD(ai, zb)), V4_MUL(mhalf, V4_SUB(ar, br)));
    }
}

/* (2) features() per fine bin, (3) standardize() and clamp +-6, stored feature-major with the row layout of
 * scene_mask.h for conv1 (fine bin f at column SMASK_PAD + f; the 1025th bin is the overlapping last vector). */
SMASK_STAGE void smask_features(const struct scene_mask *m, float feat[SMASK_FEAT][SMASK_FROW])
{
    const v4 eps7 = V4_DUP(1e-7f), eps9 = V4_DUP(1e-9f), six = V4_DUP(6.0f), msix = V4_DUP(-6.0f);
    int j, i;
    for (j = 0; j < SMASK_FBINS; j += 4) {
        v4 lr, li, rr, ri, pl, pr, mag, f[SMASK_FEAT];
        if (j + 4 > SMASK_FBINS) j = SMASK_FBINS - 4;
        V4_LD2(&m->L[j].re, &lr, &li);
        V4_LD2(&m->R[j].re, &rr, &ri);
        pl = V4_MLA(V4_MUL(lr, lr), li, li);
        pr = V4_MLA(V4_MUL(rr, rr), ri, ri);
        mag = V4_ADD(V4_SQRT(V4_MUL(pl, pr)), eps9);
        f[0] = v4_log(V4_ADD(pl, eps7));
        f[1] = v4_log(V4_ADD(pr, eps7));
        f[2] = V4_DIV(V4_MLA(V4_MUL(lr, rr), li, ri), mag);              /* Re L conj(R) / |L||R| */
        f[3] = V4_DIV(V4_SUB(V4_MUL(li, rr), V4_MUL(lr, ri)), mag);      /* Im L conj(R) / |L||R| */
        for (i = 0; i < SMASK_FEAT; i++) {
            v4 v = V4_MUL(V4_SUB(f[i], V4_DUP(m->feat_mean[i])), V4_DUP(m->feat_istd[i]));
            V4_ST(&feat[i][SMASK_PAD + j], V4_MIN(V4_MAX(v, msix), six));
        }
    }
}

/* (4) conv1: Conv1d(4, ch, k 8, stride 4, pad 4) + tanh; 1025 -> 257 outputs exactly (the [:257] is a no-op).
 * Output o reads fine bins 4o-4 .. 4o+3 = columns 4o .. 4o+7: a stride-4 load of 16 columns gives the taps
 * 0..3 of outputs o .. o+3, the next one the taps 4..7. Tiles of 4 channels x 4 bins. */
SMASK_STAGE void smask_conv1(const struct scene_mask *m, float out[SMASK_CH][SMASK_ROW])
{
    int c, o, i;
    for (c = 0; c < SMASK_CH; c += 4) {
        for (o = 0; o < SMASK_BINS; o += 4) {
            const float *w = &m->c1_wt[c / 4][0][0];
            v4 a0, a1, a2, a3;
            if (o + 4 > SMASK_BINS) o = SMASK_BINS - 4;
            a0 = V4_DUP(m->c1_b[c]); a1 = V4_DUP(m->c1_b[c + 1]); a2 = V4_DUP(m->c1_b[c + 2]); a3 = V4_DUP(m->c1_b[c + 3]);
            for (i = 0; i < SMASK_FEAT; i++, w += SMASK_K1 * 4) {
                const float *x = &m->feat[i][4 * o];
                v4 x0, x1, x2, x3;
                V4_LD4(x, &x0, &x1, &x2, &x3);
                V4_MLA4W(a0, a1, a2, a3, x0, w);
                V4_MLA4W(a0, a1, a2, a3, x1, w + 4);
                V4_MLA4W(a0, a1, a2, a3, x2, w + 8);
                V4_MLA4W(a0, a1, a2, a3, x3, w + 12);
                V4_LD4(x + 4, &x0, &x1, &x2, &x3);
                V4_MLA4W(a0, a1, a2, a3, x0, w + 16);
                V4_MLA4W(a0, a1, a2, a3, x1, w + 20);
                V4_MLA4W(a0, a1, a2, a3, x2, w + 24);
                V4_MLA4W(a0, a1, a2, a3, x3, w + 28);
            }
            V4_ST(&out[c][SMASK_PAD + o], v4_tanh(a0));
            V4_ST(&out[c + 1][SMASK_PAD + o], v4_tanh(a1));
            V4_ST(&out[c + 2][SMASK_PAD + o], v4_tanh(a2));
            V4_ST(&out[c + 3][SMASK_PAD + o], v4_tanh(a3));
        }
    }
}

/* Conv1d(ch, ch, k 5, pad 2) + residual, tanh: out[c][o] = tanh(b[c] + sum_{i,k} w[c][i][k] in[i][o+k-2] + in[c][o]).
 * Tiles of 8 channels x 4 bins; the five tap vectors of an input row come out of three aligned loads
 * (x = columns o-4 .. o+7, the zero pad columns included) by lane extraction. */
#define SMASK_CONV5_ROW(a0, a1, a2, a3, a4, a5, a6, a7, x, w) do {                                  \
        const float *xr_ = (x), *wr_ = (w);                                                        \
        const v4 v0 = V4_LD(xr_), v1 = V4_LD(xr_ + 4), v2 = V4_LD(xr_ + 8);                        \
        V4_MLA8W(a0, a1, a2, a3, a4, a5, a6, a7, V4_EXT2(v0, v1), wr_);        /* o-2 .. o+1 */   \
        V4_MLA8W(a0, a1, a2, a3, a4, a5, a6, a7, V4_EXT3(v0, v1), wr_ + 8);    /* o-1 .. o+2 */   \
        V4_MLA8W(a0, a1, a2, a3, a4, a5, a6, a7, v1,              wr_ + 16);   /* o   .. o+3 */   \
        V4_MLA8W(a0, a1, a2, a3, a4, a5, a6, a7, V4_EXT1(v1, v2), wr_ + 24);   /* o+1 .. o+4 */   \
        V4_MLA8W(a0, a1, a2, a3, a4, a5, a6, a7, V4_EXT2(v1, v2), wr_ + 32);   /* o+2 .. o+5 */   \
    } while (0)
SMASK_STAGE void smask_conv5(const float wt[SMASK_CH / 8][SMASK_CH * SMASK_K2][8], const float *b,
                             const float in[SMASK_CH][SMASK_ROW], float out[SMASK_CH][SMASK_ROW])
{
    int c, o, i;
    for (c = 0; c < SMASK_CH; c += 8) {
        for (o = 0; o < SMASK_BINS; o += 4) {
            const float *w = &wt[c / 8][0][0];
            v4 a0, a1, a2, a3, a4, a5, a6, a7;
            if (o + 4 > SMASK_BINS) o = SMASK_BINS - 4;
            a0 = V4_DUP(b[c]);     a1 = V4_DUP(b[c + 1]); a2 = V4_DUP(b[c + 2]); a3 = V4_DUP(b[c + 3]);
            a4 = V4_DUP(b[c + 4]); a5 = V4_DUP(b[c + 5]); a6 = V4_DUP(b[c + 6]); a7 = V4_DUP(b[c + 7]);
            for (i = 0; i < SMASK_CH; i += 2, w += 2 * SMASK_K2 * 8) {           /* two input rows per step */
                SMASK_CONV5_ROW(a0, a1, a2, a3, a4, a5, a6, a7, &in[i][SMASK_PAD + o - 4], w);
                SMASK_CONV5_ROW(a0, a1, a2, a3, a4, a5, a6, a7, &in[i + 1][SMASK_PAD + o - 4], w + SMASK_K2 * 8);
            }
            V4_ST(&out[c][SMASK_PAD + o],     v4_tanh(V4_ADD(a0, V4_LD(&in[c][SMASK_PAD + o]))));
            V4_ST(&out[c + 1][SMASK_PAD + o], v4_tanh(V4_ADD(a1, V4_LD(&in[c + 1][SMASK_PAD + o]))));
            V4_ST(&out[c + 2][SMASK_PAD + o], v4_tanh(V4_ADD(a2, V4_LD(&in[c + 2][SMASK_PAD + o]))));
            V4_ST(&out[c + 3][SMASK_PAD + o], v4_tanh(V4_ADD(a3, V4_LD(&in[c + 3][SMASK_PAD + o]))));
            V4_ST(&out[c + 4][SMASK_PAD + o], v4_tanh(V4_ADD(a4, V4_LD(&in[c + 4][SMASK_PAD + o]))));
            V4_ST(&out[c + 5][SMASK_PAD + o], v4_tanh(V4_ADD(a5, V4_LD(&in[c + 5][SMASK_PAD + o]))));
            V4_ST(&out[c + 6][SMASK_PAD + o], v4_tanh(V4_ADD(a6, V4_LD(&in[c + 6][SMASK_PAD + o]))));
            V4_ST(&out[c + 7][SMASK_PAD + o], v4_tanh(V4_ADD(a7, V4_LD(&in[c + 7][SMASK_PAD + o]))));
        }
    }
}

/* four gates (or outputs) of eight bins (two vectors: col and col + 4): b + W x over the given rows
 * (a multiple of four: unrolled by hand, -O2 does not) */
#define SMASK_DOT8_ROW(a0, a1, a2, a3, b0, b1, b2, b3, xp, wp) do {                                \
        V4_MLA4W(a0, a1, a2, a3, V4_LD(xp), wp);                                                   \
        V4_MLA4W(b0, b1, b2, b3, V4_LD((xp) + 4), wp); } while (0)
#define SMASK_DOT8(a0, a1, a2, a3, b0, b1, b2, b3, rows, src, col, wt) do {                        \
        int i_;                                                                                    \
        for (i_ = 0; i_ < (rows); i_ += 4) {                                                       \
            SMASK_DOT8_ROW(a0, a1, a2, a3, b0, b1, b2, b3, &(src)[i_][col],     &(wt)[i_][0]);     \
            SMASK_DOT8_ROW(a0, a1, a2, a3, b0, b1, b2, b3, &(src)[i_ + 1][col], &(wt)[i_ + 1][0]); \
            SMASK_DOT8_ROW(a0, a1, a2, a3, b0, b1, b2, b3, &(src)[i_ + 2][col], &(wt)[i_ + 2][0]); \
            SMASK_DOT8_ROW(a0, a1, a2, a3, b0, b1, b2, b3, &(src)[i_ + 3][col], &(wt)[i_ + 3][0]); \
        } } while (0)

/* (5) the per-bin GRU over controls (torch.nn.GRU, gates r, z, n), eight bins per step:
 *   r = s(Wir x + Whr h + bir + bhr), z = s(Wiz x + Whz h + biz + bhz),
 *   n = tanh(Win x + bin + r (Whn h + bhn)), h' = (1 - z) n + z h = n + z (h - n). */
SMASK_STAGE void smask_gru(struct scene_mask *m)
{
    int o, t, j;
    for (o = 0; o < SMASK_BINS; o += 8) {
        const int col = SMASK_PAD + o;
        v4 ga[3 * SMASK_HID], gb[3 * SMASK_HID], na[SMASK_HID], nb[SMASK_HID];
        for (t = 0; t < 3 * SMASK_HID; t += 4) {
            v4 a0 = V4_DUP(m->gru_b[t]), a1 = V4_DUP(m->gru_b[t + 1]), a2 = V4_DUP(m->gru_b[t + 2]), a3 = V4_DUP(m->gru_b[t + 3]);
            v4 b0 = a0, b1 = a1, b2 = a2, b3 = a3;
            SMASK_DOT8(a0, a1, a2, a3, b0, b1, b2, b3, SMASK_CH, m->h3, col, m->gru_wt[t / 4]);
            if (t < 2 * SMASK_HID)
                SMASK_DOT8(a0, a1, a2, a3, b0, b1, b2, b3, SMASK_HID, m->h, col, m->gru_ut[t / 4]);
            ga[t] = a0; ga[t + 1] = a1; ga[t + 2] = a2; ga[t + 3] = a3;
            gb[t] = b0; gb[t + 1] = b1; gb[t + 2] = b2; gb[t + 3] = b3;
        }
        for (t = 0; t < SMASK_HID; t += 4) {
            v4 a0 = V4_DUP(m->gru_bn[t]), a1 = V4_DUP(m->gru_bn[t + 1]), a2 = V4_DUP(m->gru_bn[t + 2]), a3 = V4_DUP(m->gru_bn[t + 3]);
            v4 b0 = a0, b1 = a1, b2 = a2, b3 = a3;
            SMASK_DOT8(a0, a1, a2, a3, b0, b1, b2, b3, SMASK_HID, m->h, col, m->gru_ut[(2 * SMASK_HID + t) / 4]);
            na[t] = a0; na[t + 1] = a1; na[t + 2] = a2; na[t + 3] = a3;
            nb[t] = b0; nb[t + 1] = b1; nb[t + 2] = b2; nb[t + 3] = b3;
        }
        for (j = 0; j < SMASK_HID; j++) {
            v4 r = v4_sigmoid(ga[j]), z = v4_sigmoid(ga[SMASK_HID + j]);
            v4 n = v4_tanh(V4_MLA(ga[2 * SMASK_HID + j], r, na[j]));
            v4 h = V4_LD(&m->h[j][col]);
            V4_ST(&m->h[j][col], V4_MLA(n, z, V4_SUB(h, n)));
            r = v4_sigmoid(gb[j]); z = v4_sigmoid(gb[SMASK_HID + j]);
            n = v4_tanh(V4_MLA(gb[2 * SMASK_HID + j], r, nb[j]));
            h = V4_LD(&m->h[j][col + 4]);
            V4_ST(&m->h[j][col + 4], V4_MLA(n, z, V4_SUB(h, n)));
        }
    }
}

/* (6) head: Linear(ch + hidden, n_out) on cat(conv features, GRU output); out_act = sigmoid (softmax_out nets
 * are refused by the exporter). (7) rear_eligibility(): eligible posteriors summed, clamped to 1; calib;
 * protect = 1 - elig. */
SCENE_FFT_INLINE void smask_head_rows(struct scene_mask *m, int o, v4 a0, v4 a1, v4 a2, v4 a3)
{
    const v4 one = V4_DUP(1.0f), zero = V4_DUP(0.0f);
    v4 a[SMASK_OUT], e = zero;
    int n;
    a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3;
    for (n = 0; n < SMASK_OUT; n++) {
        v4 p = v4_sigmoid(a[n]);
        V4_ST(&m->logits[n][o], a[n]);
        V4_ST(&m->post[n][o], p);
        e = V4_MLA(e, V4_DUP(m->elig_w[n]), p);
    }
    e = V4_MIN(e, one);
    if (m->has_calib != 0.0f) {
        v4 c = V4_MIN(V4_MAX(e, V4_DUP(1e-4f)), V4_DUP(1.0f - 1e-4f));
        e = v4_sigmoid(V4_MLA(V4_DUP(m->calib_b), V4_DUP(m->calib_a), v4_log(V4_DIV(c, V4_SUB(one, c)))));
    }
    e = V4_MAX(e, zero);
    V4_ST(&m->elig[o], e);
    V4_ST(&m->prot[o], V4_SUB(one, e));
}

SMASK_STAGE void smask_head(struct scene_mask *m)
{
    int o;
    for (o = 0; o < SMASK_BINS; o += 8) {
        const int col = SMASK_PAD + o;
        v4 a0 = V4_DUP(m->head_b[0]), a1 = V4_DUP(m->head_b[1]), a2 = V4_DUP(m->head_b[2]), a3 = V4_DUP(m->head_b[3]);
        v4 b0 = a0, b1 = a1, b2 = a2, b3 = a3;
        SMASK_DOT8(a0, a1, a2, a3, b0, b1, b2, b3, SMASK_CH, m->h3, col, m->head_wt);
        SMASK_DOT8(a0, a1, a2, a3, b0, b1, b2, b3, SMASK_HID, m->h, col, m->head_wt + SMASK_CH);
        smask_head_rows(m, o, a0, a1, a2, a3);
        smask_head_rows(m, o + 4, b0, b1, b2, b3);
    }
}

int scene_mask_control(struct scene_mask *m, const float *in512, float *elig257, float *prot257)
{
    /* history: the newest 2048 samples (ponytail: a shift of 12 KB per control, not a ring) */
    memmove(m->hist[0], m->hist[SMASK_HOP], sizeof(float) * 2 * (SMASK_NFFT - SMASK_HOP));
    memcpy(m->hist[SMASK_NFFT - SMASK_HOP], in512, sizeof(float) * 2 * SMASK_HOP);
    m->calls++;
    if (m->calls == 2) {                     /* x[0..1023] known: torch's reflect pad, x[-i] = x[i] */
        for (int i = 1; i <= SMASK_HOP; i++) {
            m->hist[SMASK_NFFT / 2 - i][0] = m->hist[SMASK_NFFT / 2 + i][0];
            m->hist[SMASK_NFFT / 2 - i][1] = m->hist[SMASK_NFFT / 2 + i][1];
        }
    }
    if (m->calls < 3) return 0;              /* control 0 = frame 1 = x[-512, 1536) needs three calls */

    smask_analysis(m);
    smask_features(m, m->feat);
    smask_conv1(m, m->h1);
    /* Explicit const casts: ISO C before C2X does not convert float (*)[N] to const float (*)[N]
     * implicitly, which gcc -pedantic -Werror (host tests) rejects. No code change. */
    smask_conv5((const float (*)[SMASK_CH * SMASK_K2][8])m->c2_wt, m->c2_b,
                (const float (*)[SMASK_ROW])m->h1, m->h2);
    smask_conv5((const float (*)[SMASK_CH * SMASK_K2][8])m->c3_wt, m->c3_b,
                (const float (*)[SMASK_ROW])m->h2, m->h3);
    smask_gru(m);
    smask_head(m);
    if (elig257) memcpy(elig257, m->elig, sizeof(float) * SMASK_BINS);
    if (prot257) memcpy(prot257, m->prot, sizeof(float) * SMASK_BINS);
    return 1;
}
