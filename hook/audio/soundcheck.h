/*
 * soundcheck.h — the bounded SoundCheck gain stage used by buffered CarPlay.
 *
 * MainLoudnessParameters follows the Pioneer receiver's recovered calculation.
 * The optional RMS fallback and sample-peak safety limiter are deliberately
 * separate policy switches and default off; neither is presented as Apple's
 * metadata nor as a true-peak limiter.
 */
#ifndef SPA_SOUNDCHECK_H
#define SPA_SOUNDCHECK_H

#include <stddef.h>
#include <stdint.h>

enum {
    SPA_SOUNDCHECK_FALLBACK_OFF = 0,
    SPA_SOUNDCHECK_FALLBACK_RMS = 1
};

enum {
    SPA_SOUNDCHECK_LIMITER_OFF = 0,
    SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK = 1
};

typedef struct {
    int enabled;
    int fallbackMode;
    int limiterMode;
    uint32_t sampleRate;
    uint32_t rampFrames;
    float fallbackTargetDbfs;
    float limiterCeilingDbfs;
} spa_soundcheck_config_t;

typedef struct {
    spa_soundcheck_config_t config;
    int haveSetupTarget;
    uint8_t setupMediaKind;
    int8_t setupTargetDb;
    int haveMetadata;
    int haveAppliedGain;
    uint8_t metadataMediaKind;
    uint32_t metadataBits[3];
    float currentGain;
    float targetGain;
    float gainStep;
    uint32_t rampRemaining;
    float fallbackPower;
    uint64_t fallbackFrames;
    float limiterGain;
    uint64_t metadataUpdates;
    uint64_t metadataRejected;
    uint64_t legacyRecords;
    uint64_t fallbackUpdates;
    uint64_t limitedFrames;
} spa_soundcheck_state_t;

void spa_soundcheck_default_config(spa_soundcheck_config_t *config);
void spa_soundcheck_init(spa_soundcheck_state_t *state,
                         const spa_soundcheck_config_t *config,
                         int haveSetupTarget, uint8_t setupMediaKind,
                         int setupTargetDb);

/* Exact Pioneer MainLoudnessParameters gain. Returns 1 on a usable tuple and
 * writes both the rounded dB value and its linear gain; otherwise returns 0
 * and writes unity. */
int spa_soundcheck_main_gain(int targetDb, const float parameters[3],
                             int *roundedDb, float *linearGain);

/* Returns 1 when a new usable tuple changed the gain, 0 for a duplicate or an
 * inapplicable/invalid tuple. */
int spa_soundcheck_update_main(spa_soundcheck_state_t *state,
                               uint8_t mediaKind,
                               const float parameters[3]);
void spa_soundcheck_note_legacy(spa_soundcheck_state_t *state,
                                size_t recordCount);

/* Clear per-item metadata/fallback history without jumping the current gain.
 * The next valid item tuple ramps from the currently audible level. */
void spa_soundcheck_discontinuity(spa_soundcheck_state_t *state);

/* Feed the optional metadata-free RMS fallback without changing the samples.
 * This belongs on decoded source PCM, before any nonlinear channel synthesis. */
void spa_soundcheck_observe_s16(spa_soundcheck_state_t *state,
                                const int16_t *samples, size_t frames,
                                unsigned channels);

/* Apply the current/ramped gain to interleaved S16 without running fallback
 * analysis. Unity/disabled is bit-exact. */
void spa_soundcheck_apply_s16(spa_soundcheck_state_t *state,
                              int16_t *samples, size_t frames,
                              unsigned channels);

/* Compatibility helper: observe, then apply to the same PCM. */
void spa_soundcheck_process_s16(spa_soundcheck_state_t *state,
                                int16_t *samples, size_t frames,
                                unsigned channels);

/* Optional final sample-peak guard, called after channel synthesis. */
void spa_soundcheck_limit_s16(spa_soundcheck_state_t *state,
                              int16_t *samples, size_t frames,
                              unsigned channels);

#endif
