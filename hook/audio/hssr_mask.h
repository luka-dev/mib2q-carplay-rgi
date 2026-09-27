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
    SPA_MASK_KERNEL = 5
};

struct spa_mask_model {
    void *_allocation;
    size_t float_count;
    const float *feat_mean, *feat_std;          /* [4], [4] */
    const float *desc_mean, *desc_std;          /* [9], [9] */
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
    uint32_t inference_count;
};

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
