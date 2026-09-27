/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "hssr_dsp.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SPA_HSSR_DSP_HAVE_NEON 1
#else
#define SPA_HSSR_DSP_HAVE_NEON 0
#endif

#include <math.h>
#include <string.h>

#ifndef SPA_HSSR_UNIVERSAL_SEMANTIC_MAKEUP
#define SPA_HSSR_UNIVERSAL_SEMANTIC_MAKEUP 16.0f
#endif

enum {
    SPA_HSSR_LEFT = 0,
    SPA_HSSR_RIGHT = 1
};

static const uint16_t kHssrBandEdgeHz[SPA_HSSR_BANDS + 1] = {
    0, 60, 90, 125, 170, 220, 280, 350,
    440, 550, 680, 830, 1000, 1200, 1450, 1750,
    2100, 2500, 3000, 3600, 4300, 5100, 6000, 7000,
    8200, 9600, 11200, 13000, 15000, 17500, 20000, 22000,
    24001
};

/* Universal never raises a complete rear band.  This curve is consulted only
 * after a coherent object passes the semantic, pan and slot guards. */
static const float kHssrObjectFocusPresence[SPA_HSSR_BANDS] = {
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.10f, 0.20f,
    0.35f, 0.50f, 0.70f, 0.85f, 1.00f, 1.00f, 1.00f, 1.00f,
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    1.00f, 0.95f, 0.75f, 0.50f, 0.30f, 0.15f, 0.05f, 0.00f
};

/* A semantic drum stem is too broad for rear routing, but its transient
 * upper bands contain the shaker/cymbal/click events that are useful when a
 * song has no separable guitar, keys or synth bed.  This curve is only used
 * together with instantaneous positive spectral flux. */
static const float kHssrPercussionPresence[SPA_HSSR_BANDS] = {
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.05f, 0.12f, 0.25f, 0.42f, 0.60f,
    0.75f, 0.88f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    1.00f, 0.95f, 0.82f, 0.65f, 0.45f, 0.25f, 0.10f, 0.00f
};

/* Scene-bed decorrelation delays per band in hops (2.667 ms), Ls then Rs.
 * Shape follows the Dolby residual delay tables (long at low frequencies,
 * one or two units above 3 kHz); the absolute scale assumes 64-sample Dolby
 * units, a hypothesis recorded in the plan document. */
static const uint8_t kHssrBedDelayL[SPA_HSSR_BANDS] = {
    12, 12, 12, 12, 12,  9, 9, 9, 9, 9,  4, 4, 4, 4,  2, 2, 2, 2,
     1,  1,  1,  1,  1,  1, 1, 1, 1, 1,  1, 1, 1, 1
};
static const uint8_t kHssrBedDelayR[SPA_HSSR_BANDS] = {
    15, 15, 15, 15, 15,  7, 7, 7, 7, 7,  6, 6, 6, 6,  3, 3, 3, 3,
     2,  2,  2,  2,  2,  2, 2, 2, 2, 2,  2, 2, 2, 2
};
/* Dolby 0.98169 per unit; two units per hop. */
#define SPA_HSSR_ATTACK_BETA 0.9637f

struct spa_hssr_band_accumulator {
    float pl;
    float pr;
    float cross_re;
    float cross_im;
    float mid;
    float side;
    float magnitude;
    float log_magnitude;
    float flux;
    uint16_t bins;
};

static float spa_hssr_dsp_bound(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static float spa_hssr_dsp_finite(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000))
        return 0.0f;
    return value;
}

/* The three loops below walk whole contiguous runs of bins or samples with no
 * branches, which is what a vector unit wants.  Each keeps the scalar order of
 * operations, so the result is identical on a target without fused
 * multiply-add, and falls back to the same scalar body elsewhere. */
static void spa_hssr_window_run(float *destination_re, float *destination_im,
                                const float *input, const float *window,
                                unsigned count)
{
    unsigned i = 0;

#if SPA_HSSR_DSP_HAVE_NEON
    for (; i + 4u <= count; i += 4u) {
        vst1q_f32(destination_re + i,
                  vmulq_f32(vld1q_f32(input + i), vld1q_f32(window + i)));
        vst1q_f32(destination_im + i, vdupq_n_f32(0.0f));
    }
#endif
    for (; i < count; ++i) {
        destination_re[i] = input[i] * window[i];
        destination_im[i] = 0.0f;
    }
}

/* out += source * window, with the same infinity/NaN flush as the scalar
 * spa_hssr_dsp_finite: an exponent field of all ones becomes zero. */
static void spa_hssr_overlap_run(float *out_a, float *out_b,
                                 const float *source_a, const float *source_b,
                                 const float *window, unsigned count)
{
    unsigned i = 0;

#if SPA_HSSR_DSP_HAVE_NEON
    {
        const uint32x4_t exponent = vdupq_n_u32(UINT32_C(0x7f800000));
        const float32x4_t zero = vdupq_n_f32(0.0f);

        for (; i + 4u <= count; i += 4u) {
            float32x4_t w = vld1q_f32(window + i);
            float32x4_t a = vaddq_f32(vld1q_f32(out_a + i),
                                      vmulq_f32(vld1q_f32(source_a + i), w));
            float32x4_t b = vaddq_f32(vld1q_f32(out_b + i),
                                      vmulq_f32(vld1q_f32(source_b + i), w));

            vst1q_f32(out_a + i, vbslq_f32(
                vceqq_u32(vandq_u32(vreinterpretq_u32_f32(a), exponent), exponent),
                zero, a));
            vst1q_f32(out_b + i, vbslq_f32(
                vceqq_u32(vandq_u32(vreinterpretq_u32_f32(b), exponent), exponent),
                zero, b));
        }
    }
#endif
    for (; i < count; ++i) {
        out_a[i] = spa_hssr_dsp_finite(out_a[i] + source_a[i] * window[i]);
        out_b[i] = spa_hssr_dsp_finite(out_b[i] + source_b[i] * window[i]);
    }
}

static float spa_hssr_dsp_sigmoid(float value)
{
    float square;
    float tanh_value;

    value = spa_hssr_dsp_bound(value * 0.5f, -3.0f, 3.0f);
    square = value * value;
    tanh_value = value * (27.0f + square) / (27.0f + 9.0f * square);
    return 0.5f * (tanh_value + 1.0f);
}

static float spa_hssr_dsp_smoothstep(float value, float low, float high)
{
    float normalized;

    if (value <= low) return 0.0f;
    if (value >= high) return 1.0f;
    normalized = (value - low) / (high - low);
    return normalized * normalized * (3.0f - 2.0f * normalized);
}

static float spa_hssr_dsp_tanh(float value)
{
    float square;

    value = spa_hssr_dsp_bound(value, -3.0f, 3.0f);
    square = value * value;
    return value * (27.0f + square) / (27.0f + 9.0f * square);
}

/* In-place radix-2 complex FFT, 512 points.  The permutation comes from a
 * precomputed swap table and the inverse sign from a negated twiddle table, so
 * neither the reversal loop nor the butterfly carries a branch.  The inverse
 * transform does NOT rescale by 1/N: both consumers multiply by
 * synthesis_window_scaled, which carries that factor. */
static void spa_hssr_fft(struct spa_hssr_dsp *state,
                         unsigned channel, int inverse)
{
    float *real = state->fft_re[channel];
    float *imaginary = state->fft_im[channel];
    const float *twiddle_imaginary_pass = inverse
        ? state->pass_twiddle_im_inverse : state->pass_twiddle_im;
    unsigned index;
    unsigned length;

    for (index = 0; index < state->bit_swaps; ++index) {
        unsigned a = state->bit_swap[index][0];
        unsigned b = state->bit_swap[index][1];
        float temporary = real[a];

        real[a] = real[b];
        real[b] = temporary;
        temporary = imaginary[a];
        imaginary[a] = imaginary[b];
        imaginary[b] = temporary;
    }

#if SPA_HSSR_DSP_HAVE_NEON
    /* The first two passes fused, four groups of four at a time: vld4 puts
     * element k of each group in lane set k, so both passes are lane-wise
     * vector arithmetic.  Same operations in the same order as the scalar
     * passes below (pass 1 add/sub, pass 2 separate products then one
     * subtract), so the result is unchanged; the scalar half=2 pass could not
     * use the four-wide loop further down. */
    {
        const float32x4_t w0r = vdupq_n_f32(state->pass_twiddle_re[0]);
        const float32x4_t w1r = vdupq_n_f32(state->pass_twiddle_re[1]);
        const float32x4_t w0i = vdupq_n_f32(twiddle_imaginary_pass[0]);
        const float32x4_t w1i = vdupq_n_f32(twiddle_imaginary_pass[1]);

        for (index = 0; index < SPA_HSSR_FFT_SIZE; index += 16) {
            float32x4x4_t r = vld4q_f32(real + index);
            float32x4x4_t m = vld4q_f32(imaginary + index);
            float32x4_t a0r = vaddq_f32(r.val[0], r.val[1]);
            float32x4_t a0i = vaddq_f32(m.val[0], m.val[1]);
            float32x4_t a1r = vsubq_f32(r.val[0], r.val[1]);
            float32x4_t a1i = vsubq_f32(m.val[0], m.val[1]);
            float32x4_t a2r = vaddq_f32(r.val[2], r.val[3]);
            float32x4_t a2i = vaddq_f32(m.val[2], m.val[3]);
            float32x4_t a3r = vsubq_f32(r.val[2], r.val[3]);
            float32x4_t a3i = vsubq_f32(m.val[2], m.val[3]);
            float32x4_t t0r = vsubq_f32(vmulq_f32(w0r, a2r), vmulq_f32(w0i, a2i));
            float32x4_t t0i = vaddq_f32(vmulq_f32(w0r, a2i), vmulq_f32(w0i, a2r));
            float32x4_t t1r = vsubq_f32(vmulq_f32(w1r, a3r), vmulq_f32(w1i, a3i));
            float32x4_t t1i = vaddq_f32(vmulq_f32(w1r, a3i), vmulq_f32(w1i, a3r));

            r.val[0] = vaddq_f32(a0r, t0r);
            r.val[2] = vsubq_f32(a0r, t0r);
            r.val[1] = vaddq_f32(a1r, t1r);
            r.val[3] = vsubq_f32(a1r, t1r);
            m.val[0] = vaddq_f32(a0i, t0i);
            m.val[2] = vsubq_f32(a0i, t0i);
            m.val[1] = vaddq_f32(a1i, t1i);
            m.val[3] = vsubq_f32(a1i, t1i);
            vst4q_f32(real + index, r);
            vst4q_f32(imaginary + index, m);
        }
        length = 8;
    }
#else
    /* First pass: the only twiddle is (1, 0), so it is pure add/sub.  This is
     * a quarter of all butterflies and it carried four needless products. */
    for (index = 0; index < SPA_HSSR_FFT_SIZE; index += 2) {
        float even_re = real[index];
        float even_im = imaginary[index];
        float odd_re = real[index + 1];
        float odd_im = imaginary[index + 1];

        real[index] = even_re + odd_re;
        imaginary[index] = even_im + odd_im;
        real[index + 1] = even_re - odd_re;
        imaginary[index + 1] = even_im - odd_im;
    }
    length = 4;
#endif
    for (; length <= SPA_HSSR_FFT_SIZE; length <<= 1) {
        unsigned half = length >> 1;
        const float *pass_re = state->pass_twiddle_re + (half - 2);
        const float *pass_im = twiddle_imaginary_pass + (half - 2);
        unsigned base;

        for (base = 0; base < SPA_HSSR_FFT_SIZE; base += length) {
            unsigned offset = 0;

#if SPA_HSSR_DSP_HAVE_NEON
            /* Four butterflies at a time.  Split real/imaginary arrays and the
             * regrouped twiddles make every operand a contiguous load, and the
             * products stay separate multiplies and one subtract, exactly as
             * in the scalar path below. */
            for (; offset + 4u <= half; offset += 4u) {
                unsigned even = base + offset;
                unsigned odd = even + half;
                float32x4_t wr = vld1q_f32(pass_re + offset);
                float32x4_t wi = vld1q_f32(pass_im + offset);
                float32x4_t odd_r = vld1q_f32(real + odd);
                float32x4_t odd_i = vld1q_f32(imaginary + odd);
                float32x4_t even_r = vld1q_f32(real + even);
                float32x4_t even_i = vld1q_f32(imaginary + even);
                float32x4_t rotated_r = vsubq_f32(vmulq_f32(wr, odd_r),
                                                  vmulq_f32(wi, odd_i));
                float32x4_t rotated_i = vaddq_f32(vmulq_f32(wr, odd_i),
                                                  vmulq_f32(wi, odd_r));

                vst1q_f32(real + even, vaddq_f32(even_r, rotated_r));
                vst1q_f32(imaginary + even, vaddq_f32(even_i, rotated_i));
                vst1q_f32(real + odd, vsubq_f32(even_r, rotated_r));
                vst1q_f32(imaginary + odd, vsubq_f32(even_i, rotated_i));
            }
#endif
            for (; offset < half; ++offset) {
                float wr = pass_re[offset];
                float wi = pass_im[offset];
                unsigned even = base + offset;
                unsigned odd = even + half;
                float odd_re = wr * real[odd] - wi * imaginary[odd];
                float odd_im = wr * imaginary[odd] + wi * real[odd];
                float even_re = real[even];
                float even_im = imaginary[even];

                real[even] = even_re + odd_re;
                imaginary[even] = even_im + odd_im;
                real[odd] = even_re - odd_re;
                imaginary[odd] = even_im - odd_im;
            }
        }
    }
}

