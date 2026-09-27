/*
 * Per-cell rear-eligibility mask network ("full_probe", mask_net.py) —
 * float reference runtime.
 *
 * One control (4 hops) in: the 257-bin features of the control hop
 * [log|L|^2, log|R|^2, cos IPD, sin IPD]; out: rear eligibility per bin in
 * [0, 1] (the softmax probability of the "other" role) and the band vocal
 * presence.  Model loading allocates and reads a file; inference never
 * allocates, locks or logs.  Stateful (a GRU over controls): reset on every
 * stream start.
 *
 * Blob HSSRMSK1: 8-byte magic, u32 float count, then float32 little-endian
 * arrays in the order of spa_mask_assign() (mask_net.py export-blob).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef HSSR_MASK_H
#define HSSR_MASK_H

#include <stddef.h>
#include <stdint.h>

enum {
    SPA_MASK_BINS = 257,
    SPA_MASK_FEATURES = 4,
    SPA_MASK_STEM_IN = 5,            /* level, ILD, cos, sin, position */
    SPA_MASK_CH = 16,
    SPA_MASK_C1 = 32,
    SPA_MASK_C2 = 48,
    SPA_MASK_C3 = 64,
    SPA_MASK_N1 = 129,
    SPA_MASK_N2 = 65,
    SPA_MASK_N3 = 33,
    SPA_MASK_HIDDEN = 64,
    SPA_MASK_GATES = 3 * SPA_MASK_HIDDEN,
    SPA_MASK_BANDS = 32,
    SPA_MASK_DESC = 9,               /* 5 neighbours - mean, mean, cos, sin, position */
    SPA_MASK_DEC_IN = 3 * SPA_MASK_CH + SPA_MASK_DESC,
    SPA_MASK_DEC = 32,
    SPA_MASK_ROLES = 5,              /* vocals, drum_core, cymbals, bass, other */
    SPA_MASK_ROLE_OTHER = 4,
    SPA_MASK_KERNEL = 5,
    SPA_MASK_DEC_PAD = 64,           /* 57 decoder inputs padded to 64 int8 lanes */
    SPA_MASK_PAD = 8,                /* zero padding around every convolution input row */
    SPA_MASK_PAD_STRIDE = SPA_MASK_BINS + 2 * SPA_MASK_PAD
};

struct spa_mask_model {
    void *_allocation;
    size_t float_count;
    const float *feat_mean, *feat_std;          /* [4], [4] */
    const float *desc_mean, *desc_std;          /* [9], [9] */
    float feat_inv[SPA_MASK_FEATURES], desc_inv[SPA_MASK_DESC];   /* 1 / std */
    const float *stem_w, *stem_b;               /* [16][5][5], [16] */
    const float *dw_w[3], *dw_b[3];             /* [ci][5], [ci] */
    const float *pw_w[3], *pw_b[3];             /* [co][ci], [co] */
    const float *flat_w, *flat_b;               /* [64][64*33], [64] */
    const float *gru_wih, *gru_whh;             /* [192][64] rows r,z,n */
    const float *gru_bih, *gru_bhh;             /* [192], [192] */
    const float *gproj_w, *gproj_b;             /* [16][64], [16] */
    const float *lproj_w, *lproj_b;             /* [16][64], [16] */
    const float *pres_w, *pres_b;               /* [32][64], [32] */
    const float *dec1_w, *dec1_b;               /* [32][57], [32] */
    const float *dec2_w, *dec2_b;               /* [32][32], [32] */
    const float *dec3_w, *dec3_b;               /* [5][32], [5] */
    /* Phase 1b: the per-cell decoder as int8 (60 % of the net's MACs; a
     * [257 x 57] x [57 x 32] product per control).  Weights quantised per
     * output row at load time; layer-1 inputs quantised per control with one
     * dynamic scale, layers 2/3 inputs are tanh outputs (fixed scale 127).
     * Rows padded to SPA_MASK_DEC_PAD lanes for 16-wide int8 dots. */
    int8_t *dec1_wq, *dec2_wq, *dec3_wq;         /* [32][64], [32][32], [5][32] */
    /* output scales: dec1 per row (times the control's input scale at run
     * time), dec2/dec3 per row including the fixed 1/127 input scale */
    float dec1_ws[SPA_MASK_DEC], dec2_ws[SPA_MASK_DEC], dec3_ws[SPA_MASK_ROLES];
    uint32_t valid;
};

