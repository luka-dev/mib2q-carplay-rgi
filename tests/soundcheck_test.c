#include "audio/soundcheck.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int nearf(float a, float b, float tolerance)
{
    float d = a - b;
    return (d < 0.0f ? -d : d) <= tolerance;
}

static void test_pioneer_main_gain(void)
{
    static const float aac[3] = { -6.5f, -1.0f, 0.96875f };
    static const float apacA[3] = { -18.0f, -1.8125f, -1.78125f };
    static const float apacB[3] = { -19.25f, -2.25f, -2.25f };
    float gain = 0.0f;
    int db = 0;

    assert(spa_soundcheck_main_gain(-16, aac, &db, &gain));
    assert(db == -9 && nearf(gain, 0.3548134f, 0.000001f));
    assert(spa_soundcheck_main_gain(-16, apacA, &db, &gain));
    assert(db == 1 && nearf(gain, 1.1220185f, 0.000001f));
    assert(spa_soundcheck_main_gain(-16, apacB, &db, &gain));
    assert(db == 1 && nearf(gain, 1.1220185f, 0.000001f));
    {
        float invalidPeak[3] = { -20.0f, -1.0f, NAN };
        assert(!spa_soundcheck_main_gain(-16, invalidPeak, &db, &gain));
        assert(db == 0 && gain == 1.0f);
    }
}

static void test_metadata_and_ramp(void)
{
    spa_soundcheck_config_t config;
    spa_soundcheck_state_t state;
    float unity[3] = { -16.0f, -1.0f, -2.0f };
    float quiet[3] = { -6.0f, -1.0f, -2.0f };
    int16_t samples[960 * 2];
    size_t i;

    spa_soundcheck_default_config(&config);
    spa_soundcheck_init(&state, &config, 1, 1, -16);
    assert(spa_soundcheck_update_main(&state, 1, unity));
    assert(state.currentGain == 1.0f && state.metadataUpdates == 1);
    assert(!spa_soundcheck_update_main(&state, 1, unity));
    assert(state.metadataUpdates == 1);

    assert(spa_soundcheck_update_main(&state, 1, quiet));
    assert(state.rampRemaining == config.rampFrames);
    for (i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) samples[i] = 10000;
    spa_soundcheck_process_s16(&state, samples, 960, 2);
    assert(state.rampRemaining == 0);
    assert(nearf(state.currentGain, state.targetGain, 0.000001f));
    assert(samples[0] < 10000 && samples[1918] < samples[0]);

    spa_soundcheck_discontinuity(&state);
    assert(!state.haveMetadata);
    assert(nearf(state.currentGain, state.targetGain, 0.000001f));
}

static void test_bypass_fallback_and_limiter(void)
{
    spa_soundcheck_config_t config;
    spa_soundcheck_state_t state;
    int16_t block[1024 * 2];
    int16_t original[1024 * 2];
    unsigned pass;
    size_t i;

    for (i = 0; i < sizeof(block) / sizeof(block[0]); i++)
        block[i] = (i & 1) ? -12000 : 12000;
    memcpy(original, block, sizeof(block));
    spa_soundcheck_default_config(&config);
    spa_soundcheck_init(&state, &config, 0, 0, 0);
    spa_soundcheck_process_s16(&state, block, 1024, 2);
    assert(memcmp(block, original, sizeof(block)) == 0);

    config.fallbackMode = SPA_SOUNDCHECK_FALLBACK_RMS;
    spa_soundcheck_init(&state, &config, 0, 0, 0);
    for (pass = 0; pass < 100; pass++)
        spa_soundcheck_process_s16(&state, block, 1024, 2);
    assert(state.fallbackUpdates > 0 && state.haveAppliedGain);

    spa_soundcheck_default_config(&config);
    config.limiterMode = SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK;
    spa_soundcheck_init(&state, &config, 0, 0, 0);
    for (i = 0; i < sizeof(block) / sizeof(block[0]); i++) block[i] = 32767;
    spa_soundcheck_limit_s16(&state, block, 1024, 2);
    for (i = 0; i < sizeof(block) / sizeof(block[0]); i++)
        assert(block[i] <= 29205);
    assert(state.limitedFrames > 0);

    config.enabled = 0;
    spa_soundcheck_init(&state, &config, 1, 1, -16);
    memcpy(block, original, sizeof(block));
    spa_soundcheck_process_s16(&state, block, 1024, 2);
    assert(memcmp(block, original, sizeof(block)) == 0);
}