void spa_hssr_dsp_prepare(struct spa_hssr_dsp *state)
{
    const double window_step_re = 0.99998117528260114266;
    const double window_step_im = 0.00613588464915447536;
    const double window_start_re = 0.99999529380957617151;
    const double window_start_im = 0.00306795676296597627;
    const double fft_step_re = 0.99992470183914454092;
    const double fft_step_im = -0.01227153828571992608;
    double re;
    double im;
    unsigned sample;
    unsigned phase;

    if (!state) return;
    memset(state, 0, sizeof(*state));

    re = window_start_re;
    im = window_start_im;
    for (sample = 0; sample < SPA_HSSR_FFT_SIZE; ++sample) {
        double next_re;
        double next_im;

        state->analysis_window[sample] = (float)im;
        next_re = re * window_step_re - im * window_step_im;
        next_im = im * window_step_re + re * window_step_im;
        re = next_re;
        im = next_im;
    }
    for (phase = 0; phase < SPA_HSSR_HOP_SIZE; ++phase) {
        float normalization = 0.0f;
        unsigned overlap;

        for (overlap = 0; overlap < 4; ++overlap) {
            float window = state->analysis_window[
                phase + overlap * SPA_HSSR_HOP_SIZE];
            normalization += window * window;
        }
        for (overlap = 0; overlap < 4; ++overlap) {
            unsigned index = phase + overlap * SPA_HSSR_HOP_SIZE;
            state->synthesis_window[index] =
                state->analysis_window[index] / normalization;
        }
    }

    re = 1.0;
    im = 0.0;
    for (sample = 0; sample < SPA_HSSR_FFT_SIZE / 2; ++sample) {
        double next_re;
        double next_im;

        state->twiddle_re[sample] = (float)re;
        state->twiddle_im[sample] = (float)im;
        state->twiddle_im_inverse[sample] = -(float)im;
        next_re = re * fft_step_re - im * fft_step_im;
        next_im = im * fft_step_re + re * fft_step_im;
        re = next_re;
        im = next_im;
    }
    {
        unsigned half;

        for (half = 2; half < SPA_HSSR_FFT_SIZE; half <<= 1) {
            unsigned stride = (SPA_HSSR_FFT_SIZE / 2) / half;
            unsigned k;

            for (k = 0; k < half; ++k) {
                state->pass_twiddle_re[half - 2 + k] =
                    state->twiddle_re[k * stride];
                state->pass_twiddle_im[half - 2 + k] =
                    state->twiddle_im[k * stride];
                state->pass_twiddle_im_inverse[half - 2 + k] =
                    state->twiddle_im_inverse[k * stride];
            }
        }
    }

    {
        unsigned reversed = 0;

        for (sample = 1; sample < SPA_HSSR_FFT_SIZE; ++sample) {
            unsigned bit = SPA_HSSR_FFT_SIZE >> 1;

            while (reversed & bit) {
                reversed ^= bit;
                bit >>= 1;
            }
            reversed ^= bit;
            if (sample < reversed) {
                state->bit_swap[state->bit_swaps][0] = (uint16_t)sample;
                state->bit_swap[state->bit_swaps][1] = (uint16_t)reversed;
                state->bit_swaps++;
            }
        }
    }
    for (sample = 0; sample < SPA_HSSR_FFT_SIZE; ++sample)
        state->synthesis_window_scaled[sample] =
            state->synthesis_window[sample] * (1.0f / (float)SPA_HSSR_FFT_SIZE);

    for (sample = 0; sample < SPA_HSSR_BINS; ++sample) {
        unsigned frequency_hz =
            (sample * 48000u + SPA_HSSR_FFT_SIZE / 2) /
            SPA_HSSR_FFT_SIZE;
        unsigned band = 0;

        while (band + 1 < SPA_HSSR_BANDS &&
               frequency_hz >= kHssrBandEdgeHz[band + 1])
            ++band;
        state->bin_band[sample] = (uint8_t)band;
    }
    state->universal_makeup = 1.0f;
}

void spa_hssr_dsp_reset(struct spa_hssr_dsp *state)
{
    spa_hssr_dsp_prepare(state);
}

static void spa_hssr_extract_features(
    struct spa_hssr_dsp *state,
    const struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS])
{
    const float epsilon = 1.0e-8f;
    float total_energy = 0.0f;
    float total_mid = 0.0f;
    float total_side = 0.0f;
    float weighted_coherence = 0.0f;
    float weighted_flux = 0.0f;
    float weighted_harmonic = 0.0f;
    float weighted_diffuse = 0.0f;
    float weighted_pan = 0.0f;
    float weighted_frequency = 0.0f;
    float sub120 = 0.0f;
    float low250 = 0.0f;
    float transient_bands = 0.0f;
    float pan_entropy = 0.0f;
    float peak = 0.0f;
    float sample_energy = 0.0f;
    float log_total;
    float crest;
    unsigned band;
    unsigned sample;

    for (band = 0; band < SPA_HSSR_BANDS; ++band)
        total_energy += bands[band].pl + bands[band].pr;
    if (total_energy < epsilon) total_energy = epsilon;

    for (band = 0; band < SPA_HSSR_BANDS; ++band) {
        const struct spa_hssr_band_accumulator *value = &bands[band];
        float energy = value->pl + value->pr;
        float cross_abs = sqrtf(value->cross_re * value->cross_re +
                                value->cross_im * value->cross_im);
        float coherence = spa_hssr_dsp_bound(
            cross_abs / sqrtf(value->pl * value->pr + epsilon),
            0.0f, 1.0f);
        float cosine = spa_hssr_dsp_bound(
            value->cross_re / (cross_abs + epsilon), -1.0f, 1.0f);
        float sine = spa_hssr_dsp_bound(
            value->cross_im / (cross_abs + epsilon), -1.0f, 1.0f);
        float flatness;
        float tonality;
        float flux = spa_hssr_dsp_bound(
            value->flux / (value->magnitude + epsilon), 0.0f, 4.0f);
        float tail;
        float left_amplitude = sqrtf(value->pl + epsilon);
        float right_amplitude = sqrtf(value->pr + epsilon);
        float pan = (left_amplitude - right_amplitude) /
                    (left_amplitude + right_amplitude);
        float probability = energy / total_energy;
        float centre_frequency = 0.5f *
            ((float)kHssrBandEdgeHz[band] +
             (float)kHssrBandEdgeHz[band + 1]);
        unsigned base = band * 9;

        if (value->bins) {
            float arithmetic = value->magnitude / (float)value->bins;
            float geometric = expf(value->log_magnitude /
                                   (float)value->bins);
            flatness = geometric / (arithmetic + epsilon);
        } else {
            /* NumPy training accumulator leaves empty FFT bands at zero
             * flatness.  The value is constant for this fixed geometry, but
             * matching it avoids a clipped six-sigma normalization offset. */
            flatness = 0.0f;
        }
        flatness = spa_hssr_dsp_bound(flatness, 0.0f, 1.0f);
        tonality = 1.0f - flatness;
        tail = spa_hssr_dsp_bound(
            state->band_slow_energy[band] /
            (energy + state->band_slow_energy[band] + epsilon),
            0.0f, 1.0f) * (1.0f - spa_hssr_dsp_bound(flux, 0.0f, 1.0f));

        state->features[base + 0] = logf(energy + epsilon);
        state->features[base + 1] = logf(
            (value->side + epsilon) / (value->mid + epsilon));
        state->features[base + 2] = 0.5f * logf(
            (value->pl + epsilon) / (value->pr + epsilon));
        state->features[base + 3] = coherence;
        state->features[base + 4] = cosine;
        state->features[base + 5] = sine;
        state->features[base + 6] = flux;
        state->features[base + 7] = tonality;
        state->features[base + 8] = tail;

        total_mid += value->mid;
        total_side += value->side;
        weighted_coherence += coherence * energy;
        weighted_flux += flux * energy;
        weighted_harmonic += tonality * energy;
        weighted_diffuse += (1.0f - coherence) * energy;
        weighted_pan += pan * energy;
        weighted_frequency += probability * centre_frequency;
        if (band <= 2) sub120 += energy;
        if (band >= 3 && band <= 5) low250 += energy;
        if (flux > 0.20f) transient_bands += 1.0f;
        if (probability > epsilon)
            pan_entropy -= probability * logf(probability);
    }

    /* 64 contiguous four-bin log-magnitude shape values.  Nyquist is
     * intentionally excluded, matching train_hssr.py. */
    for (band = 0; band < SPA_HSSR_SHAPE_BANDS; ++band) {
        float sum = 0.0f;
        unsigned offset;

        for (offset = 0; offset < 4; ++offset) {
            unsigned bin = band * 4 + offset;
            float left_re = state->fft_re[SPA_HSSR_LEFT][bin];
            float left_im = state->fft_im[SPA_HSSR_LEFT][bin];
            float right_re = state->fft_re[SPA_HSSR_RIGHT][bin];
            float right_im = state->fft_im[SPA_HSSR_RIGHT][bin];
            float magnitude = sqrtf(0.5f *
                (left_re * left_re + left_im * left_im +
                 right_re * right_re + right_im * right_im));
            sum += logf(magnitude + epsilon);
        }
        state->features[SPA_HSSR_BANDS * 9 + band] = 0.25f * sum;
    }

    for (sample = 0; sample < SPA_HSSR_FFT_SIZE; ++sample) {
        unsigned history = (state->input_position + sample) &
                           (SPA_HSSR_FFT_SIZE - 1);
        float left = state->input[SPA_HSSR_LEFT][history];
        float right = state->input[SPA_HSSR_RIGHT][history];
        float left_abs = left < 0.0f ? -left : left;
        float right_abs = right < 0.0f ? -right : right;

        if (left_abs > peak) peak = left_abs;
        if (right_abs > peak) peak = right_abs;
        sample_energy += left * left + right * right;
    }
    crest = peak / sqrtf(sample_energy /
                         (2.0f * SPA_HSSR_FFT_SIZE) + epsilon);
    log_total = logf(total_energy);
    {
        unsigned base = SPA_HSSR_BANDS * 9 + SPA_HSSR_SHAPE_BANDS;

        state->features[base + 0] = log_total;
        state->features[base + 1] = crest;
        state->features[base + 2] = total_mid /
            (total_mid + total_side + epsilon);
        state->features[base + 3] = total_side /
            (total_mid + total_side + epsilon);
        state->features[base + 4] = weighted_coherence / total_energy;
        state->features[base + 5] = sub120 / total_energy;
        state->features[base + 6] = low250 / total_energy;
        state->features[base + 7] = weighted_frequency / 24000.0f;
        state->features[base + 8] = weighted_flux / total_energy;
        state->features[base + 9] = transient_bands / SPA_HSSR_BANDS;
        state->features[base + 10] = weighted_harmonic / total_energy;
        state->features[base + 11] = weighted_diffuse / total_energy;
        state->features[base + 12] = weighted_pan / total_energy;
        state->features[base + 13] = pan_entropy / logf(32.0f);
        state->features[base + 14] = 0.0f;
        state->features[base + 15] = state->current_log_delta;
    }

    for (sample = 0; sample < SPA_HSSR_FEATURES; ++sample)
        state->features[sample] = spa_hssr_dsp_finite(
            state->features[sample]);
}

