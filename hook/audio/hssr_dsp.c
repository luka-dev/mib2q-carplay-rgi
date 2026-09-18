/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "hssr_dsp.h"

#include <math.h>
#if defined(SPA_HSSR_SCENE_DEBUG) || defined(SPA_HSSR_SCENE_DEBUG_SLOTS)
#include <stdio.h>
#endif
#include <string.h>

#ifdef SPA_HSSR_SCENE
#define SPA_HSSR_SCENE_ON 1   /* source-aligned transfer path always computed */
#else
#define SPA_HSSR_SCENE_ON 0
#endif
#ifdef SPA_HSSR_SCENE_TRUEPAN
#define SPA_HSSR_SCENE_TRUEPAN_ON 1   /* objects keep their own pan behind */
#else
#define SPA_HSSR_SCENE_TRUEPAN_ON 0
#endif

/* Host-only research ablation, never enabled implicitly in a vehicle build. */
#if (defined(SPA_HSSR_EXPERIMENT_CENTER_NULL) || defined(SPA_HSSR_EXPERIMENT_SIDE_OBJECT)) && !defined(SPA_HSSR_CAPTURE_TRANSFERS)
#error Experimental extraction requires the host transfer-capture build
#endif
#if defined(SPA_HSSR_EXPERIMENT_CENTER_NULL) && defined(SPA_HSSR_EXPERIMENT_SIDE_OBJECT)
#error Choose one extraction ablation
#endif

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

