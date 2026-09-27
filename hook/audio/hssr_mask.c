/*
 * The full_probe mask net runtime (see hssr_mask.h).  Mirrors FullNet._logits
 * in tools/stereo-upmix-render/mask_net.py operation for operation.  Parity:
 * tools/stereo-upmix-render/mask_infer_test.c on the host against the Python
 * reference, `stereo_upmix_bench --mask-selftest` host vs QEMU for the NEON
 * path (tools/stereo-upmix-render/bench/README.md).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "hssr_mask.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SPA_MASK_HAVE_NEON 1
#else
#define SPA_MASK_HAVE_NEON 0
#endif

#define SPA_MASK_MAGIC "HSSRMSK1"
#define SPA_MASK_VALID UINT32_C(0x4d534b31)
#define SPA_MASK_TANH_LIMIT 4.97f

static const int spa_mask_cin[3] = { SPA_MASK_CH, SPA_MASK_C1, SPA_MASK_C2 };
static const int spa_mask_cout[3] = { SPA_MASK_C1, SPA_MASK_C2, SPA_MASK_C3 };
static const int spa_mask_nin[3] = { SPA_MASK_BINS, SPA_MASK_N1, SPA_MASK_N2 };
static const int spa_mask_nout[3] = { SPA_MASK_N1, SPA_MASK_N2, SPA_MASK_N3 };

static float spa_mask_clamp6(float v)
{
    return v < -6.0f ? -6.0f : (v > 6.0f ? 6.0f : v);
}

static float spa_mask_sigmoid(float v)
{
    return 1.0f / (1.0f + spa_mask_exp_fast(-v));
}

/* log(e^a + e^b) */
static float spa_mask_logaddexp(float a, float b)
{
    float hi = a > b ? a : b, lo = a > b ? b : a;
    return hi + spa_mask_log_fast(1.0f + spa_mask_exp_fast(lo - hi));
}

/* tanh for the decoder / conv nonlinearities: a clamped rational form,
 * |err| < 2e-3 over the range.  The NEON spa_mask_tanh4 is the same formula. */
static float spa_mask_tanh_fast(float x)
{
    float x2;
    if (x > SPA_MASK_TANH_LIMIT) x = SPA_MASK_TANH_LIMIT;
    if (x < -SPA_MASK_TANH_LIMIT) x = -SPA_MASK_TANH_LIMIT;
    x2 = x * x;
    return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2))) /
           (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
}

/* round half away from zero, clamped to [-127, 127] */
static int8_t spa_mask_q8(float value, float multiplier)
{
    float scaled = value * multiplier;
    if (scaled >= 127.0f) return 127;
    if (scaled <= -127.0f) return -127;
    return (int8_t)(scaled >= 0.0f ? (int)(scaled + 0.5f) : (int)(scaled - 0.5f));
}

#if SPA_MASK_HAVE_NEON
static inline float32x4_t spa_mask_tanh4(float32x4_t x)
{
    const float32x4_t lim = vdupq_n_f32(SPA_MASK_TANH_LIMIT);
    float32x4_t x2, p, q, r;
    x = vminq_f32(vmaxq_f32(x, vnegq_f32(lim)), lim);
    x2 = vmulq_f32(x, x);
    p = vaddq_f32(vdupq_n_f32(378.0f), x2);
    p = vmlaq_f32(vdupq_n_f32(17325.0f), x2, p);
    p = vmlaq_f32(vdupq_n_f32(135135.0f), x2, p);
    p = vmulq_f32(x, p);
    q = vmlaq_f32(vdupq_n_f32(3150.0f), x2, vdupq_n_f32(28.0f));
    q = vmlaq_f32(vdupq_n_f32(62370.0f), x2, q);
    q = vmlaq_f32(vdupq_n_f32(135135.0f), x2, q);
    r = vrecpeq_f32(q);
    r = vmulq_f32(r, vrecpsq_f32(q, r));
    r = vmulq_f32(r, vrecpsq_f32(q, r));
    return vmulq_f32(p, r);
}

/* value * mult rounded half away from zero, as spa_mask_q8 */
static inline int32x4_t spa_mask_round4(float32x4_t v, float32x4_t mult)
{
    float32x4_t s = vmulq_f32(v, mult);
    uint32x4_t sign = vandq_u32(vreinterpretq_u32_f32(s), vdupq_n_u32(0x80000000u));
    float32x4_t half = vreinterpretq_f32_u32(vorrq_u32(sign, vdupq_n_u32(0x3f000000u)));
    return vcvtq_s32_f32(vaddq_f32(s, half));
}

