/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "stereo_upmix.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#define SPA_STEREO_UPMIX_MAGIC UINT32_C(0x5355504d) /* "SUPM" */

struct spa_upmix_biquad_coefficients {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
};

/* Cabin-Aware LFE V2 deliberately stays below the conservative 48-Hz
 * SoundAmp sub-band boundary.  This is one 48-kHz, Q=1/sqrt(2) Butterworth
 * section cascaded twice below for an LR4 (-24 dB/octave) low-pass. */
static const struct spa_upmix_biquad_coefficients kLfeLow48 = {
    9.82591682048203443e-06,
    1.96518336409640689e-05,
    9.82591682048203443e-06,
    -1.99111429220165359,
    0.991153595868935477
};

/* Rear-presence shelf for the fixed 48-kHz rail.  This is an RBJ high shelf
 * with S=1, +3 dB and a 4-kHz midpoint: effectively flat below 1 kHz,
 * +1.5 dB at 4 kHz and asymptotically +3 dB.  It is applied only to the
 * separated Ls/Rs objects, before their linked limiter. */
static const struct spa_upmix_biquad_coefficients kRearHighShelf4k3dB = {
    1.327279882166f,
    -1.771448848238f,
    0.671953838362f,
    -1.220826978259f,
    0.448611850550f
};

/* Recovered A07/profile-0/bank-1 SoundAmp gains. */
static const float kCabinFront = 0.794328f;
static const float kCabinCentreFront = 0.501187f;
static const float kCabinFrontRear = 0.707946f;
static const float kCabinRear = 1.995262f;
static const float kCabinFrontLfeReciprocal = 2.51188751f;
static const float kCabinCentreLfeReciprocal = 3.16227532f;
static const float kCabinLfeReciprocal = 0.70794556f;
static const float kCabinRearLfeReciprocal = 5.01187815f;
static const float kCabinSampleLimit = 32275.0f; /* 0.985 FS + rounding margin */

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

