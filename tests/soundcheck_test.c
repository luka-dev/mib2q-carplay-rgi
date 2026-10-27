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
}

/* take_gains + scale_s16 (the upmix's path) is apply_s16 bit for bit, ramp
 * included; unity and disabled hand out nothing. */
static void test_taken_gains_match_apply(void)
{
    static const float unity[3] = { -16.0f, -1.0f, -2.0f };
    static const float quiet[3] = { -6.0f, -1.0f, -2.0f };
    spa_soundcheck_config_t config;
    spa_soundcheck_state_t a, b;
    int16_t x[1500 * 2], y[1500 * 2];
    float gains[1500];
    size_t i;

    for (i = 0; i < 1500 * 2; i++) x[i] = y[i] = (int16_t)((int)(i * 7919u % 60001u) - 30000);
    spa_soundcheck_default_config(&config);
    spa_soundcheck_init(&a, &config, 1, 1, -16);
    spa_soundcheck_init(&b, &config, 1, 1, -16);
    assert(spa_soundcheck_take_gains(&a, gains, 1500) == NULL);       /* no gain yet */
    assert(spa_soundcheck_update_main(&a, 1, unity) && spa_soundcheck_update_main(&b, 1, unity));
    assert(spa_soundcheck_take_gains(&a, gains, 1500) == NULL);       /* unity */
    assert(spa_soundcheck_update_main(&a, 1, quiet) && spa_soundcheck_update_main(&b, 1, quiet));
    spa_soundcheck_apply_s16(&b, y, 1500, 2);                         /* 960-frame ramp, then flat */
    assert(spa_soundcheck_take_gains(&a, gains, 1500) == gains);
    spa_soundcheck_scale_s16(x, 1500, 2, gains);
    assert(memcmp(x, y, sizeof(x)) == 0 && a.currentGain == b.currentGain);
    config.enabled = 0;
    spa_soundcheck_init(&a, &config, 1, 1, -16);
    assert(spa_soundcheck_take_gains(&a, gains, 1500) == NULL);
}

int main(void)
{
    test_pioneer_main_gain();
    test_metadata_and_ramp();
    test_bypass_fallback_and_limiter();
    test_observe_and_apply_are_separate();
    test_taken_gains_match_apply();
    puts("OK  Pioneer main loudness, smooth metadata changes, exact bypass, opt-in RMS/peak guards");
    return 0;
}