static void spa_hssr_update_controls(struct spa_hssr_dsp *state)
{
    float global_safety = spa_hssr_dsp_sigmoid(
        state->control[SPA_HSSR_BANDS * 8 + 7]);
    float lfe_bass_sum = 0.0f;
    float lfe_drum_sum = 0.0f;
    float lfe_weight = 0.0f;
    unsigned band;

    state->scene_confidence = spa_hssr_dsp_sigmoid(
        state->control[SPA_HSSR_BANDS * 8 + 3]);
    state->rear_activity_target = spa_hssr_dsp_sigmoid(
        state->control[SPA_HSSR_BANDS * 8 + 4]);
    state->scene_stability = spa_hssr_dsp_sigmoid(
        state->control[SPA_HSSR_BANDS * 8 + 5]);
    state->fx_present = spa_hssr_dsp_sigmoid(
        state->control[SPA_HSSR_BANDS * 8 + 6]);

    for (band = 0; band < SPA_HSSR_BANDS; ++band) {
        const float *raw = &state->control[band * 8];
        float maximum = raw[0];
        float probabilities[6];
        float denominator = 0.0f;
        float target;
        float pan;
        float alpha;
        float lead_keep;
        float bass_keep;
        float object_boost_target = 0.0f;
        float focus_rescue_target = 0.0f;
        float universal_safety_target = 0.0f;
        unsigned class_index;
        float guard_vocal = 0.0f;   /* guard's vocal posterior, keys-discounted */

        for (class_index = 1; class_index < 6; ++class_index)
            if (raw[class_index] > maximum) maximum = raw[class_index];
        for (class_index = 0; class_index < 6; ++class_index) {
            probabilities[class_index] = expf(
                spa_hssr_dsp_bound(raw[class_index] - maximum,
                                   -12.0f, 0.0f));
            denominator += probabilities[class_index];
        }
        for (class_index = 0; class_index < 6; ++class_index)
            probabilities[class_index] /= denominator;

        /* Reuse the trained low-band stem posteriors for LFE V2.  Weighting
         * by the measured 0..125 Hz energy prevents an empty band from
         * steering the body/punch blend. */
        if (band < 3u) {
            float weight = state->band_slow_energy[band] + 1.0e-12f;

            lfe_bass_sum += weight * probabilities[1];
            lfe_drum_sum += weight * probabilities[2];
            lfe_weight += weight;
        }
        if (state->guard_valid) {
            /* Hybrid veto: the guard (V11, 'everything harmonic is vocal')
             * keeps protecting a voice the main model misreads, unless the
             * main model is confident the band is keys. */
            const float *graw = &state->guard_control[band * 8];
            float gmax = graw[0];
            float gsum = 0.0f;
            float gvocal;
            float combined;
            unsigned gi;

            for (gi = 1; gi < 6; ++gi) if (graw[gi] > gmax) gmax = graw[gi];
            for (gi = 0; gi < 6; ++gi)
                gsum += expf(spa_hssr_dsp_bound(graw[gi] - gmax, -12.0f, 0.0f));
            gvocal = expf(spa_hssr_dsp_bound(graw[0] - gmax, -12.0f, 0.0f)) / gsum;
            /* Discount the guard by the main model's confidence that the band
             * is keys or strings/background (its two 'safe rear' classes). */
            combined = gvocal * spa_hssr_dsp_bound(
                1.0f - probabilities[5] - probabilities[4], 0.0f, 1.0f);
            guard_vocal = combined;
            if (combined > probabilities[0]) probabilities[0] = combined;
        }
        /* MoisesDB-trained control model (q6d): keys/guitar read as vocal at
         * <=0.02 / <=0.07 (90 %), leads at >=0.55 (90 %) but down to 0.19 on
         * heavily processed voices.  Boost the vocal probability used by the
         * veto so a hesitant 0.2..0.5 still protects; instruments stay <0.2. */
        if (probabilities[0] < 0.4f) probabilities[0] *= 2.5f;
        else probabilities[0] = 1.0f;

        target = spa_hssr_dsp_sigmoid(raw[6]);
        /* The network's rearability posterior is compressed: real positives
         * cluster around 0.25 while its negative floor is near 0.13. */
        target = spa_hssr_dsp_smoothstep(target, 0.12f, 0.36f);
        /* Rearability is learned, but false positives are much more audible
         * on lead vocal than false negatives on backing material.  Apply a
         * steep semantic veto before makeup; this permits an audible rear
         * level without lifting the model's low-confidence vocal floor. */
        {
            float background_fx = probabilities[4] + probabilities[5];
            float semantic_focus = spa_hssr_dsp_smoothstep(
                background_fx + 0.20f * probabilities[2] -
                    0.40f * probabilities[0] -
                    0.20f * probabilities[1],
                0.05f, 0.55f);

            /* Universal is selective rather than a global rear boost. */
            bass_keep = 1.0f - 0.85f * probabilities[1];
            {
                /* Spatially separated material is allowed to survive a
                 * confused vocal posterior; a centred lead still receives a
                 * steep fourth-power veto and cannot acquire an object slot. */
                float vocal_risk = probabilities[0] *
                    (1.0f - 0.80f * state->band_spatiality[band]);

                /* A voice the guard hears is a voice even when it is wide:
                 * the spatial escape exists for V11's over-called vocals,
                 * not for a lead the main model misreads as keys. */
                if (guard_vocal > vocal_risk) vocal_risk = guard_vocal;
                /* The main model's learned rearability is high where it
                 * misreads a voice as keys; the guard overrides it too. */
                target *= (1.0f - 0.95f * guard_vocal) * (1.0f - 0.95f * guard_vocal);
                lead_keep = 1.0f - 0.92f * vocal_risk;
                target *= lead_keep * lead_keep * lead_keep * lead_keep;
            }
            target *= bass_keep * bass_keep * bass_keep * bass_keep *
                      bass_keep;
            target *= 1.0f - 0.35f * probabilities[2];
            target *= 1.0f - 0.10f * probabilities[3];
            target *= 1.0f + 0.85f * background_fx;
            target *= 0.72f + 0.28f * semantic_focus;
            target *= 0.70f + 0.30f * global_safety;
            target *= SPA_HSSR_UNIVERSAL_SEMANTIC_MAKEUP;
            {
                float objectness = background_fx +
                    0.25f * probabilities[3] +
                    0.15f * probabilities[2] -
                    0.75f * probabilities[0] -
                    0.35f * probabilities[1];

                float maximum_boost =
                    0.52f + 0.38f * kHssrObjectFocusPresence[band];

                object_boost_target = maximum_boost *
                    spa_hssr_dsp_smoothstep(objectness, 0.05f, 0.50f);
            }
            {
                const unsigned global_base =
                    SPA_HSSR_BANDS * 9 + SPA_HSSR_SHAPE_BANDS;
                float scene_width = spa_hssr_dsp_smoothstep(
                    state->features[global_base + 3], 0.035f, 0.16f);
                float transient_objectness =
                    probabilities[2] +
                    0.35f * probabilities[3] +
                    probabilities[4] + probabilities[5] -
                    0.25f * probabilities[0] -
                    0.35f * probabilities[1];

                /* Dense electronic masters are commonly misclassified by
                 * the four-stem teacher as drums/low instead of background
                 * or FX.  Their learned rearability then falls below the
                 * calibrated 0.12 floor and the ordinary semantic path goes
                 * silent (Bangarang/Centipede are representative cases).
                 *
                 * Rescue only the mid/high presence bands of a measurably
                 * wide scene.  The object tracker still requires directed
                 * ILD, ridge/transient evidence and a persistent half-plane
                 * slot, so exact mono, centred dry vocal and sub cannot enter
                 * through this path. */
                focus_rescue_target =
                    0.65f * scene_width *
                    kHssrObjectFocusPresence[band] *
                    spa_hssr_dsp_smoothstep(
                        transient_objectness, 0.12f, 0.52f) *
                    (1.0f - 0.55f * probabilities[0]) *
                    (1.0f - 0.70f * probabilities[1]);
                focus_rescue_target *= (1.0f - guard_vocal) * (1.0f - guard_vocal);
                focus_rescue_target = spa_hssr_dsp_bound(
                    focus_rescue_target, 0.0f, 0.65f);
            }
            {
                /* MoisesDB-trained model names guitar explicitly (V11 filed
                 * it under background/fx); rhythm guitar is second-scene
                 * material, so it counts 0.80 instead of 0.35. */
                float safe_class = probabilities[2] +
                    0.80f * probabilities[3] + probabilities[4] +
                    probabilities[5];
                float vocal_keep = 1.0f - 0.97f * probabilities[0];
                float low_keep = 1.0f - 0.90f * probabilities[1];

                universal_safety_target = spa_hssr_dsp_smoothstep(
                    safe_class - 0.65f * probabilities[0] -
                        0.35f * probabilities[1],
                    -0.05f, 0.55f);
                universal_safety_target *= vocal_keep * vocal_keep *
                    vocal_keep * low_keep * low_keep;
            }
        }
        if (kHssrBandEdgeHz[band + 1] <= 220)
            target = 0.0f;
        else if (kHssrBandEdgeHz[band] < 350)
            target *= ((float)kHssrBandEdgeHz[band + 1] - 220.0f) /
                      (350.0f - 220.0f);
        if (kHssrBandEdgeHz[band] >= 20000)
            target *= 0.25f;
        else if (kHssrBandEdgeHz[band] >= 15000)
            target *= 0.65f;
        target = spa_hssr_dsp_bound(target, 0.0f, 1.0f);

        alpha = target > state->band_mask[band] ? 0.35f : 0.035f;
        state->band_mask[band] += alpha *
            (target - state->band_mask[band]);
        pan = spa_hssr_dsp_bound(
            spa_hssr_dsp_tanh(raw[7]), -0.85f, 0.85f);
        state->band_pan[band] += 0.28f *
            (pan - state->band_pan[band]);
        alpha = object_boost_target > state->band_object_boost[band]
            ? 0.35f : 0.04f;
        state->band_object_boost[band] += alpha *
            (object_boost_target - state->band_object_boost[band]);
        alpha = focus_rescue_target > state->band_focus_rescue[band]
            ? 0.40f : 0.025f;
        state->band_focus_rescue[band] += alpha *
            (focus_rescue_target - state->band_focus_rescue[band]);
        alpha = universal_safety_target < state->band_universal_safety[band]
            ? 0.45f : 0.12f;
        state->band_universal_safety[band] += alpha *
            (universal_safety_target - state->band_universal_safety[band]);
        /* Instant vocal veto for the render stage (see vocal_gate).  Symmetric:
         * the next syllable is protected as fast as the last one is released. */
        {
            /* Fast attack, ~0.35 s release: a voice does not vanish between
             * syllables, and holding a band front is never heard as a jump. */
            float veto_target = spa_hssr_dsp_smoothstep(probabilities[0], 0.5f, 0.9f);

            state->band_vocal_veto[band] +=
                (veto_target > state->band_vocal_veto[band] ? 0.35f : 0.03f) *
                (veto_target - state->band_vocal_veto[band]);
        }
    }

    {
        /* Whole-voice presence for the gate: body bands 350..2500 Hz. */
        float called = 0.0f;
        unsigned body = 0;

        for (band = 0; band < SPA_HSSR_BANDS; ++band) {
            if (kHssrBandEdgeHz[band] < 350 || kHssrBandEdgeHz[band] >= 2500) continue;
            called += state->band_vocal_veto[band];
            ++body;
        }
        {
            /* ~0.1 s attack so a brief misread does not engage the gate,
             * ~0.5 s release. */
            float presence_target = spa_hssr_dsp_smoothstep(
                called / (float)(body ? body : 1u), 0.30f, 0.60f);

            state->vocal_presence +=
                (presence_target > state->vocal_presence ? 0.10f : 0.02f) *
                (presence_target - state->vocal_presence);
        }
    }
    {
        float bass_target = lfe_weight > 1.0e-11f
            ? lfe_bass_sum / lfe_weight : 0.0f;
        float drum_target = lfe_weight > 1.0e-11f
            ? lfe_drum_sum / lfe_weight : 0.0f;
        float bass_alpha = bass_target > state->lfe_bass_confidence
            ? 0.30f : 0.06f;
        float drum_alpha = drum_target > state->lfe_drum_confidence
            ? 0.30f : 0.06f;

        state->lfe_bass_confidence += bass_alpha *
            (bass_target - state->lfe_bass_confidence);
        state->lfe_drum_confidence += drum_alpha *
            (drum_target - state->lfe_drum_confidence);
    }
}