static void test_observe_and_apply_are_separate(void)
{
    static const float aac[3] = { -6.5f, -1.0f, 0.96875f };
    spa_soundcheck_config_t config;
    spa_soundcheck_state_t state;
    int16_t source[4] = { 10000, -10000, 5000, -5000 };
    int16_t original[4];
    int16_t rendered[6] = { 10000, -10000, 5000, -5000, 2500, -2500 };
    size_t i;

    memcpy(original, source, sizeof(source));
    spa_soundcheck_default_config(&config);
    spa_soundcheck_init(&state, &config, 1, 1, -16);
    assert(spa_soundcheck_update_main(&state, 1, aac));

    spa_soundcheck_observe_s16(&state, source, 2, 2);
    assert(memcmp(source, original, sizeof(source)) == 0);

    spa_soundcheck_apply_s16(&state, rendered, 1, 6);
    for (i = 0; i < 6; i++)
        assert(rendered[i] == (int16_t)((float)(i < 4 ? original[i] :
            (i == 4 ? 2500 : -2500)) * state.targetGain +
            (rendered[i] >= 0 ? 0.5f : -0.5f)));

    {
        float gains[2] = { 0.0f, 0.0f };
        int16_t scaled[4];
        memcpy(scaled, original, sizeof(original));
        spa_soundcheck_take_gains(&state, gains, 2);
        assert(nearf(gains[0], state.targetGain, 0.000001f));
        assert(nearf(gains[1], state.targetGain, 0.000001f));
        spa_soundcheck_scale_s16(scaled, 2, 2, gains);
        for (i = 0; i < 4; i++)
            assert(scaled[i] == (int16_t)((float)original[i] * gains[i / 2] +
                (original[i] >= 0 ? 0.5f : -0.5f)));
    }
}

static void test_f32_headroom_and_limiter(void)
{
    static const float aac[3] = { -6.5f, -1.0f, 0.96875f };
    spa_soundcheck_config_t config;
    spa_soundcheck_state_t state;
    float over[6] = { 1.25f, -1.25f, 1.40f, 0.50f, -1.10f, 1.10f };
    float scaled[4] = { 1.20f, -0.80f, 0.50f, -0.25f };
    const float gains[2] = { 0.5f, 1.0f };
    size_t i;

    /* Float32 path preserves > 0 dBFS APAC/downmix peaks into SoundCheck
     * attenuation without clipping at 1.0f first. */
    spa_soundcheck_default_config(&config);
    spa_soundcheck_init(&state, &config, 1, 1, -16);
    assert(spa_soundcheck_update_main(&state, 1, aac));
    spa_soundcheck_observe_f32(&state, over, 1, 6);
    spa_soundcheck_apply_f32(&state, over, 1, 6);
    assert(nearf(over[0], 1.25f * state.targetGain, 1e-6f));
    assert(nearf(over[2], 1.40f * state.targetGain, 1e-6f));
    assert(over[2] < 1.0f); /* +2.9 dBFS peak lands cleanly below 0 dBFS */

    spa_soundcheck_scale_f32(scaled, 2, 2, gains);
    assert(nearf(scaled[0], 0.60f, 1e-6f) && nearf(scaled[1], -0.40f, 1e-6f));
    assert(scaled[2] == 0.50f && scaled[3] == -0.25f);

    config.limiterMode = SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK;
    spa_soundcheck_init(&state, &config, 0, 0, 0);
    for (i = 0; i < 6; i++) over[i] = 1.25f;
    spa_soundcheck_limit_f32(&state, over, 1, 6);
    for (i = 0; i < 6; i++) assert(over[i] <= 0.892f);
    assert(state.limitedFrames == 1);
}

int main(void)
{
    test_pioneer_main_gain();
    test_metadata_and_ramp();
    test_bypass_fallback_and_limiter();
    test_observe_and_apply_are_separate();
    test_f32_headroom_and_limiter();
    puts("OK  Pioneer main loudness, smooth metadata changes, exact bypass, opt-in RMS/peak guards, F32 headroom");
    return 0;
}