static void spa_hssr_fft(struct spa_hssr_dsp *state,
                         unsigned channel, int inverse)
{
    float *real = state->fft_re[channel];
    float *imaginary = state->fft_im[channel];
    unsigned index;
    unsigned reversed = 0;
    unsigned length;

    for (index = 1; index < SPA_HSSR_FFT_SIZE; ++index) {
        unsigned bit = SPA_HSSR_FFT_SIZE >> 1;

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

    for (length = 2; length <= SPA_HSSR_FFT_SIZE; length <<= 1) {
        unsigned half = length >> 1;
        unsigned stride = SPA_HSSR_FFT_SIZE / length;
        unsigned base;

        for (base = 0; base < SPA_HSSR_FFT_SIZE; base += length) {
            unsigned offset;

            for (offset = 0; offset < half; ++offset) {
                unsigned twiddle = offset * stride;
                float wr = state->twiddle_re[twiddle];
                float wi = state->twiddle_im[twiddle];
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
        const float scale = 1.0f / (float)SPA_HSSR_FFT_SIZE;

        for (index = 0; index < SPA_HSSR_FFT_SIZE; ++index) {
            real[index] = spa_hssr_dsp_finite(real[index] * scale);
            imaginary[index] = spa_hssr_dsp_finite(
                imaginary[index] * scale);
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
        next_re = re * fft_step_re - im * fft_step_im;
        next_im = im * fft_step_re + re * fft_step_im;
        re = next_re;
        im = next_im;
    }

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
#ifdef SPA_HSSR_SCENE
        float guard_vocal = 0.0f;   /* guard's vocal posterior, keys-discounted */
#endif

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
#ifdef SPA_HSSR_SCENE
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
#endif

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

#ifdef SPA_HSSR_SCENE
                /* A voice the guard hears is a voice even when it is wide:
                 * the spatial escape exists for V11's over-called vocals,
                 * not for a lead the main model misreads as keys. */
                if (guard_vocal > vocal_risk) vocal_risk = guard_vocal;
                /* The main model's learned rearability is high where it
                 * misreads a voice as keys; the guard overrides it too. */
                target *= (1.0f - 0.95f * guard_vocal) * (1.0f - 0.95f * guard_vocal);
#endif
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
#ifdef SPA_HSSR_SCENE
                focus_rescue_target *= (1.0f - guard_vocal) * (1.0f - guard_vocal);
#endif
                focus_rescue_target = spa_hssr_dsp_bound(
                    focus_rescue_target, 0.0f, 0.65f);
            }
            {
#ifdef SPA_HSSR_SCENE
                /* MoisesDB-trained model names guitar explicitly (V11 filed
                 * it under background/fx); rhythm guitar is second-scene
                 * material, so it counts 0.80 instead of 0.35. */
                float safe_class = probabilities[2] +
                    0.80f * probabilities[3] + probabilities[4] +
                    probabilities[5];
#else
                float safe_class = probabilities[2] +
                    0.35f * probabilities[3] + probabilities[4] +
                    probabilities[5];
#endif
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
        if (kHssrBandEdgeHz[band + 1] <= 125)
            target = 0.0f;
        else if (kHssrBandEdgeHz[band] < 220)
            target *= ((float)kHssrBandEdgeHz[band + 1] - 125.0f) /
                      (220.0f - 125.0f);
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
#ifdef SPA_HSSR_SCENE_SLOTS4
    float alt_pan[2] = { 0.0f, 0.0f };   /* seed for an idle secondary slot: bands far from the primary, per side */
    float alt_w[2] = { 0.0f, 0.0f };
#endif
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

        if (kHssrBandEdgeHz[band + 1] <= 125) continue;
        for (slot_index = 0; slot_index < SPA_HSSR_OBJECT_SLOTS;
             ++slot_index) {
            float directed_pan = (slot_index & 1u) == 0u ? pan : -pan;
            float spatial = spa_hssr_dsp_smoothstep(
                directed_pan, 0.025f, 0.40f);
            float candidate = semantic * spatial *
                (0.55f + 0.45f * coherence);
#ifdef SPA_HSSR_SCENE_SLOTS4
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
#endif
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
#ifdef SPA_HSSR_SCENE_SLOTS4
            weighted_pan[slot_index] += weighted * pan_share * pan;
            pan_weight[slot_index] += weighted * pan_share;
#else
            weighted_pan[slot_index] += weighted * pan;
            pan_weight[slot_index] += weighted;
#endif
            if (candidate > peak_candidate[slot_index])
                peak_candidate[slot_index] = candidate;
        }
    }

    for (slot_index = 0; slot_index < SPA_HSSR_OBJECT_SLOTS;
         ++slot_index) {
        struct spa_hssr_object_slot *slot =
            &state->object_slot[slot_index];
#ifdef SPA_HSSR_SCENE_SLOTS4
        /* Confidence judges the SIDE's object evidence; the split between the
         * two same-side slots only shapes their band weights and pans. */
        float energy_share = (candidate_energy[slot_index] +
            candidate_energy[slot_index ^ 2u]) / total_energy;
#else
        float energy_share = candidate_energy[slot_index] / total_energy;
#endif
        float target_confidence = spa_hssr_dsp_smoothstep(
                energy_share, 0.0008f, 0.040f) *
            spa_hssr_dsp_smoothstep(peak_candidate[slot_index],
                0.018f, 0.24f);
        float confidence_alpha;

        target_confidence *= 0.70f + 0.30f * state->scene_confidence;
        target_confidence *= 0.75f + 0.25f * state->scene_stability;
        if (target_confidence > 0.12f) {
#ifdef SPA_HSSR_SCENE_HOLD
            /* Hold stays at the stock 250 ms: the sweep 250..1200 ms changed
             * slot toggling by <=2/min; the slow confidence release and the
             * calm pan below are what keep an object.  Override for tests. */
#ifndef SPA_HSSR_SCENE_HOLD_HOPS
#define SPA_HSSR_SCENE_HOLD_HOPS 94u
#endif
            slot->hold_hops = SPA_HSSR_SCENE_HOLD_HOPS;
#else
            /* 94 hops at 48 kHz/128 is almost exactly 250 ms. */
            slot->hold_hops = 94u;
#endif
        } else if (slot->hold_hops != 0u) {
            --slot->hold_hops;
            if (target_confidence < 0.65f * slot->confidence)
                target_confidence = 0.65f * slot->confidence;
        }

#ifdef SPA_HSSR_SCENE_HOLD
        confidence_alpha = target_confidence > slot->confidence
            ? 0.26f : 0.006f;
#else
        confidence_alpha = target_confidence > slot->confidence
            ? 0.26f : 0.018f;
#endif
        slot->confidence += confidence_alpha *
            (target_confidence - slot->confidence);
        slot->confidence = spa_hssr_dsp_bound(slot->confidence, 0.0f, 1.0f);

#ifdef SPA_HSSR_SCENE_SLOTS4
        if (slot_index >= 2u && slot->confidence < 0.03f &&
            alt_w[slot_index - 2u] > epsilon) {
            float seed = alt_pan[slot_index - 2u] / alt_w[slot_index - 2u];
            slot->pan = (slot_index & 1u) == 0u
                ? spa_hssr_dsp_bound(seed, 0.05f, 0.92f)
                : spa_hssr_dsp_bound(seed, -0.92f, -0.05f);
        }
#endif
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
#ifdef SPA_HSSR_SCENE_HOLD
                /* A confident object keeps its place; only a weak one follows. */
                slot->pan += (slot->confidence > 0.30f ? 0.020f : 0.055f) *
                    (target_pan - slot->pan);
#else
                slot->pan += 0.055f * (target_pan - slot->pan);
#endif
        }

        for (band = 0; band < SPA_HSSR_BANDS; ++band) {
            float target = band_target[slot_index][band] *
                slot->confidence;
#ifdef SPA_HSSR_SCENE_HOLD
            float alpha = target > slot->band_weight[band]
                ? 0.34f : 0.010f;
#else
            float alpha = target > slot->band_weight[band]
                ? 0.34f : 0.020f;
#endif

            slot->band_weight[band] += alpha *
                (target - slot->band_weight[band]);
            slot->band_weight[band] = spa_hssr_dsp_bound(
                slot->band_weight[band], 0.0f, 1.0f);
        }
    }
#ifdef SPA_HSSR_SCENE_DEBUG_SLOTS
    fprintf(stderr, "SLOT %.3f %.3f %.3f %.3f %.4f %.5f\n",
        state->object_slot[0].confidence, state->object_slot[0].pan,
        state->object_slot[1].confidence, state->object_slot[1].pan,
        state->dbg_mask_sum / (float)SPA_HSSR_BINS,
        state->dbg_mask_delta / (float)SPA_HSSR_BINS);
    state->dbg_mask_sum = 0.0f; state->dbg_mask_delta = 0.0f;
#endif
}

/* Stage 1: window, FFT and feature-state analysis.  It is deliberately kept
 * on the audio thread because later masks consume this exact spectrum. */
static int spa_hssr_analyze_hop(
    struct spa_hssr_dsp *state,
    struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS])
{
    unsigned channel;
    unsigned sample;
    unsigned bin;
    unsigned band;
    int full_frame = state->frames_seen >= SPA_HSSR_FFT_SIZE;

    memset(bands, 0, sizeof(*bands) * SPA_HSSR_BANDS);
    for (channel = 0; channel < 2; ++channel) {
        for (sample = 0; sample < SPA_HSSR_FFT_SIZE; ++sample) {
            unsigned history = (state->input_position + sample) &
                               (SPA_HSSR_FFT_SIZE - 1);
            state->fft_re[channel][sample] =
                state->input[channel][history] * state->analysis_window[sample];
            state->fft_im[channel][sample] = 0.0f;
        }
        spa_hssr_fft(state, channel, 0);
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
#ifdef SPA_HSSR_SCENE
            state->guard_valid = state->guard_model &&
                spa_hssr_infer(state->guard_model, &state->guard_network,
                               state->features, state->guard_control) == SPA_HSSR_OK;
#endif
            spa_hssr_update_controls(state);
        } else {
            spa_hssr_control_failsafe(state);
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
#ifdef SPA_HSSR_SCENE
            state->guard_valid = provider->receive_guard &&
                provider->receive_guard(provider->context, state->guard_control) > 0;
#endif
            spa_hssr_update_controls(state);
        } else {
            deadline_missed = 1;
        }
    }

    if (!provider->submit ||
        provider->submit(provider->context,
                         state->control_generation,
                         state->control_sequence,
                         state->features) <= 0) {
        deadline_missed = 1;
    }
    if (deadline_missed) {
        state->control_deadlines_missed++;
        /* One late period is harmless: keep the preceding stable mask.
         * Repeated lateness decays toward a quiet, bounded rear image. */
        if (state->control_deadlines_missed >= 3u)
            spa_hssr_control_failsafe(state);
    }
}

