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

/* Offline evaluation builds may override these separation controls without
 * changing the runtime ABI.  Production builds use the measured defaults.
 * Keeping it compile-time-only prevents a vehicle environment typo from
 * turning coherent vocals into an unexpectedly loud rear feed. */
#ifndef SPA_STEREO_UPMIX_TD_CORRELATION_LOW
#define SPA_STEREO_UPMIX_TD_CORRELATION_LOW 0.45f
#endif
#ifndef SPA_STEREO_UPMIX_TD_CORRELATION_HIGH
#define SPA_STEREO_UPMIX_TD_CORRELATION_HIGH 0.92f
#endif
#ifndef SPA_STEREO_UPMIX_TD_COVARIANCE_ATTACK
#define SPA_STEREO_UPMIX_TD_COVARIANCE_ATTACK 0.02f
#endif
#ifndef SPA_STEREO_UPMIX_TD_COVARIANCE_RELEASE
#define SPA_STEREO_UPMIX_TD_COVARIANCE_RELEASE 0.0026f
#endif

/* hsr-td-active keeps the HSR classifier intact and only adapts the level of
 * residual that it has already accepted as spatial.  The two gates prevent
 * a low-level numerical residual or a briefly uncertain coherent source from
 * being promoted.  Rise is deliberately slower than release so vocals snap
 * back to the front rather than hanging in the rear field. */
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_POWER_ALPHA
#define SPA_STEREO_UPMIX_TD_ACTIVE_POWER_ALPHA 0.01f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_ALPHA
#define SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_ALPHA 0.0772553f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_RATIO_LOW
#define SPA_STEREO_UPMIX_TD_ACTIVE_RATIO_LOW 0.00001f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_RATIO_HIGH
#define SPA_STEREO_UPMIX_TD_ACTIVE_RATIO_HIGH 0.001f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_LOW
#define SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_LOW 0.005f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_HIGH
#define SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_HIGH 0.03f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_RISE
#define SPA_STEREO_UPMIX_TD_ACTIVE_RISE 0.00035f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_FALL
#define SPA_STEREO_UPMIX_TD_ACTIVE_FALL 0.02f
#endif
#ifndef SPA_STEREO_UPMIX_TD_ACTIVE_MAX_MAKEUP
#define SPA_STEREO_UPMIX_TD_ACTIVE_MAX_MAKEUP 1.0f
#endif

/* music-stage deliberately admits stable off-centre components that faithful
 * HSR keeps in front.  It is therefore a separate profile, not a silent retune
 * of either HSR mode.  A soft edge gate protects centred vocals while retaining
 * the input channel of a bandwise panned instrument. */
#ifndef SPA_STEREO_UPMIX_MUSIC_EDGE_LOW
#define SPA_STEREO_UPMIX_MUSIC_EDGE_LOW 0.12f
#endif
#ifndef SPA_STEREO_UPMIX_MUSIC_EDGE_HIGH
#define SPA_STEREO_UPMIX_MUSIC_EDGE_HIGH 0.55f
#endif
#ifndef SPA_STEREO_UPMIX_MUSIC_DIRECT_MIX
#define SPA_STEREO_UPMIX_MUSIC_DIRECT_MIX 1.5f
#endif
#ifndef SPA_STEREO_UPMIX_MUSIC_RESIDUAL_MIX
#define SPA_STEREO_UPMIX_MUSIC_RESIDUAL_MIX 2.0f
#endif

/* 24 Bark-like control bands.  They are clean-room control boundaries, not
 * recovered Pioneer preset data.  The last band intentionally absorbs the
 * unused spectrum above 12 kHz; the rear frequency taper is already zero by
 * 7 kHz. */
static const uint16_t kHqBandEdgeHz[SPA_STEREO_UPMIX_HQ_BANDS + 1] = {
    0, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480,
    1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700,
    9500, 12000, 24001
};

enum {
    SPA_HQ_ACCUM_L = 0,
    SPA_HQ_ACCUM_R = 1,
    SPA_HQ_ACCUM_RE = 2,
    SPA_HQ_ACCUM_IM = 3,
    SPA_HQ_ACCUM_FLUX = 4,
    SPA_HQ_ACCUM_MAGNITUDE = 5,

    SPA_HQ_PARAM_VL = 0,
    SPA_HQ_PARAM_VR_RE = 1,
    SPA_HQ_PARAM_VR_IM = 2,
    SPA_HQ_PARAM_DIRECT = 3,
    SPA_HQ_PARAM_MASK = 4
};

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

static float spa_upmix_snap_small(float value)
{
    return value > -1.0e-20f && value < 1.0e-20f ? 0.0f : value;
}

static float spa_upmix_finite_bound(float value, float limit)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000))
        return 0.0f;
    return spa_upmix_bound(value, limit);
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

static int16_t spa_upmix_transfer_front(int16_t front, int16_t rear,
                                        uint32_t front_limit_q15)
{
    /* A conventional stereo fold-down returns each surround at -3 dB.  Pull
     * that exact contribution out of the matching dry front, so active mode
     * redistributes accepted ambience instead of merely duplicating it. */
    int32_t adjusted = (int32_t)front -
        (int32_t)spa_upmix_scale_s16_q15(rear, UINT32_C(23170));
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

    output = spa_upmix_snap_small(spa_upmix_bound(output, 4.0f));
    /* Stop an inaudible all-pass tail before it enters binary32's denormal
     * range.  Some MHI2Q FPSCR configurations may not flush subnormals in
     * hardware, turning silence after an impulse into avoidable CPU work. */
    feedback = spa_upmix_snap_small(spa_upmix_bound(
        input + coefficient * output, 4.0f));
    delay[index] = feedback;

    index++;
    if (index == delay_length) index = 0;
    *position = index;
    return output;
}

/* Build the fixed 48 kHz HQ tables without libm.  Double-precision complex
 * recurrences start from pre-rounded sin/cos constants; the synthesis window
 * is normalized per hop phase so the four sqrt-Hann products sum to unity
 * even after float rounding. */
