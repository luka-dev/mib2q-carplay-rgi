/*
 * HSSR Universal stereo upmixer for the fixed MHI2Q PCM6 rail.
 *
 * The output order is always L, R, C, LFE, Ls, Rs.  C is deliberately zero
 * because the Audi amplifier already derives its centre.  Universal can feed
 * a small, explicitly bounded 80 Hz bass-assist signal into LFE.
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
    SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT = 30802,
    SPA_STEREO_UPMIX_REAR_Q15_DEFAULT = 5898,
    SPA_STEREO_UPMIX_REAR_Q15_MAX = 8192,
    SPA_STEREO_UPMIX_LFE_Q15_DEFAULT = 5833,
    SPA_STEREO_UPMIX_LFE_Q15_MAX = 5833
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
    enum spa_stereo_upmix_backend backend;
    const struct spa_hssr_model *hssr_model;
};

/* Runtime details are public only so callers can embed the complete state and
 * the realtime path never has to allocate.  Callers must not inspect or alter
 * fields whose names start with an underscore. */
struct spa_stereo_upmix_runtime {
    /* Two identical 2nd-order Butterworth sections form a 4th-order
     * Linkwitz-Riley low-pass at 80 Hz. */
    double _lfe_biquad_state[2][2];
    float _limiter_gain;
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
};

/* The reusable default is the exact OFF bypass.  Production selects Universal
 * after attaching its validated model. */
void spa_stereo_upmix_default_config(struct spa_stereo_upmix_config *config);

/* Select the paired safe front default: ~=0.94 with bass assist and ~=0.997
 * without it.  The LFE gain itself is preserved across disable/enable. */
void spa_stereo_upmix_config_set_lfe(
    struct spa_stereo_upmix_config *config, int enabled);

/* The caller owns the model and keeps it alive for the upmix state lifetime. */
void spa_stereo_upmix_config_set_hssr_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_model *model);

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

#endif /* STEREO_UPMIX_H */
