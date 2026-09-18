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
#ifdef SPA_HSSR_SCENE_SLOTS4
    SPA_HSSR_OBJECT_SLOTS = 4,   /* two per rear half-plane: 0/2 left, 1/3 right */
#else
    SPA_HSSR_OBJECT_SLOTS = 2,
#endif
    SPA_HSSR_CONTROL_HOPS = 4,
    SPA_HSSR_LATENCY_FRAMES = 512
};

/* Optional off-audio-thread control-network provider.  Analysis, masks, FFT and
 * synthesis always remain in the calling audio thread.  A provider only owns
 * the recurrent GRU state and moves feature/control vectors through bounded,
 * non-blocking mailboxes. */
struct spa_hssr_control_provider {
    void *context;
    int (*submit)(void *context, uint32_t generation, uint32_t sequence,
                  const float features[SPA_HSSR_FEATURES]);
    int (*receive)(void *context, uint32_t generation,
                   uint32_t maximum_sequence,
                   float control[SPA_HSSR_OUTPUTS],
                   uint32_t *sequence);
    void (*reset)(void *context, uint32_t generation);
    /* Optional: copy the guard model's control block for the result just
     * returned by receive().  NULL when the provider runs no guard model. */
    int (*receive_guard)(void *context, float control[SPA_HSSR_OUTPUTS]);
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
    /* Opt-in cabin renderer: source image is separate from rear placement. */
    float source_fft_re[2][SPA_HSSR_FFT_SIZE];
    float source_fft_im[2][SPA_HSSR_FFT_SIZE];
    float source_ola[2][SPA_HSSR_FFT_SIZE];
    float source_sample[2];
    uint32_t cabin_policy;
#if defined(SPA_HSSR_SCENE_AZIMUTH) || defined(SPA_HSSR_SCENE_COH)
    /* Research: per-bin smoothed statistics for continuous azimuth relocation. */
    float bin_scene_pan[SPA_HSSR_BINS];
    float bin_scene_pl[SPA_HSSR_BINS];
    float bin_scene_pr[SPA_HSSR_BINS];
    float bin_scene_cross_re[SPA_HSSR_BINS];
    float bin_scene_cross_im[SPA_HSSR_BINS];
#endif
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
#ifdef SPA_HSSR_SCENE
    float bin_scene_lr[SPA_HSSR_BINS];   /* smoothed (PL-PR)/(PL+PR) per bin */
#endif
#ifdef SPA_HSSR_SCENE_DEBUG_SLOTS
    float dbg_mask_sum, dbg_mask_delta;
#endif
#ifdef SPA_HSSR_SCENE_COH
    float bin_scene_coh[SPA_HSSR_BINS];
#endif
    struct spa_hssr_object_slot object_slot[SPA_HSSR_OBJECT_SLOTS];
    float scene_confidence;
    float scene_stability;
    float rear_activity_target;
    float fx_present;
    float lfe_bass_confidence;
    float lfe_drum_confidence;
    float universal_input_power;
    float universal_rear_power;
    float universal_makeup;
    float previous_log_total;
    float current_log_delta;
    float features[SPA_HSSR_FEATURES];
    float control[SPA_HSSR_OUTPUTS];
    float lfe_semantic_delay[2][SPA_HSSR_FFT_SIZE];
    uint8_t bin_band[SPA_HSSR_BINS];
    uint16_t input_position;
    uint16_t hop_fill;
    uint8_t control_hop;
    uint32_t frames_seen;
    uint32_t inference_failures;
    uint32_t control_generation;
    uint32_t control_sequence;
    uint32_t applied_control_sequence;
    uint32_t control_deadlines_missed;
    struct spa_hssr_state network;
#ifdef SPA_HSSR_SCENE
    /* Guard model (V11): its 'vocal' posterior is a safety net for the
     * MoisesDB-trained main model, which misses some processed leads. */
    const struct spa_hssr_model *guard_model;
    struct spa_hssr_state guard_network;
    float guard_control[SPA_HSSR_OUTPUTS];
    uint32_t guard_valid;
#endif
};

void spa_hssr_dsp_prepare(struct spa_hssr_dsp *state);
void spa_hssr_dsp_reset(struct spa_hssr_dsp *state);

#ifdef SPA_HSSR_CAPTURE_TRANSFERS
/* Host-only diagnostic: real, row-major 2x2 matrices, rear then source.
 * Called once per positive-frequency bin, every hop, only in cabin mode.
 * Not enabled or linked into the vehicle hook. */
void spa_hssr_capture_transfer(unsigned bin, const float matrix[8]);
#endif

/* One input frame in, one delayed front/rear frame out.  The returned fronts
 * are exact delayed S16 input samples.  Rear is normalized float. */
int spa_hssr_dsp_process_sample(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    int16_t input_l, int16_t input_r,
    int16_t *front_l, int16_t *front_r,
    float *rear_l, float *rear_r);

/* Block entry point used by the production upmixer.  The four output arrays
 * contain one sample per input frame.  Work is advanced in chunks ending on
 * STFT hop boundaries; the controller is synchronous when provider is NULL. */
int spa_hssr_dsp_process_block(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    const struct spa_hssr_control_provider *provider,
    const int16_t *pcm2, size_t frames,
    int16_t *front2, float *rear2, float *lfe_control2);

int spa_hssr_dsp_process_block_sources(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    const struct spa_hssr_control_provider *provider,
    const int16_t *pcm2, size_t frames,
    int16_t *front2, float *rear2, float *lfe_control2, float *source2);

#endif /* HSSR_DSP_H */