static void spa_upmix_hq_prepare(struct spa_stereo_upmix_runtime *runtime)
{
    const double window_step_re = 0.99999529380957617151;
    const double window_step_im = 0.00306795676296597627;
    const double window_start_re = 0.99999882345170190993;
    const double window_start_im = 0.00153398018628476561;
    const double fft_step_re = 0.99998117528260114266;
    const double fft_step_im = -0.00613588464915447536;
    double re = window_start_re;
    double im = window_start_im;
    unsigned sample;
    unsigned phase;

    for (sample = 0; sample < SPA_STEREO_UPMIX_HQ_FFT_SIZE; sample++) {
        double next_re;
        double next_im;

        runtime->_hq_analysis_window[sample] = (float)im;
        next_re = re * window_step_re - im * window_step_im;
        next_im = im * window_step_re + re * window_step_im;
        re = next_re;
        im = next_im;
    }

    for (phase = 0; phase < SPA_STEREO_UPMIX_HQ_HOP_SIZE; phase++) {
        float normalization = 0.0f;
        unsigned overlap;

        for (overlap = 0; overlap < 4; overlap++) {
            float window = runtime->_hq_analysis_window[
                phase + overlap * SPA_STEREO_UPMIX_HQ_HOP_SIZE];
            normalization += window * window;
        }
        for (overlap = 0; overlap < 4; overlap++) {
            unsigned index = phase +
                overlap * SPA_STEREO_UPMIX_HQ_HOP_SIZE;
            runtime->_hq_synthesis_window[index] =
                runtime->_hq_analysis_window[index] / normalization;
        }
    }

    re = 1.0;
    im = 0.0;
    for (sample = 0; sample < SPA_STEREO_UPMIX_HQ_FFT_SIZE / 2;
         sample++) {
        double next_re;
        double next_im;

        runtime->_hq_twiddle_re[sample] = (float)re;
        runtime->_hq_twiddle_im[sample] = (float)im;
        next_re = re * fft_step_re - im * fft_step_im;
        next_im = im * fft_step_re + re * fft_step_im;
        re = next_re;
        im = next_im;
    }

    for (sample = 0; sample < SPA_STEREO_UPMIX_HQ_BINS; sample++) {
        unsigned frequency_hz =
            (sample * SPA_STEREO_UPMIX_SAMPLE_RATE +
             SPA_STEREO_UPMIX_HQ_FFT_SIZE / 2) /
            SPA_STEREO_UPMIX_HQ_FFT_SIZE;
        unsigned band = 0;
        float frequency_weight;

        while (band + 1 < SPA_STEREO_UPMIX_HQ_BANDS &&
               frequency_hz >= kHqBandEdgeHz[band + 1])
            band++;
        runtime->_hq_bin_band[sample] = (uint8_t)band;

        if (frequency_hz <= 120 || frequency_hz >= 7000)
            frequency_weight = 0.0f;
        else if (frequency_hz < 220)
            frequency_weight = spa_upmix_smoothstep(
                (float)frequency_hz, 120.0f, 220.0f);
        else if (frequency_hz <= 5000)
            frequency_weight = 1.0f;
        else
            frequency_weight = 1.0f - spa_upmix_smoothstep(
                (float)frequency_hz, 5000.0f, 7000.0f);
        runtime->_hq_frequency_weight[sample] = frequency_weight;
    }
}