/* 16 floats -> int8 in [-127, 127], bit-identical to spa_mask_q8 */
static inline int8x16_t spa_mask_q8x16(float32x4_t v0, float32x4_t v1, float32x4_t v2,
                                       float32x4_t v3, float32x4_t mult)
{
    int16x8_t lo = vcombine_s16(vqmovn_s32(spa_mask_round4(v0, mult)),
                                vqmovn_s32(spa_mask_round4(v1, mult)));
    int16x8_t hi = vcombine_s16(vqmovn_s32(spa_mask_round4(v2, mult)),
                                vqmovn_s32(spa_mask_round4(v3, mult)));
    return vmaxq_s8(vcombine_s8(vqmovn_s16(lo), vqmovn_s16(hi)), vdupq_n_s8(-127));
}

static inline float spa_mask_hsum(float32x4_t v)
{
    float32x2_t s = vadd_f32(vget_low_f32(v), vget_high_f32(v));
    return vget_lane_f32(vpadd_f32(s, s), 0);
}

/* int8 dot of one weight row against `chunks` 16-lane input chunks; the four
 * int32 lanes still need adding (spa_mask_hsum4_s32).  Two products fit an
 * int16 lane (127 * 127 * 2 < 32768), so vmlal before the widening add. */
static inline int32x4_t spa_mask_dot_s8_row(const int8_t *w, const int8x16_t *x, int chunks)
{
    int32x4_t acc = vdupq_n_s32(0);
    int c;
    for (c = 0; c < chunks; ++c) {
        int8x16_t wv = vld1q_s8(w + 16 * c);
        int16x8_t p = vmull_s8(vget_low_s8(wv), vget_low_s8(x[c]));
        p = vmlal_s8(p, vget_high_s8(wv), vget_high_s8(x[c]));
        acc = vpadalq_s16(acc, p);
    }
    return acc;
}

static inline int32x4_t spa_mask_hsum4_s32(int32x4_t a0, int32x4_t a1, int32x4_t a2, int32x4_t a3)
{
    int32x2_t s0 = vadd_s32(vget_low_s32(a0), vget_high_s32(a0));
    int32x2_t s1 = vadd_s32(vget_low_s32(a1), vget_high_s32(a1));
    int32x2_t s2 = vadd_s32(vget_low_s32(a2), vget_high_s32(a2));
    int32x2_t s3 = vadd_s32(vget_low_s32(a3), vget_high_s32(a3));
    return vcombine_s32(vpadd_s32(s0, s1), vpadd_s32(s2, s3));
}

/* one int8 decoder layer, `rows` (a multiple of 16) rows of `chunks` chunks:
 * out[r] = q8(tanh(dot[r] * scale[r] + bias[r]), 127), packed 16 per chunk */
static void spa_mask_layer_s8(const int8_t *wq, int stride, const float *scale, const float *bias,
                              int rows, const int8x16_t *x, int chunks, int8x16_t *out)
{
    int r;
    for (r = 0; r < rows; r += 16) {
        float32x4_t f[4];
        int g;
        for (g = 0; g < 4; ++g) {
            const int8_t *w = wq + (size_t)(r + 4 * g) * stride;
            int32x4_t d = spa_mask_hsum4_s32(spa_mask_dot_s8_row(w, x, chunks),
                                             spa_mask_dot_s8_row(w + stride, x, chunks),
                                             spa_mask_dot_s8_row(w + 2 * stride, x, chunks),
                                             spa_mask_dot_s8_row(w + 3 * stride, x, chunks));
            f[g] = spa_mask_tanh4(vmlaq_f32(vld1q_f32(bias + r + 4 * g), vcvtq_f32_s32(d),
                                            vld1q_f32(scale + r + 4 * g)));
        }
        out[r / 16] = spa_mask_q8x16(f[0], f[1], f[2], f[3], vdupq_n_f32(127.0f));
    }
}
#else
static int32_t spa_mask_dot_s8(const int8_t *a, const int8_t *b, int count)
{
    int32_t sum = 0;
    int at;
    for (at = 0; at < count; ++at) sum += (int32_t)a[at] * (int32_t)b[at];
    return sum;
}
#endif

