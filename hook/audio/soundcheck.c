#include "soundcheck.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define SPA_SOUNDCHECK_DEFAULT_RAMP_MS 20u
#define SPA_SOUNDCHECK_FALLBACK_WARMUP_SECONDS 2u
#define SPA_SOUNDCHECK_FALLBACK_WINDOW_SECONDS 3u
#define SPA_SOUNDCHECK_LIMITER_RELEASE_MS 100u

static int spa_soundcheck_finite(float value)
{
    return value == value && value <= FLT_MAX && value >= -FLT_MAX;
}

static uint32_t spa_soundcheck_float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float spa_soundcheck_db_gain(float db)
{
    return powf(10.0f, db * 0.05f);
}

void spa_soundcheck_default_config(spa_soundcheck_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->enabled = 1;
    config->sampleRate = 48000;
    config->rampFrames = config->sampleRate *
                         SPA_SOUNDCHECK_DEFAULT_RAMP_MS / 1000u;
    config->fallbackTargetDbfs = -18.0f;
    config->limiterCeilingDbfs = -1.0f;
}

void spa_soundcheck_init(spa_soundcheck_state_t *state,
                         const spa_soundcheck_config_t *config,
                         int haveSetupTarget, uint8_t setupMediaKind,
                         int setupTargetDb)
{
    spa_soundcheck_config_t defaults;

    if (!state) return;
    spa_soundcheck_default_config(&defaults);
    memset(state, 0, sizeof(*state));
    state->config = config ? *config : defaults;
    if (!state->config.sampleRate) state->config.sampleRate = defaults.sampleRate;
    if (!state->config.rampFrames)
        state->config.rampFrames = state->config.sampleRate *
                                   SPA_SOUNDCHECK_DEFAULT_RAMP_MS / 1000u;
    state->haveSetupTarget = haveSetupTarget &&
                             setupTargetDb >= -128 && setupTargetDb <= 127;
    state->setupMediaKind = setupMediaKind;
    state->setupTargetDb = (int8_t)setupTargetDb;
    state->currentGain = 1.0f;
    state->targetGain = 1.0f;
    state->limiterGain = 1.0f;
}

int spa_soundcheck_main_gain(int targetDb, const float parameters[3],
                             int *roundedDb, float *linearGain)
{
    float desired;
    int rounded = 0;

    if (linearGain) *linearGain = 1.0f;
    if (roundedDb) *roundedDb = 0;
    if (!parameters || !spa_soundcheck_finite(parameters[0])) return 0;

    desired = (float)targetDb - parameters[0];
    if (!spa_soundcheck_finite(desired)) return 0;
    if (desired > 0.0f) {
        float headroom;
        if (!spa_soundcheck_finite(parameters[2])) return 0;
        headroom = -1.0f - parameters[2];
        if (headroom < 0.0f) desired = 0.0f;
        else if (desired > headroom) desired = headroom;
    }

    /* This is deliberately not C99 roundf(): Pioneer uses floorf(x + .5),
     * which resolves negative half-dB values toward zero. */
    rounded = (int)floorf(desired + 0.5f);
    if (rounded > 20) rounded = 20;
    if (rounded < -128) rounded = -128;
    if (roundedDb) *roundedDb = rounded;
    if (linearGain) *linearGain = spa_soundcheck_db_gain((float)rounded);
    return 1;
}

static void spa_soundcheck_set_gain(spa_soundcheck_state_t *state, float gain)
{
    if (!state || !spa_soundcheck_finite(gain) || gain < 0.0f) return;
    state->targetGain = gain;
    if (!state->haveAppliedGain || !state->config.rampFrames) {
        state->currentGain = gain;
        state->gainStep = 0.0f;
        state->rampRemaining = 0;
        state->haveAppliedGain = 1;
        return;
    }
    state->rampRemaining = state->config.rampFrames;
    state->gainStep = (gain - state->currentGain) /
                      (float)state->rampRemaining;
}

int spa_soundcheck_update_main(spa_soundcheck_state_t *state,
                               uint8_t mediaKind,
                               const float parameters[3])
{
    uint32_t bits[3];
    float gain = 1.0f;
    int roundedDb = 0;

    if (!state || !parameters || !state->config.enabled ||
        !state->haveSetupTarget || mediaKind != state->setupMediaKind)
        return 0;
    bits[0] = spa_soundcheck_float_bits(parameters[0]);
    bits[1] = spa_soundcheck_float_bits(parameters[1]);
    bits[2] = spa_soundcheck_float_bits(parameters[2]);
    if (state->haveMetadata && state->metadataMediaKind == mediaKind &&
        state->metadataBits[0] == bits[0] &&
        state->metadataBits[1] == bits[1] &&
        state->metadataBits[2] == bits[2])
        return 0;

    if (!spa_soundcheck_main_gain(state->setupTargetDb, parameters,
                                  &roundedDb, &gain)) {
        state->metadataRejected++;
        return 0;
    }
    (void)roundedDb;
    state->metadataMediaKind = mediaKind;
    memcpy(state->metadataBits, bits, sizeof(bits));
    state->haveMetadata = 1;
    state->metadataUpdates++;
    state->fallbackPower = 0.0f;
    state->fallbackFrames = 0;
    spa_soundcheck_set_gain(state, gain);
    return 1;
}

void spa_soundcheck_note_legacy(spa_soundcheck_state_t *state,
                                size_t recordCount)
{
    if (state) state->legacyRecords += recordCount;
}

