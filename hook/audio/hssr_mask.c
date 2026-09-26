/*
 * Float reference of the full_probe mask net (see hssr_mask.h).  Mirrors
 * FullNet._logits in tools/stereo-upmix-render/mask_net.py operation for
 * operation; the parity test is tools/stereo-upmix-render/mask_infer_test.c.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "hssr_mask.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPA_MASK_MAGIC "HSSRMSK1"
#define SPA_MASK_VALID UINT32_C(0x4d534b31)

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
    return 1.0f / (1.0f + expf(-v));
}

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
    return (size_t)(c - base);
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
    model->valid = SPA_MASK_VALID;
    return 0;
}

void spa_mask_model_unload(struct spa_mask_model *model)
{
    if (!model) return;
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

/* out[co][n] = tanh(b + sum_ci sum_k w[co][ci][k] * in[ci][n + k - 2]), zero padded */
static void spa_mask_conv5(const float *w, const float *b, int cin, int cout, int n,
                           const float *in, float *out)
{
    int co, ci, i, k;
    for (co = 0; co < cout; ++co) {
        float *o = out + (size_t)co * n;
        for (i = 0; i < n; ++i) o[i] = b[co];
        for (ci = 0; ci < cin; ++ci) {
            const float *x = in + (size_t)ci * n;
            const float *wk = w + ((size_t)co * cin + ci) * SPA_MASK_KERNEL;
            for (i = 0; i < n; ++i) {
                float acc = 0.0f;
                for (k = 0; k < SPA_MASK_KERNEL; ++k) {
                    int j = i + k - 2;
                    if (j >= 0 && j < n) acc += wk[k] * x[j];
                }
                o[i] += acc;
            }
        }
        for (i = 0; i < n; ++i) o[i] = tanhf(o[i]);
    }
}

/* depthwise k5 stride 2 pad 2 (no activation), then pointwise 1x1 + tanh */
static void spa_mask_down(const struct spa_mask_model *m, int stage,
                          const float *in, float *tmp, float *out)
{
    const int cin = spa_mask_cin[stage], cout = spa_mask_cout[stage];
    const int nin = spa_mask_nin[stage], nout = spa_mask_nout[stage];
    int c, i, k, co;
    for (c = 0; c < cin; ++c) {
        const float *x = in + (size_t)c * nin;
        const float *wk = m->dw_w[stage] + (size_t)c * SPA_MASK_KERNEL;
        float *t = tmp + (size_t)c * nout;
        for (i = 0; i < nout; ++i) {
            float acc = m->dw_b[stage][c];
            for (k = 0; k < SPA_MASK_KERNEL; ++k) {
                int j = 2 * i + k - 2;
                if (j >= 0 && j < nin) acc += wk[k] * x[j];
            }
            t[i] = acc;
        }
    }
    for (co = 0; co < cout; ++co) {
        const float *wr = m->pw_w[stage] + (size_t)co * cin;
        float *o = out + (size_t)co * nout;
        for (i = 0; i < nout; ++i) {
            float acc = m->pw_b[stage][co];
            for (c = 0; c < cin; ++c) acc += wr[c] * tmp[(size_t)c * nout + i];
            o[i] = tanhf(acc);
        }
    }
}

static void spa_mask_gemv(const float *w, const float *b, int rows, int cols,
                          const float *x, float *y)
{
    int r, c;
    for (r = 0; r < rows; ++r) {
        const float *wr = w + (size_t)r * cols;
        float acc = b ? b[r] : 0.0f;
        for (c = 0; c < cols; ++c) acc += wr[c] * x[c];
        y[r] = acc;
    }
}