/* Stage 3: deterministic mask construction, spectral routing and overlap-add
 * synthesis.  Like analysis, it never leaves the audio thread. */
static void spa_hssr_render_hop(
    struct spa_hssr_dsp *state,
    const struct spa_hssr_band_accumulator bands[SPA_HSSR_BANDS])
{
    unsigned channel;
    unsigned sample;
    unsigned bin;
    unsigned band;

    {
        const float epsilon = 1.0e-8f;

        spa_hssr_update_object_slots(state, bands);
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
            float average;
            float contrast;
            float ridge_gate;
            float best_score = 0.0f;
            float best_pan = state->bin_object_pan[bin];
            float object_target;
            float ambience_target;
            float universal_target;
            float focus_presence;
            float focus_transient;
            float alpha;
            unsigned slot_index;

            band = state->bin_band[bin];
            average = bands[band].bins
                ? bands[band].magnitude / (float)bands[band].bins
                : 0.0f;
            contrast = magnitude / (average + epsilon);
            ridge_gate = spa_hssr_dsp_smoothstep(
                contrast, 0.70f, 1.65f);
            focus_presence = 0.0f;
            focus_transient = 0.0f;
            {
                focus_presence = kHssrObjectFocusPresence[band];
                focus_transient = spa_hssr_dsp_smoothstep(
                    bands[band].flux /
                        (bands[band].magnitude + epsilon),
                    0.015f, 0.30f);
            }

            for (slot_index = 0; slot_index < SPA_HSSR_OBJECT_SLOTS;
                 ++slot_index) {
                const struct spa_hssr_object_slot *slot =
                    &state->object_slot[slot_index];
                float pan_match = 1.0f - spa_hssr_dsp_smoothstep(
                    fabsf(instant_pan - slot->pan), 0.12f, 0.52f);
                float score = slot->band_weight[band] * pan_match;

                if (score > best_score) {
                    best_score = score;
                    best_pan = slot->pan;
                }
            }

            /* A ridge is useful only after it agrees with a persistent
             * spatial slot, rejecting unrelated centred harmonics. */
            object_target = 2.10f * best_score *
                (0.18f + 0.82f * ridge_gate);
            object_target *= 1.15f +
                0.35f * state->rear_activity_target;
            object_target *= 1.0f + 0.40f * focus_presence *
                (0.30f + 0.70f * focus_transient);
            object_target = spa_hssr_dsp_bound(object_target, 0.0f, 1.0f);
#ifdef SPA_HSSR_SCENE_COH
            {
                /* Inter-channel coherence per bin, ~45 ms (Avendano & Jot
                 * 2004 ambience index; DirAC diffuseness).  A panned object
                 * is coherent; reverb/room is not and must not be steered. */
                const float ema = 0.06f;
                float coh;

                state->bin_scene_pl[bin] += ema * (pl - state->bin_scene_pl[bin]);
                state->bin_scene_pr[bin] += ema * (pr - state->bin_scene_pr[bin]);
                state->bin_scene_cross_re[bin] += ema *
                    (left_re * right_re + left_im * right_im - state->bin_scene_cross_re[bin]);
                state->bin_scene_cross_im[bin] += ema *
                    (left_im * right_re - left_re * right_im - state->bin_scene_cross_im[bin]);
                coh = spa_hssr_dsp_bound(
                    sqrtf(state->bin_scene_cross_re[bin] * state->bin_scene_cross_re[bin] +
                          state->bin_scene_cross_im[bin] * state->bin_scene_cross_im[bin]) /
                    sqrtf(state->bin_scene_pl[bin] * state->bin_scene_pr[bin] + 1.0e-12f),
                    0.0f, 1.0f);
                state->bin_scene_coh[bin] = coh;
                object_target *= 0.50f + 0.50f *
                    spa_hssr_dsp_smoothstep(coh, 0.25f, 0.55f);
            }
#endif
#ifdef SPA_HSSR_SCENE_AZIMUTH
            {
                /* Continuous azimuth objects (research; adds nothing under the current semantic veto, see q5-scene-ab): measured pan of THIS bin decides relocation
                 * continuously (a whole panned part moves together, keeping
                 * its own L/R image).  Wide vocals are not caught by pan, so
                 * the learned semantic veto (band_mask, vocal fourth-power
                 * keep) gates it exactly like the slot path. */
                /* Per-bin smoothed inter-channel statistics (~50 ms): a band
                 * mixes bins of different pans and reads as 'grey', a bin of
                 * one panned source reads coherent. */
                const float ema = 0.22f;
                float safety = state->band_universal_safety[band];
                float coherence;
                float side;
                float scene_target;

                state->bin_scene_pl[bin] += ema * (pl - state->bin_scene_pl[bin]);
                state->bin_scene_pr[bin] += ema * (pr - state->bin_scene_pr[bin]);
                state->bin_scene_cross_re[bin] += ema *
                    (left_re * right_re + left_im * right_im - state->bin_scene_cross_re[bin]);
                state->bin_scene_cross_im[bin] += ema *
                    (left_im * right_re - left_re * right_im - state->bin_scene_cross_im[bin]);
                coherence = spa_hssr_dsp_bound(
                    sqrtf(state->bin_scene_cross_re[bin] * state->bin_scene_cross_re[bin] +
                          state->bin_scene_cross_im[bin] * state->bin_scene_cross_im[bin]) /
                    sqrtf(state->bin_scene_pl[bin] * state->bin_scene_pr[bin] + 1.0e-12f),
                    0.0f, 1.0f);
                {
                    float sl = sqrtf(state->bin_scene_pl[bin] + 1.0e-12f);
                    float sr = sqrtf(state->bin_scene_pr[bin] + 1.0e-12f);
                    state->bin_scene_pan[bin] = (sl - sr) / (sl + sr + 1.0e-9f);
                }
                side = spa_hssr_dsp_smoothstep(
                    fabsf(state->bin_scene_pan[bin]), 0.12f, 0.35f);
                scene_target = side * safety * focus_presence *
                    spa_hssr_dsp_smoothstep(coherence, 0.35f, 0.60f) *
                    (0.55f + 0.45f * ridge_gate);
                scene_target = spa_hssr_dsp_bound(scene_target, 0.0f, 1.0f);
#ifdef SPA_HSSR_SCENE_DEBUG
                {
                    static double acc[7], wsum; static unsigned long n;
                    if (bin >= 12 && bin < 200) {
                        double w = pl + pr;
                        acc[0] += w * side; acc[1] += w * safety; acc[2] += w * coherence; acc[3] += w * focus_presence;
                        acc[4] += w * scene_target; acc[5] += w * object_target; acc[6] += w * (scene_target > object_target);
                        wsum += w; n++;
                        if (n % 2000000 == 0)
                            fprintf(stderr, "scene-dbg(energy-weighted) side=%.3f safety=%.3f coh=%.3f focus=%.3f scene=%.3f slot=%.3f scene>slot=%.3f\n",
                                    acc[0]/wsum, acc[1]/wsum, acc[2]/wsum, acc[3]/wsum, acc[4]/wsum, acc[5]/wsum, acc[6]/wsum);
                    }
                }
#endif
                if (scene_target > object_target) {
                    object_target = scene_target;
                    best_score = scene_target;
                    best_pan = state->bin_scene_pan[bin];
                }
            }
#endif
            if (state->cabin_policy) {
                /* The tracker/rescue may nominate a spatial candidate, but
                 * cannot override the learned permission to transfer it. */
                if (object_target > state->band_mask[band])
                    object_target = state->band_mask[band];
                object_target *= state->band_universal_safety[band];
            }
            alpha = object_target > state->bin_mask[bin]
                ? 0.50f : 0.020f;
            state->bin_mask[bin] += alpha *
                (object_target - state->bin_mask[bin]);
#ifdef SPA_HSSR_SCENE_DEBUG_SLOTS
            state->dbg_mask_delta += fabsf(alpha * (object_target - state->bin_mask[bin]) / (alpha > 0.1f ? 1.0f : 1.0f));
            state->dbg_mask_sum += state->bin_mask[bin];
#endif

            if (best_score > 0.01f)
                state->bin_object_pan[bin] += 0.18f *
                    (best_pan - state->bin_object_pan[bin]);

            /* Keep only a quiet decorrelated safety bed when no coherent
             * object is accepted.  It preserves some envelopment without
             * competing with the explicit slot image. */
            ambience_target = 0.018f * state->band_mask[band] *
                state->band_diffuse[band] * (1.0f - best_score);
            ambience_target = spa_hssr_dsp_bound(
                ambience_target, 0.0f, 0.04f);
            alpha = ambience_target > state->bin_ambience_mask[bin]
                ? 0.10f : 0.015f;
            state->bin_ambience_mask[bin] += alpha *
                (ambience_target - state->bin_ambience_mask[bin]);

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
                float learned = spa_hssr_dsp_smoothstep(
                    state->band_mask[band], 0.015f, 0.24f);
                float semantic_safety =
                    state->band_universal_safety[band];
                float percussion_safety = 0.85f * focus_transient *
                    kHssrPercussionPresence[band];

                if (percussion_safety > semantic_safety)
                    semantic_safety = percussion_safety;
                semantic_safety = 0.04f + 0.96f * semantic_safety;
                float content_floor =
                    (0.220f + 0.440f * learned) * semantic_safety;
                float ridge_support = 0.45f + 0.55f * ridge_gate;
                float motion_support = 0.70f +
                    0.30f * focus_transient;

                universal_target = object_absence * focus_presence *
                    content_floor * ridge_support * motion_support;
                universal_target = spa_hssr_dsp_bound(
                    universal_target, 0.0f, 0.55f);
            }
#ifdef SPA_HSSR_SCENE_COH
            {
                /* Diffuse layer: what is NOT coherent is room/reverb; it goes
                 * behind through the decorrelated bed, under the semantic
                 * veto, never below 220 Hz. */
                float diffuse = 0.35f *
                    (1.0f - spa_hssr_dsp_smoothstep(state->bin_scene_coh[bin], 0.25f, 0.60f)) *
                    state->band_universal_safety[band] * (1.0f - best_score);

                if (kHssrBandEdgeHz[band + 1] <= 220) diffuse = 0.0f;
                if (diffuse > universal_target) universal_target = diffuse;
            }
#endif
            /* No synthetic minimum rear activity in the cabin candidate. */
            if (state->cabin_policy) universal_target = 0.0f;
            alpha = universal_target > state->bin_universal_mask[bin]
                ? 0.28f : 0.012f;
            state->bin_universal_mask[bin] += alpha *
                (universal_target - state->bin_universal_mask[bin]);
        }
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

        band = state->bin_band[bin];
        mask = state->bin_mask[bin];
        mask *= 1.0f + state->band_object_boost[band];
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
            float ambience = state->bin_ambience_mask[bin];
            float universal = state->bin_universal_mask[bin];
