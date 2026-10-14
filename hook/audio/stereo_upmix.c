/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "stereo_upmix.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SPA_UPMIX_HAVE_NEON 1
#else
#define SPA_UPMIX_HAVE_NEON 0
#endif

#include <limits.h>
#include <math.h>
#include <string.h>

#define SPA_STEREO_UPMIX_MAGIC UINT32_C(0x5355504d) /* "SUPM" */

#ifdef SPA_STEREO_UPMIX_TESTING
void (*spa_stereo_upmix_test_scene_mask_observer)(uint64_t pushed, const float *elig,
                                                  const float *prot) = NULL;
#endif

/* One 48-kHz-rate, Q=1/sqrt(2) Butterworth section, cascaded twice below for
 * an LR4 (-24 dB/octave) low-pass.  Scene LFE corner 60 Hz: meets the front bass HP (62 Hz record) and stays
 * under the LFE-bus LP (66 Hz record), closing the 48-62 Hz hole. */
static const struct spa_upmix_biquad_coefficients kLfeLow = {
    1.5336008368362254e-05,
    3.06720167367245081e-05,
    1.5336008368362254e-05,
    -1.98889290589965317,
    0.988954249933126639
};
static const float kCabinSampleLimit = 32275.0f; /* 0.985 FS + rounding margin */

/* A903C/126A07: A07 rows are OUTPUTS. Transpose once here for the detector's
 * input-major API. One table is shared by the S16 and float guards. */
/* The floor stands in for "this channel does not feed this bus"; it is kept so
 * the table has no exact zeros, and it is small enough that a bus whose LFE
 * entry is the floor can never bound the LFE (see the guard). */
static const float kCabinMatrixFloor = 0.00001122018454f;
/* Largest bus column sum, rounded up: |bus value| <= this * max |sample|. */
static const float kCabinMatrixMaxColumnNorm = 3.53f;
#define CABIN_FLOOR 0.00001122018454f
static const float kInputMatrix[6][8] = {
    {0.794328235f, CABIN_FLOOR, 0.398107171f, 0.398107171f, 0.707945784f, CABIN_FLOOR, 0.707945784f, CABIN_FLOOR},
    {CABIN_FLOOR, 0.794328235f, 0.398107171f, 0.398107171f, CABIN_FLOOR, 0.707945784f, CABIN_FLOOR, 0.707945784f},
    {0.501187234f, 0.501187234f, 0.707945784f, 0.316227766f, CABIN_FLOOR, CABIN_FLOOR, CABIN_FLOOR, CABIN_FLOOR},
    {CABIN_FLOOR, CABIN_FLOOR, CABIN_FLOOR, 1.412537545f, CABIN_FLOOR, CABIN_FLOOR, CABIN_FLOOR, CABIN_FLOOR},
    {CABIN_FLOOR, CABIN_FLOOR, CABIN_FLOOR, 0.199526231f, 1.995262315f, CABIN_FLOOR, 2.818382931f, CABIN_FLOOR},
    {CABIN_FLOOR, CABIN_FLOOR, CABIN_FLOOR, 0.199526231f, CABIN_FLOOR, 1.995262315f, CABIN_FLOOR, 2.818382931f}
};
#undef CABIN_FLOOR

const float *spa_stereo_upmix_input_matrix(void)
{
    return &kInputMatrix[0][0];
}

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

