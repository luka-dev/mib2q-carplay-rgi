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
/* Low-mid body stays in front: RBJ low shelf -4 dB @ 500 Hz (S=1) on the
 * rear BEFORE the transfer, so nothing is removed from the front that the
 * rear does not play. */
static const struct spa_upmix_biquad_coefficients kSceneRearLowShelf = {
    0.989384126553696785,
    -1.89718730746555098,
    0.911035970849235044,
    -1.89624188905203539,
    0.901365515816446972
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

static int16_t spa_upmix_transfer_front(int16_t front, int16_t rear,
                                        uint32_t transfer_q15,
                                        uint32_t front_limit_q15)
{
    /* Transfer can exceed unity (2.5): do not narrow it to S16 before
     * subtraction. At max rear, 16384*2.5=40960 must not wrap negative. */
    int64_t product = (int64_t)rear * transfer_q15;
    int32_t transfer = product >= 0 ? (int32_t)((product + 16384) / 32768)
        : -(int32_t)((-product + 16384) / 32768);
    int32_t adjusted = (int32_t)front - transfer;
    int32_t limit = (int32_t)front_limit_q15;

    if (adjusted > limit) adjusted = limit;
    if (adjusted < -limit) adjusted = -limit;
    if (adjusted > INT16_MAX) adjusted = INT16_MAX;
    return (int16_t)adjusted;
}

static int16_t spa_upmix_float_to_s16(float value)
{
    long rounded = lrintf(spa_upmix_bound(value, 32767.0f));
    if (rounded < -32768L) rounded = -32768L;
    return (int16_t)rounded;
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

#ifdef SPA_STEREO_UPMIX_TESTING
float spa_stereo_upmix_test_rear_high_shelf(float input, float state[2])
{
    return spa_upmix_biquad(input, state, &kRearHighShelf4k3dB);
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

/* RBJ low shelf, S = 1, normalized to a0. */
static void spa_upmix_low_shelf(struct spa_upmix_biquad_coefficients *c,
                                float corner_hz, float gain_db)
{
    const float amp = powf(10.0f, gain_db / 40.0f);
    const float w0 = 6.28318530717958647693f * corner_hz /
        (float)SPA_STEREO_UPMIX_SAMPLE_RATE;
    const float cw = cosf(w0);
    const float alpha = sinf(w0) * 0.5f * sqrtf(2.0f);
    const float root = 2.0f * sqrtf(amp) * alpha;
    const float a0 = (amp + 1.0f) + (amp - 1.0f) * cw + root;

    c->b0 = amp * ((amp + 1.0f) - (amp - 1.0f) * cw + root) / a0;
    c->b1 = 2.0f * amp * ((amp - 1.0f) - (amp + 1.0f) * cw) / a0;
    c->b2 = amp * ((amp + 1.0f) - (amp - 1.0f) * cw - root) / a0;
    c->a1 = -2.0f * ((amp - 1.0f) + (amp + 1.0f) * cw) / a0;
    c->a2 = ((amp + 1.0f) + (amp - 1.0f) * cw - root) / a0;
}

/* RBJ high shelf, S = 1, normalized to a0. */
static void spa_upmix_high_shelf(struct spa_upmix_biquad_coefficients *c,
                                 float corner_hz, float gain_db)
{
    const float amp = powf(10.0f, gain_db / 40.0f);
    const float w0 = 6.28318530717958647693f * corner_hz /
        (float)SPA_STEREO_UPMIX_SAMPLE_RATE;
    const float cw = cosf(w0);
    const float alpha = sinf(w0) * 0.5f * sqrtf(2.0f);
    const float root = 2.0f * sqrtf(amp) * alpha;
    const float a0 = (amp + 1.0f) - (amp - 1.0f) * cw + root;

    c->b0 = amp * ((amp + 1.0f) + (amp - 1.0f) * cw + root) / a0;
    c->b1 = -2.0f * amp * ((amp - 1.0f) + (amp + 1.0f) * cw) / a0;
    c->b2 = amp * ((amp + 1.0f) + (amp - 1.0f) * cw - root) / a0;
    c->a1 = 2.0f * ((amp - 1.0f) - (amp + 1.0f) * cw) / a0;
    c->a2 = ((amp + 1.0f) - (amp - 1.0f) * cw - root) / a0;
}

static void spa_upmix_front_bass_prepare(struct spa_stereo_upmix *state)
{
    if (state->config.front_treble_db10)
        spa_upmix_high_shelf(&state->_runtime._front_treble_shelf,
                             (float)SPA_STEREO_UPMIX_FRONT_TREBLE_HZ,
                             (float)state->config.front_treble_db10 * 0.1f);
    /* The shipped -4 dB keeps its exact double-derived constants. */
    if (state->config.rear_body_db10 == SPA_STEREO_UPMIX_REAR_BODY_DB10_DEFAULT)
        state->_runtime._scene_rear_shelf = kSceneRearLowShelf;
    else
        spa_upmix_low_shelf(&state->_runtime._scene_rear_shelf, 500.0f,
                            (float)state->config.rear_body_db10 * 0.1f);
    if (state->config.front_bass_db10)
        spa_upmix_low_shelf(&state->_runtime._front_bass_shelf,
                            (float)SPA_STEREO_UPMIX_FRONT_BASS_HZ,
                            (float)state->config.front_bass_db10 * 0.1f);
}

#if !SPA_UPMIX_HAVE_NEON
/* S16-scale in, S16-scale out; the biquad works in normalized units. */
static float spa_upmix_front_bass(struct spa_stereo_upmix *state,
                                  unsigned channel, float value)
{
    if (!state->config.front_bass_db10) return value;
    return 32768.0f * spa_upmix_biquad(value * (1.0f / 32768.0f),
        state->_runtime._front_bass_state[channel],
        &state->_runtime._front_bass_shelf);
}
#endif

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
int16_t spa_stereo_upmix_test_transfer(int16_t front, int16_t rear,
                                      uint32_t transfer, uint32_t limit)
{ return spa_upmix_transfer_front(front, rear, transfer, limit); }
#endif

static int spa_upmix_config_valid(const struct spa_stereo_upmix_config *config)
{
    if (!config) return SPA_STEREO_UPMIX_ERROR_ARGUMENT;
    if (!spa_stereo_upmix_backend_supported(config->backend))
        return SPA_STEREO_UPMIX_ERROR_UNSUPPORTED;
    if (config->sample_rate_hz != SPA_STEREO_UPMIX_SAMPLE_RATE ||
        config->front_gain_q15 > UINT32_C(32768) ||
        config->front_retention_q15 > UINT32_C(32768) ||
        config->rear_gain_q15 > SPA_STEREO_UPMIX_REAR_Q15_MAX ||
        config->lfe_gain_q15 > SPA_STEREO_UPMIX_LFE_Q15_MAX ||
        config->lfe_enabled > 1u ||
        config->front_bass_db10 > 0 ||
        config->front_bass_db10 < SPA_STEREO_UPMIX_FRONT_BASS_DB10_MIN ||
        config->front_treble_db10 > 0 || config->front_treble_db10 < -120 ||
        config->rear_body_db10 > 0 || config->rear_body_db10 < -120 ||
        config->bed_delays > 1u || config->bed_attack > SPA_HSSR_BED_ATTACK_FRONT ||
        config->attack_collapse_q15 > UINT32_C(32768) ||
        config->retention_hf_lo_hz > UINT32_C(24000) ||
        config->retention_hf_hi_hz > UINT32_C(24000) ||
        config->vocal_gate_q15 > UINT32_C(32768) ||
        config->rear_air_q15 > UINT32_C(32768))
        return SPA_STEREO_UPMIX_ERROR_CONFIG;
    if (config->backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL &&
        !spa_hssr_model_ready(config->hssr_model))
        return SPA_STEREO_UPMIX_ERROR_CONFIG;
    if (config->hssr_control_provider &&
        (!config->hssr_control_provider->submit ||
         !config->hssr_control_provider->receive ||
         !config->hssr_control_provider->reset))
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

/* Rear-only delay: the dry-front transfer keeps using the undelayed rear. */
static void spa_scene_rear_delay(struct spa_stereo_upmix *state, float *rear_l, float *rear_r)
{
    uint32_t pos = state->_runtime._scene_rear_pos;
    float delayed_l = state->_runtime._scene_rear_delay[0][pos];
    float delayed_r = state->_runtime._scene_rear_delay[1][pos];

    state->_runtime._scene_rear_delay[0][pos] = *rear_l;
    state->_runtime._scene_rear_delay[1][pos] = *rear_r;
    state->_runtime._scene_rear_pos = pos + 1u == SPA_SCENE_REAR_DELAY_FRAMES ? 0u : pos + 1u;
    *rear_l = delayed_l;
    *rear_r = delayed_r;
}

static void spa_upmix_apply_bed_config(struct spa_hssr_dsp *hssr,
                                       const struct spa_stereo_upmix_config *config)
{
    hssr->bed_delays = config->bed_delays;
    hssr->bed_attack = config->bed_attack;
    hssr->attack_collapse = (float)config->attack_collapse_q15 / 32768.0f;
    hssr->front_retention = (float)config->front_retention_q15 / 32768.0f;
    hssr->retention_hf_lo_hz = (float)config->retention_hf_lo_hz;
    hssr->retention_hf_hi_hz = (float)config->retention_hf_hi_hz;
    hssr->vocal_gate = (float)config->vocal_gate_q15 / 32768.0f;
    hssr->rear_air = (float)config->rear_air_q15 / 32768.0f;
}

void spa_stereo_upmix_default_config(struct spa_stereo_upmix_config *config)
{
    if (!config) return;
    config->sample_rate_hz = SPA_STEREO_UPMIX_SAMPLE_RATE;
    config->front_gain_q15 = SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT;
    config->rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_DEFAULT;
    config->lfe_gain_q15 = SPA_STEREO_UPMIX_LFE_Q15_DEFAULT;
    config->lfe_enabled = 0;
    config->front_retention_q15 = 0;
    config->front_bass_db10 = 0;
    config->front_treble_db10 = 0;
    config->rear_body_db10 = SPA_STEREO_UPMIX_REAR_BODY_DB10_DEFAULT;
    config->bed_delays = 0;
    config->bed_attack = SPA_HSSR_BED_ATTACK_OFF;
    config->attack_collapse_q15 = 32768u;
    config->retention_hf_lo_hz = SPA_STEREO_UPMIX_RETENTION_HF_LO_HZ;
    config->retention_hf_hi_hz = SPA_STEREO_UPMIX_RETENTION_HF_HI_HZ;
    config->vocal_gate_q15 = SPA_STEREO_UPMIX_VOCAL_GATE_Q15_DEFAULT;
    config->rear_air_q15 = 0;
    config->backend = SPA_STEREO_UPMIX_OFF;
    config->hssr_model = NULL;
    config->hssr_control_provider = NULL;
    config->hssr_guard_model = NULL;
    config->hssr_mask_model = NULL;
    config->mask_drive_q15 = 0;
    config->mask_treble_db10 = 0;
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

void spa_stereo_upmix_config_set_hssr_guard_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_model *model)
{
    if (config) config->hssr_guard_model =
        model && spa_hssr_model_ready(model) ? model : NULL;
}

void spa_stereo_upmix_config_set_hssr_mask_treble(struct spa_stereo_upmix_config *config,
                                                  int32_t treble_db10)
{
    if (!config) return;
    if (treble_db10 > 120) treble_db10 = 120;
    if (treble_db10 < -120) treble_db10 = -120;
    config->mask_treble_db10 = treble_db10;
}

void spa_stereo_upmix_config_set_hssr_mask_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_mask_model *model, uint32_t drive_q15)
{
    if (!config) return;
    config->hssr_mask_model = model && spa_mask_model_ready(model) ? model : NULL;
    config->mask_drive_q15 = config->hssr_mask_model ? (drive_q15 > 65536u ? 65536u : drive_q15) : 0u;
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
        spa_upmix_apply_bed_config(&state->_runtime._hssr, &saved);
        state->_runtime._hssr.control_generation = 1u;
        state->_runtime._hssr.guard_model = saved.hssr_guard_model;
        spa_hssr_state_reset(&state->_runtime._hssr.guard_network);
        state->_runtime._hssr.mask_model = saved.hssr_mask_model;
        spa_hssr_dsp_set_mask_drive(&state->_runtime._hssr,
                                    (float)saved.mask_drive_q15 / 32768.0f, saved.mask_treble_db10);
        spa_mask_state_reset(&state->_runtime._hssr.mask_network);
        if (saved.hssr_control_provider)
            saved.hssr_control_provider->reset(
                saved.hssr_control_provider->context, 1u);
    }
    state->_runtime._lfe_polarity = 1.0f;
    state->_runtime._lfe_target_polarity = 1.0f;
    state->_runtime._lfe_dynamic_gain = 0.96f;
    state->_runtime._limiter_gain = 1.0f;
    spa_upmix_front_bass_prepare(state);
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
        spa_upmix_apply_bed_config(&state->_runtime._hssr, &state->config);
        state->_runtime._hssr.control_generation = generation;
        /* the memset above dropped the model pointers init() had handed
         * over; the hook resets on every stream start, so without this the
         * guard (synchronous path) and the mask net (both paths) die after
         * the first stream — found on the unit 2026-09-27: maskReady=0 */
        state->_runtime._hssr.guard_model = state->config.hssr_guard_model;
        state->_runtime._hssr.mask_model = state->config.hssr_mask_model;
        spa_hssr_dsp_set_mask_drive(&state->_runtime._hssr,
                                    (float)state->config.mask_drive_q15 / 32768.0f,
                                    state->config.mask_treble_db10);
        if (state->config.hssr_control_provider)
            state->config.hssr_control_provider->reset(
                state->config.hssr_control_provider->context, generation);
    }
    state->_runtime._lfe_polarity = 1.0f;
    state->_runtime._lfe_target_polarity = 1.0f;
    state->_runtime._lfe_dynamic_gain = 0.96f;
    state->_runtime._limiter_gain = 1.0f;
    spa_upmix_front_bass_prepare(state);
    return SPA_STEREO_UPMIX_OK;
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

    if (state->config.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL) {
        int hssr_status = spa_hssr_dsp_process_block_sources(
            &state->_runtime._hssr, state->config.hssr_model,
            state->config.hssr_control_provider,
            state->_input_scratch, frames,
            state->_hssr_front_scratch, state->_hssr_rear_scratch,
            state->_hssr_lfe_control_scratch, state->_hssr_source_scratch);

        if (hssr_status != SPA_HSSR_OK)
            return SPA_STEREO_UPMIX_ERROR_CONFIG;
    }

    for (frame = 0; frame < frames; frame++) {
        const int16_t input_l = state->_input_scratch[frame * 2];
        const int16_t input_r = state->_input_scratch[frame * 2 + 1];
        int16_t *output = (int16_t *)pcm6 + frame * SPA_STEREO_UPMIX_OUTPUT_CHANNELS;

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
            uint32_t retention_q15;

#if SPA_UPMIX_HAVE_NEON
            float32x2_t rear_pair = spa_upmix_biquad_pair(
                vld1_f32(&state->_hssr_rear_scratch[frame * 2]),
                state->_runtime._scene_rear_shelf_state[0],
                state->_runtime._scene_rear_shelf_state[1], &state->_runtime._scene_rear_shelf);
            /* Subtract the source-aligned object (x2.5 x rear 0.40 = once),
             * shelved the same way, never the synthetic bed. */
            float32x2_t transfer_pair = spa_upmix_biquad_pair(
                vld1_f32(&state->_hssr_source_scratch[frame * 2]),
                state->_runtime._scene_source_shelf_state[0],
                state->_runtime._scene_source_shelf_state[1], &state->_runtime._scene_rear_shelf);

            transfer_rear_l = vget_lane_f32(transfer_pair, 0);
            transfer_rear_r = vget_lane_f32(transfer_pair, 1);
#else
            rear_l = spa_upmix_biquad(rear_l,
                state->_runtime._scene_rear_shelf_state[0], &state->_runtime._scene_rear_shelf);
            rear_r = spa_upmix_biquad(rear_r,
                state->_runtime._scene_rear_shelf_state[1], &state->_runtime._scene_rear_shelf);
            /* Subtract the source-aligned object (x2.5 x rear 0.40 = once),
             * shelved the same way, never the synthetic bed. */
            transfer_rear_l = spa_upmix_biquad(state->_hssr_source_scratch[frame * 2],
                state->_runtime._scene_source_shelf_state[0], &state->_runtime._scene_rear_shelf);
            transfer_rear_r = spa_upmix_biquad(state->_hssr_source_scratch[frame * 2 + 1],
                state->_runtime._scene_source_shelf_state[1], &state->_runtime._scene_rear_shelf);
#endif
            if (state->config.lfe_enabled) {
                lfe_candidate = spa_upmix_lfe_to_s16(spa_upmix_lfe_v2_normalized(
                    state, render_l, render_r,
                    state->_hssr_lfe_control_scratch[frame * 2],
                    state->_hssr_lfe_control_scratch[frame * 2 + 1]),
                    state->config.lfe_gain_q15);
            }

#if SPA_UPMIX_HAVE_NEON
            rear_pair = spa_upmix_biquad_pair(rear_pair,
                state->_runtime._rear_high_shelf_state[0],
                state->_runtime._rear_high_shelf_state[1], &kRearHighShelf4k3dB);
            rear_l = vget_lane_f32(rear_pair, 0);
            rear_r = vget_lane_f32(rear_pair, 1);
#else
            rear_l = spa_upmix_biquad(
                rear_l, state->_runtime._rear_high_shelf_state[0],
                &kRearHighShelf4k3dB);
            rear_r = spa_upmix_biquad(
                rear_r, state->_runtime._rear_high_shelf_state[1],
                &kRearHighShelf4k3dB);
#endif

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

            spa_scene_rear_delay(state, &rear_l, &rear_r);
            /* With attack handling HSSR already applied the retention per band. */
            retention_q15 = state->config.bed_attack ? 0u : state->config.front_retention_q15;
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
                front_l, output_transfer_rear_l,
                (uint32_t)(((uint64_t)81920u *
                    (32768u - retention_q15) + 16384u) / 32768u),
                state->config.front_gain_q15);
            front_r = spa_upmix_transfer_front(
                front_r, output_transfer_rear_r,
                (uint32_t)(((uint64_t)81920u *
                    (32768u - retention_q15) + 16384u) / 32768u),
                state->config.front_gain_q15);
            if (state->config.front_bass_db10) {
#if SPA_UPMIX_HAVE_NEON
                float32x2_t bass = vmul_n_f32(spa_upmix_biquad_pair(
                    vmul_n_f32(spa_upmix_pair((float)front_l, (float)front_r),
                               1.0f / 32768.0f),
                    state->_runtime._front_bass_state[0],
                    state->_runtime._front_bass_state[1],
                    &state->_runtime._front_bass_shelf), 32768.0f);

                front_l = spa_upmix_float_to_s16(vget_lane_f32(bass, 0));
                front_r = spa_upmix_float_to_s16(vget_lane_f32(bass, 1));
#else
                front_l = spa_upmix_float_to_s16(
                    spa_upmix_front_bass(state, 0, (float)front_l));
                front_r = spa_upmix_float_to_s16(
                    spa_upmix_front_bass(state, 1, (float)front_r));
#endif
            }
            if (state->config.front_treble_db10) {
                front_l = spa_upmix_float_to_s16(32768.0f * spa_upmix_biquad(
                    (float)front_l * (1.0f / 32768.0f),
                    state->_runtime._front_treble_state[0],
                    &state->_runtime._front_treble_shelf));
                front_r = spa_upmix_float_to_s16(32768.0f * spa_upmix_biquad(
                    (float)front_r * (1.0f / 32768.0f),
                    state->_runtime._front_treble_state[1],
                    &state->_runtime._front_treble_shelf));
            }
            if (state->config.lfe_enabled)
                lfe = lfe_candidate;

            output[SPA_STEREO_UPMIX_CHANNEL_L] = front_l;
            output[SPA_STEREO_UPMIX_CHANNEL_R] = front_r;
            output[SPA_STEREO_UPMIX_CHANNEL_C] = 0;
            output[SPA_STEREO_UPMIX_CHANNEL_LFE] = lfe;
            output[SPA_STEREO_UPMIX_CHANNEL_LS] = output_rear_l;
            output[SPA_STEREO_UPMIX_CHANNEL_RS] = output_rear_r;
            spa_upmix_cabin_guard_s16(output, state->config.lfe_enabled);
        }
    }

    return SPA_STEREO_UPMIX_OK;
}

int spa_stereo_upmix_process_s16(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2, size_t frames,
                                 int16_t *pcm6, size_t pcm6_capacity_frames)
{
    return spa_upmix_process(state, pcm2, frames, pcm6, pcm6_capacity_frames);
}