static void spa_hssr_update_spatial_bands(
    struct spa_hssr_dsp *state,
    const struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS])
{
    const float epsilon = 1.0e-8f;
    unsigned band;

    for (band = 0; band < SPA_HSSR_BANDS; ++band) {
        float amplitude_l = sqrtf(bands[band].pl + epsilon);
        float amplitude_r = sqrtf(bands[band].pr + epsilon);
        float cross_abs = sqrtf(
            bands[band].cross_re * bands[band].cross_re +
            bands[band].cross_im * bands[band].cross_im);
        float coherence = spa_hssr_dsp_bound(
            cross_abs / sqrtf(bands[band].pl * bands[band].pr + epsilon),
            0.0f, 1.0f);
        float pan = (amplitude_l - amplitude_r) /
            (amplitude_l + amplitude_r + epsilon);

        state->band_spatiality[band] = spa_hssr_dsp_smoothstep(
            fabsf(pan), 0.035f, 0.50f);
        state->band_diffuse[band] = 1.0f - coherence;
    }
}

static void spa_hssr_update_object_slots(
    struct spa_hssr_dsp *state,
    const struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS])
{
    const float epsilon = 1.0e-8f;
    float band_target[SPA_HSSR_OBJECT_SLOTS][SPA_HSSR_BANDS];
    float weighted_pan[SPA_HSSR_OBJECT_SLOTS] = { 0.0f, 0.0f };
    float candidate_energy[SPA_HSSR_OBJECT_SLOTS] = { 0.0f, 0.0f };
    float pan_weight[SPA_HSSR_OBJECT_SLOTS] = { 0.0f, 0.0f };
    float peak_candidate[SPA_HSSR_OBJECT_SLOTS] = { 0.0f, 0.0f };
    float alt_pan[2] = { 0.0f, 0.0f };   /* seed for an idle secondary slot: bands far from the primary, per side */
    float alt_w[2] = { 0.0f, 0.0f };
    float total_energy = 0.0f;
    unsigned band;
    unsigned slot_index;

    memset(band_target, 0, sizeof(band_target));
    for (band = 0; band < SPA_HSSR_BANDS; ++band)
        total_energy += bands[band].pl + bands[band].pr;
    if (total_energy < epsilon) total_energy = epsilon;

    for (band = 0; band < SPA_HSSR_BANDS; ++band) {
        float energy = bands[band].pl + bands[band].pr;
        float amplitude_l = sqrtf(bands[band].pl + epsilon);
        float amplitude_r = sqrtf(bands[band].pr + epsilon);
        float pan = (amplitude_l - amplitude_r) /
            (amplitude_l + amplitude_r + epsilon);
        float cross_abs = sqrtf(
            bands[band].cross_re * bands[band].cross_re +
            bands[band].cross_im * bands[band].cross_im);
        float coherence = spa_hssr_dsp_bound(
            cross_abs / sqrtf(bands[band].pl * bands[band].pr + epsilon),
            0.0f, 1.0f);
        float semantic = spa_hssr_dsp_bound(
            state->band_mask[band] *
                (1.0f + state->band_object_boost[band]),
            0.0f, 1.0f);

        if (state->band_focus_rescue[band] > semantic)
            semantic = state->band_focus_rescue[band];

        /* Synth bass reads as keys and rides pan/coherence into the rear
         * (Scars: 34 % of rear energy).  No object below 220 Hz behind. */
        if (kHssrBandEdgeHz[band + 1] <= 220) continue;
        for (slot_index = 0; slot_index < SPA_HSSR_OBJECT_SLOTS;
             ++slot_index) {
            float directed_pan = (slot_index & 1u) == 0u ? pan : -pan;
            float spatial = spa_hssr_dsp_smoothstep(
                directed_pan, 0.025f, 0.40f);
            float candidate = semantic * spatial *
                (0.55f + 0.45f * coherence);
            float pan_share = 1.0f;
            {
                unsigned sibling = slot_index ^ 2u;
                const struct spa_hssr_object_slot *me = &state->object_slot[slot_index];
                const struct spa_hssr_object_slot *other = &state->object_slot[sibling];
                float prox_me = 1.0f - spa_hssr_dsp_smoothstep(fabsf(pan - me->pan), 0.08f, 0.30f);
                float prox_other = 1.0f - spa_hssr_dsp_smoothstep(fabsf(pan - other->pan), 0.08f, 0.30f);

                /* The split steers only each slot's PAN estimate; band
                 * weights stay whole (a bin later picks the nearer pan), so
                 * two slots never halve the rear. */
                if (slot_index < 2u) {
                    if (fabsf(pan - me->pan) > 0.25f) {
                        alt_pan[slot_index] += candidate * energy * pan;
                        alt_w[slot_index] += candidate * energy;
                    }
                    pan_share = other->confidence >= 0.03f
                        ? prox_me / (prox_me + prox_other + 1.0e-6f) : 1.0f;
                } else {
                    pan_share = other->confidence >= 0.03f
                        ? prox_me / (prox_me + prox_other + 1.0e-6f) : 0.0f;
                    if (me->confidence < 0.03f) pan_share = 0.0f;
                }
            }
            {
                float flux_ratio = bands[band].flux /
                    (bands[band].magnitude + epsilon);
                float transient = spa_hssr_dsp_smoothstep(
                    flux_ratio, 0.015f, 0.30f);
                float presence = kHssrObjectFocusPresence[band];

                candidate *= 1.0f + 0.45f * presence *
                    (0.35f + 0.65f * transient);
            }
            float weighted = candidate * energy;

            band_target[slot_index][band] = candidate;
            candidate_energy[slot_index] += weighted;
            weighted_pan[slot_index] += weighted * pan_share * pan;
            pan_weight[slot_index] += weighted * pan_share;
            if (candidate > peak_candidate[slot_index])
                peak_candidate[slot_index] = candidate;
        }
    }

    for (slot_index = 0; slot_index < SPA_HSSR_OBJECT_SLOTS;
         ++slot_index) {
        struct spa_hssr_object_slot *slot =
            &state->object_slot[slot_index];
        /* Confidence judges the SIDE's object evidence; the split between the
         * two same-side slots only shapes their band weights and pans. */
        float energy_share = (candidate_energy[slot_index] +
            candidate_energy[slot_index ^ 2u]) / total_energy;
        float target_confidence = spa_hssr_dsp_smoothstep(
                energy_share, 0.0008f, 0.040f) *
            spa_hssr_dsp_smoothstep(peak_candidate[slot_index],
                0.018f, 0.24f);
        float confidence_alpha;

        target_confidence *= 0.70f + 0.30f * state->scene_confidence;
        target_confidence *= 0.75f + 0.25f * state->scene_stability;
        if (target_confidence > 0.12f) {
            /* Hold stays at the stock 250 ms: the sweep 250..1200 ms changed
             * slot toggling by <=2/min; the slow confidence release and the
             * calm pan below are what keep an object.  Override for tests. */
#ifndef SPA_HSSR_SCENE_HOLD_HOPS
#define SPA_HSSR_SCENE_HOLD_HOPS 94u
#endif
            slot->hold_hops = SPA_HSSR_SCENE_HOLD_HOPS;
        } else if (slot->hold_hops != 0u) {
            --slot->hold_hops;
            if (target_confidence < 0.65f * slot->confidence)
                target_confidence = 0.65f * slot->confidence;
        }

        confidence_alpha = target_confidence > slot->confidence
            ? 0.26f : 0.006f;
        slot->confidence += confidence_alpha *
            (target_confidence - slot->confidence);
        slot->confidence = spa_hssr_dsp_bound(slot->confidence, 0.0f, 1.0f);

        if (slot_index >= 2u && slot->confidence < 0.03f &&
            alt_w[slot_index - 2u] > epsilon) {
            float seed = alt_pan[slot_index - 2u] / alt_w[slot_index - 2u];
            slot->pan = (slot_index & 1u) == 0u
                ? spa_hssr_dsp_bound(seed, 0.05f, 0.92f)
                : spa_hssr_dsp_bound(seed, -0.92f, -0.05f);
        }
        if (pan_weight[slot_index] > epsilon) {
            float target_pan = weighted_pan[slot_index] /
                pan_weight[slot_index];

            if ((slot_index & 1u) == 0u)
                target_pan = spa_hssr_dsp_bound(target_pan, 0.05f, 0.92f);
            else
                target_pan = spa_hssr_dsp_bound(target_pan, -0.92f, -0.05f);
            if (slot->confidence < 0.03f)
                slot->pan = target_pan;
            else
                /* A confident object keeps its place; only a weak one follows. */
                slot->pan += (slot->confidence > 0.30f ? 0.020f : 0.055f) *
                    (target_pan - slot->pan);
        }

        for (band = 0; band < SPA_HSSR_BANDS; ++band) {
            float target = band_target[slot_index][band] *
                slot->confidence;
            float alpha = target > slot->band_weight[band]
                ? 0.34f : 0.010f;

            slot->band_weight[band] += alpha *
                (target - slot->band_weight[band]);
            slot->band_weight[band] = spa_hssr_dsp_bound(
                slot->band_weight[band], 0.0f, 1.0f);
        }
    }
}

/* Stage 1: window, FFT and feature-state analysis.  It is deliberately kept
 * on the audio thread because later masks consume this exact spectrum. */
static int spa_hssr_analyze_hop(
    struct spa_hssr_dsp *state,
    struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS])
{
    unsigned bin;
    unsigned band;
    int full_frame = state->frames_seen >= SPA_HSSR_FFT_SIZE;

    memset(bands, 0, sizeof(*bands) * SPA_HSSR_BANDS);
    {
        unsigned channel;
        /* The ring read is two contiguous runs, not 512 masked indices. */
        const unsigned start = state->input_position & (SPA_HSSR_FFT_SIZE - 1);
        const unsigned first = SPA_HSSR_FFT_SIZE - start;

#ifdef SPA_HSSR_TEST_MASK
        if (state->test_inject_pcm) {
            for (channel = 0; channel < 2; ++channel) {
                spa_hssr_window_run(state->fft_re[channel], state->fft_im[channel],
                                    state->inject[channel] + start, state->analysis_window, first);
                spa_hssr_window_run(state->fft_re[channel] + first, state->fft_im[channel] + first,
                                    state->inject[channel], state->analysis_window + first, SPA_HSSR_FFT_SIZE - first);
                spa_hssr_fft(state, channel, 0);
                memcpy(state->inject_re[channel], state->fft_re[channel], sizeof(state->inject_re[channel]));
                memcpy(state->inject_im[channel], state->fft_im[channel], sizeof(state->inject_im[channel]));
            }
        }
#endif
        for (channel = 0; channel < 2; ++channel) {
            spa_hssr_window_run(state->fft_re[channel], state->fft_im[channel],
                                state->input[channel] + start,
                                state->analysis_window, first);
            spa_hssr_window_run(state->fft_re[channel] + first,
                                state->fft_im[channel] + first,
                                state->input[channel],
                                state->analysis_window + first,
                                SPA_HSSR_FFT_SIZE - first);
            spa_hssr_fft(state, channel, 0);
        }
    }

    for (bin = 0; bin < SPA_HSSR_BINS; ++bin) {
        float left_re = state->fft_re[SPA_HSSR_LEFT][bin];
        float left_im = state->fft_im[SPA_HSSR_LEFT][bin];
        float right_re = state->fft_re[SPA_HSSR_RIGHT][bin];
        float right_im = state->fft_im[SPA_HSSR_RIGHT][bin];
        float pl = left_re * left_re + left_im * left_im;
        float pr = right_re * right_re + right_im * right_im;
        float magnitude = sqrtf(0.5f * (pl + pr));
        float previous = full_frame ? state->previous_magnitude[bin] : 0.0f;
        struct spa_hssr_band_accumulator *value;

        band = state->bin_band[bin];
        value = &bands[band];
        value->pl += pl;
        value->pr += pr;
        value->cross_re += left_re * right_re + left_im * right_im;
        value->cross_im += left_im * right_re - left_re * right_im;
        value->mid += 0.25f *
            ((left_re + right_re) * (left_re + right_re) +
             (left_im + right_im) * (left_im + right_im));
        value->side += 0.25f *
            ((left_re - right_re) * (left_re - right_re) +
             (left_im - right_im) * (left_im - right_im));
        value->magnitude += magnitude;
        value->log_magnitude += logf(magnitude + 1.0e-8f);
        if (magnitude > previous) value->flux += magnitude - previous;
        value->bins++;
        if (full_frame) state->previous_magnitude[bin] = magnitude;
    }
    spa_hssr_update_spatial_bands(state, bands);

    /* These temporal features are updated on every STFT hop.  Inference runs
     * only every fourth hop, but train_hssr.py constructs the slow tail and
     * log-energy delta at the full 375 Hz analysis rate before subsampling. */
    if (full_frame) {
        const float epsilon = 1.0e-8f;
        float total_energy = 0.0f;
        float log_total;

        for (band = 0; band < SPA_HSSR_BANDS; ++band) {
            float energy = bands[band].pl + bands[band].pr;

            total_energy += energy;
            state->band_slow_energy[band] += 0.08f *
                (energy - state->band_slow_energy[band]);
        }
        log_total = logf(total_energy + epsilon);
        state->current_log_delta = state->frames_seen > SPA_HSSR_HOP_SIZE
            ? fabsf(log_total - state->previous_log_total) : 0.0f;
        state->previous_log_total = log_total;
    }

    return full_frame;
}