static float spa_upmix_smoothstep(float value, float low, float high)
{
    float normalized;

    if (value <= low) return 0.0f;
    if (value >= high) return 1.0f;
    normalized = (value - low) / (high - low);
    return normalized * normalized * (3.0f - 2.0f * normalized);
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

static int16_t spa_upmix_lfe_to_s16(float normalized, uint32_t gain_q15)
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

static float spa_upmix_biquad(
    float input, float state[2],
    const struct spa_upmix_biquad_coefficients *coefficients)
{
    float output = coefficients->b0 * input + state[0];
    float next_z1;
    float next_z2;

    output = spa_upmix_bound(output, 4.0f);
    next_z1 = coefficients->b1 * input - coefficients->a1 * output +
              state[1];
    next_z2 = coefficients->b2 * input - coefficients->a2 * output;
    if (next_z1 > -1.0e-20f && next_z1 < 1.0e-20f) next_z1 = 0.0f;
    if (next_z2 > -1.0e-20f && next_z2 < 1.0e-20f) next_z2 = 0.0f;
    state[0] = spa_upmix_bound(next_z1, 4.0f);
    state[1] = spa_upmix_bound(next_z2, 4.0f);
    return output;
}

#ifdef SPA_STEREO_UPMIX_TESTING
float spa_stereo_upmix_test_rear_high_shelf(float input, float state[2])
{
    return spa_upmix_biquad(input, state, &kRearHighShelf4k3dB);
}
#endif

static float spa_upmix_lfe_lr4(
    float input, float state[2][2],
    const struct spa_upmix_biquad_coefficients *coefficients)
{
    float output = spa_upmix_biquad(
        input, state[0], coefficients);

    return spa_upmix_biquad(output, state[1], coefficients);
}

static int16_t spa_upmix_lfe_v2(
    struct spa_stereo_upmix *state, int16_t render_l, int16_t render_r,
    float bass_confidence, float drum_confidence)
{
    const float normalized_l = (float)render_l * (1.0f / 32768.0f);
    const float normalized_r = (float)render_r * (1.0f / 32768.0f);
    float low48_l = spa_upmix_lfe_lr4(
        normalized_l, state->_runtime._lfe_low48_state[0], &kLfeLow48);
    float low48_r = spa_upmix_lfe_lr4(
        normalized_r, state->_runtime._lfe_low48_state[1], &kLfeLow48);
    const float power_alpha = 0.00075f;
    float target_polarity;
    float polarity_alpha;
    float normalization;
    float sub_band;
    float magnitude;
    float transient_ratio;
    float transient;
    float target_gain;
    float gain_alpha;

    state->_runtime._lfe_power_l += power_alpha *
        (low48_l * low48_l - state->_runtime._lfe_power_l);
    state->_runtime._lfe_power_r += power_alpha *
        (low48_r * low48_r - state->_runtime._lfe_power_r);
    state->_runtime._lfe_cross += power_alpha *
        (low48_l * low48_r - state->_runtime._lfe_cross);
    /* Preserve coherent anti-phase bass instead of cancelling it in a mono
     * sum. Correlation is a slow control signal, so its square root runs at
     * 1.5 kHz while the continuous polarity move remains sample-rate. */
    if ((state->_runtime._lfe_correlation_tick++ & 31u) == 0u) {
        float denominator = sqrtf(state->_runtime._lfe_power_l *
            state->_runtime._lfe_power_r + 1.0e-20f);
        float correlation = denominator > 1.0e-10f
            ? state->_runtime._lfe_cross / denominator : 1.0f;
        float anti_phase;

        correlation = spa_upmix_bound(correlation, 1.0f);
        anti_phase = spa_upmix_smoothstep(
            -correlation, 0.20f, 0.75f);
        state->_runtime._lfe_target_polarity = 1.0f - 2.0f * anti_phase;
    }
    target_polarity = state->_runtime._lfe_target_polarity;
    polarity_alpha = target_polarity < state->_runtime._lfe_polarity
        ? 0.0018f : 0.00045f;
    state->_runtime._lfe_polarity += polarity_alpha *
        (target_polarity - state->_runtime._lfe_polarity);
    normalization = 0.70710678118654752440f -
        0.20710678118654752440f *
        spa_upmix_absf(state->_runtime._lfe_polarity);
    sub_band = normalization * (low48_l +
        state->_runtime._lfe_polarity * low48_r);

    magnitude = fabsf(low48_l) > fabsf(low48_r)
        ? fabsf(low48_l) : fabsf(low48_r);
    state->_runtime._lfe_fast_envelope +=
        (magnitude > state->_runtime._lfe_fast_envelope ? 0.080f : 0.003f) *
        (magnitude - state->_runtime._lfe_fast_envelope);
    state->_runtime._lfe_slow_envelope +=
        (magnitude > state->_runtime._lfe_slow_envelope ? 0.0010f : 0.0002f) *
        (magnitude - state->_runtime._lfe_slow_envelope);
    transient_ratio = state->_runtime._lfe_fast_envelope /
        (state->_runtime._lfe_slow_envelope + 1.0e-8f);
    transient = spa_upmix_smoothstep(transient_ratio, 1.15f, 2.20f);
    bass_confidence = spa_upmix_bound(bass_confidence, 1.0f);
    if (bass_confidence < 0.0f) bass_confidence = 0.0f;
    drum_confidence = spa_upmix_bound(drum_confidence, 1.0f);
    if (drum_confidence < 0.0f) drum_confidence = 0.0f;
    target_gain = 0.96f + 0.08f * bass_confidence +
        0.08f * drum_confidence + 0.08f * transient;
    if (target_gain > 1.10f) target_gain = 1.10f;
    gain_alpha = target_gain > state->_runtime._lfe_dynamic_gain
        ? 0.0040f : 0.0005f;
    state->_runtime._lfe_dynamic_gain += gain_alpha *
        (target_gain - state->_runtime._lfe_dynamic_gain);

    return spa_upmix_lfe_to_s16(
        sub_band * state->_runtime._lfe_dynamic_gain,
        state->config.lfe_gain_q15);
}

static void spa_upmix_lfe_intersect(
    float base, float reciprocal_coefficient,
    float *minimum, float *maximum)
{
    float low = (-kCabinSampleLimit - base) * reciprocal_coefficient;
    float high = (kCabinSampleLimit - base) * reciprocal_coefficient;

    if (low > *minimum) *minimum = low;
    if (high < *maximum) *maximum = high;
}

static int16_t spa_upmix_lfe_cabin_limit(
    int16_t candidate, int16_t front_l, int16_t front_r,
    int16_t rear_l, int16_t rear_r)
{
    float minimum = -32768.0f;
    float maximum = 32767.0f;
    float limited = (float)candidate;
    int32_t rounded;

    spa_upmix_lfe_intersect(
        kCabinFront * (float)front_l +
            kCabinFrontRear * (float)rear_l,
        kCabinFrontLfeReciprocal, &minimum, &maximum);
    spa_upmix_lfe_intersect(
        kCabinFront * (float)front_r +
            kCabinFrontRear * (float)rear_r,
        kCabinFrontLfeReciprocal, &minimum, &maximum);
    spa_upmix_lfe_intersect(
        kCabinCentreFront * ((float)front_l + (float)front_r),
        kCabinCentreLfeReciprocal, &minimum, &maximum);
    spa_upmix_lfe_intersect(
        0.0f, kCabinLfeReciprocal, &minimum, &maximum);
    spa_upmix_lfe_intersect(
        kCabinRear * (float)rear_l,
        kCabinRearLfeReciprocal, &minimum, &maximum);
    spa_upmix_lfe_intersect(
        kCabinRear * (float)rear_r,
        kCabinRearLfeReciprocal, &minimum, &maximum);

    if (minimum > maximum) return 0;
    if (limited < minimum) limited = minimum;
    if (limited > maximum) limited = maximum;
    rounded = (int32_t)limited;
    if ((float)rounded > maximum) rounded--;
    if ((float)rounded < minimum) rounded++;
    if (rounded > INT16_MAX) rounded = INT16_MAX;
    if (rounded < INT16_MIN) rounded = INT16_MIN;
    return (int16_t)rounded;
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
    if (config->hssr_control_provider &&
        (!config->hssr_control_provider->submit ||
         !config->hssr_control_provider->receive ||
         !config->hssr_control_provider->reset))
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
    config->hssr_control_provider = NULL;
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

void spa_stereo_upmix_config_set_hssr_control_provider(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_control_provider *provider)
{
    if (config) config->hssr_control_provider = provider;
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
    if (saved.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL) {
        spa_hssr_dsp_prepare(&state->_runtime._hssr);
        state->_runtime._hssr.control_generation = 1u;
        if (saved.hssr_control_provider)
            saved.hssr_control_provider->reset(
                saved.hssr_control_provider->context, 1u);
    }
    state->_runtime._lfe_polarity = 1.0f;
    state->_runtime._lfe_target_polarity = 1.0f;
    state->_runtime._lfe_dynamic_gain = 0.96f;
    state->_runtime._limiter_gain = 1.0f;
    state->_initialized = SPA_STEREO_UPMIX_MAGIC;
    return SPA_STEREO_UPMIX_OK;
}

int spa_stereo_upmix_reset(struct spa_stereo_upmix *state)
{
    uint32_t generation;

    if (!state) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (state->_initialized != SPA_STEREO_UPMIX_MAGIC)
        return SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED;
    generation = state->_runtime._hssr.control_generation + 1u;
    if (!generation) generation++;
    memset(&state->_runtime, 0, sizeof(state->_runtime));
    memset(state->_input_scratch, 0, sizeof(state->_input_scratch));
    memset(state->_hssr_front_scratch, 0,
           sizeof(state->_hssr_front_scratch));
    memset(state->_hssr_rear_scratch, 0,
           sizeof(state->_hssr_rear_scratch));
    memset(state->_hssr_lfe_control_scratch, 0,
           sizeof(state->_hssr_lfe_control_scratch));
    if (state->config.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL) {
        spa_hssr_dsp_prepare(&state->_runtime._hssr);
        state->_runtime._hssr.control_generation = generation;
        if (state->config.hssr_control_provider)
            state->config.hssr_control_provider->reset(
                state->config.hssr_control_provider->context, generation);
    }
    state->_runtime._lfe_polarity = 1.0f;
    state->_runtime._lfe_target_polarity = 1.0f;
    state->_runtime._lfe_dynamic_gain = 0.96f;
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

    if (state->config.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL) {
        int hssr_status = spa_hssr_dsp_process_block(
            &state->_runtime._hssr, state->config.hssr_model,
            state->config.hssr_control_provider,
            state->_input_scratch, frames,
            state->_hssr_front_scratch, state->_hssr_rear_scratch,
            state->_hssr_lfe_control_scratch);

        if (hssr_status != SPA_HSSR_OK)
            return SPA_STEREO_UPMIX_ERROR_CONFIG;
    }

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
            int16_t render_l = state->_hssr_front_scratch[frame * 2];
            int16_t render_r = state->_hssr_front_scratch[frame * 2 + 1];
            int16_t lfe_candidate = 0;
            int16_t lfe = 0;
            float rear_l = state->_hssr_rear_scratch[frame * 2];
            float rear_r = state->_hssr_rear_scratch[frame * 2 + 1];
            float transfer_rear_l = rear_l;
            float transfer_rear_r = rear_r;
            int16_t front_l;
            int16_t front_r;
            int16_t output_rear_l;
            int16_t output_rear_r;
            int16_t output_transfer_rear_l;
            int16_t output_transfer_rear_r;

            if (state->config.lfe_enabled) {
                lfe_candidate = spa_upmix_lfe_v2(
                    state, render_l, render_r,
                    state->_hssr_lfe_control_scratch[frame * 2],
                    state->_hssr_lfe_control_scratch[frame * 2 + 1]);
            }

            rear_l = spa_upmix_biquad(
                rear_l, state->_runtime._rear_high_shelf_state[0],
                &kRearHighShelf4k3dB);
            rear_r = spa_upmix_biquad(
                rear_r, state->_runtime._rear_high_shelf_state[1],
                &kRearHighShelf4k3dB);

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
                /* Speaker-presence EQ must not change the amount of the
                 * separated object transferred out of the dry fronts. */
                transfer_rear_l = spa_upmix_bound(
                    transfer_rear_l * state->_runtime._limiter_gain, 1.0f);
                transfer_rear_r = spa_upmix_bound(
                    transfer_rear_r * state->_runtime._limiter_gain, 1.0f);
            }

            front_l = spa_upmix_scale_s16_q15(
                render_l, state->config.front_gain_q15);
            front_r = spa_upmix_scale_s16_q15(
                render_r, state->config.front_gain_q15);
            output_rear_l = spa_upmix_rear_to_s16(
                rear_l, state->config.rear_gain_q15);
            output_rear_r = spa_upmix_rear_to_s16(
                rear_r, state->config.rear_gain_q15);
            output_transfer_rear_l = spa_upmix_rear_to_s16(
                transfer_rear_l, state->config.rear_gain_q15);
            output_transfer_rear_r = spa_upmix_rear_to_s16(
                transfer_rear_r, state->config.rear_gain_q15);
            front_l = spa_upmix_transfer_front(
                front_l, output_transfer_rear_l, UINT32_C(81920),
                state->config.front_gain_q15);
            front_r = spa_upmix_transfer_front(
                front_r, output_transfer_rear_r, UINT32_C(81920),
                state->config.front_gain_q15);
            if (state->config.lfe_enabled)
                lfe = spa_upmix_lfe_cabin_limit(
                    lfe_candidate, front_l, front_r,
                    output_rear_l, output_rear_r);

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