static void spa_upmix_hq_fft(struct spa_stereo_upmix_runtime *runtime,
                             unsigned channel, int inverse)
{
    float *real = runtime->_hq_fft_re[channel];
    float *imaginary = runtime->_hq_fft_im[channel];
    unsigned index;
    unsigned reversed = 0;
    unsigned length;

    for (index = 1; index < SPA_STEREO_UPMIX_HQ_FFT_SIZE; index++) {
        unsigned bit = SPA_STEREO_UPMIX_HQ_FFT_SIZE >> 1;

        while (reversed & bit) {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
        if (index < reversed) {
            float temporary = real[index];
            real[index] = real[reversed];
            real[reversed] = temporary;
            temporary = imaginary[index];
            imaginary[index] = imaginary[reversed];
            imaginary[reversed] = temporary;
        }
    }

    for (length = 2; length <= SPA_STEREO_UPMIX_HQ_FFT_SIZE;
         length <<= 1) {
        unsigned half = length >> 1;
        unsigned stride = SPA_STEREO_UPMIX_HQ_FFT_SIZE / length;
        unsigned base;

        for (base = 0; base < SPA_STEREO_UPMIX_HQ_FFT_SIZE;
             base += length) {
            unsigned offset;

            for (offset = 0; offset < half; offset++) {
                unsigned twiddle = offset * stride;
                float wr = runtime->_hq_twiddle_re[twiddle];
                float wi = runtime->_hq_twiddle_im[twiddle];
                unsigned even = base + offset;
                unsigned odd = even + half;
                float odd_re;
                float odd_im;
                float even_re;
                float even_im;

                if (inverse) wi = -wi;
                odd_re = wr * real[odd] - wi * imaginary[odd];
                odd_im = wr * imaginary[odd] + wi * real[odd];
                even_re = real[even];
                even_im = imaginary[even];
                real[even] = even_re + odd_re;
                imaginary[even] = even_im + odd_im;
                real[odd] = even_re - odd_re;
                imaginary[odd] = even_im - odd_im;
            }
        }
    }

    if (inverse) {
        const float scale = 1.0f / (float)SPA_STEREO_UPMIX_HQ_FFT_SIZE;

        for (index = 0; index < SPA_STEREO_UPMIX_HQ_FFT_SIZE; index++) {
            real[index] = spa_upmix_snap_small(real[index] * scale);
            imaginary[index] = spa_upmix_snap_small(
                imaginary[index] * scale);
        }
    }
}

/* One causal 1024/256 STFT step.  A single EMA coefficient updates all four
 * entries of each Hermitian covariance matrix, preserving positive
 * semidefiniteness.  The principal component is treated as direct sound only
 * when its phase is positive/coherent (or the source is effectively one-sided);
 * deliberately anti-phase stereo therefore remains eligible as ambience. */
static void spa_upmix_hq_run_hop(struct spa_stereo_upmix *state)
{
    struct spa_stereo_upmix_runtime *runtime = &state->_runtime;
    unsigned channel;
    unsigned sample;
    unsigned bin;
    unsigned band;

    memset(runtime->_hq_band_accumulator, 0,
           sizeof(runtime->_hq_band_accumulator));

    for (channel = 0; channel < SPA_STEREO_UPMIX_INPUT_CHANNELS;
         channel++) {
        for (sample = 0; sample < SPA_STEREO_UPMIX_HQ_FFT_SIZE;
             sample++) {
            unsigned history = (runtime->_hq_input_position + sample) &
                (SPA_STEREO_UPMIX_HQ_FFT_SIZE - 1);
            runtime->_hq_fft_re[channel][sample] =
                runtime->_hq_input[channel][history] *
                runtime->_hq_analysis_window[sample];
            runtime->_hq_fft_im[channel][sample] = 0.0f;
        }
        spa_upmix_hq_fft(runtime, channel, 0);
    }

    for (bin = 0; bin < SPA_STEREO_UPMIX_HQ_BINS; bin++) {
        float left_re = runtime->_hq_fft_re[SPA_UPMIX_LEFT][bin];
        float left_im = runtime->_hq_fft_im[SPA_UPMIX_LEFT][bin];
        float right_re = runtime->_hq_fft_re[SPA_UPMIX_RIGHT][bin];
        float right_im = runtime->_hq_fft_im[SPA_UPMIX_RIGHT][bin];
        float power_l = left_re * left_re + left_im * left_im;
        float power_r = right_re * right_re + right_im * right_im;
        float cross_re = left_re * right_re + left_im * right_im;
        float cross_im = left_im * right_re - left_re * right_im;
        float magnitude = spa_upmix_sqrt_positive(
            0.5f * (power_l + power_r));
        float previous = runtime->_hq_previous_magnitude[bin];
        float *accumulator;

        band = runtime->_hq_bin_band[bin];
        accumulator = runtime->_hq_band_accumulator[band];
        accumulator[SPA_HQ_ACCUM_L] += power_l;
        accumulator[SPA_HQ_ACCUM_R] += power_r;
        accumulator[SPA_HQ_ACCUM_RE] += cross_re;
        accumulator[SPA_HQ_ACCUM_IM] += cross_im;
        if (magnitude > previous)
            accumulator[SPA_HQ_ACCUM_FLUX] += magnitude - previous;
        accumulator[SPA_HQ_ACCUM_MAGNITUDE] += magnitude;
        runtime->_hq_previous_magnitude[bin] =
            spa_upmix_snap_small(magnitude);
    }

    for (band = 0; band < SPA_STEREO_UPMIX_HQ_BANDS; band++) {
        float *accumulator = runtime->_hq_band_accumulator[band];
        float *parameters = runtime->_hq_band_parameters[band];
        float old_trace = runtime->_hq_cov_l[band] +
                          runtime->_hq_cov_r[band];
        float current_trace = accumulator[SPA_HQ_ACCUM_L] +
                              accumulator[SPA_HQ_ACCUM_R];
        float covariance_alpha = current_trace > old_trace ? 0.20f : 0.021f;
        float power_l;
        float power_r;
        float cross_re;
        float cross_im;
        float trace;
        float cross_abs;
        float delta;
        float lambda_1;
        float lambda_2;
        float gamma;
        float cosine_phase;
        float edge;
        float direct_confidence;
        float wiener_direct;
        float flux;
        float transient_gain;
        float target_mask;
        float mask_alpha;
        float vector_l;
        float vector_r_re;
        float vector_r_im;
        float vector_norm;

        runtime->_hq_cov_l[band] += covariance_alpha *
            (accumulator[SPA_HQ_ACCUM_L] - runtime->_hq_cov_l[band]);
        runtime->_hq_cov_r[band] += covariance_alpha *
            (accumulator[SPA_HQ_ACCUM_R] - runtime->_hq_cov_r[band]);
        runtime->_hq_cov_re[band] += covariance_alpha *
            (accumulator[SPA_HQ_ACCUM_RE] - runtime->_hq_cov_re[band]);
        runtime->_hq_cov_im[band] += covariance_alpha *
            (accumulator[SPA_HQ_ACCUM_IM] - runtime->_hq_cov_im[band]);
        runtime->_hq_cov_l[band] = spa_upmix_snap_small(
            runtime->_hq_cov_l[band]);
        runtime->_hq_cov_r[band] = spa_upmix_snap_small(
            runtime->_hq_cov_r[band]);
        runtime->_hq_cov_re[band] = spa_upmix_snap_small(
            runtime->_hq_cov_re[band]);
        runtime->_hq_cov_im[band] = spa_upmix_snap_small(
            runtime->_hq_cov_im[band]);

        power_l = runtime->_hq_cov_l[band];
        power_r = runtime->_hq_cov_r[band];
        cross_re = runtime->_hq_cov_re[band];
        cross_im = runtime->_hq_cov_im[band];
        trace = power_l + power_r;
        cross_abs = spa_upmix_sqrt_positive(
            cross_re * cross_re + cross_im * cross_im);
        delta = spa_upmix_sqrt_positive(
            (power_l - power_r) * (power_l - power_r) +
            4.0f * cross_abs * cross_abs);
        if (delta > trace) delta = trace;
        lambda_1 = 0.5f * (trace + delta);
        lambda_2 = 0.5f * (trace - delta);
        if (lambda_2 < 0.0f) lambda_2 = 0.0f;
        gamma = power_l > 1.0e-16f && power_r > 1.0e-16f
            ? cross_abs / (spa_upmix_sqrt_positive(power_l * power_r) +
                           1.0e-20f) : 0.0f;
        gamma = spa_upmix_bound(gamma, 1.0f);
        if (gamma < 0.0f) gamma = 0.0f;
        cosine_phase = cross_abs > 1.0e-16f
            ? cross_re / (cross_abs + 1.0e-20f) : 0.0f;
        cosine_phase = spa_upmix_bound(cosine_phase, 1.0f);
        edge = trace > 1.0e-16f
            ? spa_upmix_absf(power_l - power_r) / (trace + 1.0e-20f)
            : 0.0f;
        direct_confidence = gamma * spa_upmix_smoothstep(
            cosine_phase, 0.50f, 0.90f);
        {
            float edge_confidence = spa_upmix_smoothstep(
                edge, 0.96f, 0.995f);
            if (edge_confidence > direct_confidence)
                direct_confidence = edge_confidence;
        }
        wiener_direct = lambda_1 > 1.0e-16f
            ? 1.0f - lambda_2 / (lambda_1 + 1.0e-20f) : 0.0f;
        wiener_direct = spa_upmix_bound(wiener_direct, 1.0f);
        if (wiener_direct < 0.0f) wiener_direct = 0.0f;
        flux = accumulator[SPA_HQ_ACCUM_FLUX] /
            (accumulator[SPA_HQ_ACCUM_MAGNITUDE] + 1.0e-20f);
        /* The projector below already removes the coherent rank-one
         * component.  Do not multiply its residual by diffuseness again:
         * doing so squares the rejection and makes legitimate quiet room
         * energy inaudible.  Spectral flux is only an attack protector; the
         * wider knee keeps stationary stochastic ambience out of the
         * transient class. */
        transient_gain = 1.0f - 0.85f * spa_upmix_smoothstep(
            flux, 0.20f, 0.60f);
        target_mask = trace > 1.0e-16f
            ? transient_gain : 0.0f;
        mask_alpha = target_mask < runtime->_hq_mask[band]
            ? 0.35f : 0.035f;
        runtime->_hq_mask[band] += mask_alpha *
            (target_mask - runtime->_hq_mask[band]);
        runtime->_hq_mask[band] = spa_upmix_snap_small(
            runtime->_hq_mask[band]);

        if (cross_abs > 1.0e-16f) {
            /* Both forms describe the same principal eigenvector.  Select
             * the one whose real diagonal difference is large; always using
             * [|C|,(lambda-PL)*conj(C)/|C|] is ill-conditioned when PL is the
             * dominant diagonal and creates a measurable fixed-left residue
             * after S16 quantization. */
            if (power_l >= power_r) {
                vector_l = lambda_1 - power_r;
                vector_r_re = cross_re;
                vector_r_im = -cross_im;
            } else {
                float second = lambda_1 - power_l;

                vector_l = cross_abs;
                vector_r_re = second * cross_re / cross_abs;
                vector_r_im = -second * cross_im / cross_abs;
            }
        } else if (power_l >= power_r) {
            vector_l = 1.0f;
            vector_r_re = 0.0f;
            vector_r_im = 0.0f;
        } else {
            vector_l = 0.0f;
            vector_r_re = 1.0f;
            vector_r_im = 0.0f;
        }
        vector_norm = spa_upmix_sqrt_positive(
            vector_l * vector_l + vector_r_re * vector_r_re +
            vector_r_im * vector_r_im);
        if (vector_norm > 1.0e-20f) {
            vector_l /= vector_norm;
            vector_r_re /= vector_norm;
            vector_r_im /= vector_norm;
        }

        parameters[SPA_HQ_PARAM_VL] = vector_l;
        parameters[SPA_HQ_PARAM_VR_RE] = vector_r_re;
        parameters[SPA_HQ_PARAM_VR_IM] = vector_r_im;
        parameters[SPA_HQ_PARAM_DIRECT] =
            direct_confidence * wiener_direct;
        parameters[SPA_HQ_PARAM_MASK] = runtime->_hq_mask[band];
    }

    for (bin = 0; bin < SPA_STEREO_UPMIX_HQ_BINS; bin++) {
        float left_re = runtime->_hq_fft_re[SPA_UPMIX_LEFT][bin];
        float left_im = runtime->_hq_fft_im[SPA_UPMIX_LEFT][bin];
        float right_re = runtime->_hq_fft_re[SPA_UPMIX_RIGHT][bin];
        float right_im = runtime->_hq_fft_im[SPA_UPMIX_RIGHT][bin];
        float *parameters = runtime->_hq_band_parameters[
            runtime->_hq_bin_band[bin]];
        float vector_l = parameters[SPA_HQ_PARAM_VL];
        float vector_r_re = parameters[SPA_HQ_PARAM_VR_RE];
        float vector_r_im = parameters[SPA_HQ_PARAM_VR_IM];
        float projection_re = vector_l * left_re +
            vector_r_re * right_re + vector_r_im * right_im;
        float projection_im = vector_l * left_im +
            vector_r_re * right_im - vector_r_im * right_re;
        float direct = parameters[SPA_HQ_PARAM_DIRECT];
        float gain = parameters[SPA_HQ_PARAM_MASK] *
                     runtime->_hq_frequency_weight[bin];
        float primary_l_re = vector_l * projection_re;
        float primary_l_im = vector_l * projection_im;
        float primary_r_re = vector_r_re * projection_re -
                             vector_r_im * projection_im;
        float primary_r_im = vector_r_re * projection_im +
                             vector_r_im * projection_re;

        runtime->_hq_fft_re[SPA_UPMIX_LEFT][bin] = gain *
            (left_re - direct * primary_l_re);
        runtime->_hq_fft_im[SPA_UPMIX_LEFT][bin] = gain *
            (left_im - direct * primary_l_im);
        runtime->_hq_fft_re[SPA_UPMIX_RIGHT][bin] = gain *
            (right_re - direct * primary_r_re);
        runtime->_hq_fft_im[SPA_UPMIX_RIGHT][bin] = gain *
            (right_im - direct * primary_r_im);
    }

    /* Rebuild the negative-frequency half so both inverse transforms remain
     * exactly real.  DC and Nyquist are in the configured stop bands. */
    for (channel = 0; channel < SPA_STEREO_UPMIX_INPUT_CHANNELS;
         channel++) {
        runtime->_hq_fft_im[channel][0] = 0.0f;
        runtime->_hq_fft_im[channel][SPA_STEREO_UPMIX_HQ_FFT_SIZE / 2] = 0.0f;
        for (bin = 1; bin < SPA_STEREO_UPMIX_HQ_FFT_SIZE / 2; bin++) {
            unsigned mirror = SPA_STEREO_UPMIX_HQ_FFT_SIZE - bin;

            runtime->_hq_fft_re[channel][mirror] =
                runtime->_hq_fft_re[channel][bin];
            runtime->_hq_fft_im[channel][mirror] =
                -runtime->_hq_fft_im[channel][bin];
        }
        spa_upmix_hq_fft(runtime, channel, 1);
    }

    for (sample = 0; sample < SPA_STEREO_UPMIX_HQ_FFT_SIZE; sample++) {
        unsigned output = (runtime->_hq_input_position + sample) &
            (SPA_STEREO_UPMIX_HQ_FFT_SIZE - 1);

        for (channel = 0; channel < SPA_STEREO_UPMIX_INPUT_CHANNELS;
             channel++) {
            float addition = spa_upmix_finite_bound(
                runtime->_hq_fft_re[channel][sample] *
                runtime->_hq_synthesis_window[sample], 4.0f);
            runtime->_hq_ola[channel][output] = spa_upmix_finite_bound(
                runtime->_hq_ola[channel][output] + addition, 8.0f);
        }
    }
}

static void spa_upmix_hq_process_sample(struct spa_stereo_upmix *state,
                                        float input_l, float input_r,
                                        float *rear_l, float *rear_r)
{
    struct spa_stereo_upmix_runtime *runtime = &state->_runtime;
    unsigned position = runtime->_hq_input_position;

    *rear_l = spa_upmix_snap_small(
        runtime->_hq_ola[SPA_UPMIX_LEFT][position]);
    *rear_r = spa_upmix_snap_small(
        runtime->_hq_ola[SPA_UPMIX_RIGHT][position]);
    if (runtime->_hq_frames_seen <
        SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES) {
        *rear_l = 0.0f;
        *rear_r = 0.0f;
    }
    runtime->_hq_ola[SPA_UPMIX_LEFT][position] = 0.0f;
    runtime->_hq_ola[SPA_UPMIX_RIGHT][position] = 0.0f;
    runtime->_hq_input[SPA_UPMIX_LEFT][position] = input_l;
    runtime->_hq_input[SPA_UPMIX_RIGHT][position] = input_r;
    if (runtime->_hq_frames_seen != UINT32_MAX)
        runtime->_hq_frames_seen++;

    runtime->_hq_input_position = (uint16_t)((position + 1) &
        (SPA_STEREO_UPMIX_HQ_FFT_SIZE - 1));
    runtime->_hq_hop_fill++;
    if (runtime->_hq_hop_fill == SPA_STEREO_UPMIX_HQ_HOP_SIZE) {
        runtime->_hq_hop_fill = 0;
        spa_upmix_hq_run_hop(state);
    }
}

/* Refresh the real 2x2 covariance projector for one hsr-td control band.
 *
 * This is a clean-room implementation of the public high-level idea behind a
 * time-domain spatial-field decoder; it is not DAM Audio HSR source code and
 * does not claim coefficient or output parity.  A positive, coherent rank-one
 * component represents a stable phantom source and stays in the dry fronts.
 * The residual represents energy not explained by that source and is eligible
 * for the surround field.  Deliberately anti-phase material is not mistaken
 * for a positive-amplitude phantom source.
 */
static void spa_upmix_td_update_band(struct spa_stereo_upmix_runtime *runtime,
                                     unsigned band)
{
    float power_l = runtime->_td_power_l[band];
    float power_r = runtime->_td_power_r[band];
    float cross = runtime->_td_cross[band];
    float trace = power_l + power_r;
    float cross_limit;
    float delta;
    float lambda_1;
    float lambda_2;
    float vector_l;
    float vector_r;
    float vector_norm;
    float correlation;
    float edge;
    float direct_confidence;
    float direct_ratio;

    if (trace <= 1.0e-16f) {
        runtime->_td_vector_l[band] = 0.0f;
        runtime->_td_vector_r[band] = 0.0f;
        runtime->_td_direct[band] = 0.0f;
        return;
    }

    /* Convex covariance updates are positive semidefinite in exact
     * arithmetic.  Clamp only the tiny binary32 excursion that can otherwise
     * make the discriminant or normalized correlation invalid. */
    cross_limit = spa_upmix_sqrt_positive(power_l * power_r);
    if (cross > cross_limit) cross = cross_limit;
    if (cross < -cross_limit) cross = -cross_limit;
    delta = spa_upmix_sqrt_positive(
        (power_l - power_r) * (power_l - power_r) +
        4.0f * cross * cross);
    if (delta > trace) delta = trace;
    lambda_1 = 0.5f * (trace + delta);
    lambda_2 = 0.5f * (trace - delta);
    if (lambda_2 < 0.0f) lambda_2 = 0.0f;

    /* Pick the better-conditioned equivalent eigenvector form.  This branch
     * also makes swapping L/R swap the rendered surround pair. */
    if (power_l >= power_r) {
        vector_l = lambda_1 - power_r;
        vector_r = cross;
    } else {
        vector_l = cross;
        vector_r = lambda_1 - power_l;
    }
    vector_norm = spa_upmix_sqrt_positive(
        vector_l * vector_l + vector_r * vector_r);
    if (vector_norm > 1.0e-20f) {
        vector_l /= vector_norm;
        vector_r /= vector_norm;
    } else if (power_l >= power_r) {
        vector_l = 1.0f;
        vector_r = 0.0f;
    } else {
        vector_l = 0.0f;
        vector_r = 1.0f;
    }

    correlation = cross_limit > 1.0e-16f
                ? cross / (cross_limit + 1.0e-20f) : 0.0f;
    correlation = spa_upmix_bound(correlation, 1.0f);
    edge = spa_upmix_absf(power_l - power_r) / (trace + 1.0e-20f);
    direct_confidence = spa_upmix_smoothstep(
        correlation, SPA_STEREO_UPMIX_TD_CORRELATION_LOW,
        SPA_STEREO_UPMIX_TD_CORRELATION_HIGH);
    {
        float edge_confidence = spa_upmix_smoothstep(edge, 0.96f, 0.997f);
        if (edge_confidence > direct_confidence)
            direct_confidence = edge_confidence;
    }
    direct_ratio = lambda_1 > 1.0e-16f
                 ? 1.0f - lambda_2 / (lambda_1 + 1.0e-20f) : 0.0f;
    direct_ratio = spa_upmix_bound(direct_ratio, 1.0f);
    if (direct_ratio < 0.0f) direct_ratio = 0.0f;

    runtime->_td_vector_l[band] = spa_upmix_snap_small(vector_l);
    runtime->_td_vector_r[band] = spa_upmix_snap_small(vector_r);
    runtime->_td_direct[band] = spa_upmix_snap_small(
        direct_confidence * direct_ratio);
}

/* Causal time-domain stereo-field analysis.  The complementary IIR bands sum
 * back to the input exactly; they exist only so simultaneous sources in
 * different spectral regions do not share one covariance estimate.  There is
 * no FFT, block scheduler, lookahead, heap work or artificial decorrelator.
 * The output therefore starts in the current frame and already-diffuse input
 * remains diffuse rather than being replaced with synthesized room energy. */
static void spa_upmix_td_process_sample(struct spa_stereo_upmix *state,
                                        float input_l, float input_r,
                                        float *rear_l, float *rear_r)
{
    struct spa_stereo_upmix_runtime *runtime = &state->_runtime;
    float band_l[SPA_STEREO_UPMIX_BANDS];
    float band_r[SPA_STEREO_UPMIX_BANDS];
    unsigned band;
    int update_parameters = runtime->_td_control_phase == 0;

    spa_upmix_split_sample(input_l, runtime->_td_split[SPA_UPMIX_LEFT],
                           band_l);
    spa_upmix_split_sample(input_r, runtime->_td_split[SPA_UPMIX_RIGHT],
                           band_r);
    *rear_l = 0.0f;
    *rear_r = 0.0f;

    for (band = 0; band < SPA_STEREO_UPMIX_BANDS; band++) {
        float sample_l = band_l[band];
        float sample_r = band_r[band];
        float instant_l = sample_l * sample_l;
        float instant_r = sample_r * sample_r;
        float old_trace = runtime->_td_power_l[band] +
                          runtime->_td_power_r[band];
        float instant_trace = instant_l + instant_r;
        float alpha = instant_trace > old_trace
                    ? SPA_STEREO_UPMIX_TD_COVARIANCE_ATTACK
                    : SPA_STEREO_UPMIX_TD_COVARIANCE_RELEASE;
        float vector_l;
        float vector_r;
        float direct;
        float projection;
        float residual_l;
        float residual_r;

        /* The same alpha updates all entries, retaining a valid covariance
         * matrix while giving attacks about 1 ms and releases about 8 ms. */
        runtime->_td_power_l[band] = spa_upmix_snap_small(
            runtime->_td_power_l[band] +
            alpha * (instant_l - runtime->_td_power_l[band]));
        runtime->_td_power_r[band] = spa_upmix_snap_small(
            runtime->_td_power_r[band] +
            alpha * (instant_r - runtime->_td_power_r[band]));
        runtime->_td_cross[band] = spa_upmix_snap_small(
            runtime->_td_cross[band] +
            alpha * (sample_l * sample_r - runtime->_td_cross[band]));

        if (update_parameters) spa_upmix_td_update_band(runtime, band);
        if (kBandWeight[band] == 0.0f) continue;

        vector_l = runtime->_td_vector_l[band];
        vector_r = runtime->_td_vector_r[band];
        direct = runtime->_td_direct[band];
        projection = vector_l * sample_l + vector_r * sample_r;
        residual_l = sample_l - direct * vector_l * projection;
        residual_r = sample_r - direct * vector_r * projection;
        *rear_l += kBandWeight[band] * residual_l;
        *rear_r += kBandWeight[band] * residual_r;
    }

    runtime->_td_control_phase++;
    if (runtime->_td_control_phase == SPA_STEREO_UPMIX_TD_CONTROL_STRIDE)
        runtime->_td_control_phase = 0;
    *rear_l = spa_upmix_finite_bound(*rear_l, 2.0f);
    *rear_r = spa_upmix_finite_bound(*rear_r, 2.0f);
}

/* Assertive music renderer.  It starts with the same HSR residual, then adds
 * a bounded share of the coherent principal component only where a control
 * band is measurably off-centre.  Unlike a global L-R matrix, retaining the
 * eigenvector's L/R ratio lets a left-panned guitar favour Ls and a right-
 * panned instrument favour Rs.  Exact centre remains excluded. */
static void spa_upmix_music_stage_process_sample(
    struct spa_stereo_upmix *state, float input_l, float input_r,
    float *rear_l, float *rear_r)
{
    struct spa_stereo_upmix_runtime *runtime = &state->_runtime;
    float band_l[SPA_STEREO_UPMIX_BANDS];
    float band_r[SPA_STEREO_UPMIX_BANDS];
    unsigned band;
    int update_parameters = runtime->_td_control_phase == 0;

    spa_upmix_split_sample(input_l, runtime->_td_split[SPA_UPMIX_LEFT],
                           band_l);
    spa_upmix_split_sample(input_r, runtime->_td_split[SPA_UPMIX_RIGHT],
                           band_r);
    *rear_l = 0.0f;
    *rear_r = 0.0f;

    for (band = 0; band < SPA_STEREO_UPMIX_BANDS; band++) {
        float sample_l = band_l[band];
        float sample_r = band_r[band];
        float instant_l = sample_l * sample_l;
        float instant_r = sample_r * sample_r;
        float old_trace = runtime->_td_power_l[band] +
                          runtime->_td_power_r[band];
        float instant_trace = instant_l + instant_r;
        float alpha = instant_trace > old_trace
                    ? SPA_STEREO_UPMIX_TD_COVARIANCE_ATTACK
                    : SPA_STEREO_UPMIX_TD_COVARIANCE_RELEASE;
        float vector_l;
        float vector_r;
        float direct;
        float projection;
        float residual_l;
        float residual_r;
        float trace;
        float edge;
        float edge_gate;
        float staged_direct;

        runtime->_td_power_l[band] = spa_upmix_snap_small(
            runtime->_td_power_l[band] +
            alpha * (instant_l - runtime->_td_power_l[band]));
        runtime->_td_power_r[band] = spa_upmix_snap_small(
            runtime->_td_power_r[band] +
            alpha * (instant_r - runtime->_td_power_r[band]));
        runtime->_td_cross[band] = spa_upmix_snap_small(
            runtime->_td_cross[band] +
            alpha * (sample_l * sample_r - runtime->_td_cross[band]));

        if (update_parameters) spa_upmix_td_update_band(runtime, band);
        if (kBandWeight[band] == 0.0f) continue;

        vector_l = runtime->_td_vector_l[band];
        vector_r = runtime->_td_vector_r[band];
        direct = runtime->_td_direct[band];
        projection = vector_l * sample_l + vector_r * sample_r;
        residual_l = SPA_STEREO_UPMIX_MUSIC_RESIDUAL_MIX *
            (sample_l - direct * vector_l * projection);
        residual_r = SPA_STEREO_UPMIX_MUSIC_RESIDUAL_MIX *
            (sample_r - direct * vector_r * projection);

        trace = runtime->_td_power_l[band] + runtime->_td_power_r[band];
        edge = trace > 1.0e-16f
             ? spa_upmix_absf(runtime->_td_power_l[band] -
                              runtime->_td_power_r[band]) /
               (trace + 1.0e-20f)
             : 0.0f;
        edge_gate = spa_upmix_smoothstep(
            edge, SPA_STEREO_UPMIX_MUSIC_EDGE_LOW,
            SPA_STEREO_UPMIX_MUSIC_EDGE_HIGH);
        staged_direct = SPA_STEREO_UPMIX_MUSIC_DIRECT_MIX *
                        edge_gate * direct;
        residual_l += staged_direct * vector_l * projection;
        residual_r += staged_direct * vector_r * projection;
        *rear_l += kBandWeight[band] * residual_l;
        *rear_r += kBandWeight[band] * residual_r;
    }

    runtime->_td_control_phase++;
    if (runtime->_td_control_phase == SPA_STEREO_UPMIX_TD_CONTROL_STRIDE)
        runtime->_td_control_phase = 0;
    *rear_l = spa_upmix_finite_bound(*rear_l, 3.0f);
    *rear_r = spa_upmix_finite_bound(*rear_r, 3.0f);
}

static void spa_upmix_td_active_apply(struct spa_stereo_upmix_runtime *runtime,
                                      float input_l, float input_r,
                                      float *rear_l, float *rear_r)
{
    float input_power = 0.5f *
        (input_l * input_l + input_r * input_r);
    float residual_power = 0.5f *
        (*rear_l * *rear_l + *rear_r * *rear_r);
    float ratio;
    float energy_gate;
    float confidence_gate;
    float target;
    float alpha;
    float makeup;
    float confidence_energy = 0.0f;
    float weighted_energy = 0.0f;
    float ambient_confidence;
    unsigned band;

    /* Use the already-smoothed HSR covariance at its 6 kHz control rate.  The
     * equivalent eight-sample EMA coefficient retains the 48 kHz envelope
     * time constant without paying for a six-band scan on every sample. */
    if (runtime->_td_control_phase == 1) {
        for (band = 0; band < SPA_STEREO_UPMIX_BANDS; band++) {
            float band_energy = runtime->_td_power_l[band] +
                                runtime->_td_power_r[band];

            weighted_energy += kBandWeight[band] * band_energy;
            confidence_energy += kBandWeight[band] * band_energy *
                                 (1.0f - runtime->_td_direct[band]);
        }
        ambient_confidence = weighted_energy > 1.0e-16f
                           ? confidence_energy /
                             (weighted_energy + 1.0e-20f)
                           : 0.0f;
        ambient_confidence = spa_upmix_bound(ambient_confidence, 1.0f);
        if (ambient_confidence < 0.0f) ambient_confidence = 0.0f;
        runtime->_td_active_confidence +=
            SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_ALPHA *
            (ambient_confidence - runtime->_td_active_confidence);
    }

    runtime->_td_active_input_power +=
        SPA_STEREO_UPMIX_TD_ACTIVE_POWER_ALPHA *
        (input_power - runtime->_td_active_input_power);
    runtime->_td_active_residual_power +=
        SPA_STEREO_UPMIX_TD_ACTIVE_POWER_ALPHA *
        (residual_power - runtime->_td_active_residual_power);
    if (runtime->_td_active_input_power <= 1.0e-10f) {
        target = 0.0f;
    } else {
        ratio = runtime->_td_active_residual_power /
                (runtime->_td_active_input_power + 1.0e-20f);
        energy_gate = spa_upmix_smoothstep(
            ratio, SPA_STEREO_UPMIX_TD_ACTIVE_RATIO_LOW,
            SPA_STEREO_UPMIX_TD_ACTIVE_RATIO_HIGH);
        confidence_gate = spa_upmix_smoothstep(
            runtime->_td_active_confidence,
            SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_LOW,
            SPA_STEREO_UPMIX_TD_ACTIVE_CONFIDENCE_HIGH);
        target = energy_gate < confidence_gate
               ? energy_gate : confidence_gate;
    }

    alpha = target > runtime->_td_active_amount
          ? SPA_STEREO_UPMIX_TD_ACTIVE_RISE
          : SPA_STEREO_UPMIX_TD_ACTIVE_FALL;
    runtime->_td_active_amount = spa_upmix_snap_small(
        runtime->_td_active_amount +
        alpha * (target - runtime->_td_active_amount));
    runtime->_td_active_amount = spa_upmix_bound(
        runtime->_td_active_amount, 1.0f);
    if (runtime->_td_active_amount < 0.0f)
        runtime->_td_active_amount = 0.0f;

    makeup = 1.0f + SPA_STEREO_UPMIX_TD_ACTIVE_MAX_MAKEUP *
                    runtime->_td_active_amount;
    *rear_l = spa_upmix_finite_bound(*rear_l * makeup, 4.0f);
    *rear_r = spa_upmix_finite_bound(*rear_r * makeup, 4.0f);
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
           backend == SPA_STEREO_UPMIX_AMBIENT_LITE ||
           backend == SPA_STEREO_UPMIX_AMBIENT_HQ ||
           backend == SPA_STEREO_UPMIX_HSR_TD ||
           backend == SPA_STEREO_UPMIX_HSR_TD_ACTIVE ||
           backend == SPA_STEREO_UPMIX_MUSIC_STAGE;
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
    if (saved.backend == SPA_STEREO_UPMIX_AMBIENT_HQ)
        spa_upmix_hq_prepare(&state->_runtime);
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
    if (state->config.backend == SPA_STEREO_UPMIX_AMBIENT_HQ)
        spa_upmix_hq_prepare(&state->_runtime);
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

        if (state->config.backend != SPA_STEREO_UPMIX_OFF &&
            state->config.lfe_enabled) {
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

        } else if (state->config.backend == SPA_STEREO_UPMIX_AMBIENT_HQ) {
            const float normalized_l =
                (float)input_l * (1.0f / 32768.0f);
            const float normalized_r =
                (float)input_r * (1.0f / 32768.0f);

            spa_upmix_hq_process_sample(state, normalized_l, normalized_r,
                                        &rear_l, &rear_r);
        } else if (state->config.backend == SPA_STEREO_UPMIX_HSR_TD ||
                   state->config.backend ==
                       SPA_STEREO_UPMIX_HSR_TD_ACTIVE) {
            const float normalized_l =
                (float)input_l * (1.0f / 32768.0f);
            const float normalized_r =
                (float)input_r * (1.0f / 32768.0f);

            spa_upmix_td_process_sample(state, normalized_l, normalized_r,
                                        &rear_l, &rear_r);
            if (state->config.backend ==
                SPA_STEREO_UPMIX_HSR_TD_ACTIVE) {
                spa_upmix_td_active_apply(&state->_runtime,
                                          normalized_l, normalized_r,
                                          &rear_l, &rear_r);
            }
        } else if (state->config.backend ==
                   SPA_STEREO_UPMIX_MUSIC_STAGE) {
            const float normalized_l =
                (float)input_l * (1.0f / 32768.0f);
            const float normalized_r =
                (float)input_r * (1.0f / 32768.0f);

            spa_upmix_music_stage_process_sample(
                state, normalized_l, normalized_r, &rear_l, &rear_r);
        }

        if (state->config.backend == SPA_STEREO_UPMIX_AMBIENT_LITE ||
            state->config.backend == SPA_STEREO_UPMIX_AMBIENT_HQ) {
            /* Both deterministic renderers end in the same independently
             * tuned Schroeder field and linked safety limiter.  HQ's WOLA
             * already contributes 21.33 ms of causal rear precedence, so no
             * additional bulk delay is inserted. */
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

        }

        if (state->config.backend == SPA_STEREO_UPMIX_AMBIENT_LITE ||
            state->config.backend == SPA_STEREO_UPMIX_AMBIENT_HQ ||
            state->config.backend == SPA_STEREO_UPMIX_HSR_TD ||
            state->config.backend == SPA_STEREO_UPMIX_HSR_TD_ACTIVE ||
            state->config.backend == SPA_STEREO_UPMIX_MUSIC_STAGE) {
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
            int16_t front_l = spa_upmix_scale_s16_q15(
                input_l, state->config.front_gain_q15);
            int16_t front_r = spa_upmix_scale_s16_q15(
                input_r, state->config.front_gain_q15);
            int16_t output_rear_l = spa_upmix_rear_to_s16(
                rear_l, state->config.rear_gain_q15);
            int16_t output_rear_r = spa_upmix_rear_to_s16(
                rear_r, state->config.rear_gain_q15);

            if (state->config.backend ==
                    SPA_STEREO_UPMIX_HSR_TD_ACTIVE ||
                state->config.backend == SPA_STEREO_UPMIX_MUSIC_STAGE) {
                front_l = spa_upmix_transfer_front(
                    front_l, output_rear_l,
                    state->config.front_gain_q15);
                front_r = spa_upmix_transfer_front(
                    front_r, output_rear_r,
                    state->config.front_gain_q15);
            }
            output[SPA_STEREO_UPMIX_CHANNEL_L] = front_l;
            output[SPA_STEREO_UPMIX_CHANNEL_R] = front_r;
            output[SPA_STEREO_UPMIX_CHANNEL_LS] = output_rear_l;
            output[SPA_STEREO_UPMIX_CHANNEL_RS] = output_rear_r;
        }
        output[SPA_STEREO_UPMIX_CHANNEL_C] = 0;
        output[SPA_STEREO_UPMIX_CHANNEL_LFE] = lfe;
        if (state->config.backend == SPA_STEREO_UPMIX_OFF) {
            output[SPA_STEREO_UPMIX_CHANNEL_LS] = 0;
            output[SPA_STEREO_UPMIX_CHANNEL_RS] = 0;
        }
    }

    return SPA_STEREO_UPMIX_OK;
}