/* The mask net's per-bin features of the control hop: log|L|^2, log|R|^2 and
 * the inter-channel phase (cos, sin), exactly mask_net.py features(). */
static void spa_hssr_extract_mask_features(struct spa_hssr_dsp *state)
{
    unsigned bin;

    for (bin = 0; bin < SPA_HSSR_BINS; ++bin) {
        float lr = state->fft_re[SPA_HSSR_LEFT][bin];
        float li = state->fft_im[SPA_HSSR_LEFT][bin];
        float rr = state->fft_re[SPA_HSSR_RIGHT][bin];
        float ri = state->fft_im[SPA_HSSR_RIGHT][bin];
        float pl = lr * lr + li * li;
        float pr = rr * rr + ri * ri;
        float mag = sqrtf(pl * pr) + 1.0e-9f;
        float *f = state->mask_features + (size_t)bin * SPA_MASK_FEATURES;

        f[0] = spa_mask_log_fast(pl + 1.0e-7f);    /* libm logf is software double on QNX 6.5 */
        f[1] = spa_mask_log_fast(pr + 1.0e-7f);
        f[2] = (lr * rr + li * ri) / mag;          /* Re(L * conj(R)) / |L||R| */
        f[3] = (li * rr - lr * ri) / mag;          /* Im(L * conj(R)) / |L||R| */
    }
}

void spa_hssr_dsp_set_mask_drive(struct spa_hssr_dsp *state, float drive, int32_t treble_db10,
                                 int32_t gamma_pct)
{
    unsigned bin;
    const float treble = powf(10.0f, (float)treble_db10 / 200.0f);
    const float gamma = (gamma_pct >= 10 && gamma_pct <= 100) ? (float)gamma_pct / 100.0f : 1.0f;
    state->mask_drive = drive;
    for (bin = 0; bin < SPA_HSSR_BINS; ++bin)
        state->mask_gamma_lut[bin] = powf((float)bin / (float)(SPA_HSSR_BINS - 1), gamma);
    for (bin = 0; bin < SPA_HSSR_BINS; ++bin) {
        float hz = (float)bin * (48000.0f / (float)SPA_HSSR_FFT_SIZE);
        float t = spa_hssr_dsp_smoothstep(hz, 1500.0f, 3000.0f);
        state->mask_drive_bin[bin] = drive * (1.0f + t * (treble - 1.0f));
    }
}

static void spa_hssr_mask_missed(struct spa_hssr_dsp *state)
{
    /* One late period keeps the previous eligibility; repeated lateness
     * hands the object back to the heuristic path (never a silent rear). */
    if (state->mask_missed < UINT32_MAX) state->mask_missed++;
    if (state->mask_missed >= 3u) {
        if (state->mask_ready) state->mask_fallbacks++;
        state->mask_ready = 0;
    }
}

static void spa_hssr_control_failsafe(struct spa_hssr_dsp *state)
{
    unsigned band;

    state->inference_failures++;
    for (band = 0; band < SPA_HSSR_BANDS; ++band) {
        state->band_mask[band] *= 0.75f;
        state->band_focus_rescue[band] *= 0.75f;
    }
}

/* Stage 2: extract the compact feature vector and update learned controls.
 * A production provider submits the GRU work without waiting.  Results are
 * accepted only from this generation and at least one control period old,
 * making normal output independent of worker scheduling. */
static void spa_hssr_run_control(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    const struct spa_hssr_control_provider *provider,
    const struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS],
    int full_frame)
{
    int deadline_missed = 0;

    if (full_frame) state->control_hop++;
    if (!full_frame || state->control_hop != SPA_HSSR_CONTROL_HOPS) return;

    state->control_hop = 0;
    spa_hssr_extract_features(state, bands);
    if (state->mask_model) spa_hssr_extract_mask_features(state);
    state->control_sequence++;
    if (!state->control_sequence) {
        state->control_sequence = 1u;
        state->applied_control_sequence = 0;
        state->control_deadlines_missed = 0;
        if (provider) {
            state->control_generation++;
            if (!state->control_generation)
                state->control_generation = 1u;
            provider->reset(provider->context,
                            state->control_generation);
        }
    }

    if (!provider) {
        if (spa_hssr_infer(model, &state->network,
                           state->features, state->control) == SPA_HSSR_OK) {
            state->guard_valid = state->guard_model &&
                spa_hssr_infer(state->guard_model, &state->guard_network,
                               state->features, state->guard_control) == SPA_HSSR_OK;
            spa_hssr_update_controls(state);
        } else {
            spa_hssr_control_failsafe(state);
        }
        if (state->mask_model && state->mask_drive > 0.0f) {
            if (spa_mask_infer(state->mask_model, &state->mask_network,
                               state->mask_features, state->mask_eligibility, NULL) == 0) {
                state->mask_ready = 1u;
                state->mask_missed = 0;
            } else {
                spa_hssr_mask_missed(state);
            }
        }
        return;
    }

    if (provider->receive && state->control_sequence > 1u) {
        uint32_t result_sequence = 0;

        if (provider->receive(provider->context,
                              state->control_generation,
                              state->control_sequence - 1u,
                              state->control,
                              &result_sequence) > 0 &&
            result_sequence > state->applied_control_sequence &&
            result_sequence < state->control_sequence) {
            state->applied_control_sequence = result_sequence;
            state->control_deadlines_missed = 0;
            state->guard_valid = provider->receive_guard &&
                provider->receive_guard(provider->context, state->guard_control) > 0;
            spa_hssr_update_controls(state);
            if (state->mask_model && state->mask_drive > 0.0f) {
                if (provider->receive_mask &&
                    provider->receive_mask(provider->context, state->mask_eligibility) > 0) {
                    state->mask_ready = 1u;
                    state->mask_missed = 0;
                } else {
                    spa_hssr_mask_missed(state);
                }
            }
        } else {
            deadline_missed = 1;
        }
    }

    if (!provider->submit ||
        provider->submit(provider->context,
                         state->control_generation,
                         state->control_sequence,
                         state->features,
                         state->mask_model ? state->mask_features : NULL) <= 0) {
        deadline_missed = 1;
    }
    if (deadline_missed) {
        if (state->mask_model) spa_hssr_mask_missed(state);
        state->control_deadlines_missed++;
        /* One late period is harmless: keep the preceding stable mask.
         * Repeated lateness decays toward a quiet, bounded rear image. */
        if (state->control_deadlines_missed >= 3u)
            spa_hssr_control_failsafe(state);
    }
}

/* Stage 3: deterministic mask construction, spectral routing and overlap-add
 * synthesis.  Like analysis, it never leaves the audio thread. */
/* Dolby-style band attack detector: 0 at <= +3 dB above the slow history,
 * 1 at >= +9 dB, instantaneous rise, geometric release. */
static float spa_hssr_attack_step(float energy, float *history, float *weight)
{
    float target = 0.0f;

    if (*history <= 0.0f) {
        *history = energy;
    } else {
        float ratio = 0.501187205f * energy / (*history + 1.0e-12f);

        if (ratio > 1.0f)
            target = spa_hssr_dsp_bound(
                0.501716673f * 1.44269504f * logf(ratio), 0.0f, 1.0f);
    }
    *weight *= SPA_HSSR_ATTACK_BETA;
    if (target > *weight) *weight = target;
    *history += (1.0f - SPA_HSSR_ATTACK_BETA) * (energy - *history);
    return *weight;
}

/* Distribute the scene bed like a Dolby residual: an undelayed share, a
 * per-band delayed share and a delayed cross term (0.79 / 0.60 / 0.14: unit
 * energy for uncorrelated copies).  Attack handling per band:
 *   OWNER  a hit stays coherent where the band owner is: with an accepted
 *          second-scene object the delayed copies fade (rear stays one
 *          source); without an owner the bed row collapses forward (x0.39);
 *   FRONT  Dolby reference, the whole bed row collapses forward on every hit. */
static void spa_hssr_mix_bed(struct spa_hssr_dsp *state)
{
    const unsigned pos = state->bed_ring_pos & (SPA_HSSR_BED_RING - 1);
    unsigned bin = 0;

    /* Everything here is constant within a band and the bands are contiguous
     * runs of bins, so walk the runs: the per-bin work becomes four loads, a
     * few products and four stores with no lookups, which vectorises directly.
     * Same operations per bin and in the same order as before. */
    while (bin < SPA_HSSR_BINS) {
        const unsigned band = state->bin_band[bin];
        unsigned end = bin;
        float w;
        float owner;
        float row;
        float fade;
        float d;
        float x;
        unsigned pl;
        unsigned pr;
        unsigned i;

        while (end < SPA_HSSR_BINS && state->bin_band[end] == band) ++end;
        w = state->bed_attack ? state->band_attack_weight[band] : 0.0f;
        owner = state->bed_attack == SPA_HSSR_BED_ATTACK_OWNER
            ? state->band_owner[band] : 0.0f;
        row = 1.0f - 0.61f * w * (1.0f - owner);
        fade = 1.0f - w * owner;
        d = 0.60f * fade;
        x = 0.14f * fade;
        pl = (pos + SPA_HSSR_BED_RING - kHssrBedDelayL[band]) &
             (SPA_HSSR_BED_RING - 1);
        pr = (pos + SPA_HSSR_BED_RING - kHssrBedDelayR[band]) &
             (SPA_HSSR_BED_RING - 1);

        for (i = bin; i < end; ++i) {
            state->bed_ring_re[0][pos][i] = state->bed_re[0][i];
            state->bed_ring_im[0][pos][i] = state->bed_im[0][i];
            state->bed_ring_re[1][pos][i] = state->bed_re[1][i];
            state->bed_ring_im[1][pos][i] = state->bed_im[1][i];
        }
        i = bin;
#if SPA_HSSR_DSP_HAVE_NEON
        {
            const float32x4_t keep = vdupq_n_f32(0.79f);
            const float32x4_t delayed = vdupq_n_f32(d);
            const float32x4_t cross = vdupq_n_f32(x);
            const float32x4_t scale = vdupq_n_f32(row);

            for (; i + 4u <= end; i += 4u) {
                float32x4_t l_re = vld1q_f32(&state->bed_re[0][i]);
                float32x4_t l_im = vld1q_f32(&state->bed_im[0][i]);
                float32x4_t r_re = vld1q_f32(&state->bed_re[1][i]);
                float32x4_t r_im = vld1q_f32(&state->bed_im[1][i]);

                if (state->bed_delays) {
                    float32x4_t dl_re = vld1q_f32(&state->bed_ring_re[0][pl][i]);
                    float32x4_t dl_im = vld1q_f32(&state->bed_ring_im[0][pl][i]);
                    float32x4_t dr_re = vld1q_f32(&state->bed_ring_re[1][pr][i]);
                    float32x4_t dr_im = vld1q_f32(&state->bed_ring_im[1][pr][i]);

                    l_re = vsubq_f32(vaddq_f32(vmulq_f32(keep, l_re),
                                               vmulq_f32(delayed, dl_re)),
                                     vmulq_f32(cross, dr_re));
                    l_im = vsubq_f32(vaddq_f32(vmulq_f32(keep, l_im),
                                               vmulq_f32(delayed, dl_im)),
                                     vmulq_f32(cross, dr_im));
                    r_re = vsubq_f32(vaddq_f32(vmulq_f32(keep, r_re),
                                               vmulq_f32(delayed, dr_re)),
                                     vmulq_f32(cross, dl_re));
                    r_im = vsubq_f32(vaddq_f32(vmulq_f32(keep, r_im),
                                               vmulq_f32(delayed, dr_im)),
                                     vmulq_f32(cross, dl_im));
                }
                vst1q_f32(&state->fft_re[SPA_HSSR_LEFT][i],
                          vaddq_f32(vld1q_f32(&state->fft_re[SPA_HSSR_LEFT][i]),
                                    vmulq_f32(scale, l_re)));
                vst1q_f32(&state->fft_im[SPA_HSSR_LEFT][i],
                          vaddq_f32(vld1q_f32(&state->fft_im[SPA_HSSR_LEFT][i]),
                                    vmulq_f32(scale, l_im)));
                vst1q_f32(&state->fft_re[SPA_HSSR_RIGHT][i],
                          vaddq_f32(vld1q_f32(&state->fft_re[SPA_HSSR_RIGHT][i]),
                                    vmulq_f32(scale, r_re)));
                vst1q_f32(&state->fft_im[SPA_HSSR_RIGHT][i],
                          vaddq_f32(vld1q_f32(&state->fft_im[SPA_HSSR_RIGHT][i]),
                                    vmulq_f32(scale, r_im)));
            }
        }
#endif
        for (; i < end; ++i) {
            float l_re = state->bed_re[0][i];
            float l_im = state->bed_im[0][i];
            float r_re = state->bed_re[1][i];
            float r_im = state->bed_im[1][i];

            if (state->bed_delays) {
                float dl_re = state->bed_ring_re[0][pl][i];
                float dl_im = state->bed_ring_im[0][pl][i];
                float dr_re = state->bed_ring_re[1][pr][i];
                float dr_im = state->bed_ring_im[1][pr][i];

                l_re = 0.79f * l_re + d * dl_re - x * dr_re;
                l_im = 0.79f * l_im + d * dl_im - x * dr_im;
                r_re = 0.79f * r_re + d * dr_re - x * dl_re;
                r_im = 0.79f * r_im + d * dr_im - x * dl_im;
            }
            state->fft_re[SPA_HSSR_LEFT][i] += row * l_re;
            state->fft_im[SPA_HSSR_LEFT][i] += row * l_im;
            state->fft_re[SPA_HSSR_RIGHT][i] += row * r_re;
            state->fft_im[SPA_HSSR_RIGHT][i] += row * r_im;
        }
        bin = end;
    }
    state->bed_ring_pos = pos + 1u;
}