int spa_mask_infer(const struct spa_mask_model *m, struct spa_mask_state *s,
                   const float *features, float *eligibility, float *presence)
{
    int i, c, k;
    float lp[SPA_MASK_BINS];
    float flat_in[SPA_MASK_C3 * SPA_MASK_N3];
    float gin[SPA_MASK_HIDDEN];
    float hnew[SPA_MASK_HIDDEN];

    if (!spa_mask_model_ready(m) || !s || !features || !eligibility) return -1;

    /* 1. standardised features -> stem input channels; log(pl+pr) for the descriptor */
    for (i = 0; i < SPA_MASK_BINS; ++i) {
        const float *f = features + (size_t)i * SPA_MASK_FEATURES;
        float st[SPA_MASK_FEATURES];
        for (c = 0; c < SPA_MASK_FEATURES; ++c)
            st[c] = spa_mask_clamp6((f[c] - m->feat_mean[c]) / m->feat_std[c]);
        s->x[0][i] = st[0] + st[1];
        s->x[1][i] = st[0] - st[1];
        s->x[2][i] = st[2];
        s->x[3][i] = st[3];
        s->x[4][i] = (float)i / (float)SPA_MASK_BINS * 2.0f - 1.0f;
        lp[i] = logf(expf(f[0]) + expf(f[1]));
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
            s->desc[i][k] = spa_mask_clamp6((d[k] - m->desc_mean[k]) / m->desc_std[k]);
    }

    /* 3. stem + three strided stages */
    spa_mask_conv5(m->stem_w, m->stem_b, SPA_MASK_STEM_IN, SPA_MASK_CH, SPA_MASK_BINS,
                   &s->x[0][0], &s->skip[0][0]);
    spa_mask_down(m, 0, &s->skip[0][0], &s->b[0][0], &s->a[0][0]);   /* -> a [32][129] */
    spa_mask_down(m, 1, &s->a[0][0], &s->b[0][0], &s->a[0][0]);      /* -> a [48][65]  */
    spa_mask_down(m, 2, &s->a[0][0], &s->b[0][0], &s->a[0][0]);      /* -> a [64][33]  */

    /* 4. flatten (channel-major) -> 64, tanh; GRU over controls */
    for (c = 0; c < SPA_MASK_C3; ++c)
        for (i = 0; i < SPA_MASK_N3; ++i)
            flat_in[c * SPA_MASK_N3 + i] = s->a[0][(size_t)c * SPA_MASK_N3 + i];
    spa_mask_gemv(m->flat_w, m->flat_b, SPA_MASK_HIDDEN, SPA_MASK_C3 * SPA_MASK_N3, flat_in, gin);
    for (i = 0; i < SPA_MASK_HIDDEN; ++i) gin[i] = tanhf(gin[i]);
    spa_mask_gemv(m->gru_wih, m->gru_bih, SPA_MASK_GATES, SPA_MASK_HIDDEN, gin, s->gates);
    spa_mask_gemv(m->gru_whh, m->gru_bhh, SPA_MASK_GATES, SPA_MASK_HIDDEN, s->hidden, s->gates_h);
    for (i = 0; i < SPA_MASK_HIDDEN; ++i) {
        float r = spa_mask_sigmoid(s->gates[i] + s->gates_h[i]);
        float z = spa_mask_sigmoid(s->gates[SPA_MASK_HIDDEN + i] + s->gates_h[SPA_MASK_HIDDEN + i]);
        float n = tanhf(s->gates[2 * SPA_MASK_HIDDEN + i] + r * s->gates_h[2 * SPA_MASK_HIDDEN + i]);
        hnew[i] = (1.0f - z) * n + z * s->hidden[i];
    }
    memcpy(s->hidden, hnew, sizeof(hnew));

    /* 5. heads off the state: global context, band presence */
    spa_mask_gemv(m->gproj_w, m->gproj_b, SPA_MASK_CH, SPA_MASK_HIDDEN, s->hidden, s->g);
    if (presence) {
        spa_mask_gemv(m->pres_w, m->pres_b, SPA_MASK_BANDS, SPA_MASK_HIDDEN, s->hidden, presence);
        for (i = 0; i < SPA_MASK_BANDS; ++i) presence[i] = spa_mask_sigmoid(presence[i]);
    }

    /* 6. local context: 1x1 over the 33-map, then linear interpolation to the bins */
    for (c = 0; c < SPA_MASK_CH; ++c) {
        const float *wr = m->lproj_w + (size_t)c * SPA_MASK_C3;
        for (i = 0; i < SPA_MASK_N3; ++i) {
            float acc = m->lproj_b[c];
            for (k = 0; k < SPA_MASK_C3; ++k) acc += wr[k] * s->a[0][(size_t)k * SPA_MASK_N3 + i];
            s->loc[c][i] = acc;
        }
    }

    /* 7. per-cell decoder */
    for (i = 0; i < SPA_MASK_BINS; ++i) {
        float z[SPA_MASK_DEC_IN], h1[SPA_MASK_DEC], h2[SPA_MASK_DEC], logit[SPA_MASK_ROLES];
        float src = (float)i * (float)(SPA_MASK_N3 - 1) / (float)(SPA_MASK_BINS - 1);   /* align_corners */
        int j0 = (int)src;
        float fr = src - (float)j0;
        int j1 = j0 + 1 < SPA_MASK_N3 ? j0 + 1 : j0;
        float mx = -1e30f, sum = 0.0f;
        for (c = 0; c < SPA_MASK_CH; ++c) {
            z[c] = s->skip[c][i];
            z[SPA_MASK_CH + c] = s->loc[c][j0] + fr * (s->loc[c][j1] - s->loc[c][j0]);
            z[2 * SPA_MASK_CH + c] = s->g[c];
        }
        for (k = 0; k < SPA_MASK_DESC; ++k) z[3 * SPA_MASK_CH + k] = s->desc[i][k];
        spa_mask_gemv(m->dec1_w, m->dec1_b, SPA_MASK_DEC, SPA_MASK_DEC_IN, z, h1);
        for (k = 0; k < SPA_MASK_DEC; ++k) h1[k] = tanhf(h1[k]);
        spa_mask_gemv(m->dec2_w, m->dec2_b, SPA_MASK_DEC, SPA_MASK_DEC, h1, h2);
        for (k = 0; k < SPA_MASK_DEC; ++k) h2[k] = tanhf(h2[k]);
        spa_mask_gemv(m->dec3_w, m->dec3_b, SPA_MASK_ROLES, SPA_MASK_DEC, h2, logit);
        for (k = 0; k < SPA_MASK_ROLES; ++k) if (logit[k] > mx) mx = logit[k];
        for (k = 0; k < SPA_MASK_ROLES; ++k) sum += expf(logit[k] - mx);
        eligibility[i] = expf(logit[SPA_MASK_ROLE_OTHER] - mx) / sum;
    }
    s->inference_count++;
    return 0;
}
