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
    SPA_HSSR_OBJECT_SLOTS = 4,   /* two per rear half-plane: 0/2 left, 1/3 right */
    SPA_HSSR_CONTROL_HOPS = 4,
    SPA_HSSR_LATENCY_FRAMES = 512,
    /* Scene-bed decorrelation ring in hops (2.667 ms each); power of two. */
    SPA_HSSR_BED_RING = 16
};

/* Attack handling of the scene bed (docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md). */
enum spa_hssr_bed_attack {
    SPA_HSSR_BED_ATTACK_OFF = 0,
    SPA_HSSR_BED_ATTACK_OWNER = 1,  /* a hit collapses toward the band owner: object -> rear, bed -> front */
    SPA_HSSR_BED_ATTACK_FRONT = 2   /* Dolby reference: the bed always collapses forward */
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

/* Interleaved per-bin state; scene_lr is the smoothed (PL-PR)/(PL+PR). */
struct spa_hssr_bin_state {
    float scene_pl;
    float scene_pr;
    float scene_cross_re;
    float scene_cross_im;
    float scene_coh;
    float scene_lr;
    float mask;
    float ambience_mask;
    float universal_mask;
    float object_pan;
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
    /* synthesis_window * 1/N: the inverse transform no longer rescales. */
    float synthesis_window_scaled[SPA_HSSR_FFT_SIZE];
    float twiddle_re[SPA_HSSR_FFT_SIZE / 2];
    float twiddle_im[SPA_HSSR_FFT_SIZE / 2];
    /* Negated twiddles and a bit-reversal table: the inverse sign and the
     * permutation leave the hot butterfly/reversal loops. */
    float twiddle_im_inverse[SPA_HSSR_FFT_SIZE / 2];
    uint16_t bit_reverse[SPA_HSSR_FFT_SIZE];
    float input[2][SPA_HSSR_FFT_SIZE];
    int16_t front_delay[2][SPA_HSSR_FFT_SIZE];
    float ola[2][SPA_HSSR_FFT_SIZE];
    float fft_re[2][SPA_HSSR_FFT_SIZE];
    float fft_im[2][SPA_HSSR_FFT_SIZE];
    /* Source image (object before steering), subtracted from the dry front. */
    float source_fft_re[2][SPA_HSSR_FFT_SIZE];
    float source_fft_im[2][SPA_HSSR_FFT_SIZE];
    float source_ola[2][SPA_HSSR_FFT_SIZE];
    float source_sample[2];
    /* Scene bed (ambience + universal) distribution.  All zero keeps the
     * legacy renderer bit-exact; set by stereo_upmix init/reset only. */
    uint32_t bed_delays;
    uint32_t bed_attack;              /* enum spa_hssr_bed_attack */
    float attack_collapse;            /* OWNER: object front share r -> r*(1 - c*w) on a hit */
    float front_retention;            /* r, applied in the STFT domain when bed_attack != OFF */
    /* Front retention is a MID control: its measured benefit is 500-2000 Hz
     * front/centre body, while the treble it keeps in the front makes the car
     * front audibly brighter.  The retention rolls off from lo_hz and is zero
     * from hi_hz up, so treble objects still leave the front completely.
     * hi <= lo disables the taper (retention flat over frequency). */
    float retention_hf_lo_hz;
    float retention_hf_hi_hz;
    float bed_re[2][SPA_HSSR_BINS];
    float bed_im[2][SPA_HSSR_BINS];
    float bed_ring_re[2][SPA_HSSR_BED_RING][SPA_HSSR_BINS];
    float bed_ring_im[2][SPA_HSSR_BED_RING][SPA_HSSR_BINS];
    uint32_t bed_ring_pos;
    float band_attack_history[SPA_HSSR_BANDS];
    float band_attack_weight[SPA_HSSR_BANDS];
    float band_owner[SPA_HSSR_BANDS];
    /* Per hop: the whole front-object factor 1 - r(f)*(1 - c*w*owner).  Every
     * term is per band, so the 257-bin masking loop only loads it. 1 = OFF. */
    float band_front_keep[SPA_HSSR_BANDS];
    /* Research: per-bin smoothed statistics for continuous azimuth relocation. */
    float previous_magnitude[SPA_HSSR_BINS];
    /* Per-bin state of the two hot loops, interleaved: a bin's ten values sit
     * in one or two cache lines instead of ten separate 1 KB streams, which
     * matters on a 16 KB L1. Same arithmetic, so the output is bit-identical. */
    struct spa_hssr_bin_state bin[SPA_HSSR_BINS];
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
    /* Guard model (V11): its 'vocal' posterior is a safety net for the
     * MoisesDB-trained main model, which misses some processed leads. */
    const struct spa_hssr_model *guard_model;
    struct spa_hssr_state guard_network;
    float guard_control[SPA_HSSR_OUTPUTS];
    uint32_t guard_valid;
};

void spa_hssr_dsp_prepare(struct spa_hssr_dsp *state);
void spa_hssr_dsp_reset(struct spa_hssr_dsp *state);

#ifdef SPA_STEREO_UPMIX_TESTING
float spa_hssr_test_attack_step(float energy, float *history, float *weight);
void spa_hssr_test_mix_bed(struct spa_hssr_dsp *state,
                           float out_re[2][SPA_HSSR_BINS],
                           float out_im[2][SPA_HSSR_BINS]);
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
