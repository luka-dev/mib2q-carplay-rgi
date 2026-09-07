/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "stereo_upmix.h"

#include <limits.h>
#include <string.h>

#define SPA_STEREO_UPMIX_MAGIC UINT32_C(0x5355504d) /* "SUPM" */

/* 48 kHz, 80 Hz, Q=1/sqrt(2) Butterworth low-pass.  Cascading this section
 * twice is a 4th-order Linkwitz-Riley crossover. */
static const double kLfeB0 = 0.000027213807988313326;
static const double kLfeB1 = 0.000054427615976626652;
static const double kLfeB2 = 0.000027213807988313326;
static const double kLfeA1 = -1.9851906578962613;
static const double kLfeA2 = 0.98529951312821462;

static float spa_upmix_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static float spa_upmix_bound(float value, float limit)
{
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}

static double spa_upmix_bound_double(double value, double limit)
{
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}

static int16_t spa_upmix_scale_s16_q15(int16_t sample, uint32_t gain_q15)
{
    int64_t product = (int64_t)sample * (int64_t)gain_q15;
    int64_t magnitude;

    if (product >= 0)
        return (int16_t)((product + INT64_C(16384)) / INT64_C(32768));
    magnitude = -product;
    return (int16_t)(-((magnitude + INT64_C(16384)) / INT64_C(32768)));
}

static int16_t spa_upmix_transfer_front(int16_t front, int16_t rear,
                                        uint32_t transfer_q15,
                                        uint32_t front_limit_q15)
{
    int32_t adjusted = (int32_t)front -
        (int32_t)spa_upmix_scale_s16_q15(rear, transfer_q15);
    int32_t limit = (int32_t)front_limit_q15;

    if (adjusted > limit) adjusted = limit;
    if (adjusted < -limit) adjusted = -limit;
    return (int16_t)adjusted;
}

static int16_t spa_upmix_rear_to_s16(float normalized, uint32_t gain_q15)
{
    float scaled;
    int32_t rounded;

    normalized = spa_upmix_bound(normalized, 1.0f);
    scaled = normalized * (float)gain_q15;
    if (scaled >= (float)gain_q15) return (int16_t)gain_q15;
    if (scaled <= -(float)gain_q15)
        return (int16_t)(-(int32_t)gain_q15);
    rounded = scaled >= 0.0f ? (int32_t)(scaled + 0.5f)
                             : (int32_t)(scaled - 0.5f);
    return (int16_t)rounded;
}

static int16_t spa_upmix_lfe_to_s16(double normalized, uint32_t gain_q15)
{
    double scaled;
    int32_t rounded;

    normalized = spa_upmix_bound_double(normalized, 1.0);
    scaled = normalized * (double)gain_q15;
    if (scaled >= (double)gain_q15) return (int16_t)gain_q15;
    if (scaled <= -(double)gain_q15)
        return (int16_t)(-(int32_t)gain_q15);
    rounded = scaled >= 0.0 ? (int32_t)(scaled + 0.5)
                            : (int32_t)(scaled - 0.5);
    return (int16_t)rounded;
}

static double spa_upmix_lfe_biquad(double input, double state[2])
{
    double output = kLfeB0 * input + state[0];
    double next_z1;
    double next_z2;

    output = spa_upmix_bound_double(output, 4.0);
    next_z1 = kLfeB1 * input - kLfeA1 * output + state[1];
    next_z2 = kLfeB2 * input - kLfeA2 * output;
    if (next_z1 > -1.0e-24 && next_z1 < 1.0e-24) next_z1 = 0.0;
    if (next_z2 > -1.0e-24 && next_z2 < 1.0e-24) next_z2 = 0.0;
    state[0] = spa_upmix_bound_double(next_z1, 4.0);
    state[1] = spa_upmix_bound_double(next_z2, 4.0);
    return output;
}

static int spa_upmix_config_valid(const struct spa_stereo_upmix_config *config)
{
    uint64_t centre_bus_q15;

    if (!config) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (!spa_stereo_upmix_backend_supported(config->backend))
        return SPA_STEREO_UPMIX_ERROR_UNSUPPORTED;
    if (config->sample_rate_hz != SPA_STEREO_UPMIX_SAMPLE_RATE ||
        config->front_gain_q15 > UINT32_C(32768) ||
        config->rear_gain_q15 > SPA_STEREO_UPMIX_REAR_Q15_MAX ||
        config->lfe_gain_q15 > SPA_STEREO_UPMIX_LFE_Q15_MAX ||
        config->lfe_enabled > 1u)
        return SPA_STEREO_UPMIX_ERROR_CONFIG;
    if (config->backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL &&
        !spa_hssr_model_ready(config->hssr_model))
        return SPA_STEREO_UPMIX_ERROR_CONFIG;

    /* Recovered SoundAmp centre row:
     * Cbus = 0.501187*L + 0.501187*R + 0.316228*LFE. */
    if (config->backend != SPA_STEREO_UPMIX_OFF && config->lfe_enabled) {
        centre_bus_q15 = UINT64_C(1002375) * config->front_gain_q15 +
                         UINT64_C(316228) * config->lfe_gain_q15;
        if (centre_bus_q15 > UINT64_C(1000000) * UINT64_C(32768))
            return SPA_STEREO_UPMIX_ERROR_CONFIG;
    }
    return SPA_STEREO_UPMIX_OK;
}