#ifdef SPA_HSSR_SCENE
            {
                float pl = left_re * left_re + left_im * left_im;
                float pr = right_re * right_re + right_im * right_im;
                state->bin_scene_lr[bin] += 0.05f *
                    ((pl - pr) / (pl + pr + 1.0e-9f) - state->bin_scene_lr[bin]);
            }
#endif

            mask = spa_hssr_dsp_bound(mask, 0.0f,
                                      state->cabin_policy ? 1.0f : 1.45f);
            pan = spa_hssr_dsp_bound(
                state->bin_object_pan[bin], -0.95f, 0.95f);
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

                if (state->cabin_policy || SPA_HSSR_SCENE_TRUEPAN_ON)
                    focused = magnitude + (0.85f - magnitude) * 0.35f *
                        spa_hssr_dsp_smoothstep(mask, 0.10f, 0.60f) *
                        spa_hssr_dsp_smoothstep(magnitude, 0.05f, 0.35f);

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
#ifdef SPA_HSSR_EXPERIMENT_CENTER_NULL
            if (state->cabin_policy) {
                /* Apply a contractive common-mode null BEFORE projection.
                 * Do not renormalize its lost energy or change controller input.
                 * Protects exact common-mode sources, NOT arbitrary lead roles. */
                extraction_l = 0.5f * (source_l - source_r);
                extraction_r = -extraction_l;
            }