static int16_t spa_upmix_float_to_s16(float value)
{
    long rounded = lrintf(spa_upmix_bound(value, 32767.0f));
    if (rounded < -32768L) rounded = -32768L;
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

/* Eight of these run per output frame and the body is a dozen operations, so
 * the call overhead was most of the cost; GCC leaves it out of line because
 * the caller is already large.  Same arithmetic, same order, same result. */
#if defined(__GNUC__)
__attribute__((always_inline, unused))
#endif
static inline float spa_upmix_biquad(
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

#if SPA_UPMIX_HAVE_NEON
/* spa_upmix_biquad for the L/R pair, one channel per lane: every lane does the
 * scalar operations in the scalar order, and the two states are independent.
 * The one NEON difference is denormal flushing, which the 1e-20 state flush
 * already applies to the state, and a denormal output is 0 in S16 either way. */
static inline float32x2_t spa_upmix_biquad_pair(
    float32x2_t input, float state_l[2], float state_r[2],
    const struct spa_upmix_biquad_coefficients *c)
{
    const float32x2_t limit = vdup_n_f32(4.0f);
    const float32x2_t negative_limit = vdup_n_f32(-4.0f);
    const float32x2_t tiny = vdup_n_f32(1.0e-20f);
    const float32x2_t zero = vdup_n_f32(0.0f);
    float32x2x2_t z = vtrn_f32(vld1_f32(state_l), vld1_f32(state_r));
    float32x2_t output = vadd_f32(vmul_n_f32(input, c->b0), z.val[0]);
    float32x2_t next_z1;
    float32x2_t next_z2;

    output = vmin_f32(vmax_f32(output, negative_limit), limit);
    next_z1 = vadd_f32(vsub_f32(vmul_n_f32(input, c->b1),
                                vmul_n_f32(output, c->a1)), z.val[1]);
    next_z2 = vsub_f32(vmul_n_f32(input, c->b2), vmul_n_f32(output, c->a2));
    next_z1 = vbsl_f32(vcalt_f32(next_z1, tiny), zero, next_z1);
    next_z2 = vbsl_f32(vcalt_f32(next_z2, tiny), zero, next_z2);
    z = vtrn_f32(vmin_f32(vmax_f32(next_z1, negative_limit), limit),
                 vmin_f32(vmax_f32(next_z2, negative_limit), limit));
    vst1_f32(state_l, z.val[0]);
    vst1_f32(state_r, z.val[1]);
    return output;
}

static inline float32x2_t spa_upmix_pair(float l, float r)
{
    return vset_lane_f32(r, vdup_n_f32(l), 1);
}
#endif

#if !SPA_UPMIX_HAVE_NEON
static float spa_upmix_lfe_lr4(
    float input, float state[2][2],
    const struct spa_upmix_biquad_coefficients *coefficients)
{
    float output = spa_upmix_biquad(
        input, state[0], coefficients);

    return spa_upmix_biquad(output, state[1], coefficients);
}
#endif

static float spa_upmix_lfe_v2_normalized(
    struct spa_stereo_upmix *state, int16_t render_l, int16_t render_r,
    float bass_confidence, float drum_confidence)
{
    const float normalized_l = (float)render_l * (1.0f / 32768.0f);
    const float normalized_r = (float)render_r * (1.0f / 32768.0f);
#if SPA_UPMIX_HAVE_NEON
    float32x2_t low48 = spa_upmix_biquad_pair(
        spa_upmix_pair(normalized_l, normalized_r),
        state->_runtime._lfe_low48_state[0][0],
        state->_runtime._lfe_low48_state[1][0], &kLfeLow);
    float low48_l;
    float low48_r;

    low48 = spa_upmix_biquad_pair(low48,
        state->_runtime._lfe_low48_state[0][1],
        state->_runtime._lfe_low48_state[1][1], &kLfeLow);
    low48_l = vget_lane_f32(low48, 0);
    low48_r = vget_lane_f32(low48, 1);
#else
    float low48_l = spa_upmix_lfe_lr4(
        normalized_l, state->_runtime._lfe_low48_state[0], &kLfeLow);
    float low48_r = spa_upmix_lfe_lr4(
        normalized_r, state->_runtime._lfe_low48_state[1], &kLfeLow);
#endif
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

    return sub_band * state->_runtime._lfe_dynamic_gain;
}

static void spa_upmix_cabin_guard_s16(int16_t sample[6], int adjust_lfe)
{
    unsigned bus, ch;
    float peak = 0.0f;

    /* Nothing here can bind while every bus row stays below the limit, and
     * |bus value| <= max column sum * max |sample|.  This is the per-frame hot
     * spot of the whole renderer, so leave before touching the matrix at all.
     * Exactly equivalent: the bound also covers the channel peaks, which are
     * then all below the limit too. */
    for (ch = 0; ch < 6; ++ch)
        if (spa_upmix_absf((float)sample[ch]) > peak)
            peak = spa_upmix_absf((float)sample[ch]);
    if (peak * kCabinMatrixMaxColumnNorm <= kCabinSampleLimit) return;
    peak = 0.0f;

    if (adjust_lfe) {
        float limited = (float)sample[3];
        /* Never synthesize extra LFE or reverse its sign to repair overload. */
        float minimum = limited < 0.0f ? limited : 0.0f;
        float maximum = limited > 0.0f ? limited : 0.0f;
        for (bus = 0; bus < 8; ++bus) {
            float base = 0.0f;
            float low, high;

            /* A floor LFE entry puts both bounds past 1e9 (|base| stays under
             * 1.2e5), so such a bus can never bound an S16 LFE sample.
             * Skipping it is exact and removes 14 of the 16 divisions. */
            if (kInputMatrix[3][bus] <= kCabinMatrixFloor) continue;
            for (ch = 0; ch < 6; ++ch)
                if (ch != 3) base += kInputMatrix[ch][bus] * sample[ch];
            low = (-kCabinSampleLimit - base) / kInputMatrix[3][bus];
            high = (kCabinSampleLimit - base) / kInputMatrix[3][bus];
            if (low > minimum) minimum = low;
            if (high < maximum) maximum = high;
        }
        minimum = ceilf(minimum); maximum = floorf(maximum);
        if (minimum <= maximum) {
            if (limited < minimum) limited = minimum;
            if (limited > maximum) limited = maximum;
            sample[3] = (int16_t)limited;
        }
        /* If LFE alone cannot repair it, preserve the candidate for the
         * linked fallback below. Returning LFE=0 is not a rail guarantee. */
    }
    for (ch = 0; ch < 6; ++ch)
        if (fabsf((float)sample[ch]) > peak) peak = fabsf((float)sample[ch]);
#if SPA_UPMIX_HAVE_NEON
    /* The table is input-major, so one row is the eight bus coefficients of one
     * channel: the whole 8x6 product is six scaled row accumulations instead of
     * forty-eight scalar ones.  Each product is rounded on its own and the
     * channels accumulate in the same order, so the result is the scalar one.
     * This is the per-frame cost that the early-out cannot remove on loud
     * material, which is exactly when the realtime deadline is at risk. */
    {
        float32x4_t low = vdupq_n_f32(0.0f);
        float32x4_t high = vdupq_n_f32(0.0f);
        float bus_value[8];

        for (ch = 0; ch < 6; ++ch) {
            float32x4_t scale = vdupq_n_f32((float)sample[ch]);

            low = vaddq_f32(low, vmulq_f32(vld1q_f32(&kInputMatrix[ch][0]), scale));
            high = vaddq_f32(high, vmulq_f32(vld1q_f32(&kInputMatrix[ch][4]), scale));
        }
        vst1q_f32(bus_value, low);
        vst1q_f32(bus_value + 4, high);
        for (bus = 0; bus < 8; ++bus)
            if (fabsf(bus_value[bus]) > peak) peak = fabsf(bus_value[bus]);
    }
#else
    for (bus = 0; bus < 8; ++bus) {
        float value = 0.0f;
        for (ch = 0; ch < 6; ++ch) value += kInputMatrix[ch][bus] * sample[ch];
        if (fabsf(value) > peak) peak = fabsf(value);
    }
#endif
    if (peak > kCabinSampleLimit) {
        /* Eight S16 units reserve more than the largest row's aggregate
         * truncation error, including cancellation and float roundoff. */
        float gain = (kCabinSampleLimit - 8.0f) / peak;
        for (ch = 0; ch < 6; ++ch) sample[ch] = (int16_t)(sample[ch] * gain);
    }
}

#ifdef SPA_STEREO_UPMIX_TESTING
void spa_stereo_upmix_test_guard_s16(int16_t sample[6], int adjust_lfe)
{ spa_upmix_cabin_guard_s16(sample, adjust_lfe); }
#endif

/* NaN lands on lo: the comparisons fail and the setter never stores it. */
static float spa_upmix_clampf(float value, float lo, float hi)
{
    return value >= lo ? (value <= hi ? value : hi) : lo;
}

void spa_stereo_upmix_config_set_scene_alpha(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.alpha = spa_upmix_clampf(value, 0.0f, 1.0f); }
void spa_stereo_upmix_config_set_scene_inst_alpha(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.inst_alpha = spa_upmix_clampf(value, 0.0f, 1.0f); }
void spa_stereo_upmix_config_set_scene_inst_gamma(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.inst_gamma = spa_upmix_clampf(value, 0.1f, 4.0f); }
void spa_stereo_upmix_config_set_scene_duck_depth(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.duck_depth = spa_upmix_clampf(value, 0.0f, 1.0f); }
void spa_stereo_upmix_config_set_scene_rear_delay_ms(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.rear_delay_ms = spa_upmix_clampf(value, -50.0f, 50.0f); }
void spa_stereo_upmix_config_set_scene_sigma_min(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.sigma_min = spa_upmix_clampf(value, 0.01f, 1.0f); }
void spa_stereo_upmix_config_set_scene_side_lp_hz(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.side_lp_hz = value > 0.0f ? spa_upmix_clampf(value, 1000.0f, 24000.0f) : 0.0f; }
void spa_stereo_upmix_config_set_scene_bleed_cancel(struct spa_stereo_upmix_config *config, float value)
{ if (config) config->scene.bleed_cancel = spa_upmix_clampf(value, 0.0f, 0.5f); }

void spa_stereo_upmix_config_set_scene_mask_blob(struct spa_stereo_upmix_config *config,
                                                 const void *blob, size_t bytes)
{
    const struct scene_mask_weights *w = (const struct scene_mask_weights *)blob;

    if (!config) return;
    /* the same checks scene_mask_init() makes, so init() can never fail on the blob */
    config->scene_mask_blob = w && bytes == sizeof(*w) &&
        memcmp(w->magic, SMASK_MAGIC, sizeof(w->magic)) == 0 &&
        w->ch == SMASK_CH && w->hidden == SMASK_HID && w->n_out == SMASK_OUT ? w : NULL;
}

void spa_stereo_upmix_config_set_scene_mask_provider(struct spa_stereo_upmix_config *config,
                                                     const struct spa_scene_mask_provider *provider)
{
    if (config) config->scene_mask_provider = provider;
}

/* A scene config is valid iff the setters leave it unchanged (one place holds
 * the ranges) and the frame geometry is the engine's. */
static int spa_scene_config_valid(const struct spa_stereo_upmix_config *config)
{
    struct spa_stereo_upmix_config probe = *config;

    spa_stereo_upmix_config_set_scene_alpha(&probe, probe.scene.alpha);
    spa_stereo_upmix_config_set_scene_inst_alpha(&probe, probe.scene.inst_alpha);
    spa_stereo_upmix_config_set_scene_inst_gamma(&probe, probe.scene.inst_gamma);
    spa_stereo_upmix_config_set_scene_duck_depth(&probe, probe.scene.duck_depth);
    spa_stereo_upmix_config_set_scene_rear_delay_ms(&probe, probe.scene.rear_delay_ms);
    spa_stereo_upmix_config_set_scene_sigma_min(&probe, probe.scene.sigma_min);
    spa_stereo_upmix_config_set_scene_side_lp_hz(&probe, probe.scene.side_lp_hz);
    spa_stereo_upmix_config_set_scene_bleed_cancel(&probe, probe.scene.bleed_cancel);
    return config->scene.n_fft == SCENE_NFFT && config->scene.hop == SCENE_HOP &&
           memcmp(&probe.scene, &config->scene, sizeof(probe.scene)) == 0;
}

static int spa_upmix_config_valid(const struct spa_stereo_upmix_config *config)
{
    if (!config) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (!spa_stereo_upmix_backend_supported(config->backend))
        return SPA_STEREO_UPMIX_ERROR_UNSUPPORTED;
    if (config->sample_rate_hz != SPA_STEREO_UPMIX_SAMPLE_RATE ||
        config->front_gain_q15 > UINT32_C(32768) ||
        config->lfe_gain_q15 > SPA_STEREO_UPMIX_LFE_Q15_MAX ||
        config->lfe_enabled > 1u ||
        config->level < 0 || config->level >= SPA_UPMIX_LEVEL_COUNT)
        return SPA_STEREO_UPMIX_ERROR_CONFIG;
    if (config->backend == SPA_STEREO_UPMIX_SCENE && !spa_scene_config_valid(config))
        return SPA_STEREO_UPMIX_ERROR_CONFIG;
    if (config->scene_mask_provider &&
        (!config->scene_mask_provider->submit ||
         !config->scene_mask_provider->receive ||
         !config->scene_mask_provider->reset))
        return SPA_STEREO_UPMIX_ERROR_CONFIG;

    /* Config bounds are not a matrix-headroom proof. S16 uses the linked
     * frame guard; float callers must apply post-synthesis protection. */
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
    config->lfe_gain_q15 = SPA_STEREO_UPMIX_LFE_Q15_DEFAULT;
    config->lfe_enabled = 0;
    config->backend = SPA_STEREO_UPMIX_OFF;
    scene_config_defaults(&config->scene);
    config->scene_mask_blob = NULL;
    config->scene_mask_provider = NULL;
    config->level = SPA_UPMIX_LEVEL_NORMAL;
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
           backend == SPA_STEREO_UPMIX_SCENE;
}

int spa_stereo_upmix_scene_set_mask(struct spa_stereo_upmix *state,
                                    const float *elig, const float *prot,
                                    int bins)
{
    if (!state) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (state->_initialized != SPA_STEREO_UPMIX_MAGIC)
        return SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED;
    if (state->config.backend != SPA_STEREO_UPMIX_SCENE)
        return SPA_STEREO_UPMIX_ERROR_UNSUPPORTED;
    scene_engine_set_mask(&state->_runtime._scene, elig, prot, bins);
    return SPA_STEREO_UPMIX_OK;
}

int spa_stereo_upmix_scene_track_change(struct spa_stereo_upmix *state)
{
    if (!state) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (state->_initialized != SPA_STEREO_UPMIX_MAGIC)
        return SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED;
    if (state->config.backend != SPA_STEREO_UPMIX_SCENE)
        return SPA_STEREO_UPMIX_ERROR_UNSUPPORTED;
    /* The engine only; the net's stream carries on, on the audio thread and
     * on the worker alike (a reset() is what re-primes it). */
    scene_engine_reset(&state->_runtime._scene);
    return SPA_STEREO_UPMIX_OK;
}

/* SCENE: arm the engine (init(): tables + state; reset(): the state only, the
 * tables are kept because a reset runs inside the realtime period callback),
 * the mask net (init from the blob; reset = a fresh stream; with a provider
 * the worker owns the net and gets the generation instead), the FIFO primed
 * with the latency of zeros and the accumulator half full of zeros
 * (SPA_SCENE_MASK_PREFILL_FRAMES).  The engine refuses only what the public
 * scene fields can still express (erb_pool 0, unmix without cluster): a config
 * error at init(). */
static void spa_upmix_apply_level(struct spa_stereo_upmix *state);
static void spa_upmix_glide_tuning(struct spa_stereo_upmix *state, size_t frames, int land);

static int spa_scene_arm(struct spa_stereo_upmix *state, int first)
{
    /* Process-wide, so every init() and reset() of any instance is a generation
     * the worker has never seen: a worker reused across init() (the realtime
     * path keeps its one) resets its net and its old rows are never accepted. */
    static volatile uint32_t generation_counter;
    struct spa_stereo_upmix_runtime *rt = &state->_runtime;
    const struct spa_scene_mask_provider *provider = state->config.scene_mask_provider;
    int status = 0;

    if (first) {
        status = scene_engine_init(&rt->_scene, &state->config.scene);
        rt->_scene_mask_on = provider != NULL ||
            (state->config.scene_mask_blob &&
             scene_mask_init(&rt->_scene_mask, state->config.scene_mask_blob) == 0);
    } else {
        scene_engine_reset_state(&rt->_scene);
        if (rt->_scene_mask_on && !provider) scene_mask_reset(&rt->_scene_mask);
    }
    do {
        rt->_scene_mask_generation =
            __atomic_add_fetch(&generation_counter, 1u, __ATOMIC_RELAXED);
    } while (!rt->_scene_mask_generation);
    if (provider) provider->reset(provider->context, rt->_scene_mask_generation);
    rt->_scene_mask_sequence = 0;
    memset(&rt->_scene_mask_row, 0, sizeof(rt->_scene_mask_row));
    memset(rt->_scene_fifo, 0, sizeof(rt->_scene_fifo));
    rt->_scene_fifo_count = SPA_SCENE_LATENCY_FRAMES;
    memset(rt->_dry_ring, 0, sizeof(rt->_dry_ring));
    rt->_dry_pos = 0;
    memset(rt->_scene_mask_acc, 0, sizeof(rt->_scene_mask_acc));
    rt->_scene_mask_fill = SPA_SCENE_MASK_PREFILL_FRAMES;
    rt->_scene_pushed = 0;
    rt->_scene_lfe_bass = 0.0f;
    rt->_scene_lfe_drum = 0.0f;
    return status == 0 ? SPA_STEREO_UPMIX_OK : SPA_STEREO_UPMIX_ERROR_CONFIG;
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
    /* SCENE leaves at the engine's levels; front_gain_q15 is only the LFE
     * assist's paired front headroom. */
    if (saved.backend == SPA_STEREO_UPMIX_SCENE && !saved.lfe_enabled)
        saved.front_gain_q15 = 32768u;
    memset(state, 0, sizeof(*state));
    state->config = saved;
    if (saved.backend == SPA_STEREO_UPMIX_SCENE) {
        status = spa_scene_arm(state, 1);
        if (status != SPA_STEREO_UPMIX_OK) return status;
    }
    state->_runtime._lfe_polarity = 1.0f;
    state->_runtime._lfe_target_polarity = 1.0f;
    state->_runtime._lfe_dynamic_gain = 0.96f;
    /* start at the configured level, tuned and without a fade */
    state->_tune[0] = state->_tune_target[0] = saved.scene.alpha;
    state->_tune[1] = state->_tune_target[1] = saved.scene.inst_alpha;
    state->_tune[2] = state->_tune_target[2] = saved.scene.duck_depth;
    state->_tune[3] = state->_tune_target[3] = saved.scene.side_lp_hz;
    state->_level_applied = SPA_UPMIX_LEVEL_NORMAL;
    state->_level_req = saved.level;
    if (saved.backend == SPA_STEREO_UPMIX_SCENE) {
        spa_upmix_apply_level(state);
        spa_upmix_glide_tuning(state, 0, 1);
    }
    state->_wet = state->_level_req != SPA_UPMIX_LEVEL_OFF ? 1.0f : 0.0f;
    state->_initialized = SPA_STEREO_UPMIX_MAGIC;
    return SPA_STEREO_UPMIX_OK;
}

int spa_stereo_upmix_reset(struct spa_stereo_upmix *state)
{
    if (!state) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (state->_initialized != SPA_STEREO_UPMIX_MAGIC)
        return SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED;
    /* Everything up to the SCENE part; the engine and the mask net keep their
     * tables and re-arm their state below (a reset runs inside the realtime
     * period callback: no table build, no 460 KB memset). */
    memset(&state->_runtime, 0, offsetof(struct spa_stereo_upmix_runtime, _scene));
    memset(state->_input_scratch, 0, sizeof(state->_input_scratch));
    memset(state->_scene_input_scratch, 0, sizeof(state->_scene_input_scratch));
    if (state->config.backend == SPA_STEREO_UPMIX_SCENE)
        (void)spa_scene_arm(state, 0);   /* same config init() accepted */
    state->_wet = state->_level_applied != SPA_UPMIX_LEVEL_OFF ? 1.0f : 0.0f;   /* a new stream starts at its level */
    if (state->config.backend == SPA_STEREO_UPMIX_SCENE) spa_upmix_glide_tuning(state, 0, 1);
    state->_runtime._lfe_polarity = 1.0f;
    state->_runtime._lfe_target_polarity = 1.0f;
    state->_runtime._lfe_dynamic_gain = 0.96f;
    return SPA_STEREO_UPMIX_OK;
}

/* The MMI levels other than NORMAL (= the configured AUDIO_SCENE_* scene):
 * alpha, inst_alpha, duck_depth, side_lp_hz.  Simulated 2026-10-13 through the
 * hook renderer and the J525 electronic model: FULL lifts our rear over the
 * amp's own front copy in the rear doors by +1.8 dB above 7 kHz with the vocal
 * still ~9 dB under the front (docs/audio/STEREO_TO_5_1_UPMIX.md). */
static const float kUpmixLevels[SPA_UPMIX_LEVEL_COUNT][4] = {
    { 0.0f, 0.0f, 0.25f,  7000.0f },   /* OFF: bypass; tuning unused */
    { 0.5f, 0.7f, 0.35f,  7000.0f },   /* MODERATE */
    { 0.7f, 0.9f, 0.25f,  7000.0f },   /* NORMAL: replaced by the config */
    { 0.9f, 1.0f, 0.15f, 11000.0f },   /* FULL */
    { 1.0f, 1.0f, 0.10f, 12000.0f },   /* AGGRESSIVE */
};

void spa_stereo_upmix_set_level(struct spa_stereo_upmix *state, int level)
{
    if (state && level >= 0 && level < SPA_UPMIX_LEVEL_COUNT) state->_level_req = level;
}

/* the level's tuning target; OFF keeps the current one running under the fade-out */
static void spa_upmix_apply_level(struct spa_stereo_upmix *state)
{
    const int level = state->_level_req;
    const struct scene_config *c = &state->config.scene;
    int k;

    if (level == state->_level_applied) return;
    state->_level_applied = level;
    if (level == SPA_UPMIX_LEVEL_OFF) return;
    for (k = 0; k < 4; k++) state->_tune_from[k] = state->_tune[k];
    state->_tune_t = 0.0f;
    if (level == SPA_UPMIX_LEVEL_NORMAL) {
        state->_tune_target[0] = c->alpha;
        state->_tune_target[1] = c->inst_alpha;
        state->_tune_target[2] = c->duck_depth;
        state->_tune_target[3] = c->side_lp_hz;
    } else {
        for (k = 0; k < 4; k++) state->_tune_target[k] = kUpmixLevels[level][k];
    }
}

/* the tuning from where it was toward the level's target, linear over SPA_UPMIX_LEVEL_FADE_MS
 * in this block's frames (all four knobs arrive together); land = jump to the target */
static void spa_upmix_glide_tuning(struct spa_stereo_upmix *state, size_t frames, int land)
{
    float *cur = state->_tune;
    float t;
    int k;

    if (state->_tune_t >= 1.0f && !land) return;
    t = land ? 1.0f : state->_tune_t + (float)frames * 1000.0f /
                                        ((float)SPA_UPMIX_LEVEL_FADE_MS * (float)SCENE_SR);
    if (t > 1.0f) t = 1.0f;
    state->_tune_t = t;
    for (k = 0; k < 4; k++)
        cur[k] = t >= 1.0f ? state->_tune_target[k]
                           : state->_tune_from[k] + (state->_tune_target[k] - state->_tune_from[k]) * t;
    scene_engine_set_tuning(&state->_runtime._scene, cur[0], cur[1], cur[2], cur[3]);
}

/* The net's bass (post[2]) and drum-core (post[1]) posteriors -> the LFE V2
 * confidences (spec §18 item 4).  The retired Universal renderer fed it q6d's
 * bass/drum posteriors of its three lowest bands (0..125 Hz) weighted by the
 * bands' slow energy, smoothed per 128-frame hop with 0.30 attack / 0.06
 * release and delayed to its front (hssr_dsp.c at tag
 * upmix-universal-last-2026-10-09).  The
 * closest equivalent here: the posteriors of the 512-point bins 0..1
 * (0..141 Hz) weighted by the control's fine-spectrum energy of each bin's
 * 93.75-Hz band, the same constants over the control's four hops
 * (1 - 0.70^4, 1 - 0.94^4), held for the control's 512 frames.  The control's
 * 2048-frame window ends 256 frames past the boundary and the output frames it
 * steers start 1024 frames behind it, so its centre sits at the start of those
 * frames: no extra delay line.  The raw pair comes from
 * spa_scene_mask_lfe_posteriors() (scene_mask_worker.h: the same arithmetic on
 * the audio thread's net and on the worker's). */
static void spa_scene_lfe_smooth(struct spa_stereo_upmix_runtime *rt, float bass, float drum)
{
    float alpha;

    alpha = bass > rt->_scene_lfe_bass ? 0.7599f : 0.2193f;
    rt->_scene_lfe_bass += alpha * (bass - rt->_scene_lfe_bass);
    alpha = drum > rt->_scene_lfe_drum ? 0.7599f : 0.2193f;
    rt->_scene_lfe_drum += alpha * (drum - rt->_scene_lfe_drum);
}

/* SCENE mask control boundary: 512 frames are in the accumulator.  Without a
 * provider the net runs here and its row is installed now (the Python's
 * timing).  With a provider the newest row the worker finished is installed
 * first, then these 512 frames are handed over: the worker's row for them
 * arrives at a later boundary (scene_mask_worker.h).  Returns the installed
 * row's elig/prot (NULL while priming or when the worker has nothing new). */
static void spa_scene_mask_boundary(struct spa_stereo_upmix *state,
                                    const float **elig, const float **prot)
{
    struct spa_stereo_upmix_runtime *rt = &state->_runtime;
    const struct spa_scene_mask_provider *provider = state->config.scene_mask_provider;
    float bass, drum;

    *elig = *prot = NULL;
    if (provider) {
        if (provider->receive(provider->context, rt->_scene_mask_generation,
                              &rt->_scene_mask_row)) {
            *elig = rt->_scene_mask_row.elig;
            *prot = rt->_scene_mask_row.prot;
            spa_scene_lfe_smooth(rt, rt->_scene_mask_row.lfe_bass, rt->_scene_mask_row.lfe_drum);
        }
        if (!++rt->_scene_mask_sequence) rt->_scene_mask_sequence = 1u;
        /* refused when the worker is behind: these frames are skipped, never waited for */
        (void)provider->submit(provider->context, rt->_scene_mask_generation,
                               rt->_scene_mask_sequence, rt->_scene_mask_acc);
    } else if (scene_mask_control(&rt->_scene_mask, rt->_scene_mask_acc, NULL, NULL)) {
        *elig = rt->_scene_mask.elig;
        *prot = rt->_scene_mask.prot;
        spa_scene_mask_lfe_posteriors(&rt->_scene_mask, &bass, &drum);
        spa_scene_lfe_smooth(rt, bass, drum);
    }
    if (*elig) scene_engine_set_mask(&rt->_scene, *elig, *prot, SMASK_BINS);
}

/* SCENE: staged int16 stereo -> float -> engine -> FIFO (primed with the
 * latency of zeros, so a pop of `frames` is never short) -> int16 PCM6 with
 * the LFE V2 assist, the front headroom and the cabin guard.  The engine's
 * float output is L R 0 0 Ls Rs at unity scale. */
static void spa_scene_process(struct spa_stereo_upmix *state, size_t frames,
                              int16_t *pcm6)
{
    struct spa_stereo_upmix_runtime *rt = &state->_runtime;
    const size_t channels = SPA_STEREO_UPMIX_OUTPUT_CHANNELS;
    size_t i, frame, pop, done = 0;
    const float target = state->_level_applied != SPA_UPMIX_LEVEL_OFF ? 1.0f : 0.0f;
    const float step = 1000.0f / ((float)SPA_UPMIX_LEVEL_FADE_MS * (float)SCENE_SR);

    for (i = 0; i < frames * SPA_STEREO_UPMIX_INPUT_CHANNELS; i++)
        state->_scene_input_scratch[i] =
            (float)state->_input_scratch[i] * (1.0f / 32768.0f);
    /* The push is split on the mask cadence (SPA_SCENE_MASK_PREFILL_FRAMES in
     * the header): whenever 512 frames have accumulated the net runs on them
     * and its row is installed before the next frame is pushed, so it applies
     * to the frames 2r, 2r+1 of the Python indexing.  Without a net the block
     * goes in whole (spatial only, no row ever installed). */
    while (done < frames) {
        const float *in = state->_scene_input_scratch +
            done * SPA_STEREO_UPMIX_INPUT_CHANNELS;
        size_t n = frames - done;

        if (rt->_scene_mask_on) {
            size_t room = SCENE_MASK_HOP - rt->_scene_mask_fill;

            if (n > room) n = room;
            memcpy(rt->_scene_mask_acc +
                   rt->_scene_mask_fill * SPA_STEREO_UPMIX_INPUT_CHANNELS,
                   in, n * SPA_STEREO_UPMIX_INPUT_CHANNELS * sizeof(float));
            rt->_scene_mask_fill += (uint32_t)n;
        }
        rt->_scene_fifo_count += (uint32_t)scene_engine_push(
            &rt->_scene, in, n,
            rt->_scene_fifo + rt->_scene_fifo_count * channels,
            SPA_SCENE_FIFO_FRAMES - rt->_scene_fifo_count);
        rt->_scene_pushed += n;
        done += n;
        if (rt->_scene_mask_on && rt->_scene_mask_fill == SCENE_MASK_HOP) {
            const float *elig, *prot;

            rt->_scene_mask_fill = 0;
            spa_scene_mask_boundary(state, &elig, &prot);
#ifdef SPA_STEREO_UPMIX_TESTING
            if (spa_stereo_upmix_test_scene_mask_observer)
                spa_stereo_upmix_test_scene_mask_observer(rt->_scene_pushed, elig, prot);
#endif
        }
    }
    pop = rt->_scene_fifo_count < frames ? rt->_scene_fifo_count : frames;

    for (frame = 0; frame < frames; frame++) {
        const float *in = rt->_scene_fifo + frame * channels;
        int16_t *output = pcm6 + frame * channels;
        int16_t *dry = rt->_dry_ring + rt->_dry_pos * SPA_STEREO_UPMIX_INPUT_CHANNELS;
        const int16_t dry_l = dry[0], dry_r = dry[1];
        int16_t render_l = 0, render_r = 0, rear_l = 0, rear_r = 0, lfe = 0;
        float w, g;

        /* the input SPA_SCENE_LATENCY_FRAMES ago: aligned with this output frame */
        dry[0] = state->_input_scratch[frame * SPA_STEREO_UPMIX_INPUT_CHANNELS];
        dry[1] = state->_input_scratch[frame * SPA_STEREO_UPMIX_INPUT_CHANNELS + 1];
        if (++rt->_dry_pos == SPA_SCENE_LATENCY_FRAMES) rt->_dry_pos = 0;
        if (frame < pop) {
            render_l = spa_upmix_float_to_s16(in[0] * 32768.0f);
            render_r = spa_upmix_float_to_s16(in[1] * 32768.0f);
            rear_l = spa_upmix_float_to_s16(in[4] * 32768.0f);
            rear_r = spa_upmix_float_to_s16(in[5] * 32768.0f);
        }
        /* Level steering from the net's bass/drum posteriors (0/0 = from the
         * front alone until the net's first control, or without a net). */
        if (state->config.lfe_enabled)
            lfe = spa_upmix_lfe_to_s16(
                spa_upmix_lfe_v2_normalized(state, render_l, render_r,
                                            rt->_scene_lfe_bass, rt->_scene_lfe_drum),
                state->config.lfe_gain_q15);
        output[SPA_STEREO_UPMIX_CHANNEL_L] =
            spa_upmix_scale_s16_q15(render_l, state->config.front_gain_q15);
        output[SPA_STEREO_UPMIX_CHANNEL_R] =
            spa_upmix_scale_s16_q15(render_r, state->config.front_gain_q15);
        output[SPA_STEREO_UPMIX_CHANNEL_C] = 0;
        output[SPA_STEREO_UPMIX_CHANNEL_LFE] = lfe;
        output[SPA_STEREO_UPMIX_CHANNEL_LS] = rear_l;
        output[SPA_STEREO_UPMIX_CHANNEL_RS] = rear_r;
        w = state->_wet;
        if (w != target) {
            w = target > w ? (w + step < target ? w + step : target) : (w - step > target ? w - step : target);
            state->_wet = w;
        }
        if (w < 1.0f) {
            /* level fade: ease in/out between the scene and the plain stereo */
            g = w * w * (3.0f - 2.0f * w);
            output[SPA_STEREO_UPMIX_CHANNEL_L] = spa_upmix_float_to_s16(
                g * (float)output[SPA_STEREO_UPMIX_CHANNEL_L] + (1.0f - g) * (float)dry_l);
            output[SPA_STEREO_UPMIX_CHANNEL_R] = spa_upmix_float_to_s16(
                g * (float)output[SPA_STEREO_UPMIX_CHANNEL_R] + (1.0f - g) * (float)dry_r);
            output[SPA_STEREO_UPMIX_CHANNEL_LFE] = spa_upmix_float_to_s16(g * (float)lfe);
            output[SPA_STEREO_UPMIX_CHANNEL_LS] = spa_upmix_float_to_s16(g * (float)rear_l);
            output[SPA_STEREO_UPMIX_CHANNEL_RS] = spa_upmix_float_to_s16(g * (float)rear_r);
        }
        spa_upmix_cabin_guard_s16(output, state->config.lfe_enabled);
    }
    rt->_scene_fifo_count -= (uint32_t)pop;
    memmove(rt->_scene_fifo, rt->_scene_fifo + pop * channels,
            rt->_scene_fifo_count * channels * sizeof(float));
}

static int spa_upmix_process(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2,
                                 size_t frames,
                                 void *pcm6,
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
    output_bytes = frames * SPA_STEREO_UPMIX_OUTPUT_CHANNELS * sizeof(int16_t);
    if (spa_upmix_ranges_overlap(state, sizeof(*state), pcm2, input_bytes) ||
        spa_upmix_ranges_overlap(state, sizeof(*state), pcm6, output_bytes))
        return SPA_STEREO_UPMIX_ERROR_STATE_OVERLAP;
    memcpy(state->_input_scratch, pcm2, input_bytes);

    if (state->config.backend == SPA_STEREO_UPMIX_SCENE) {
        spa_upmix_apply_level(state);
        spa_upmix_glide_tuning(state, frames, 0);
        spa_scene_process(state, frames, (int16_t *)pcm6);
        return SPA_STEREO_UPMIX_OK;
    }
    /* OFF: the exact bypass, [L, R, 0, 0, 0, 0]. */
    for (frame = 0; frame < frames; frame++) {
        const int16_t input_l = state->_input_scratch[frame * 2];
        const int16_t input_r = state->_input_scratch[frame * 2 + 1];
        int16_t *output = (int16_t *)pcm6 + frame * SPA_STEREO_UPMIX_OUTPUT_CHANNELS;

        output[SPA_STEREO_UPMIX_CHANNEL_L] = input_l;
        output[SPA_STEREO_UPMIX_CHANNEL_R] = input_r;
        output[SPA_STEREO_UPMIX_CHANNEL_C] = 0;
        output[SPA_STEREO_UPMIX_CHANNEL_LFE] = 0;
        output[SPA_STEREO_UPMIX_CHANNEL_LS] = 0;
        output[SPA_STEREO_UPMIX_CHANNEL_RS] = 0;
    }

    return SPA_STEREO_UPMIX_OK;
}

int spa_stereo_upmix_process_s16(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2, size_t frames,
                                 int16_t *pcm6, size_t pcm6_capacity_frames)
{
    return spa_upmix_process(state, pcm2, frames, pcm6, pcm6_capacity_frames);
}