/* The blob is a flat float array; hand out consecutive views in the export order. */
static const float *spa_mask_take(const float **cursor, size_t count)
{
    const float *p = *cursor;
    *cursor += count;
    return p;
}

static size_t spa_mask_assign(struct spa_mask_model *m, const float *base)
{
    const float *c = base;
    int i;
    m->feat_mean = spa_mask_take(&c, SPA_MASK_FEATURES);
    m->feat_std = spa_mask_take(&c, SPA_MASK_FEATURES);
    m->desc_mean = spa_mask_take(&c, SPA_MASK_DESC);
    m->desc_std = spa_mask_take(&c, SPA_MASK_DESC);
    m->stem_w = spa_mask_take(&c, (size_t)SPA_MASK_CH * SPA_MASK_STEM_IN * SPA_MASK_KERNEL);
    m->stem_b = spa_mask_take(&c, SPA_MASK_CH);
    for (i = 0; i < 3; ++i) {
        m->dw_w[i] = spa_mask_take(&c, (size_t)spa_mask_cin[i] * SPA_MASK_KERNEL);
        m->dw_b[i] = spa_mask_take(&c, (size_t)spa_mask_cin[i]);
        m->pw_w[i] = spa_mask_take(&c, (size_t)spa_mask_cout[i] * spa_mask_cin[i]);
        m->pw_b[i] = spa_mask_take(&c, (size_t)spa_mask_cout[i]);
    }
    m->flat_w = spa_mask_take(&c, (size_t)SPA_MASK_HIDDEN * SPA_MASK_C3 * SPA_MASK_N3);
    m->flat_b = spa_mask_take(&c, SPA_MASK_HIDDEN);
    m->gru_wih = spa_mask_take(&c, (size_t)SPA_MASK_GATES * SPA_MASK_HIDDEN);
    m->gru_whh = spa_mask_take(&c, (size_t)SPA_MASK_GATES * SPA_MASK_HIDDEN);
    m->gru_bih = spa_mask_take(&c, SPA_MASK_GATES);
    m->gru_bhh = spa_mask_take(&c, SPA_MASK_GATES);
    m->gproj_w = spa_mask_take(&c, (size_t)SPA_MASK_CH * SPA_MASK_HIDDEN);
    m->gproj_b = spa_mask_take(&c, SPA_MASK_CH);
    m->lproj_w = spa_mask_take(&c, (size_t)SPA_MASK_CH * SPA_MASK_C3);
    m->lproj_b = spa_mask_take(&c, SPA_MASK_CH);
    m->pres_w = spa_mask_take(&c, (size_t)SPA_MASK_BANDS * SPA_MASK_HIDDEN);
    m->pres_b = spa_mask_take(&c, SPA_MASK_BANDS);
    m->dec1_w = spa_mask_take(&c, (size_t)SPA_MASK_DEC * SPA_MASK_DEC_IN);
    m->dec1_b = spa_mask_take(&c, SPA_MASK_DEC);
    m->dec2_w = spa_mask_take(&c, (size_t)SPA_MASK_DEC * SPA_MASK_DEC);
    m->dec2_b = spa_mask_take(&c, SPA_MASK_DEC);
    m->dec3_w = spa_mask_take(&c, (size_t)SPA_MASK_ROLES * SPA_MASK_DEC);
    m->dec3_b = spa_mask_take(&c, SPA_MASK_ROLES);
    for (i = 0; i < SPA_MASK_FEATURES; ++i) m->feat_inv[i] = 1.0f / m->feat_std[i];
    for (i = 0; i < SPA_MASK_DESC; ++i) m->desc_inv[i] = 1.0f / m->desc_std[i];
    return (size_t)(c - base);
}

/* Symmetric per-row int8 weights: w ~ wq * ws[row]. */
static int8_t *spa_mask_quantize_rows(const float *w, int rows, int cols, int pad, float *ws)
{
    int8_t *q = (int8_t *)calloc((size_t)rows * pad, 1);
    int r, c;
    if (!q) return NULL;
    for (r = 0; r < rows; ++r) {
        float mx = 0.0f, mult;
        for (c = 0; c < cols; ++c) if (fabsf(w[r * cols + c]) > mx) mx = fabsf(w[r * cols + c]);
        ws[r] = mx > 0.0f ? mx / 127.0f : 1.0f;
        mult = 1.0f / ws[r];
        for (c = 0; c < cols; ++c) q[(size_t)r * pad + c] = spa_mask_q8(w[r * cols + c], mult);
    }
    return q;
}

