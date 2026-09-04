/*
 * Deterministic stereo ambience upmixer for the fixed MHI2Q PCM6 rail.
 *
 * The output order is always L, R, C, LFE, Ls, Rs.  L/R are dry apart from
 * the configured Q15 gain and C is deliberately zero because the Audi
 * amplifier already derives its centre.  The active profile can feed a small,
 * explicitly bounded 80 Hz bass-assist signal into LFE.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef STEREO_UPMIX_H
#define STEREO_UPMIX_H

#include <stddef.h>
#include <stdint.h>

enum {
    SPA_STEREO_UPMIX_INPUT_CHANNELS = 2,
    SPA_STEREO_UPMIX_OUTPUT_CHANNELS = 6,
    SPA_STEREO_UPMIX_BANDS = 6,
    SPA_STEREO_UPMIX_CROSSOVERS = 5,
    SPA_STEREO_UPMIX_PHASE_STAGES = 2,
    SPA_STEREO_UPMIX_MAX_FRAMES = 1024,
    SPA_STEREO_UPMIX_SAMPLE_RATE = 48000,

    /* ambient-hq: causal 75%-overlap STFT.  The synchronous streaming API
     * returns one frame for every input frame, so the rear field has one
     * complete FFT frame of fixed latency.  Dry fronts and LFE are not
     * delayed. */
    SPA_STEREO_UPMIX_HQ_FFT_SIZE = 1024,
    SPA_STEREO_UPMIX_HQ_HOP_SIZE = 256,
    SPA_STEREO_UPMIX_HQ_BINS = 513,
    SPA_STEREO_UPMIX_HQ_BANDS = 24,
    SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES = 1024,

    /* hsr-td: a clean-room, causal time-domain spatial-field decoder.  The
     * expensive covariance eigensystem is refreshed at a 6 kHz control rate;
     * audio filtering and residual rendering still run for every sample. */
    SPA_STEREO_UPMIX_TD_CONTROL_STRIDE = 8,

    /* Q15 gains.  32768 is unity and is intentionally representable here. */
    /* Paired profiles: legacy/no-LFE ~= 0.997; LFE-enabled ~= 0.94. */
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

enum spa_stereo_upmix_backend {
    SPA_STEREO_UPMIX_OFF = 0,
    SPA_STEREO_UPMIX_AMBIENT_LITE = 1,
    SPA_STEREO_UPMIX_AMBIENT_HQ = 2,
    SPA_STEREO_UPMIX_PIONEER_RER = 3,
    SPA_STEREO_UPMIX_CUPGAN = 4,
    SPA_STEREO_UPMIX_HSR_TD = 5,
    SPA_STEREO_UPMIX_HSR_TD_ACTIVE = 6,
    SPA_STEREO_UPMIX_MUSIC_STAGE = 7
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
};

/* Runtime details are public only so callers can embed the complete state and
 * the realtime path never has to allocate.  Callers must not inspect or alter
 * fields whose names start with an underscore. */