#ifdef SPA_STEREO_UPMIX_TESTING
float spa_hssr_test_attack_step(float energy, float *history, float *weight)
{ return spa_hssr_attack_step(energy, history, weight); }
void spa_hssr_test_mix_bed(struct spa_hssr_dsp *state,
                           float out_re[2][SPA_HSSR_BINS],
                           float out_im[2][SPA_HSSR_BINS])
{
    memset(state->fft_re, 0, sizeof(state->fft_re));
    memset(state->fft_im, 0, sizeof(state->fft_im));
    spa_hssr_mix_bed(state);
    memcpy(out_re[0], state->fft_re[0], SPA_HSSR_BINS * sizeof(float));
    memcpy(out_re[1], state->fft_re[1], SPA_HSSR_BINS * sizeof(float));
    memcpy(out_im[0], state->fft_im[0], SPA_HSSR_BINS * sizeof(float));
    memcpy(out_im[1], state->fft_im[1], SPA_HSSR_BINS * sizeof(float));
}
#endif

/* Build the full complex spectrum Z = A + jB in fft_[re|im][LEFT] from two
 * Hermitian half-spectra (bins 0..N/2), so one inverse transform yields both
 * real signals: Re -> A's signal, Im -> B's signal.
 *   k <= N/2 : Z = (Ar - Bi, Ai + Br)
 *   k >  N/2 : Z = (Ar' + Bi', Br' - Ai'),  ' = index N-k
 * When A aliases fft_[re|im][LEFT] the upper half must be written first,
 * because it reads A at 1..N/2-1 which the lower half then overwrites. */
static void spa_hssr_pack_hermitian(struct spa_hssr_dsp *state,
                                    const float *a_re, const float *a_im,
                                    const float *b_re, const float *b_im,
                                    int a_aliases_destination)
{
    float *zr = state->fft_re[SPA_HSSR_LEFT];
    float *zi = state->fft_im[SPA_HSSR_LEFT];
    unsigned bin;
    unsigned pass;

    for (pass = 0; pass < 2; ++pass) {
        int upper = a_aliases_destination ? (pass == 0) : (pass == 1);

        if (upper) {
            for (bin = SPA_HSSR_FFT_SIZE / 2 + 1; bin < SPA_HSSR_FFT_SIZE; ++bin) {
                unsigned mirror = SPA_HSSR_FFT_SIZE - bin;

                zr[bin] = a_re[mirror] + b_im[mirror];
                zi[bin] = b_re[mirror] - a_im[mirror];
            }
        } else {
            for (bin = 0; bin <= SPA_HSSR_FFT_SIZE / 2; ++bin) {
                float ar = a_re[bin];
                float ai = (bin == 0 || bin == SPA_HSSR_FFT_SIZE / 2)
                    ? 0.0f : a_im[bin];
                float br = b_re[bin];
                float bi = (bin == 0 || bin == SPA_HSSR_FFT_SIZE / 2)
                    ? 0.0f : b_im[bin];

                zr[bin] = ar - bi;
                zi[bin] = ai + br;
            }
        }
    }
}