static int spa_mask_quantize_decoder(struct spa_mask_model *m)
{
    int k;
    m->dec1_wq = spa_mask_quantize_rows(m->dec1_w, SPA_MASK_DEC, SPA_MASK_DEC_IN, SPA_MASK_DEC_PAD, m->dec1_ws);
    m->dec2_wq = spa_mask_quantize_rows(m->dec2_w, SPA_MASK_DEC, SPA_MASK_DEC, SPA_MASK_DEC, m->dec2_ws);
    m->dec3_wq = spa_mask_quantize_rows(m->dec3_w, SPA_MASK_ROLES, SPA_MASK_DEC, SPA_MASK_DEC, m->dec3_ws);
    /* layers 2 and 3 read tanh outputs quantised at a fixed 127 */
    for (k = 0; k < SPA_MASK_DEC; ++k) m->dec2_ws[k] *= 1.0f / 127.0f;
    for (k = 0; k < SPA_MASK_ROLES; ++k) m->dec3_ws[k] *= 1.0f / 127.0f;
    return (m->dec1_wq && m->dec2_wq && m->dec3_wq) ? 0 : -1;
}

int spa_mask_model_load_file(struct spa_mask_model *model, const char *path)
{
    FILE *f;
    unsigned char head[12];
    uint32_t count;
    float *data;
    size_t need;

    if (!model || !path) return -1;
    memset(model, 0, sizeof(*model));
    f = fopen(path, "rb");
    if (!f) return -2;
    if (fread(head, 1, sizeof(head), f) != sizeof(head) || memcmp(head, SPA_MASK_MAGIC, 8) != 0) {
        fclose(f);
        return -3;
    }
    count = (uint32_t)head[8] | ((uint32_t)head[9] << 8) | ((uint32_t)head[10] << 16) | ((uint32_t)head[11] << 24);
    data = (float *)malloc((size_t)count * sizeof(float));
    if (!data) {
        fclose(f);
        return -4;
    }
    if (fread(data, sizeof(float), count, f) != count) {
        free(data);
        fclose(f);
        return -3;
    }
    fclose(f);
    need = spa_mask_assign(model, data);
    if (need != count) {
        memset(model, 0, sizeof(*model));
        free(data);
        return -3;
    }
    model->_allocation = data;
    model->float_count = count;
    if (spa_mask_quantize_decoder(model) != 0) {
        free(data);
        memset(model, 0, sizeof(*model));
        return -4;
    }
    model->valid = SPA_MASK_VALID;
    return 0;
}

void spa_mask_model_unload(struct spa_mask_model *model)
{
    if (!model) return;
    free(model->dec1_wq); free(model->dec2_wq); free(model->dec3_wq);
    free(model->_allocation);
    memset(model, 0, sizeof(*model));
}

int spa_mask_model_ready(const struct spa_mask_model *model)
{
    return model && model->valid == SPA_MASK_VALID;
}

void spa_mask_state_reset(struct spa_mask_state *state)
{
    if (state) memset(state, 0, sizeof(*state));
}

/* Copy `rows` rows of n floats into the zero-padded scratch (SPA_MASK_PAD
 * zeros on both sides), so no tap needs a bounds check; returns the stride.
 * The 4-wide loops below compute the last block of a row at n - 4, which
 * recomputes up to three outputs identically instead of needing a tail. */
static int spa_mask_pad_rows(const float *in, int rows, int n, float *pad)
{
    const int stride = n + 2 * SPA_MASK_PAD;
    int r;
    for (r = 0; r < rows; ++r) {
        float *p = pad + (size_t)r * stride;
        memset(p, 0, SPA_MASK_PAD * sizeof(float));
        memcpy(p + SPA_MASK_PAD, in + (size_t)r * n, (size_t)n * sizeof(float));
        memset(p + SPA_MASK_PAD + n, 0, SPA_MASK_PAD * sizeof(float));
    }
    return stride;
}