struct spa_stereo_upmix_runtime {
    float _split[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                [SPA_STEREO_UPMIX_CROSSOVERS];
    float _power[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                [SPA_STEREO_UPMIX_BANDS];
    float _phase_x[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                  [SPA_STEREO_UPMIX_PHASE_STAGES]
                  [SPA_STEREO_UPMIX_BANDS];
    float _phase_y[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                  [SPA_STEREO_UPMIX_PHASE_STAGES]
                  [SPA_STEREO_UPMIX_BANDS];

    /* Two identical 2nd-order Butterworth sections form a 4th-order
     * Linkwitz-Riley low-pass at 80 Hz.  Transposed-DFII double state avoids
     * loss of precision from the very low normalized cutoff. */
    double _lfe_biquad_state[2][2];

    /* Two stable Schroeder all-pass sections per rear channel.  Their unequal
     * prime delays are inspired by the recovered Pioneer RER graph; these are
     * clean-room coefficients, not copied Pioneer preset data. */
    float _rear_l_delay_0[149];
    float _rear_l_delay_1[263];
    float _rear_r_delay_0[211];
    float _rear_r_delay_1[293];
    uint16_t _rear_delay_pos[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                            [SPA_STEREO_UPMIX_PHASE_STAGES];
    float _limiter_gain;

    /* ambient-hq tables and caller-owned streaming state.  Keeping all FFT,
     * covariance and overlap-add storage here guarantees that processing does
     * not allocate or borrow the optional APAC decoder's C++ FFT. */
    float _hq_analysis_window[SPA_STEREO_UPMIX_HQ_FFT_SIZE];
    float _hq_synthesis_window[SPA_STEREO_UPMIX_HQ_FFT_SIZE];
    float _hq_twiddle_re[SPA_STEREO_UPMIX_HQ_FFT_SIZE / 2];
    float _hq_twiddle_im[SPA_STEREO_UPMIX_HQ_FFT_SIZE / 2];
    float _hq_input[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                   [SPA_STEREO_UPMIX_HQ_FFT_SIZE];
    float _hq_ola[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                 [SPA_STEREO_UPMIX_HQ_FFT_SIZE];
    float _hq_fft_re[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                    [SPA_STEREO_UPMIX_HQ_FFT_SIZE];
    float _hq_fft_im[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                    [SPA_STEREO_UPMIX_HQ_FFT_SIZE];
    float _hq_cov_l[SPA_STEREO_UPMIX_HQ_BANDS];
    float _hq_cov_r[SPA_STEREO_UPMIX_HQ_BANDS];
    float _hq_cov_re[SPA_STEREO_UPMIX_HQ_BANDS];
    float _hq_cov_im[SPA_STEREO_UPMIX_HQ_BANDS];
    float _hq_previous_magnitude[SPA_STEREO_UPMIX_HQ_BINS];
    float _hq_mask[SPA_STEREO_UPMIX_HQ_BANDS];
    float _hq_band_accumulator[SPA_STEREO_UPMIX_HQ_BANDS][6];
    float _hq_band_parameters[SPA_STEREO_UPMIX_HQ_BANDS][6];
    float _hq_frequency_weight[SPA_STEREO_UPMIX_HQ_BINS];
    uint8_t _hq_bin_band[SPA_STEREO_UPMIX_HQ_BINS];
    uint16_t _hq_input_position;
    uint16_t _hq_hop_fill;
    uint32_t _hq_frames_seen;

    /* hsr-td owns a second complementary six-band bank so changing backend
     * never aliases its statistics with ambient-lite.  Each band's smoothed
     * real covariance estimates one coherent rank-one component.  The two
     * saved vector entries and direct coefficient describe its projector;
     * the unclaimed residual is the spatial/diffuse rear field. */
    float _td_split[SPA_STEREO_UPMIX_INPUT_CHANNELS]
                   [SPA_STEREO_UPMIX_CROSSOVERS];
    float _td_power_l[SPA_STEREO_UPMIX_BANDS];
    float _td_power_r[SPA_STEREO_UPMIX_BANDS];
    float _td_cross[SPA_STEREO_UPMIX_BANDS];
    float _td_vector_l[SPA_STEREO_UPMIX_BANDS];
    float _td_vector_r[SPA_STEREO_UPMIX_BANDS];
    float _td_direct[SPA_STEREO_UPMIX_BANDS];
    float _td_active_input_power;
    float _td_active_residual_power;
    float _td_active_confidence;
    float _td_active_amount;
    uint8_t _td_control_phase;
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

/* The reusable default selects ambient-lite at 48 kHz with bass assist off,
 * front ~= 0.997 and rear ~= 0.18.  Enable bass assist explicitly through
 * spa_stereo_upmix_config_set_lfe(); it selects the paired front ~= 0.94 and
 * LFE ~= 0.178 profile whose known SoundAmp centre sum stays below unity.
 * OFF remains an exact compatibility bypass regardless of gains. */
void spa_stereo_upmix_default_config(struct spa_stereo_upmix_config *config);

/* Convenience for building a configuration before init.  It selects the
 * paired safe front default: ~=0.94 when enabled, ~=0.997 when disabled.  The
 * configured LFE gain is preserved so a disable/enable round trip is lossless. */
void spa_stereo_upmix_config_set_lfe(
    struct spa_stereo_upmix_config *config, int enabled);

/* OFF, AMBIENT_LITE, AMBIENT_HQ, HSR_TD, HSR_TD_ACTIVE and MUSIC_STAGE are
 * implemented. Reserved backends return false rather than silently changing
 * the requested algorithm. */
int spa_stereo_upmix_backend_supported(enum spa_stereo_upmix_backend backend);

/* Initialise/reset caller-owned state.  Reset preserves the configuration and
 * immediately removes every filter, delay and limiter tail. */
int spa_stereo_upmix_init(struct spa_stereo_upmix *state,
                          const struct spa_stereo_upmix_config *config);
int spa_stereo_upmix_reset(struct spa_stereo_upmix *state);

/* Convert interleaved PCM2/S16 into interleaved PCM6/S16 in chronological
 * order.  pcm6_capacity_frames is a frame count, not a sample or byte count.
 * Active output is [dry L, dry R, 0, optional LFE, Ls, Rs].  OFF always emits
 * [input L, input R, 0, 0, 0, 0] bit-exactly, ignoring configured gains.
 *
 * frames must be <= SPA_STEREO_UPMIX_MAX_FRAMES and pcm6 must have room for
 * frames * 6 samples.  The complete PCM input range may overlap the output
 * range, including pcm2 == pcm6.  Neither range may overlap the state object.
 * A validation error leaves both output and DSP state unchanged.
 *
 * The state is single-owner and non-reentrant.  Processing performs no heap
 * allocation, locking, thread creation, logging or environment access. */
int spa_stereo_upmix_process_s16(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2,
                                 size_t frames,
                                 int16_t *pcm6,
                                 size_t pcm6_capacity_frames);

#endif /* STEREO_UPMIX_H */
