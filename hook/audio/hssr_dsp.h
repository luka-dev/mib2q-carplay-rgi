/*
 * Streaming spectral DSP around the quantized HSSR semantic controller.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef HSSR_DSP_H
#define HSSR_DSP_H

#include <stddef.h>
#include <stdint.h>

#include "hssr_neon.h"

enum {
    SPA_HSSR_FFT_SIZE = 512,
    SPA_HSSR_HOP_SIZE = 128,
    SPA_HSSR_BINS = 257,
    SPA_HSSR_BANDS = 32,
    SPA_HSSR_SHAPE_BANDS = 64,
    SPA_HSSR_OBJECT_SLOTS = 2,
    SPA_HSSR_CONTROL_HOPS = 4,
    SPA_HSSR_LATENCY_FRAMES = 512
};

struct spa_hssr_object_slot {
    /* Positive pan is left.  Universal keeps one persistent coherent object
     * per rear half-plane, the identifiable case for two-channel input. */
    float pan;
    float confidence;
    float band_weight[SPA_HSSR_BANDS];
    uint16_t hold_hops;
};

struct spa_hssr_dsp {
    float analysis_window[SPA_HSSR_FFT_SIZE];
    float synthesis_window[SPA_HSSR_FFT_SIZE];
    float twiddle_re[SPA_HSSR_FFT_SIZE / 2];
    float twiddle_im[SPA_HSSR_FFT_SIZE / 2];
    float input[2][SPA_HSSR_FFT_SIZE];
    int16_t front_delay[2][SPA_HSSR_FFT_SIZE];
    float ola[2][SPA_HSSR_FFT_SIZE];
    float fft_re[2][SPA_HSSR_FFT_SIZE];
    float fft_im[2][SPA_HSSR_FFT_SIZE];
    float previous_magnitude[SPA_HSSR_BINS];
    float bin_mask[SPA_HSSR_BINS];
    float bin_ambience_mask[SPA_HSSR_BINS];
    float bin_universal_mask[SPA_HSSR_BINS];
    float bin_object_pan[SPA_HSSR_BINS];
    float band_slow_energy[SPA_HSSR_BANDS];
    float band_mask[SPA_HSSR_BANDS];
    float band_pan[SPA_HSSR_BANDS];
    float band_spatiality[SPA_HSSR_BANDS];
    float band_diffuse[SPA_HSSR_BANDS];
    float band_object_boost[SPA_HSSR_BANDS];
    float band_focus_rescue[SPA_HSSR_BANDS];
    float band_universal_safety[SPA_HSSR_BANDS];
    struct spa_hssr_object_slot object_slot[SPA_HSSR_OBJECT_SLOTS];
    float scene_confidence;
    float scene_stability;
    float rear_activity_target;
    float fx_present;
    float universal_input_power;
    float universal_rear_power;
    float universal_makeup;
    float previous_log_total;
    float current_log_delta;
    float features[SPA_HSSR_FEATURES];
    float control[SPA_HSSR_OUTPUTS];
    uint8_t bin_band[SPA_HSSR_BINS];
    uint16_t input_position;
    uint16_t hop_fill;
    uint8_t control_hop;
    uint32_t frames_seen;
    uint32_t inference_failures;
    struct spa_hssr_state network;
};

void spa_hssr_dsp_prepare(struct spa_hssr_dsp *state);
void spa_hssr_dsp_reset(struct spa_hssr_dsp *state);

/* One input frame in, one delayed front/rear frame out.  The returned fronts
 * are exact delayed S16 input samples.  Rear is normalized float. */
int spa_hssr_dsp_process_sample(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    int16_t input_l, int16_t input_r,
    int16_t *front_l, int16_t *front_r,
    float *rear_l, float *rear_r);

#endif /* HSSR_DSP_H */