/* out[co][i] = tanh(b[co] + sum_ci sum_k w[co][ci][k] * in[ci][i + k - 2]) */
static void spa_mask_conv5(const float *w, const float *b, int cin, int cout, int n,
                           const float *in, float *out, float *pad)
{
    const int stride = spa_mask_pad_rows(in, cin, n, pad);
    int co, ci, i, k;
    for (co = 0; co < cout; ++co) {
        float *o = out + (size_t)co * n;
#if SPA_MASK_HAVE_NEON
        for (i = 0; i < n; i += 4) {
            float32x4_t acc;
            if (i + 4 > n) i = n - 4;
            acc = vdupq_n_f32(b[co]);
            for (ci = 0; ci < cin; ++ci) {
                const float *x = pad + (size_t)ci * stride + SPA_MASK_PAD + i - 2;
                const float *wk = w + ((size_t)co * cin + ci) * SPA_MASK_KERNEL;
                for (k = 0; k < SPA_MASK_KERNEL; ++k)
                    acc = vmlaq_n_f32(acc, vld1q_f32(x + k), wk[k]);
            }
            vst1q_f32(o + i, spa_mask_tanh4(acc));
        }
#else
        for (i = 0; i < n; ++i) {
            float acc = b[co];
            for (ci = 0; ci < cin; ++ci) {
                const float *x = pad + (size_t)ci * stride + SPA_MASK_PAD + i - 2;
                const float *wk = w + ((size_t)co * cin + ci) * SPA_MASK_KERNEL;
                for (k = 0; k < SPA_MASK_KERNEL; ++k) acc += wk[k] * x[k];
            }
            o[i] = spa_mask_tanh_fast(acc);
        }
#endif
    }
}

/* out[co][i] = act(b[co] + sum_c w[co][c] * in[c][i]) */
static void spa_mask_pointwise(const float *w, const float *b, int cin, int cout, int n,
                               const float *in, float *out, int activate)
{
    int co, c, i;
    for (co = 0; co < cout; ++co) {
        const float *wr = w + (size_t)co * cin;
        float *o = out + (size_t)co * n;
#if SPA_MASK_HAVE_NEON
        for (i = 0; i < n; i += 4) {
            float32x4_t acc;
            if (i + 4 > n) i = n - 4;
            acc = vdupq_n_f32(b[co]);
            for (c = 0; c < cin; ++c)
                acc = vmlaq_n_f32(acc, vld1q_f32(in + (size_t)c * n + i), wr[c]);
            vst1q_f32(o + i, activate ? spa_mask_tanh4(acc) : acc);
        }
#else
        for (i = 0; i < n; ++i) {
            float acc = b[co];
            for (c = 0; c < cin; ++c) acc += wr[c] * in[(size_t)c * n + i];
            o[i] = activate ? spa_mask_tanh_fast(acc) : acc;
        }
#endif
    }
}

/* depthwise k5 stride 2 pad 2 (no activation), then pointwise 1x1 + tanh */
static void spa_mask_down(const struct spa_mask_model *m, int stage,
                          const float *in, float *tmp, float *out, float *pad)
{
    const int cin = spa_mask_cin[stage], cout = spa_mask_cout[stage];
    const int nin = spa_mask_nin[stage], nout = spa_mask_nout[stage];
    const int stride = spa_mask_pad_rows(in, cin, nin, pad);
    int c, i;
    for (c = 0; c < cin; ++c) {
        const float *x = pad + (size_t)c * stride + SPA_MASK_PAD;   /* x[j] = in[c][j], zero outside */
        const float *wk = m->dw_w[stage] + (size_t)c * SPA_MASK_KERNEL;
        const float bias = m->dw_b[stage][c];
        float *t = tmp + (size_t)c * nout;
#if SPA_MASK_HAVE_NEON
        /* outputs i..i+3 read x[2i-2 .. 2i+8]: two de-interleaving loads give the
         * even taps (0, 2, 4) and the odd taps (1, 3) as shifted lane windows */
        for (i = 0; i < nout; i += 4) {
            float32x4x2_t lo, hi;
            float32x4_t acc;
            if (i + 4 > nout) i = nout - 4;
            lo = vld2q_f32(x + 2 * i - 2);
            hi = vld2q_f32(x + 2 * i + 6);
            acc = vdupq_n_f32(bias);
            acc = vmlaq_n_f32(acc, lo.val[0], wk[0]);
            acc = vmlaq_n_f32(acc, lo.val[1], wk[1]);
            acc = vmlaq_n_f32(acc, vextq_f32(lo.val[0], hi.val[0], 1), wk[2]);
            acc = vmlaq_n_f32(acc, vextq_f32(lo.val[1], hi.val[1], 1), wk[3]);
            acc = vmlaq_n_f32(acc, vextq_f32(lo.val[0], hi.val[0], 2), wk[4]);
            vst1q_f32(t + i, acc);
        }
#else
        for (i = 0; i < nout; ++i) {
            float acc = bias;
            int k;
            for (k = 0; k < SPA_MASK_KERNEL; ++k) acc += wk[k] * x[2 * i + k - 2];
            t[i] = acc;
        }
#endif
    }
    spa_mask_pointwise(m->pw_w[stage], m->pw_b[stage], cin, cout, nout, tmp, out, 1);
}

