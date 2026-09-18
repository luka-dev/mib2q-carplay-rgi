/*
 * HSSR Universal stereo upmixer for the fixed MHI2Q PCM6 rail.
 *
 * The output order is always L, R, C, LFE, Ls, Rs.  C is deliberately zero
 * because the Audi amplifier already derives its centre. Universal can feed
 * the legacy phase-recovered LFE V2 or an opt-in quiet mono bass assist.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef STEREO_UPMIX_H
#define STEREO_UPMIX_H

#include <stddef.h>
#include <stdint.h>

#include "hssr_dsp.h"

enum {
    SPA_STEREO_UPMIX_INPUT_CHANNELS = 2,
    SPA_STEREO_UPMIX_OUTPUT_CHANNELS = 6,
    SPA_STEREO_UPMIX_MAX_FRAMES = 1024,
    SPA_STEREO_UPMIX_SAMPLE_RATE = 48000,

    /* Q15 gains.  32768 is unity and is intentionally representable here. */
    SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT = 32670,
    SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT = 31392,
    SPA_STEREO_UPMIX_REAR_Q15_DEFAULT = 5898,
#ifdef SPA_HSSR_SCENE
    /* Scene build: rear may run up to 0.5 (-6 dB) and is delayed 12 ms so the
     * front anchors the image (precedence) and the rear reads as a second scene. */
    SPA_STEREO_UPMIX_REAR_Q15_MAX = 16384,
    SPA_SCENE_REAR_DELAY_FRAMES = 576,
    /* The 5.1 bank feeds the subwoofer from PCM LFE only (L/R/C -> Bus_LFE
     * are -100 dB), so this assist is the whole sub: +9.5 dB over the
     * production 4096; the MMI Subwoofer knob sits on top.  Rails stay
     * protected per sample by spa_upmix_lfe_cabin_limit. */
    SPA_STEREO_UPMIX_LFE_Q15_DEFAULT = 12288,
    SPA_STEREO_UPMIX_LFE_Q15_MAX = 16384
#else
    SPA_STEREO_UPMIX_REAR_Q15_MAX = 8192,
    SPA_STEREO_UPMIX_LFE_Q15_DEFAULT = 4096,
    SPA_STEREO_UPMIX_LFE_Q15_MAX = 4096
#endif
};

enum spa_stereo_upmix_output_channel {
    SPA_STEREO_UPMIX_CHANNEL_L = 0,
    SPA_STEREO_UPMIX_CHANNEL_R = 1,
    SPA_STEREO_UPMIX_CHANNEL_C = 2,
    SPA_STEREO_UPMIX_CHANNEL_LFE = 3,
    SPA_STEREO_UPMIX_CHANNEL_LS = 4,
    SPA_STEREO_UPMIX_CHANNEL_RS = 5
};

/* OFF is a safety bypass, not an alternative upmix profile. */
enum spa_stereo_upmix_backend {
    SPA_STEREO_UPMIX_OFF = 0,
    SPA_STEREO_UPMIX_HSSR_UNIVERSAL = 1
};

enum spa_stereo_upmix_status {
    SPA_STEREO_UPMIX_OK = 0,
    SPA_STEREO_UPMIX_ERROR_ARGUMENT = -1,
    SPA_STEREO_UPMIX_ERROR_CONFIG = -2,
    SPA_STEREO_UPMIX_ERROR_UNSUPPORTED = -3,
    SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED = -4,
    SPA_STEREO_UPMIX_ERROR_BLOCK_TOO_LARGE = -5,
    SPA_STEREO_UPMIX_ERROR_OUTPUT_TOO_SMALL = -6,
    SPA_STEREO_UPMIX_ERROR_STATE_OVERLAP = -7
};

struct spa_stereo_upmix_config {
    uint32_t sample_rate_hz;
    uint32_t front_gain_q15;
    uint32_t rear_gain_q15;
    uint32_t lfe_gain_q15;
    uint32_t lfe_enabled;
    uint32_t cabin_policy;
    uint32_t quiet_lfe;
    enum spa_stereo_upmix_backend backend;
    const struct spa_hssr_model *hssr_model;
    const struct spa_hssr_control_provider *hssr_control_provider;
    const struct spa_hssr_model *hssr_guard_model;   /* scene builds: V11 vocal safety net, optional */
};

/* Runtime details are public only so callers can embed the complete state and
 * the realtime path never has to allocate.  Callers must not inspect or alter
 * fields whose names start with an underscore. */
struct spa_stereo_upmix_runtime {
    /* Shared 48-Hz LR4 filters. Legacy V2 adds phase recovery/dynamics;
     * quiet_lfe or cabin_policy uses a fixed half-level mono assist. */
    float _lfe_low48_state[2][2][2];
    float _lfe_power_l;
    float _lfe_power_r;
    float _lfe_cross;
    float _lfe_polarity;
    float _lfe_target_polarity;
    float _lfe_fast_envelope;
    float _lfe_slow_envelope;
    float _lfe_dynamic_gain;
    uint32_t _lfe_correlation_tick;
    /* Legacy +3 dB / 4-kHz rear shelf, bypassed by cabin_policy. */
    float _rear_high_shelf_state[2][2];
    float _limiter_gain;
#ifdef SPA_HSSR_SCENE
    float _scene_rear_delay[2][SPA_SCENE_REAR_DELAY_FRAMES];
    uint32_t _scene_rear_pos;
    float _scene_rear_shelf_state[2][2];
    float _scene_source_shelf_state[2][2];
#endif
    struct spa_hssr_dsp _hssr;
};