void spa_soundcheck_discontinuity(spa_soundcheck_state_t *state)
{
    if (!state) return;
    state->haveMetadata = 0;
    memset(state->metadataBits, 0, sizeof(state->metadataBits));
    state->fallbackPower = 0.0f;
    state->fallbackFrames = 0;
}

static void spa_soundcheck_update_fallback(spa_soundcheck_state_t *state,
                                           const int16_t *samples,
                                           size_t frames, unsigned channels)
{
    double sum = 0.0;
    size_t count, i;
    float blockPower, weight, db, desiredDb, gain;

    if (!state || !samples || !frames || !channels || state->haveMetadata ||
        state->config.fallbackMode != SPA_SOUNDCHECK_FALLBACK_RMS)
        return;
    count = frames * (size_t)channels;
    for (i = 0; i < count; i++) {
        double value = (double)samples[i] / 32768.0;
        sum += value * value;
    }
    blockPower = (float)(sum / (double)count);
    weight = (float)frames /
             ((float)state->config.sampleRate *
              SPA_SOUNDCHECK_FALLBACK_WINDOW_SECONDS);
    if (weight > 1.0f) weight = 1.0f;
    if (!state->fallbackFrames) state->fallbackPower = blockPower;
    else state->fallbackPower += (blockPower - state->fallbackPower) * weight;
    state->fallbackFrames += frames;
    if (state->fallbackFrames <
        (uint64_t)state->config.sampleRate *
        SPA_SOUNDCHECK_FALLBACK_WARMUP_SECONDS ||
        state->fallbackPower <= 1.0e-12f)
        return;

    db = 10.0f * log10f(state->fallbackPower);
    desiredDb = state->config.fallbackTargetDbfs - db;
    if (desiredDb > 6.0f) desiredDb = 6.0f;
    if (desiredDb < -12.0f) desiredDb = -12.0f;
    gain = spa_soundcheck_db_gain(desiredDb);
    if (!state->haveAppliedGain ||
        gain > state->targetGain * 1.01f || gain < state->targetGain * 0.99f) {
        spa_soundcheck_set_gain(state, gain);
        state->fallbackUpdates++;
    }
}

static int16_t spa_soundcheck_s16(float value)
{
    if (value >= 32767.0f) return 32767;
    if (value <= -32768.0f) return -32768;
    return (int16_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

void spa_soundcheck_observe_s16(spa_soundcheck_state_t *state,
                                const int16_t *samples, size_t frames,
                                unsigned channels)
{
    if (!state || !samples || !frames || !channels || !state->config.enabled)
        return;
    spa_soundcheck_update_fallback(state, samples, frames, channels);
}

void spa_soundcheck_apply_s16(spa_soundcheck_state_t *state,
                              int16_t *samples, size_t frames,
                              unsigned channels)
{
    size_t frame, channel;

    if (!state || !samples || !frames || !channels || !state->config.enabled)
        return;
    if (!state->haveAppliedGain) return;

    for (frame = 0; frame < frames; frame++) {
        float gain;

        if (state->rampRemaining) {
            state->currentGain += state->gainStep;
            state->rampRemaining--;
            if (!state->rampRemaining) state->currentGain = state->targetGain;
        }
        gain = state->currentGain;
        if (gain == 1.0f) continue;
        for (channel = 0; channel < channels; channel++) {
            size_t at = frame * channels + channel;
            samples[at] = spa_soundcheck_s16((float)samples[at] * gain);
        }
    }
}

void spa_soundcheck_process_s16(spa_soundcheck_state_t *state,
                                int16_t *samples, size_t frames,
                                unsigned channels)
{
    spa_soundcheck_observe_s16(state, samples, frames, channels);
    spa_soundcheck_apply_s16(state, samples, frames, channels);
}

void spa_soundcheck_limit_s16(spa_soundcheck_state_t *state,
                              int16_t *samples, size_t frames,
                              unsigned channels)
{
    float ceiling, releaseStep;
    size_t frame, channel;

    if (!state || !samples || !frames || !channels || !state->config.enabled ||
        state->config.limiterMode != SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK)
        return;
    ceiling = 32767.0f * spa_soundcheck_db_gain(
        state->config.limiterCeilingDbfs);
    releaseStep = 1.0f /
        ((float)state->config.sampleRate *
         SPA_SOUNDCHECK_LIMITER_RELEASE_MS / 1000.0f);

    for (frame = 0; frame < frames; frame++) {
        float limiter = state->limiterGain;
        float peak = 0.0f;

        for (channel = 0; channel < channels; channel++) {
            float value = (float)samples[frame * channels + channel];
            float magnitude = value < 0.0f ? -value : value;
            if (magnitude > peak) peak = magnitude;
        }
        if (peak > 0.0f && limiter > ceiling / peak) {
            limiter = ceiling / peak;
            state->limitedFrames++;
        } else if (limiter < 1.0f) {
            limiter += releaseStep;
            if (limiter > 1.0f) limiter = 1.0f;
            if (peak > 0.0f && limiter > ceiling / peak)
                limiter = ceiling / peak;
        }
        state->limiterGain = limiter;
        if (limiter == 1.0f) continue;
        for (channel = 0; channel < channels; channel++) {
            size_t at = frame * channels + channel;
            samples[at] = spa_soundcheck_s16((float)samples[at] * limiter);
        }
    }
}