/* y = b + W x; rows and cols are multiples of 4 for every caller */
static void spa_mask_gemv(const float *w, const float *b, int rows, int cols,
                          const float *x, float *y)
{
    int r, i;
#if SPA_MASK_HAVE_NEON
    for (r = 0; r < rows; r += 4) {
        const float *w0 = w + (size_t)r * cols;
        float32x4_t a0 = vdupq_n_f32(0.0f), a1 = a0, a2 = a0, a3 = a0;
        for (i = 0; i < cols; i += 4) {
            float32x4_t xv = vld1q_f32(x + i);
            a0 = vmlaq_f32(a0, vld1q_f32(w0 + i), xv);
            a1 = vmlaq_f32(a1, vld1q_f32(w0 + cols + i), xv);
            a2 = vmlaq_f32(a2, vld1q_f32(w0 + 2 * cols + i), xv);
            a3 = vmlaq_f32(a3, vld1q_f32(w0 + 3 * cols + i), xv);
        }
        y[r] = b[r] + spa_mask_hsum(a0);
        y[r + 1] = b[r + 1] + spa_mask_hsum(a1);
        y[r + 2] = b[r + 2] + spa_mask_hsum(a2);
        y[r + 3] = b[r + 3] + spa_mask_hsum(a3);
    }
#else
    for (r = 0; r < rows; ++r) {
        const float *wr = w + (size_t)r * cols;
        float acc = 0.0f;
        for (i = 0; i < cols; ++i) acc += wr[i] * x[i];
        y[r] = b[r] + acc;
    }
#endif
}