static void spa_hssr_render_hop(
    struct spa_hssr_dsp *state,
    const struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS])
{
    unsigned bin;
    unsigned band;
    const int split = state->bed_delays || state->bed_attack;
    float band_best[SPA_HSSR_BANDS];
    float band_inverse_average[SPA_HSSR_BANDS];
    float band_transient[SPA_HSSR_BANDS];
    float band_content_floor[SPA_HSSR_BANDS];
    float band_motion_support[SPA_HSSR_BANDS];
    float band_ambience_scale[SPA_HSSR_BANDS];
    float band_diffuse_scale[SPA_HSSR_BANDS];
    /* Vocal gate per band: veto x whole-voice presence, then per bin by
     * comodulation with the voice body (spa_hssr_como).  Sparing a guitar that
     * shares the band by PAN proximity was tried first: the voice's own top
     * bins have a noisy pan and lost half their protection for little gain. */
    float band_gate[SPA_HSSR_BANDS];
    float band_air[SPA_HSSR_BANDS];
    float body_log;

    memset(band_best, 0, sizeof(band_best));
    for (band = 0; band < SPA_HSSR_BANDS; ++band)
        band_air[band] = state->rear_air * spa_hssr_dsp_smoothstep(
            (float)kHssrBandEdgeHz[band], 2000.0f, 4000.0f);
    {
        float body = 0.0f;

        for (band = 0; band < SPA_HSSR_BANDS; ++band) {
            band_gate[band] = state->vocal_gate * state->vocal_presence *
                state->band_vocal_veto[band];
            if (kHssrBandEdgeHz[band] < 350 || kHssrBandEdgeHz[band] >= 2500) continue;
            body += state->band_vocal_veto[band] * (bands[band].pl + bands[band].pr);
        }
        body_log = logf(body + 1.0e-6f);
    }
    if (state->bed_attack) {
        for (band = 0; band < SPA_HSSR_BANDS; ++band)
            spa_hssr_attack_step(bands[band].pl + bands[band].pr,
                                 &state->band_attack_history[band],
                                 &state->band_attack_weight[band]);
    }

    {
        const float epsilon = 1.0e-8f;

        spa_hssr_update_object_slots(state, bands);
        /* Everything below that depends only on the BAND is computed once per
         * band instead of once per bin: 257 bins share 32 bands, so this used
         * to repeat two divisions and three smoothsteps about eight times per
         * value.  Pure hoisting, same arithmetic. */
        for (band = 0; band < SPA_HSSR_BANDS; ++band) {
            float average = bands[band].bins
                ? bands[band].magnitude / (float)bands[band].bins
                : 0.0f;
            float transient = spa_hssr_dsp_smoothstep(
                bands[band].flux / (bands[band].magnitude + epsilon),
                0.015f, 0.30f);
            float learned = spa_hssr_dsp_smoothstep(
                state->band_mask[band], 0.015f, 0.24f);
            float safety = state->band_universal_safety[band];
            float percussion = 0.85f * transient * kHssrPercussionPresence[band];

            if (percussion > safety) safety = percussion;
            safety = 0.04f + 0.96f * safety;
            band_inverse_average[band] = 1.0f / (average + epsilon);
            band_transient[band] = transient;
            band_content_floor[band] = (0.220f + 0.440f * learned) * safety;
            band_motion_support[band] = 0.70f + 0.30f * transient;
            band_ambience_scale[band] = 0.018f * state->band_mask[band] *
                state->band_diffuse[band];
            band_diffuse_scale[band] = kHssrBandEdgeHz[band + 1] <= 220
                ? 0.0f : 0.35f * state->band_universal_safety[band];
        }
        for (bin = 0; bin < SPA_HSSR_BINS; ++bin) {
            float left_re = state->fft_re[SPA_HSSR_LEFT][bin];
            float left_im = state->fft_im[SPA_HSSR_LEFT][bin];
            float right_re = state->fft_re[SPA_HSSR_RIGHT][bin];
            float right_im = state->fft_im[SPA_HSSR_RIGHT][bin];
            float pl = left_re * left_re + left_im * left_im;
            float pr = right_re * right_re + right_im * right_im;
            float magnitude = sqrtf(0.5f * (pl + pr));
            float amplitude_l = sqrtf(pl + epsilon);
            float amplitude_r = sqrtf(pr + epsilon);
            float instant_pan = (amplitude_l - amplitude_r) /
                (amplitude_l + amplitude_r + epsilon);
            float contrast;
            float ridge_gate;
            float best_score = 0.0f;
            float best_pan = state->bin[bin].object_pan;
            float object_target;
            float ambience_target;
            float universal_target;
            float focus_presence;
            float focus_transient;
            float alpha;
            unsigned slot_index;

            band = state->bin_band[bin];
            contrast = magnitude * band_inverse_average[band];
            ridge_gate = spa_hssr_dsp_smoothstep(
                contrast, 0.70f, 1.65f);
            focus_presence = kHssrObjectFocusPresence[band];
            focus_transient = band_transient[band];

            for (slot_index = 0; slot_index < SPA_HSSR_OBJECT_SLOTS;
                 ++slot_index) {
                const struct spa_hssr_object_slot *slot =
                    &state->object_slot[slot_index];
                float weight = slot->band_weight[band];
                float pan_match;
                float score;

                /* pan_match is in [0,1], so a slot whose band weight cannot
                 * beat the incumbent cannot win: skipping it is exact and
                 * removes the smoothstep for every idle slot. */
                if (weight <= best_score) continue;
                pan_match = 1.0f - spa_hssr_dsp_smoothstep(
                    fabsf(instant_pan - slot->pan), 0.12f, 0.52f);
                score = weight * pan_match;
                if (score > best_score) {
                    best_score = score;
                    best_pan = slot->pan;
                }
            }

            if (best_score > band_best[band]) band_best[band] = best_score;

            /* A ridge is useful only after it agrees with a persistent
             * spatial slot, rejecting unrelated centred harmonics. */
            object_target = 2.10f * best_score *
                (0.18f + 0.82f * ridge_gate);
            object_target *= 1.15f +
                0.35f * state->rear_activity_target;
            object_target *= 1.0f + 0.40f * focus_presence *
                (0.30f + 0.70f * focus_transient);
            object_target = spa_hssr_dsp_bound(object_target, 0.0f, 1.0f);
            {
                /* Inter-channel coherence per bin, ~45 ms (Avendano & Jot
                 * 2004 ambience index; DirAC diffuseness).  A panned object
                 * is coherent; reverb/room is not and must not be steered. */
                const float ema = 0.06f;
                float coh;

                state->bin[bin].scene_pl += ema * (pl - state->bin[bin].scene_pl);
                state->bin[bin].scene_pr += ema * (pr - state->bin[bin].scene_pr);
                state->bin[bin].scene_cross_re += ema *
                    (left_re * right_re + left_im * right_im - state->bin[bin].scene_cross_re);
                state->bin[bin].scene_cross_im += ema *
                    (left_im * right_re - left_re * right_im - state->bin[bin].scene_cross_im);
                coh = spa_hssr_dsp_bound(
                    sqrtf((state->bin[bin].scene_cross_re * state->bin[bin].scene_cross_re +
                           state->bin[bin].scene_cross_im * state->bin[bin].scene_cross_im) /
                          (state->bin[bin].scene_pl * state->bin[bin].scene_pr + 1.0e-12f)),
                    0.0f, 1.0f);
                state->bin[bin].scene_coh = coh;
                object_target *= 0.50f + 0.50f *
                    spa_hssr_dsp_smoothstep(coh, 0.25f, 0.55f);
            }
            alpha = object_target > state->bin[bin].mask
                ? 0.50f : 0.020f;
            state->bin[bin].mask += alpha *
                (object_target - state->bin[bin].mask);

            if (best_score > 0.01f)
                state->bin[bin].object_pan += 0.18f *
                    (best_pan - state->bin[bin].object_pan);

            /* Keep only a quiet decorrelated safety bed when no coherent
             * object is accepted.  It preserves some envelopment without
             * competing with the explicit slot image. */
            ambience_target = band_ambience_scale[band] * (1.0f - best_score);
            ambience_target = spa_hssr_dsp_bound(
                ambience_target, 0.0f, 0.04f);
            alpha = ambience_target > state->bin[bin].ambience_mask
                ? 0.10f : 0.015f;
            state->bin[bin].ambience_mask += alpha *
                (ambience_target - state->bin[bin].ambience_mask);

            /* Universal is the no-dead-rear policy.  Prefer the explicit
             * half-plane object above; only while that path is weak, retain a
             * bounded mid/high TF ridge selected by the learned semantic
             * mask.  A small content-driven floor remains when the model is
             * uncertain, so a dense or nearly mono master cannot collapse to
             * completely silent rear channels.  The renderer below phase-
             * rotates and frequency-steers this bed; it is deliberately not
             * presented as a clean separated stem. */
            universal_target = 0.0f;
            {
                float object_absence = 0.40f + 0.60f *
                    (1.0f - spa_hssr_dsp_smoothstep(
                        best_score, 0.040f, 0.28f));
                float ridge_support = 0.45f + 0.55f * ridge_gate;

                universal_target = object_absence * focus_presence *
                    band_content_floor[band] * ridge_support *
                    band_motion_support[band];
                universal_target = spa_hssr_dsp_bound(
                    universal_target, 0.0f, 0.55f);
            }
            {
                /* Diffuse layer: what is NOT coherent is room/reverb; it goes
                 * behind through the decorrelated bed, under the semantic
                 * veto, never below 220 Hz. */
                float diffuse = band_diffuse_scale[band] *
                    (1.0f - spa_hssr_dsp_smoothstep(state->bin[bin].scene_coh, 0.25f, 0.60f)) *
                    (1.0f - best_score);

                if (diffuse > universal_target) universal_target = diffuse;
            }
            alpha = universal_target > state->bin[bin].universal_mask
                ? 0.28f : 0.012f;
            state->bin[bin].universal_mask += alpha *
                (universal_target - state->bin[bin].universal_mask);
        }
    }
    for (band = 0; band < SPA_HSSR_BANDS; ++band) {
        float owner = spa_hssr_dsp_smoothstep(band_best[band], 0.040f, 0.28f);
        float retention = state->front_retention;

        state->band_owner[band] = owner;
        /* Retention taper: keep the object in the front only where the body
         * lives.  Without it, half of a bright object stays in the front and
         * measures +1.3 dB in the front 2-16 kHz on real cuts. */
        if (state->retention_hf_hi_hz > state->retention_hf_lo_hz)
            retention *= 1.0f - spa_hssr_dsp_smoothstep(
                (float)kHssrBandEdgeHz[band],
                state->retention_hf_lo_hz, state->retention_hf_hi_hz);
        if (!state->bed_attack)
            state->band_front_keep[band] = 1.0f; /* time-domain retention */
        else if (state->bed_attack == SPA_HSSR_BED_ATTACK_OWNER)
            state->band_front_keep[band] = 1.0f - retention *
                (1.0f - state->attack_collapse *
                 state->band_attack_weight[band] * owner);
        else
            state->band_front_keep[band] = 1.0f - retention;
    }

    for (bin = 0; bin < SPA_HSSR_BINS; ++bin) {
        float left_re = state->fft_re[SPA_HSSR_LEFT][bin];
        float left_im = state->fft_im[SPA_HSSR_LEFT][bin];
        float right_re = state->fft_re[SPA_HSSR_RIGHT][bin];
        float right_im = state->fft_im[SPA_HSSR_RIGHT][bin];
        float mid_re = 0.5f * (left_re + right_re);
        float mid_im = 0.5f * (left_im + right_im);
        float mask;
        float pan;
        /* the OBJECT's source spectrum: the mixture, or the injected eligible audio */
        float obj_l_re = left_re, obj_l_im = left_im, obj_r_re = right_re, obj_r_im = right_im;

        band = state->bin_band[bin];
        mask = state->bin[bin].mask;
        mask *= 1.0f + state->band_object_boost[band];
        if (state->mask_ready && state->mask_drive > 0.0f) {
            /* Phase 1: the learned eligibility drives the object (amplitude
             * sqrt(e), pan = the bin's own balance); the heuristic slots /
             * ridge / coherence are bypassed, the vocal gate below stays. */
            float e = state->mask_eligibility[bin];

            if (e < 0.0f) e = 0.0f;
            if (e > 1.0f) e = 1.0f;
            e = state->mask_gamma_lut[(unsigned)(e * (float)(SPA_HSSR_BINS - 1) + 0.5f)];
            mask = sqrtf(e) * state->mask_drive_bin[bin];
            state->bin[bin].object_pan = state->bin[bin].scene_lr;
        }
#ifdef SPA_HSSR_TEST_MASK
        if (state->test_inject_pcm) {
            float pl, pr;
            obj_l_re = state->inject_re[SPA_HSSR_LEFT][bin]; obj_l_im = state->inject_im[SPA_HSSR_LEFT][bin];
            obj_r_re = state->inject_re[SPA_HSSR_RIGHT][bin]; obj_r_im = state->inject_im[SPA_HSSR_RIGHT][bin];
            pl = obj_l_re * obj_l_re + obj_l_im * obj_l_im;
            pr = obj_r_re * obj_r_re + obj_r_im * obj_r_im;
            mask = state->test_mask_drive > 0.0f ? state->test_mask_drive : 1.0f;
            state->bin[bin].object_pan = (pl - pr) / (pl + pr + 1.0e-9f);
            goto masked;
        }
        if (state->test_mask) {
            uint32_t idx = state->control_sequence ? state->control_sequence - 1u : 0u;
            float protect;

            if (idx >= state->test_mask_controls) idx = state->test_mask_controls - 1u;
            protect = state->test_mask[idx * SPA_HSSR_BINS + bin];
            if (state->test_mask_drive > 0.0f) {
                mask = (1.0f - protect) * state->test_mask_drive;
                state->bin[bin].object_pan = state->bin[bin].scene_lr;
                if (!state->test_mask_keep_gate) goto masked;
            } else {
                mask *= 1.0f - state->vocal_gate * protect;
                goto masked;
            }
        }
#endif
        if (band_gate[band] > 0.0f) {
            const float a = 0.012f;                       /* ~0.22 s at 2.67 ms hops */
            struct spa_hssr_como *cm = &state->como[bin];
            float x = logf(left_re * left_re + left_im * left_im +
                           right_re * right_re + right_im * right_im + 1.0e-6f);
            float corr;

            cm->mx += a * (x - cm->mx);
            cm->my += a * (body_log - cm->my);
            cm->xx += a * ((x - cm->mx) * (x - cm->mx) - cm->xx);
            cm->yy += a * ((body_log - cm->my) * (body_log - cm->my) - cm->yy);
            cm->xy += a * ((x - cm->mx) * (body_log - cm->my) - cm->xy);
            corr = cm->xy / sqrtf(cm->xx * cm->yy + 1.0e-6f);
            cm->g += 0.05f * (spa_hssr_dsp_smoothstep(corr, 0.0f, 0.40f) - cm->g);
            mask *= 1.0f - band_gate[band] * cm->g;
        }
#ifdef SPA_HSSR_TEST_MASK
masked:
#endif
        {
            float source_l;
            float source_r;
            float source_scale;
            float steering_l;
            float steering_r;
            float steering_scale;
            float projected_re;
            float projected_im;
            float extraction_l;
            float extraction_r;
            float side_re = 0.5f * (left_re - right_re);
            float side_im = 0.5f * (left_im - right_im);
            float ambience = state->bin[bin].ambience_mask;
            float universal = state->bin[bin].universal_mask;
            {
                float pl = left_re * left_re + left_im * left_im;
                float pr = right_re * right_re + right_im * right_im;
                state->bin[bin].scene_lr += 0.05f *
                    ((pl - pr) / (pl + pr + 1.0e-9f) - state->bin[bin].scene_lr);
            }

            mask = spa_hssr_dsp_bound(mask, 0.0f, 1.45f);
            pan = spa_hssr_dsp_bound(
                state->bin[bin].object_pan, -0.95f, 0.95f);
            /* Extract with the measured source vector and hard-steer only the
             * output vector, keeping unrelated material out of the rear. */
            source_l = 1.0f + pan;
            source_r = 1.0f - pan;
            source_scale = sqrtf(2.0f /
                (source_l * source_l + source_r * source_r + 1.0e-8f));
            source_l *= source_scale;
            source_r *= source_scale;
            {
                float magnitude = fabsf(pan);
                float focused = 0.78f + 0.17f *
                    spa_hssr_dsp_smoothstep(magnitude, 0.05f, 0.92f);

                pan = pan < 0.0f ? -focused : focused;
            }
            steering_l = 1.0f + pan;
            steering_r = 1.0f - pan;
            steering_scale = sqrtf(2.0f /
                (steering_l * steering_l + steering_r * steering_r +
                 1.0e-8f));
            steering_l *= steering_scale;
            steering_r *= steering_scale;

            /* Rank-one intensity-pan projection.  For a source matching the
             * tracked steering vector this reconstructs its original stereo
             * image; material at a different pan is attenuated before the
             * source-aware front transfer in stereo_upmix.c. */
            extraction_l = source_l;
            extraction_r = source_r;
            projected_re = 0.5f *
                (extraction_l * obj_l_re + extraction_r * obj_r_re);
            projected_im = 0.5f *
                (extraction_l * obj_l_im + extraction_r * obj_r_im);
            {
                /* Original source image, BEFORE steering or ambience.  The
                 * subtraction never exceeds the source itself (mask <= 1). */
                float source_mask = mask > 1.0f ? 1.0f : mask;

                /* With attack handling the front retention lives here per
                 * band and hop; stereo_upmix.c then transfers the whole
                 * source.  OWNER: a hit pulls the object to the rear. */
                source_mask *= state->band_front_keep[band];
                state->source_fft_re[0][bin] = source_mask * source_l * projected_re;
                state->source_fft_im[0][bin] = source_mask * source_l * projected_im;
                state->source_fft_re[1][bin] = source_mask * source_r * projected_re;
                state->source_fft_im[1][bin] = source_mask * source_r * projected_im;
            }
            if (!split) {
                state->fft_re[SPA_HSSR_LEFT][bin] =
                    mask * steering_l * projected_re + ambience * side_re;
                state->fft_im[SPA_HSSR_LEFT][bin] =
                    mask * steering_l * projected_im + ambience * side_im;
                state->fft_re[SPA_HSSR_RIGHT][bin] =
                    mask * steering_r * projected_re - ambience * side_re;
                state->fft_im[SPA_HSSR_RIGHT][bin] =
                    mask * steering_r * projected_im - ambience * side_im;
            } else {
                /* Object only; the bed is distributed by spa_hssr_mix_bed. */
                state->fft_re[SPA_HSSR_LEFT][bin] = mask * steering_l * projected_re;
                state->fft_im[SPA_HSSR_LEFT][bin] = mask * steering_l * projected_im;
                state->fft_re[SPA_HSSR_RIGHT][bin] = mask * steering_r * projected_re;
                state->fft_im[SPA_HSSR_RIGHT][bin] = mask * steering_r * projected_im;
                state->bed_re[0][bin] = ambience * side_re + band_air[band] * left_re;
                state->bed_im[0][bin] = ambience * side_im + band_air[band] * left_im;
                state->bed_re[1][bin] = -ambience * side_re + band_air[band] * right_re;
                state->bed_im[1][bin] = -ambience * side_im + band_air[band] * right_im;
            }
            if (universal > 0.0f) {
                /* A bounded conjugate phase pair prevents the fallback from
                 * degenerating into identical SL/SR or x/-x.  Alternating
                 * broad-band steering makes percussion/keys/pads perceptibly
                 * distinct without time-varying pan wandering. */
                unsigned rotation = 18u + (bin * 13u) % 47u;
                float wr = state->twiddle_re[rotation];
                float wi = -state->twiddle_im[rotation];
                float fallback_pan = 0.72f * spa_hssr_dsp_bound(
                    3.0f * state->bin[bin].scene_lr, -1.0f, 1.0f);
                float fallback_l = sqrtf(1.0f + fallback_pan);
                float fallback_r = sqrtf(1.0f - fallback_pan);
                float rotated_l_re = wr * mid_re - wi * mid_im;
                float rotated_l_im = wr * mid_im + wi * mid_re;
                float rotated_r_re = wr * mid_re + wi * mid_im;
                float rotated_r_im = wr * mid_im - wi * mid_re;

                universal *= state->universal_makeup;
                if (!split) {
                    state->fft_re[SPA_HSSR_LEFT][bin] +=
                        universal * fallback_l * rotated_l_re;
                    state->fft_im[SPA_HSSR_LEFT][bin] +=
                        universal * fallback_l * rotated_l_im;
                    state->fft_re[SPA_HSSR_RIGHT][bin] +=
                        universal * fallback_r * rotated_r_re;
                    state->fft_im[SPA_HSSR_RIGHT][bin] +=
                        universal * fallback_r * rotated_r_im;
                } else {
                    state->bed_re[0][bin] += universal * fallback_l * rotated_l_re;
                    state->bed_im[0][bin] += universal * fallback_l * rotated_l_im;
                    state->bed_re[1][bin] += universal * fallback_r * rotated_r_re;
                    state->bed_im[1][bin] += universal * fallback_r * rotated_r_im;
                }
            }
        }
    }

    if (split) spa_hssr_mix_bed(state);

    /* Both outputs are real, so Z = A + jB inverts to a + jb in ONE transform:
     * the real part is the left channel and the imaginary part the right.
     * Exact in real arithmetic; halves the synthesis transform count. */
    spa_hssr_pack_hermitian(state, state->fft_re[SPA_HSSR_LEFT],
                            state->fft_im[SPA_HSSR_LEFT],
                            state->fft_re[SPA_HSSR_RIGHT],
                            state->fft_im[SPA_HSSR_RIGHT], 1);
    spa_hssr_fft(state, SPA_HSSR_LEFT, 1);
    {
        const unsigned start = state->input_position & (SPA_HSSR_FFT_SIZE - 1);
        const unsigned first = SPA_HSSR_FFT_SIZE - start;

        spa_hssr_overlap_run(state->ola[SPA_HSSR_LEFT] + start,
                             state->ola[SPA_HSSR_RIGHT] + start,
                             state->fft_re[SPA_HSSR_LEFT],
                             state->fft_im[SPA_HSSR_LEFT],
                             state->synthesis_window_scaled, first);
        spa_hssr_overlap_run(state->ola[SPA_HSSR_LEFT],
                             state->ola[SPA_HSSR_RIGHT],
                             state->fft_re[SPA_HSSR_LEFT] + first,
                             state->fft_im[SPA_HSSR_LEFT] + first,
                             state->synthesis_window_scaled + first,
                             SPA_HSSR_FFT_SIZE - first);
    }
    /* Source image, same pairing.  Its spectra live in their own arrays, so the
     * packing has no aliasing to respect. */
    spa_hssr_pack_hermitian(state, state->source_fft_re[0],
                            state->source_fft_im[0],
                            state->source_fft_re[1],
                            state->source_fft_im[1], 0);
    spa_hssr_fft(state, SPA_HSSR_LEFT, 1);
    {
        const unsigned start = state->input_position & (SPA_HSSR_FFT_SIZE - 1);
        const unsigned first = SPA_HSSR_FFT_SIZE - start;

        spa_hssr_overlap_run(state->source_ola[0] + start,
                             state->source_ola[1] + start,
                             state->fft_re[SPA_HSSR_LEFT],
                             state->fft_im[SPA_HSSR_LEFT],
                             state->synthesis_window_scaled, first);
        spa_hssr_overlap_run(state->source_ola[0], state->source_ola[1],
                             state->fft_re[SPA_HSSR_LEFT] + first,
                             state->fft_im[SPA_HSSR_LEFT] + first,
                             state->synthesis_window_scaled + first,
                             SPA_HSSR_FFT_SIZE - first);
    }
}