#elif defined(SPA_HSSR_EXPERIMENT_SIDE_OBJECT)
            if (state->cabin_policy) {
                /* Orthonormal side extraction: object operator norm <= mask,
                 * without the extra near-centre pan attenuation above.
                 * Remove only its actual side image from the front, preserving
                 * the L+R/center feed. Still NOT a semantic lead separator. */
                source_l = source_l >= source_r ? 1.0f : -1.0f;
                source_r = -source_l;
                extraction_l = source_l;
                extraction_r = source_r;
            }
#endif
            projected_re = 0.5f *
                (extraction_l * left_re + extraction_r * right_re);
            projected_im = 0.5f *
                (extraction_l * left_im + extraction_r * right_im);
#ifdef SPA_HSSR_CAPTURE_TRANSFERS
            if (state->cabin_policy) {
                const float matrix[8] = {
                    0.5f * (mask * steering_l * extraction_l + ambience),
                    0.5f * (mask * steering_l * extraction_r - ambience),
                    0.5f * (mask * steering_r * extraction_l - ambience),
                    0.5f * (mask * steering_r * extraction_r + ambience),
                    0.5f * mask * source_l * extraction_l,
                    0.5f * mask * source_l * extraction_r,
                    0.5f * mask * source_r * extraction_l,
                    0.5f * mask * source_r * extraction_r
                };
                spa_hssr_capture_transfer(bin, matrix);
            }