int spa_mask_infer(const struct spa_mask_model *m, struct spa_mask_state *s,
                   const float *features, float *eligibility, float *presence)
{
    int i, c, k;
    float lp[SPA_MASK_BINS];
    float gin[SPA_MASK_HIDDEN];
    float hnew[SPA_MASK_HIDDEN];
    float dec1_scale[SPA_MASK_DEC];
    float zmax = 1.0e-6f, zscale, zmult;

    if (!spa_mask_model_ready(m) || !s || !features || !eligibility) return -1;

    /* 1. standardised features -> stem input channels; log(pl+pr) for the descriptor */
    for (i = 0; i < SPA_MASK_BINS; ++i) {
        const float *f = features + (size_t)i * SPA_MASK_FEATURES;
        float st[SPA_MASK_FEATURES];
        for (c = 0; c < SPA_MASK_FEATURES; ++c)
            st[c] = spa_mask_clamp6((f[c] - m->feat_mean[c]) * m->feat_inv[c]);
        s->x[0][i] = st[0] + st[1];
        s->x[1][i] = st[0] - st[1];
        s->x[2][i] = st[2];
        s->x[3][i] = st[3];
        s->x[4][i] = (float)i / (float)SPA_MASK_BINS * 2.0f - 1.0f;
        lp[i] = spa_mask_logaddexp(f[0], f[1]);
    }

    /* 2. the probe descriptor per bin (replicate-padded 5-bin neighbourhood) */
    for (i = 0; i < SPA_MASK_BINS; ++i) {
        float nb[5], mean = 0.0f, d[SPA_MASK_DESC];
        for (k = 0; k < 5; ++k) {
            int j = i + k - 2;
            if (j < 0) j = 0;
            if (j >= SPA_MASK_BINS) j = SPA_MASK_BINS - 1;
            nb[k] = lp[j];
            mean += nb[k];
        }
        mean *= 0.2f;
        for (k = 0; k < 5; ++k) d[k] = nb[k] - mean;
        d[5] = mean;
        d[6] = features[(size_t)i * SPA_MASK_FEATURES + 2];
        d[7] = features[(size_t)i * SPA_MASK_FEATURES + 3];
        d[8] = (float)i / (float)SPA_MASK_BINS;
        for (k = 0; k < SPA_MASK_DESC; ++k)
            s->desc[i][k] = spa_mask_clamp6((d[k] - m->desc_mean[k]) * m->desc_inv[k]);
    }

    /* 3. stem + three strided stages -> a [64][33] */
    spa_mask_conv5(m->stem_w, m->stem_b, SPA_MASK_STEM_IN, SPA_MASK_CH, SPA_MASK_BINS,
                   &s->x[0][0], &s->skip[0][0], s->pad);
    spa_mask_down(m, 0, &s->skip[0][0], s->b, s->a, s->pad);   /* -> a [32][129] */
    spa_mask_down(m, 1, s->a, s->b, s->a, s->pad);      /* -> a [48][65]  */
    spa_mask_down(m, 2, s->a, s->b, s->a, s->pad);      /* -> a [64][33]  */

    /* 4. flatten (channel-major = the a layout) -> 64, tanh; GRU over controls */
    spa_mask_gemv(m->flat_w, m->flat_b, SPA_MASK_HIDDEN, SPA_MASK_C3 * SPA_MASK_N3, s->a, gin);
    for (i = 0; i < SPA_MASK_HIDDEN; ++i) gin[i] = spa_mask_tanh_fast(gin[i]);
    spa_mask_gemv(m->gru_wih, m->gru_bih, SPA_MASK_GATES, SPA_MASK_HIDDEN, gin, s->gates);
    spa_mask_gemv(m->gru_whh, m->gru_bhh, SPA_MASK_GATES, SPA_MASK_HIDDEN, s->hidden, s->gates_h);
    for (i = 0; i < SPA_MASK_HIDDEN; ++i) {
        float r = spa_mask_sigmoid(s->gates[i] + s->gates_h[i]);
        float z = spa_mask_sigmoid(s->gates[SPA_MASK_HIDDEN + i] + s->gates_h[SPA_MASK_HIDDEN + i]);
        float n = spa_mask_tanh_fast(s->gates[2 * SPA_MASK_HIDDEN + i] + r * s->gates_h[2 * SPA_MASK_HIDDEN + i]);
        hnew[i] = (1.0f - z) * n + z * s->hidden[i];
    }
    memcpy(s->hidden, hnew, sizeof(hnew));

    /* 5. heads off the state: global context, band presence */
    spa_mask_gemv(m->gproj_w, m->gproj_b, SPA_MASK_CH, SPA_MASK_HIDDEN, s->hidden, s->g);
    if (presence) {
        spa_mask_gemv(m->pres_w, m->pres_b, SPA_MASK_BANDS, SPA_MASK_HIDDEN, s->hidden, presence);
        for (i = 0; i < SPA_MASK_BANDS; ++i) presence[i] = spa_mask_sigmoid(presence[i]);
    }

    /* 6. local context: 1x1 over the 33-map (interpolated to the bins below) */
    spa_mask_pointwise(m->lproj_w, m->lproj_b, SPA_MASK_C3, SPA_MASK_CH, SPA_MASK_N3,
                       s->a, &s->loc[0][0], 0);

    /* 7. per-cell decoder, int8: gather every cell's 57 inputs, one dynamic
     * scale per control for layer 1, tanh outputs at a fixed scale after. */
    for (i = 0; i < SPA_MASK_BINS; ++i) {
        float *z = s->zf[i];
        float src = (float)i * (float)(SPA_MASK_N3 - 1) / (float)(SPA_MASK_BINS - 1);   /* align_corners */
        int j0 = (int)src;
        float fr = src - (float)j0;
        int j1 = j0 + 1 < SPA_MASK_N3 ? j0 + 1 : j0;
        for (c = 0; c < SPA_MASK_CH; ++c) {
            z[c] = s->skip[c][i];
            z[SPA_MASK_CH + c] = s->loc[c][j0] + fr * (s->loc[c][j1] - s->loc[c][j0]);
            z[2 * SPA_MASK_CH + c] = s->g[c];
        }
        for (k = 0; k < SPA_MASK_DESC; ++k) z[3 * SPA_MASK_CH + k] = s->desc[i][k];
#if SPA_MASK_HAVE_NEON
        {
            float32x4_t mx = vdupq_n_f32(0.0f);
            float32x2_t m2;
            for (k = 0; k < SPA_MASK_DEC_PAD; k += 4) mx = vmaxq_f32(mx, vabsq_f32(vld1q_f32(z + k)));
            m2 = vmax_f32(vget_low_f32(mx), vget_high_f32(mx));
            m2 = vpmax_f32(m2, m2);
            if (vget_lane_f32(m2, 0) > zmax) zmax = vget_lane_f32(m2, 0);
        }
#else
        for (k = 0; k < SPA_MASK_DEC_IN; ++k) if (fabsf(z[k]) > zmax) zmax = fabsf(z[k]);
#endif
    }
    zscale = zmax / 127.0f;
    zmult = 1.0f / zscale;
    for (k = 0; k < SPA_MASK_DEC; ++k) dec1_scale[k] = zscale * m->dec1_ws[k];
    for (i = 0; i < SPA_MASK_BINS; ++i) {
        float logit[SPA_MASK_ROLES], mx = -1e30f, sum = 0.0f;
#if SPA_MASK_HAVE_NEON
        const float *z = s->zf[i];
        int8x16_t zq[4], h1[2], h2[2];
        float32x4_t mult = vdupq_n_f32(zmult);
        int32x4_t d;
        int32x2_t d2;
        for (k = 0; k < 4; ++k)
            zq[k] = spa_mask_q8x16(vld1q_f32(z + 16 * k), vld1q_f32(z + 16 * k + 4),
                                   vld1q_f32(z + 16 * k + 8), vld1q_f32(z + 16 * k + 12), mult);
        spa_mask_layer_s8(m->dec1_wq, SPA_MASK_DEC_PAD, dec1_scale, m->dec1_b, SPA_MASK_DEC, zq, 4, h1);
        spa_mask_layer_s8(m->dec2_wq, SPA_MASK_DEC, m->dec2_ws, m->dec2_b, SPA_MASK_DEC, h1, 2, h2);
        d = spa_mask_hsum4_s32(spa_mask_dot_s8_row(m->dec3_wq, h2, 2),
                               spa_mask_dot_s8_row(m->dec3_wq + SPA_MASK_DEC, h2, 2),
                               spa_mask_dot_s8_row(m->dec3_wq + 2 * SPA_MASK_DEC, h2, 2),
                               spa_mask_dot_s8_row(m->dec3_wq + 3 * SPA_MASK_DEC, h2, 2));
        vst1q_f32(logit, vmlaq_f32(vld1q_f32(m->dec3_b), vcvtq_f32_s32(d), vld1q_f32(m->dec3_ws)));
        d = spa_mask_dot_s8_row(m->dec3_wq + 4 * SPA_MASK_DEC, h2, 2);
        d2 = vadd_s32(vget_low_s32(d), vget_high_s32(d));
        d2 = vpadd_s32(d2, d2);
        logit[4] = (float)vget_lane_s32(d2, 0) * m->dec3_ws[4] + m->dec3_b[4];
#else
        int8_t zq[SPA_MASK_DEC_PAD], h1q[SPA_MASK_DEC], h2q[SPA_MASK_DEC];
        for (k = 0; k < SPA_MASK_DEC_PAD; ++k) zq[k] = spa_mask_q8(s->zf[i][k], zmult);
        for (k = 0; k < SPA_MASK_DEC; ++k) {
            float v = (float)spa_mask_dot_s8(m->dec1_wq + (size_t)k * SPA_MASK_DEC_PAD, zq, SPA_MASK_DEC_PAD)
                      * dec1_scale[k] + m->dec1_b[k];
            h1q[k] = spa_mask_q8(spa_mask_tanh_fast(v), 127.0f);
        }
        for (k = 0; k < SPA_MASK_DEC; ++k) {
            float v = (float)spa_mask_dot_s8(m->dec2_wq + (size_t)k * SPA_MASK_DEC, h1q, SPA_MASK_DEC)
                      * m->dec2_ws[k] + m->dec2_b[k];
            h2q[k] = spa_mask_q8(spa_mask_tanh_fast(v), 127.0f);
        }
        for (k = 0; k < SPA_MASK_ROLES; ++k)
            logit[k] = (float)spa_mask_dot_s8(m->dec3_wq + (size_t)k * SPA_MASK_DEC, h2q, SPA_MASK_DEC)
                       * m->dec3_ws[k] + m->dec3_b[k];
#endif
        for (k = 0; k < SPA_MASK_ROLES; ++k) if (logit[k] > mx) mx = logit[k];
        for (k = 0; k < SPA_MASK_ROLES; ++k) sum += spa_mask_exp_fast(logit[k] - mx);
        eligibility[i] = spa_mask_exp_fast(logit[SPA_MASK_ROLE_OTHER] - mx) / sum;
    }
    s->inference_count++;
    return 0;
}