static void spa_hssr_run_hop(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    const struct spa_hssr_control_provider *provider)
{
    struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS];
    int full_frame = spa_hssr_analyze_hop(state, bands);

    spa_hssr_run_control(state, model, provider, bands, full_frame);
    spa_hssr_render_hop(state, bands);
}

static void spa_hssr_dsp_process_frame(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    const struct spa_hssr_control_provider *provider,
    int16_t input_l, int16_t input_r,
    int16_t *front_l, int16_t *front_r, float *rear_l, float *rear_r,
    float *lfe_bass, float *lfe_drums)
{
    unsigned position;
    int ready;

    position = state->input_position;
    ready = state->frames_seen >= SPA_HSSR_LATENCY_FRAMES;
    *front_l = ready ? state->front_delay[SPA_HSSR_LEFT][position] : 0;
    *front_r = ready ? state->front_delay[SPA_HSSR_RIGHT][position] : 0;
    *rear_l = ready ? state->ola[SPA_HSSR_LEFT][position] : 0.0f;
    *rear_r = ready ? state->ola[SPA_HSSR_RIGHT][position] : 0.0f;
    state->source_sample[0] = ready ? state->source_ola[0][position] : 0.0f;
    state->source_sample[1] = ready ? state->source_ola[1][position] : 0.0f;
    state->source_ola[0][position] = state->source_ola[1][position] = 0.0f;
    *lfe_bass = ready ? state->lfe_semantic_delay[0][position] : 0.0f;
    *lfe_drums = ready ? state->lfe_semantic_delay[1][position] : 0.0f;
    if (ready) {
        const float power_alpha = 0.0005f;
        float normalized_l = (float)input_l * (1.0f / 32768.0f);
        float normalized_r = (float)input_r * (1.0f / 32768.0f);
        float input_power = 0.5f *
            (normalized_l * normalized_l + normalized_r * normalized_r);
        float rear_power = 0.5f *
            (*rear_l * *rear_l + *rear_r * *rear_r);
        float ratio;
        float desired;
        float makeup_alpha;

        state->universal_input_power += power_alpha *
            (input_power - state->universal_input_power);
        state->universal_rear_power += power_alpha *
            (rear_power - state->universal_rear_power);
        ratio = state->universal_rear_power /
            (state->universal_input_power + 1.0e-8f);
        desired = state->universal_input_power > 1.0e-7f
            ? sqrtf(0.32f / (ratio + 1.0e-8f)) : 1.0f;
        /* No forced 32 % rear activity: the bed may only double, never x3.5. */
        desired = spa_hssr_dsp_bound(desired, 1.0f, 1.50f);
        makeup_alpha = desired > state->universal_makeup
            ? 0.0020f : 0.00035f;
        state->universal_makeup += makeup_alpha *
            (desired - state->universal_makeup);
    }
    state->ola[SPA_HSSR_LEFT][position] = 0.0f;
    state->ola[SPA_HSSR_RIGHT][position] = 0.0f;
    state->front_delay[SPA_HSSR_LEFT][position] = input_l;
    state->front_delay[SPA_HSSR_RIGHT][position] = input_r;
    state->lfe_semantic_delay[0][position] = state->lfe_bass_confidence;
    state->lfe_semantic_delay[1][position] = state->lfe_drum_confidence;
    state->input[SPA_HSSR_LEFT][position] =
        (float)input_l * (1.0f / 32768.0f);
    state->input[SPA_HSSR_RIGHT][position] =
        (float)input_r * (1.0f / 32768.0f);
#ifdef SPA_HSSR_TEST_MASK
    if (state->test_inject_pcm) {
        uint32_t k = state->test_inject_pos < state->test_inject_frames ? state->test_inject_pos : state->test_inject_frames - 1u;
        state->inject[SPA_HSSR_LEFT][position] = (float)state->test_inject_pcm[2u * k] * (1.0f / 32768.0f);
        state->inject[SPA_HSSR_RIGHT][position] = (float)state->test_inject_pcm[2u * k + 1u] * (1.0f / 32768.0f);
        state->test_inject_pos++;
    }
#endif
    if (state->frames_seen != UINT32_MAX) state->frames_seen++;

    state->input_position = (uint16_t)((position + 1) &
                            (SPA_HSSR_FFT_SIZE - 1));
    state->hop_fill++;
    if (state->hop_fill == SPA_HSSR_HOP_SIZE) {
        state->hop_fill = 0;
        spa_hssr_run_hop(state, model, provider);
    }
    *rear_l = spa_hssr_dsp_bound(
        spa_hssr_dsp_finite(*rear_l), -2.0f, 2.0f);
    *rear_r = spa_hssr_dsp_bound(
        spa_hssr_dsp_finite(*rear_r), -2.0f, 2.0f);
}

int spa_hssr_dsp_process_sample(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    int16_t input_l, int16_t input_r,
    int16_t *front_l, int16_t *front_r,
    float *rear_l, float *rear_r)
{
    float lfe_bass;
    float lfe_drums;

    if (!state || !model || !front_l || !front_r || !rear_l || !rear_r)
        return SPA_HSSR_ERROR_ARGUMENT;
    if (!spa_hssr_model_ready(model)) return SPA_HSSR_ERROR_NOT_READY;

    spa_hssr_dsp_process_frame(state, model, NULL, input_l, input_r,
                               front_l, front_r, rear_l, rear_r,
                               &lfe_bass, &lfe_drums);
    return SPA_HSSR_OK;
}

int spa_hssr_dsp_process_block(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    const struct spa_hssr_control_provider *provider,
    const int16_t *pcm2, size_t frames,
    int16_t *front2, float *rear2, float *lfe_control2)
{
    return spa_hssr_dsp_process_block_sources(state, model, provider, pcm2,
        frames, front2, rear2, lfe_control2, NULL);
}

int spa_hssr_dsp_process_block_sources(
    struct spa_hssr_dsp *state, const struct spa_hssr_model *model,
    const struct spa_hssr_control_provider *provider,
    const int16_t *pcm2, size_t frames,
    int16_t *front2, float *rear2, float *lfe_control2, float *source2)
{
    size_t offset = 0;

    if (!state || !model || (!pcm2 && frames) ||
        (!front2 && frames) || (!rear2 && frames) ||
        (!lfe_control2 && frames))
        return SPA_HSSR_ERROR_ARGUMENT;
    if (!spa_hssr_model_ready(model)) return SPA_HSSR_ERROR_NOT_READY;
    if (provider && (!provider->submit || !provider->receive ||
                     !provider->reset))
        return SPA_HSSR_ERROR_ARGUMENT;

    while (offset < frames) {
        size_t chunk = SPA_HSSR_HOP_SIZE - state->hop_fill;
        size_t index;

        if (chunk > frames - offset) chunk = frames - offset;
        for (index = 0; index < chunk; ++index) {
            size_t frame = offset + index;

            spa_hssr_dsp_process_frame(
                state, model, provider,
                pcm2[frame * 2u], pcm2[frame * 2u + 1u],
                &front2[frame * 2u], &front2[frame * 2u + 1u],
                &rear2[frame * 2u], &rear2[frame * 2u + 1u],
                &lfe_control2[frame * 2u],
                &lfe_control2[frame * 2u + 1u]);
            if (source2) {
                source2[frame*2] = state->source_sample[0];
                source2[frame*2+1] = state->source_sample[1];
            }
        }
        offset += chunk;
    }
    return SPA_HSSR_OK;
}