#endif
            if (state->cabin_policy || SPA_HSSR_SCENE_ON) {
                /* Original source image, BEFORE steering or ambience.  The
                 * subtraction never exceeds the source itself (mask <= 1). */
                float source_mask = mask > 1.0f ? 1.0f : mask;
                state->source_fft_re[0][bin] = source_mask * source_l * projected_re;
                state->source_fft_im[0][bin] = source_mask * source_l * projected_im;
                state->source_fft_re[1][bin] = source_mask * source_r * projected_re;
                state->source_fft_im[1][bin] = source_mask * source_r * projected_im;
            }
            state->fft_re[SPA_HSSR_LEFT][bin] =
                mask * steering_l * projected_re + ambience * side_re;
            state->fft_im[SPA_HSSR_LEFT][bin] =
                mask * steering_l * projected_im + ambience * side_im;
            state->fft_re[SPA_HSSR_RIGHT][bin] =
                mask * steering_r * projected_re - ambience * side_re;
            state->fft_im[SPA_HSSR_RIGHT][bin] =
                mask * steering_r * projected_im - ambience * side_im;
            if (universal > 0.0f) {
                /* A bounded conjugate phase pair prevents the fallback from
                 * degenerating into identical SL/SR or x/-x.  Alternating
                 * broad-band steering makes percussion/keys/pads perceptibly
                 * distinct without time-varying pan wandering. */
                unsigned rotation = 18u + (bin * 13u) % 47u;
                float wr = state->twiddle_re[rotation];
                float wi = -state->twiddle_im[rotation];
#ifdef SPA_HSSR_SCENE
                float fallback_pan = 0.72f * spa_hssr_dsp_bound(
                    3.0f * state->bin_scene_lr[bin], -1.0f, 1.0f);
#else
                float fallback_pan = ((band / 3u) & 1u) ? -0.72f : 0.72f;
#endif
                float fallback_l = sqrtf(1.0f + fallback_pan);
                float fallback_r = sqrtf(1.0f - fallback_pan);
                float rotated_l_re = wr * mid_re - wi * mid_im;
                float rotated_l_im = wr * mid_im + wi * mid_re;
                float rotated_r_re = wr * mid_re + wi * mid_im;
                float rotated_r_im = wr * mid_im - wi * mid_re;

                universal *= state->universal_makeup;
                state->fft_re[SPA_HSSR_LEFT][bin] +=
                    universal * fallback_l * rotated_l_re;
                state->fft_im[SPA_HSSR_LEFT][bin] +=
                    universal * fallback_l * rotated_l_im;
                state->fft_re[SPA_HSSR_RIGHT][bin] +=
                    universal * fallback_r * rotated_r_re;
                state->fft_im[SPA_HSSR_RIGHT][bin] +=
                    universal * fallback_r * rotated_r_im;
            }