static int spa_upmix_ranges_overlap(const void *left, size_t left_size,
                                    const void *right, size_t right_size)
{
    uintptr_t left_begin = (uintptr_t)left;
    uintptr_t right_begin = (uintptr_t)right;
    uintptr_t left_end;
    uintptr_t right_end;

    if (left_size == 0 || right_size == 0) return 0;
    if (left_size > UINTPTR_MAX - left_begin ||
        right_size > UINTPTR_MAX - right_begin)
        return 1;
    left_end = left_begin + left_size;
    right_end = right_begin + right_size;
    return left_begin < right_end && right_begin < left_end;
}

void spa_stereo_upmix_default_config(struct spa_stereo_upmix_config *config)
{
    if (!config) return;
    config->sample_rate_hz = SPA_STEREO_UPMIX_SAMPLE_RATE;
    config->front_gain_q15 = SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT;
    config->rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_DEFAULT;
    config->lfe_gain_q15 = SPA_STEREO_UPMIX_LFE_Q15_DEFAULT;
    config->lfe_enabled = 0;
    config->backend = SPA_STEREO_UPMIX_OFF;
    config->hssr_model = NULL;
}

void spa_stereo_upmix_config_set_lfe(
    struct spa_stereo_upmix_config *config, int enabled)
{
    if (!config) return;
    config->lfe_enabled = enabled ? 1u : 0u;
    config->front_gain_q15 = enabled
        ? SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT
        : SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT;
}

void spa_stereo_upmix_config_set_hssr_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_model *model)
{
    if (config) config->hssr_model = model;
}

int spa_stereo_upmix_backend_supported(enum spa_stereo_upmix_backend backend)
{
    return backend == SPA_STEREO_UPMIX_OFF ||
           backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
}

int spa_stereo_upmix_init(struct spa_stereo_upmix *state,
                          const struct spa_stereo_upmix_config *config)
{
    struct spa_stereo_upmix_config saved;
    int status;

    if (!state || !config) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    status = spa_upmix_config_valid(config);
    if (status != SPA_STEREO_UPMIX_OK) return status;
    saved = *config;
    memset(state, 0, sizeof(*state));
    state->config = saved;
    if (saved.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL)
        spa_hssr_dsp_prepare(&state->_runtime._hssr);
    state->_runtime._limiter_gain = 1.0f;
    state->_initialized = SPA_STEREO_UPMIX_MAGIC;
    return SPA_STEREO_UPMIX_OK;
}

int spa_stereo_upmix_reset(struct spa_stereo_upmix *state)
{
    if (!state) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (state->_initialized != SPA_STEREO_UPMIX_MAGIC)
        return SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED;
    memset(&state->_runtime, 0, sizeof(state->_runtime));
    memset(state->_input_scratch, 0, sizeof(state->_input_scratch));
    if (state->config.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL)
        spa_hssr_dsp_prepare(&state->_runtime._hssr);
    state->_runtime._limiter_gain = 1.0f;
    return SPA_STEREO_UPMIX_OK;
}