struct spa_mask_state {
    float hidden[SPA_MASK_HIDDEN];
    /* scratch (sized for the largest stage; never allocated at run time) */
    float x[SPA_MASK_STEM_IN][SPA_MASK_BINS];
    float skip[SPA_MASK_CH][SPA_MASK_BINS];
    /* flat: the strided stages index them as [channel * positions + position] with
     * a different stride per stage (129 / 65 / 33) */
    float a[SPA_MASK_C3 * SPA_MASK_N1];
    float b[SPA_MASK_C3 * SPA_MASK_N1];
    float loc[SPA_MASK_CH][SPA_MASK_N3];
    float g[SPA_MASK_CH];
    float desc[SPA_MASK_BINS][SPA_MASK_DESC];
    float gates[SPA_MASK_GATES];
    float gates_h[SPA_MASK_GATES];
    float pad[SPA_MASK_CH * SPA_MASK_PAD_STRIDE];  /* zero-padded copy of a stage's input rows */
    float zf[SPA_MASK_BINS][SPA_MASK_DEC_PAD];     /* the decoder inputs of one control; lanes 57.. stay 0 */
    uint32_t inference_count;
};

/* natural log of x > 0 (|abs err| ~1e-7): exponent + the atanh series of the
 * mantissa.  Shared with the feature extractor in hssr_dsp.c. */
static inline float spa_mask_log_fast(float x)
{
    union { float f; uint32_t u; } bits;
    float m, t, t2;
    int e;
    if (!(x > 1.17549435e-38f)) x = 1.17549435e-38f;
    bits.f = x;
    e = (int)((bits.u >> 23) & 0xffu) - 127;
    bits.u = (bits.u & 0x007fffffu) | 0x3f800000u;
    m = bits.f;
    if (m > 1.41421356f) { m *= 0.5f; ++e; }
    t = (m - 1.0f) / (m + 1.0f);
    t2 = t * t;
    return (float)e * 0.693147181f +
           2.0f * t * (1.0f + t2 * (0.333333333f + t2 * (0.2f + t2 * (0.142857143f + t2 * 0.111111111f))));
}

/* e^x for x in [-87, 88] (|rel err| < 2e-7): 2^n times a degree-6 series on [-1/2, 1/2] */
static inline float spa_mask_exp_fast(float x)
{
    union { float f; uint32_t u; } scale;
    float t, f, p;
    int n;
    if (x > 88.0f) x = 88.0f;
    if (x < -87.0f) x = -87.0f;
    t = x * 1.44269504f;
    n = (int)t;
    if (t < (float)n) --n;
    f = t - (float)n;
    if (f > 0.5f) { f -= 1.0f; ++n; }
    p = 1.0f + f * (0.693147181f + f * (0.240226507f + f * (0.0555041087f +
        f * (0.00961812911f + f * (0.00133335581f + f * 0.000154035304f)))));
    scale.u = (uint32_t)(n + 127) << 23;
    return p * scale.f;
}

int spa_mask_model_load_file(struct spa_mask_model *model, const char *path);
void spa_mask_model_unload(struct spa_mask_model *model);
int spa_mask_model_ready(const struct spa_mask_model *model);
void spa_mask_state_reset(struct spa_mask_state *state);

/* features: [257][4] of the control hop.  eligibility: [257] rear eligibility
 * (p_other); presence: [32] band vocal-presence probabilities (may be NULL). */
int spa_mask_infer(const struct spa_mask_model *model,
                   struct spa_mask_state *state,
                   const float *features,
                   float *eligibility,
                   float *presence);

#endif /* HSSR_MASK_H */