#if defined(SPA_HSSR_CAPTURE_TRANSFERS) && defined(SPA_HSSR_SCENE)
            if (!state->cabin_policy) {
                /* Complex 2x2 rear transfer of this bin, universal bed
                 * included (universal already carries the makeup):
                 * rear = M * [L, R], M = re/im of LL, LR, RL, RR. */
                float ubl = 0.0f, ubr = 0.0f, uwr = 0.0f, uwi = 0.0f;
                float matrix[8];
                if (universal > 0.0f) {
                    unsigned rotation = 18u + (bin * 13u) % 47u;
                    float fallback_pan;
                    uwr = state->twiddle_re[rotation];
                    uwi = -state->twiddle_im[rotation];
                    fallback_pan = 0.72f * spa_hssr_dsp_bound(
                        3.0f * state->bin_scene_lr[bin], -1.0f, 1.0f);
                    ubl = 0.5f * universal * sqrtf(1.0f + fallback_pan);
                    ubr = 0.5f * universal * sqrtf(1.0f - fallback_pan);
                }
                matrix[0] = 0.5f * (mask * steering_l * extraction_l + ambience) + ubl * uwr;
                matrix[1] = ubl * uwi;
                matrix[2] = 0.5f * (mask * steering_l * extraction_r - ambience) + ubl * uwr;
                matrix[3] = ubl * uwi;
                matrix[4] = 0.5f * (mask * steering_r * extraction_l - ambience) + ubr * uwr;
                matrix[5] = -ubr * uwi;
                matrix[6] = 0.5f * (mask * steering_r * extraction_r + ambience) + ubr * uwr;
                matrix[7] = -ubr * uwi;
                spa_hssr_capture_transfer(bin, matrix);
            }