struct spa_stereo_upmix {
    struct spa_stereo_upmix_config config;
    uint32_t _initialized;
    struct spa_stereo_upmix_runtime _runtime;

    /* A complete 1024-frame stereo packet is staged before output starts.
     * This makes widening safe even when the PCM2 and PCM6 ranges overlap. */
    int16_t _input_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                           SPA_STEREO_UPMIX_INPUT_CHANNELS];
    int16_t _hssr_front_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                                SPA_STEREO_UPMIX_INPUT_CHANNELS];
    float _hssr_rear_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                             SPA_STEREO_UPMIX_INPUT_CHANNELS];
    float _hssr_lfe_control_scratch[SPA_STEREO_UPMIX_MAX_FRAMES * 2];
    float _hssr_source_scratch[SPA_STEREO_UPMIX_MAX_FRAMES * 2];
};

/* The reusable default is the exact OFF bypass.  Production selects Universal
 * after attaching its validated model. */
void spa_stereo_upmix_default_config(struct spa_stereo_upmix_config *config);

/* Legacy front ~=0.958 with LFE V2, otherwise ~=0.997. Quiet/cabin profiles
 * retain ~=0.997 with assist; the dynamic limiter still bounds output. */
void spa_stereo_upmix_config_set_lfe(
    struct spa_stereo_upmix_config *config, int enabled);

/* Experimental source-aligned renderer, quiet fixed mono bass assist. */
void spa_stereo_upmix_config_set_cabin(
    struct spa_stereo_upmix_config *config, int enabled);

/* Bass-only change, independent of experimental spatial routing. */
void spa_stereo_upmix_config_set_quiet_lfe(
    struct spa_stereo_upmix_config *config, int enabled);

/* The caller owns the model and keeps it alive for the upmix state lifetime. */
void spa_stereo_upmix_config_set_hssr_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_model *model);

/* Attach a hook-owned asynchronous controller.  NULL retains the exact
 * synchronous renderer used by tools and deterministic regression tests. */
void spa_stereo_upmix_config_set_hssr_guard_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_model *model);
void spa_stereo_upmix_config_set_hssr_control_provider(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_control_provider *provider);

/* Only the exact OFF bypass and the single production Universal renderer are
 * supported.  Universal requires a validated model. */
int spa_stereo_upmix_backend_supported(enum spa_stereo_upmix_backend backend);

int spa_stereo_upmix_init(struct spa_stereo_upmix *state,
                          const struct spa_stereo_upmix_config *config);
int spa_stereo_upmix_reset(struct spa_stereo_upmix *state);

/* Convert interleaved PCM2/S16 into interleaved PCM6/S16.  OFF emits
 * [input L, input R, 0, 0, 0, 0] bit-exactly regardless of configured gains.
 * Universal emits [front L, front R, 0, optional LFE, Ls, Rs].
 *
 * frames must be <= SPA_STEREO_UPMIX_MAX_FRAMES and pcm6_capacity_frames is a
 * frame count.  The complete PCM input may overlap output, including exact
 * in-place use.  Neither PCM range may overlap the state object.  Validation
 * errors leave output and DSP state unchanged.  Processing allocates nothing. */
int spa_stereo_upmix_process_s16(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2,
                                 size_t frames,
                                 int16_t *pcm6,
                                 size_t pcm6_capacity_frames);

/* Experimental headroom-preserving synthesis for offline evaluation.
 * Float units are S16 units (32768 = 0 dBFS), NOT normalized [-1,1].
 * Analysis is identical to the S16 renderer, including its delayed dry front.
 * Front transfer, rear shelf and LFE synthesis are not quantized or limited
 * to the output rail here. Apply programme gain and linked peak protection
 * afterwards, then quantize once. This is not yet the production hook path.
 * The size/overlap/error contract above also applies to this float output. */
int spa_stereo_upmix_process_f32(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2, size_t frames,
                                 float *pcm6, size_t pcm6_capacity_frames);

/* Profile-0/bank-1 J525 input matrix, first six inputs x eight buses, row-major.
 * Static storage. Includes the verified -99 dB effective input-matrix floor.
 * For downstream peak detection only; not an acoustic model or an EQ inverse. */
const float *spa_stereo_upmix_input_matrix(void);

#ifdef SPA_STEREO_UPMIX_TESTING
/* Test-only access to the exact shipping rear shelf. */
float spa_stereo_upmix_test_rear_high_shelf(float input, float state[2]);
#endif

#endif /* STEREO_UPMIX_H */
