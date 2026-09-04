/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "stereo_upmix.h"

#include <limits.h>
#include <string.h>

#define SPA_STEREO_UPMIX_MAGIC UINT32_C(0x5355504d) /* "SUPM" */

enum {
    SPA_UPMIX_LEFT = 0,
    SPA_UPMIX_RIGHT = 1
};

/* One-pole low-pass coefficients, 1-exp(-2*pi*f/48000), for complementary
 * boundaries 220, 500, 1000, 2000 and 5000 Hz. */
static const float kSplitAlpha[SPA_STEREO_UPMIX_CROSSOVERS] = {
    0.0283872242f, 0.0633539788f, 0.122694231f,
    0.230334588f, 0.480297356f
};

/* Conservative vehicle canary passband.  Sub-bass and >5 kHz stay in front;
 * transition bands are eased into the rear field. */
static const float kBandWeight[SPA_STEREO_UPMIX_BANDS] = {
    0.0f, 0.55f, 0.85f, 1.0f, 0.75f, 0.0f
};

/* Distinct, stable first-order all-pass coefficients for each band and rear.
 * They prevent the band residuals from recombining into identical Ls/Rs. */
static const float
kPhaseAllpass[SPA_STEREO_UPMIX_INPUT_CHANNELS]
              [SPA_STEREO_UPMIX_PHASE_STAGES]
              [SPA_STEREO_UPMIX_BANDS] = {
    {
        { 0.31f, 0.37f, 0.43f, 0.49f, 0.55f, 0.59f },
        { -0.29f, -0.35f, -0.41f, -0.47f, -0.53f, -0.57f }
    },
    {
        { -0.53f, -0.47f, -0.41f, -0.35f, -0.29f, -0.23f },
        { 0.57f, 0.51f, 0.45f, 0.39f, 0.33f, 0.27f }
    }
};

/* 48 kHz, 80 Hz, Q=1/sqrt(2) Butterworth low-pass from the bilinear/RBJ
 * transform.  Cascading this section twice is a 4th-order Linkwitz-Riley
 * crossover: unity at DC and -6.0206 dB at 80 Hz.  The difference equation is
 * y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y. */
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

/* Self-contained positive square root.  The bit seed plus two Newton steps is
 * accurate enough for a soft direction estimate and avoids a libm dependency.
 * memcpy keeps the C99 implementation free of strict-aliasing violations. */