#endif
        }
    }

    for (channel = 0; channel < 2; ++channel) {
        state->fft_im[channel][0] = 0.0f;
        state->fft_im[channel][SPA_HSSR_FFT_SIZE / 2] = 0.0f;
        for (bin = 1; bin < SPA_HSSR_FFT_SIZE / 2; ++bin) {
            unsigned mirror = SPA_HSSR_FFT_SIZE - bin;
            state->fft_re[channel][mirror] = state->fft_re[channel][bin];
            state->fft_im[channel][mirror] = -state->fft_im[channel][bin];
        }
        spa_hssr_fft(state, channel, 1);
    }

    for (sample = 0; sample < SPA_HSSR_FFT_SIZE; ++sample) {
        unsigned output = (state->input_position + sample) &
                          (SPA_HSSR_FFT_SIZE - 1);

        for (channel = 0; channel < 2; ++channel) {
            float addition = state->fft_re[channel][sample] *
                             state->synthesis_window[sample];
            state->ola[channel][output] = spa_hssr_dsp_finite(
                state->ola[channel][output] + addition);
        }
    }
    if (state->cabin_policy || SPA_HSSR_SCENE_ON) {
        for (channel = 0; channel < 2; ++channel) {
            memcpy(state->fft_re[channel], state->source_fft_re[channel],
                   SPA_HSSR_BINS * sizeof(float));
            memcpy(state->fft_im[channel], state->source_fft_im[channel],
                   SPA_HSSR_BINS * sizeof(float));
            state->fft_im[channel][0] = 0.0f;
            state->fft_im[channel][SPA_HSSR_FFT_SIZE / 2] = 0.0f;
            for (bin = 1; bin < SPA_HSSR_FFT_SIZE / 2; ++bin) {
                state->fft_re[channel][SPA_HSSR_FFT_SIZE-bin] = state->fft_re[channel][bin];
                state->fft_im[channel][SPA_HSSR_FFT_SIZE-bin] = -state->fft_im[channel][bin];
            }
            spa_hssr_fft(state, channel, 1);
            for (sample = 0; sample < SPA_HSSR_FFT_SIZE; ++sample) {
                unsigned output = (state->input_position + sample) & (SPA_HSSR_FFT_SIZE-1);
                state->source_ola[channel][output] = spa_hssr_dsp_finite(
                    state->source_ola[channel][output] +
                    state->fft_re[channel][sample] * state->synthesis_window[sample]);
            }
        }
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
    if (ready && !state->cabin_policy) {
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
#ifdef SPA_HSSR_SCENE
        /* No forced 32 % rear activity: the bed may only double, never x3.5. */
        desired = spa_hssr_dsp_bound(desired, 1.0f, 1.50f);
#else
        desired = spa_hssr_dsp_bound(desired, 1.0f, 3.50f);
#endif
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