int spa_stereo_upmix_process_s16(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2,
                                 size_t frames,
                                 int16_t *pcm6,
                                 size_t pcm6_capacity_frames)
{
    size_t input_bytes;
    size_t output_bytes;
    size_t input_samples;
    size_t frame;
    int status;

    if (!state) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (state->_initialized != SPA_STEREO_UPMIX_MAGIC)
        return SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED;
    status = spa_upmix_config_valid(&state->config);
    if (status != SPA_STEREO_UPMIX_OK) return status;
    if (frames > SPA_STEREO_UPMIX_MAX_FRAMES)
        return SPA_STEREO_UPMIX_ERROR_BLOCK_TOO_LARGE;
    if (pcm6_capacity_frames < frames)
        return SPA_STEREO_UPMIX_ERROR_OUTPUT_TOO_SMALL;
    if (frames == 0) return SPA_STEREO_UPMIX_OK;
    if (!pcm2 || !pcm6) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;

    input_samples = frames * SPA_STEREO_UPMIX_INPUT_CHANNELS;
    input_bytes = input_samples * sizeof(*pcm2);
    output_bytes = frames * SPA_STEREO_UPMIX_OUTPUT_CHANNELS * sizeof(*pcm6);
    if (spa_upmix_ranges_overlap(state, sizeof(*state), pcm2, input_bytes) ||
        spa_upmix_ranges_overlap(state, sizeof(*state), pcm6, output_bytes))
        return SPA_STEREO_UPMIX_ERROR_STATE_OVERLAP;
    memcpy(state->_input_scratch, pcm2, input_bytes);

    for (frame = 0; frame < frames; frame++) {
        const int16_t input_l = state->_input_scratch[frame * 2];
        const int16_t input_r = state->_input_scratch[frame * 2 + 1];
        int16_t *output = pcm6 + frame * SPA_STEREO_UPMIX_OUTPUT_CHANNELS;

        if (state->config.backend == SPA_STEREO_UPMIX_OFF) {
            output[SPA_STEREO_UPMIX_CHANNEL_L] = input_l;
            output[SPA_STEREO_UPMIX_CHANNEL_R] = input_r;
            output[SPA_STEREO_UPMIX_CHANNEL_C] = 0;
            output[SPA_STEREO_UPMIX_CHANNEL_LFE] = 0;
            output[SPA_STEREO_UPMIX_CHANNEL_LS] = 0;
            output[SPA_STEREO_UPMIX_CHANNEL_RS] = 0;
        } else {
            int16_t render_l = input_l;
            int16_t render_r = input_r;
            int16_t lfe = 0;
            float rear_l = 0.0f;
            float rear_r = 0.0f;
            int hssr_status = spa_hssr_dsp_process_sample(
                &state->_runtime._hssr, state->config.hssr_model,
                input_l, input_r, &render_l, &render_r, &rear_l, &rear_r);
            int16_t front_l;
            int16_t front_r;
            int16_t output_rear_l;
            int16_t output_rear_r;

            if (hssr_status != SPA_HSSR_OK)
                return SPA_STEREO_UPMIX_ERROR_CONFIG;
            if (state->config.lfe_enabled) {
                double mid = ((double)render_l + (double)render_r) *
                             (1.0 / 65536.0);
                double bass = spa_upmix_lfe_biquad(
                    mid, state->_runtime._lfe_biquad_state[0]);
                bass = spa_upmix_lfe_biquad(
                    bass, state->_runtime._lfe_biquad_state[1]);
                lfe = spa_upmix_lfe_to_s16(
                    bass, state->config.lfe_gain_q15);
            }

            /* Linked limiter: instantaneous attack, about 42 ms release. */
            {
                float peak_l = spa_upmix_absf(rear_l);
                float peak_r = spa_upmix_absf(rear_r);
                float peak = peak_l > peak_r ? peak_l : peak_r;

                if (peak > 1.0f) {
                    float required = 1.0f / peak;
                    if (required < state->_runtime._limiter_gain)
                        state->_runtime._limiter_gain = required;
                } else {
                    state->_runtime._limiter_gain += 0.0005f *
                        (1.0f - state->_runtime._limiter_gain);
                }
                rear_l = spa_upmix_bound(
                    rear_l * state->_runtime._limiter_gain, 1.0f);
                rear_r = spa_upmix_bound(
                    rear_r * state->_runtime._limiter_gain, 1.0f);
            }

            front_l = spa_upmix_scale_s16_q15(
                render_l, state->config.front_gain_q15);
            front_r = spa_upmix_scale_s16_q15(
                render_r, state->config.front_gain_q15);
            output_rear_l = spa_upmix_rear_to_s16(
                rear_l, state->config.rear_gain_q15);
            output_rear_r = spa_upmix_rear_to_s16(
                rear_r, state->config.rear_gain_q15);
            front_l = spa_upmix_transfer_front(
                front_l, output_rear_l, UINT32_C(81920),
                state->config.front_gain_q15);
            front_r = spa_upmix_transfer_front(
                front_r, output_rear_r, UINT32_C(81920),
                state->config.front_gain_q15);

            output[SPA_STEREO_UPMIX_CHANNEL_L] = front_l;
            output[SPA_STEREO_UPMIX_CHANNEL_R] = front_r;
            output[SPA_STEREO_UPMIX_CHANNEL_C] = 0;
            output[SPA_STEREO_UPMIX_CHANNEL_LFE] = lfe;
            output[SPA_STEREO_UPMIX_CHANNEL_LS] = output_rear_l;
            output[SPA_STEREO_UPMIX_CHANNEL_RS] = output_rear_r;
        }
    }

    return SPA_STEREO_UPMIX_OK;
}