static float spa_upmix_sqrt_positive(float value)
{
    uint32_t bits;
    float inverse;
    float half;

    if (value <= 1.0e-20f) return 0.0f;

    half = value * 0.5f;
    memcpy(&bits, &value, sizeof(bits));
    bits = UINT32_C(0x5f375a86) - (bits >> 1);
    memcpy(&inverse, &bits, sizeof(inverse));
    inverse = inverse * (1.5f - half * inverse * inverse);
    inverse = inverse * (1.5f - half * inverse * inverse);
    return value * inverse;
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

static int16_t spa_upmix_rear_to_s16(float normalized, uint32_t gain_q15)
{
    float scaled;
    int32_t rounded;

    normalized = spa_upmix_bound(normalized, 1.0f);
    scaled = normalized * (float)gain_q15;
    if (scaled >= (float)gain_q15) return (int16_t)gain_q15;
    if (scaled <= -(float)gain_q15) return (int16_t)(-(int32_t)gain_q15);

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

    /* Normal full-scale PCM stays close to unity.  These remote bounds only
     * contain corrupted state; they do not participate in the LR4 response. */
    output = spa_upmix_bound_double(output, 4.0);
    next_z1 = kLfeB1 * input - kLfeA1 * output + state[1];
    next_z2 = kLfeB2 * input - kLfeA2 * output;
    /* Cortex-A15 must not spend realtime cycles walking an inaudible denormal
     * tail after playback has stopped.  1e-24 is below -470 dBFS. */
    if (next_z1 > -1.0e-24 && next_z1 < 1.0e-24) next_z1 = 0.0;
    if (next_z2 > -1.0e-24 && next_z2 < 1.0e-24) next_z2 = 0.0;
    state[0] = spa_upmix_bound_double(next_z1, 4.0);
    state[1] = spa_upmix_bound_double(next_z2, 4.0);
    return output;
}

static void spa_upmix_split_sample(float input,
                                   float state[SPA_STEREO_UPMIX_CROSSOVERS],
                                   float bands[SPA_STEREO_UPMIX_BANDS])
{
    float low[SPA_STEREO_UPMIX_CROSSOVERS];
    int boundary;

    for (boundary = 0; boundary < SPA_STEREO_UPMIX_CROSSOVERS; boundary++) {
        state[boundary] += kSplitAlpha[boundary] *
                           (input - state[boundary]);
        low[boundary] = state[boundary];
    }

    bands[0] = low[0];
    for (boundary = 1; boundary < SPA_STEREO_UPMIX_CROSSOVERS; boundary++)
        bands[boundary] = low[boundary] - low[boundary - 1];
    bands[SPA_STEREO_UPMIX_BANDS - 1] =
        input - low[SPA_STEREO_UPMIX_CROSSOVERS - 1];
}

static float spa_upmix_first_order_allpass(float input, float coefficient,
                                           float *previous_input,
                                           float *previous_output)
{
    float output = -coefficient * input + *previous_input +
                   coefficient * *previous_output;

    /* The unclipped network is stable for |coefficient| < 1.  These remote
     * guards additionally contain corrupted state without affecting normal
     * operation. */
    output = spa_upmix_bound(output, 4.0f);
    *previous_input = input;
    *previous_output = output;
    return output;
}

static float spa_upmix_delay_allpass(float input, float coefficient,
                                     float *delay, uint16_t delay_length,
                                     uint16_t *position)
{
    uint16_t index = *position;
    float delayed = delay[index];
    float output = delayed - coefficient * input;
    float feedback;

    output = spa_upmix_bound(output, 4.0f);
    feedback = spa_upmix_bound(input + coefficient * output, 4.0f);
    delay[index] = feedback;

    index++;
    if (index == delay_length) index = 0;
    *position = index;
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

    /* Recovered SoundAmp centre row:
     *   Cbus = 0.501187*L + 0.501187*R + 0.316228*LFE.
     * Prove the configured active profile cannot exceed unity for bounded,
     * correlated inputs.  OFF ignores all gains and does not synthesize LFE. */
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
    config->backend = SPA_STEREO_UPMIX_AMBIENT_LITE;
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

int spa_stereo_upmix_backend_supported(enum spa_stereo_upmix_backend backend)
{
    return backend == SPA_STEREO_UPMIX_OFF ||
           backend == SPA_STEREO_UPMIX_AMBIENT_LITE;
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

    /* Stage before any output store: forward temporal processing is now safe
     * for exact in-place and every other overlap between pcm2 and pcm6. */
    memcpy(state->_input_scratch, pcm2, input_bytes);

    for (frame = 0; frame < frames; frame++) {
        const int16_t input_l = state->_input_scratch[frame * 2];
        const int16_t input_r = state->_input_scratch[frame * 2 + 1];
        int16_t *output = pcm6 + frame * SPA_STEREO_UPMIX_OUTPUT_CHANNELS;
        int16_t lfe = 0;
        float rear_l = 0.0f;
        float rear_r = 0.0f;

        if (state->config.backend == SPA_STEREO_UPMIX_AMBIENT_LITE) {
            float band_l[SPA_STEREO_UPMIX_BANDS];
            float band_r[SPA_STEREO_UPMIX_BANDS];
            const float normalized_l = (float)input_l * (1.0f / 32768.0f);
            const float normalized_r = (float)input_r * (1.0f / 32768.0f);
            int band;

            spa_upmix_split_sample(normalized_l,
                                   state->_runtime._split[SPA_UPMIX_LEFT],
                                   band_l);
            spa_upmix_split_sample(normalized_r,
                                   state->_runtime._split[SPA_UPMIX_RIGHT],
                                   band_r);

            if (state->config.lfe_enabled) {
                /* The sum is promoted before scaling, so INT16_MIN+INT16_MIN
                 * cannot overflow and mid remains exactly bounded by [-1,1). */
                double mid = ((double)input_l + (double)input_r) *
                             (1.0 / 65536.0);
                double bass = spa_upmix_lfe_biquad(
                    mid, state->_runtime._lfe_biquad_state[0]);
                bass = spa_upmix_lfe_biquad(
                    bass, state->_runtime._lfe_biquad_state[1]);
                lfe = spa_upmix_lfe_to_s16(
                    bass, state->config.lfe_gain_q15);
            }

            for (band = 0; band < SPA_STEREO_UPMIX_BANDS; band++) {
                float target_l = band_l[band] * band_l[band];
                float target_r = band_r[band] * band_r[band];
                float *power_l = &state->_runtime._power[SPA_UPMIX_LEFT][band];
                float *power_r = &state->_runtime._power[SPA_UPMIX_RIGHT][band];
                float alpha_l = target_l > *power_l ? 0.015f : 0.0001042f;
                float alpha_r = target_r > *power_r ? 0.015f : 0.0001042f;
                float amplitude_l;
                float amplitude_r;
                float denominator;
                float ambience;
                float rendered_l;
                float rendered_r;
                int stage_index;

                *power_l += alpha_l * (target_l - *power_l);
                *power_r += alpha_r * (target_r - *power_r);

                if (kBandWeight[band] == 0.0f) continue;
                amplitude_l = spa_upmix_sqrt_positive(*power_l);
                amplitude_r = spa_upmix_sqrt_positive(*power_r);
                denominator = amplitude_l + amplitude_r;
                if (denominator <= 1.0e-8f) continue;

                /* A stable positive amplitude pan cancels because its level
                 * vector is removed before the residual is formed. */
                ambience = (amplitude_l * band_r[band] -
                            amplitude_r * band_l[band]) / denominator;
                ambience = spa_upmix_bound(ambience, 1.0f);
                rendered_l = ambience;
                rendered_r = ambience;

                for (stage_index = 0;
                     stage_index < SPA_STEREO_UPMIX_PHASE_STAGES;
                     stage_index++) {
                    rendered_l = spa_upmix_first_order_allpass(
                        rendered_l,
                        kPhaseAllpass[SPA_UPMIX_LEFT][stage_index][band],
                        &state->_runtime._phase_x[SPA_UPMIX_LEFT]
                                                        [stage_index][band],
                        &state->_runtime._phase_y[SPA_UPMIX_LEFT]
                                                        [stage_index][band]);
                    rendered_r = spa_upmix_first_order_allpass(
                        rendered_r,
                        kPhaseAllpass[SPA_UPMIX_RIGHT][stage_index][band],
                        &state->_runtime._phase_x[SPA_UPMIX_RIGHT]
                                                        [stage_index][band],
                        &state->_runtime._phase_y[SPA_UPMIX_RIGHT]
                                                        [stage_index][band]);
                }

                rear_l += kBandWeight[band] * rendered_l;
                rear_r += kBandWeight[band] * rendered_r;
            }

            rear_l = spa_upmix_delay_allpass(
                spa_upmix_bound(rear_l, 2.0f), 0.55f,
                state->_runtime._rear_l_delay_0, 149,
                &state->_runtime._rear_delay_pos[SPA_UPMIX_LEFT][0]);
            rear_l = spa_upmix_delay_allpass(
                rear_l, -0.47f, state->_runtime._rear_l_delay_1, 263,
                &state->_runtime._rear_delay_pos[SPA_UPMIX_LEFT][1]);
            rear_r = spa_upmix_delay_allpass(
                spa_upmix_bound(rear_r, 2.0f), -0.51f,
                state->_runtime._rear_r_delay_0, 211,
                &state->_runtime._rear_delay_pos[SPA_UPMIX_RIGHT][0]);
            rear_r = spa_upmix_delay_allpass(
                rear_r, 0.43f, state->_runtime._rear_r_delay_1, 293,
                &state->_runtime._rear_delay_pos[SPA_UPMIX_RIGHT][1]);

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
        }

        if (state->config.backend == SPA_STEREO_UPMIX_OFF) {
            output[SPA_STEREO_UPMIX_CHANNEL_L] = input_l;
            output[SPA_STEREO_UPMIX_CHANNEL_R] = input_r;
            lfe = 0;
        } else {
            output[SPA_STEREO_UPMIX_CHANNEL_L] = spa_upmix_scale_s16_q15(
                input_l, state->config.front_gain_q15);
            output[SPA_STEREO_UPMIX_CHANNEL_R] = spa_upmix_scale_s16_q15(
                input_r, state->config.front_gain_q15);
        }
        output[SPA_STEREO_UPMIX_CHANNEL_C] = 0;
        output[SPA_STEREO_UPMIX_CHANNEL_LFE] = lfe;
        output[SPA_STEREO_UPMIX_CHANNEL_LS] = spa_upmix_rear_to_s16(
            rear_l, state->config.rear_gain_q15);
        output[SPA_STEREO_UPMIX_CHANNEL_RS] = spa_upmix_rear_to_s16(
            rear_r, state->config.rear_gain_q15);
    }

    return SPA_STEREO_UPMIX_OK;
}
