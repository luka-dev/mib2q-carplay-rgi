/*
 * Host-side deterministic tests for the realtime stereo ambience upmixer.
 *
 * Example:
 *   cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Ihook \
 *      tests/stereo_upmix_test.c hook/audio/stereo_upmix.c \
 *      -o /tmp/stereo_upmix_test && /tmp/stereo_upmix_test
 */

#include "audio/stereo_upmix.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_BLOCK_FRAMES = SPA_STEREO_UPMIX_MAX_FRAMES,
    TEST_LONG_FRAMES = 32768
};

static int16_t g_input[TEST_LONG_FRAMES * SPA_STEREO_UPMIX_INPUT_CHANNELS];
static int16_t g_output_a[TEST_LONG_FRAMES * SPA_STEREO_UPMIX_OUTPUT_CHANNELS];
static int16_t g_output_b[TEST_LONG_FRAMES * SPA_STEREO_UPMIX_OUTPUT_CHANNELS];

/* Individual DSP states below are static as well.  The HQ state deliberately
 * owns its FFT/WOLA storage, so placing several instances on main's stack
 * would test the host stack limit rather than the upmixer. */

static double test_absd(double value)
{
    return value < 0.0 ? -value : value;
}

static double test_sqrt(double value)
{
    double estimate = value > 1.0 ? value : 1.0;
    int iteration;

    if (value <= 0.0) return 0.0;
    for (iteration = 0; iteration < 24; iteration++)
        estimate = 0.5 * (estimate + value / estimate);
    return estimate;
}

static uint32_t test_prng(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int16_t test_noise_sample(uint32_t *state)
{
    int32_t value = (int32_t)(test_prng(state) >> 16) - 32768;
    return (int16_t)(value / 2);
}

static int16_t test_q15_scale(int16_t sample, uint32_t gain_q15)
{
    int64_t product = (int64_t)sample * (int64_t)gain_q15;

    if (product >= 0)
        return (int16_t)((product + INT64_C(16384)) / INT64_C(32768));
    product = -product;
    return (int16_t)(-((product + INT64_C(16384)) / INT64_C(32768)));
}

static void test_init(struct spa_stereo_upmix *state,
                      enum spa_stereo_upmix_backend backend,
                      uint32_t front_gain_q15,
                      uint32_t rear_gain_q15)
{
    struct spa_stereo_upmix_config config;

    spa_stereo_upmix_default_config(&config);
    spa_stereo_upmix_config_set_lfe(&config, 0);
    config.backend = backend;
    config.front_gain_q15 = front_gain_q15;
    config.rear_gain_q15 = rear_gain_q15;
    assert(spa_stereo_upmix_init(state, &config) == SPA_STEREO_UPMIX_OK);
}

static void test_init_lfe(struct spa_stereo_upmix *state,
                          enum spa_stereo_upmix_backend backend,
                          uint32_t rear_gain_q15)
{
    struct spa_stereo_upmix_config config;

    spa_stereo_upmix_default_config(&config);
    spa_stereo_upmix_config_set_lfe(&config, 1);
    config.backend = backend;
    config.rear_gain_q15 = rear_gain_q15;
    assert(spa_stereo_upmix_init(state, &config) == SPA_STEREO_UPMIX_OK);
}

static void test_process_blocks(struct spa_stereo_upmix *state,
                                const int16_t *input, size_t frames,
                                int16_t *output, size_t first_block)
{
    static const size_t kPattern[] = { 1, 7, 31, 257, 13, 509, 3, 211 };
    size_t offset = 0;
    size_t pattern_index = 0;

    while (offset < frames) {
        size_t count = pattern_index == 0 && first_block != 0
                     ? first_block
                     : kPattern[pattern_index %
                                (sizeof(kPattern) / sizeof(kPattern[0]))];
        if (count > SPA_STEREO_UPMIX_MAX_FRAMES)
            count = SPA_STEREO_UPMIX_MAX_FRAMES;
        if (count > frames - offset) count = frames - offset;
        assert(spa_stereo_upmix_process_s16(
                   state,
                   input + offset * SPA_STEREO_UPMIX_INPUT_CHANNELS,
                   count,
                   output + offset * SPA_STEREO_UPMIX_OUTPUT_CHANNELS,
                   count) == SPA_STEREO_UPMIX_OK);
        offset += count;
        pattern_index++;
    }
}

static void test_process_fixed_blocks(struct spa_stereo_upmix *state,
                                      const int16_t *input, size_t frames,
                                      int16_t *output, size_t block_frames)
{
    size_t offset = 0;

    assert(block_frames > 0);
    assert(block_frames <= SPA_STEREO_UPMIX_MAX_FRAMES);
    while (offset < frames) {
        size_t count = frames - offset;

        if (count > block_frames) count = block_frames;
        assert(spa_stereo_upmix_process_s16(
                   state,
                   input + offset * SPA_STEREO_UPMIX_INPUT_CHANNELS,
                   count,
                   output + offset * SPA_STEREO_UPMIX_OUTPUT_CHANNELS,
                   count) == SPA_STEREO_UPMIX_OK);
        offset += count;
    }
}

static void test_assert_zero_center(const int16_t *output, size_t frames)
{
    size_t frame;

    for (frame = 0; frame < frames; frame++)
        assert(output[frame * 6 + 2] == 0);
}

static void test_default_and_validation(void)
{
    static struct spa_stereo_upmix state;
    static struct spa_stereo_upmix before;
    struct spa_stereo_upmix_config config;
    int16_t input[2] = { 123, -456 };
    int16_t output[12];
    int16_t untouched[12];

    spa_stereo_upmix_default_config(&config);
    assert(config.sample_rate_hz == SPA_STEREO_UPMIX_SAMPLE_RATE);
    assert(config.front_gain_q15 == SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT);
    assert(config.rear_gain_q15 == SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(config.lfe_gain_q15 == SPA_STEREO_UPMIX_LFE_Q15_DEFAULT);
    assert(config.lfe_enabled == 0);
    assert(config.backend == SPA_STEREO_UPMIX_AMBIENT_LITE);

    spa_stereo_upmix_config_set_lfe(&config, 0);
    assert(config.lfe_enabled == 0);
    assert(config.front_gain_q15 == SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT);
    assert(config.lfe_gain_q15 == SPA_STEREO_UPMIX_LFE_Q15_DEFAULT);
    spa_stereo_upmix_config_set_lfe(&config, 1);
    assert(config.lfe_enabled == 1);
    assert(config.front_gain_q15 ==
           SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT);
    assert(config.lfe_gain_q15 == SPA_STEREO_UPMIX_LFE_Q15_DEFAULT);
    spa_stereo_upmix_config_set_lfe(NULL, 1);
    assert(spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_OFF));
    assert(spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_AMBIENT_LITE));
    assert(spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_AMBIENT_HQ));
    assert(spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_HSR_TD));
    assert(spa_stereo_upmix_backend_supported(
               SPA_STEREO_UPMIX_HSR_TD_ACTIVE));
    assert(spa_stereo_upmix_backend_supported(
               SPA_STEREO_UPMIX_MUSIC_STAGE));
    assert(!spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_PIONEER_RER));
    assert(!spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_CUPGAN));

    memset(&state, 0, sizeof(state));
    memset(output, 0x5a, sizeof(output));
    memcpy(untouched, output, sizeof(output));
    assert(spa_stereo_upmix_process_s16(&state, input, 1, output, 1) ==
           SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED);
    assert(memcmp(output, untouched, sizeof(output)) == 0);

    config.backend = SPA_STEREO_UPMIX_AMBIENT_HQ;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    config.backend = SPA_STEREO_UPMIX_AMBIENT_LITE;
    config.sample_rate_hz = 44100;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.sample_rate_hz = SPA_STEREO_UPMIX_SAMPLE_RATE;
    config.front_gain_q15 = 32769;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.front_gain_q15 = SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT;
    config.rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_MAX + 1;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_DEFAULT;
    config.lfe_gain_q15 = SPA_STEREO_UPMIX_LFE_Q15_MAX + 1;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.lfe_gain_q15 = SPA_STEREO_UPMIX_LFE_Q15_DEFAULT;
    config.lfe_enabled = 2;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.lfe_enabled = 1;
    config.front_gain_q15 = SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);

    test_init_lfe(&state, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    before = state;
    memset(output, 0x5a, sizeof(output));
    memcpy(untouched, output, sizeof(output));
    assert(spa_stereo_upmix_process_s16(
               &state, input, SPA_STEREO_UPMIX_MAX_FRAMES + 1,
               output, SPA_STEREO_UPMIX_MAX_FRAMES + 1) ==
           SPA_STEREO_UPMIX_ERROR_BLOCK_TOO_LARGE);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(memcmp(output, untouched, sizeof(output)) == 0);

    assert(spa_stereo_upmix_process_s16(&state, input, 1, output, 0) ==
           SPA_STEREO_UPMIX_ERROR_OUTPUT_TOO_SMALL);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(memcmp(output, untouched, sizeof(output)) == 0);
    assert(spa_stereo_upmix_process_s16(&state, NULL, 1, output, 1) ==
           SPA_STEREO_UPMIX_ERROR_ARGUMENT);
    assert(spa_stereo_upmix_process_s16(&state, input, 1, NULL, 1) ==
           SPA_STEREO_UPMIX_ERROR_ARGUMENT);
    assert(spa_stereo_upmix_process_s16(&state, NULL, 0, NULL, 0) ==
           SPA_STEREO_UPMIX_OK);
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    assert(spa_stereo_upmix_process_s16(
               &state, input, 1, (int16_t *)(void *)&state, 1) ==
           SPA_STEREO_UPMIX_ERROR_STATE_OVERLAP);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(memcmp(output, untouched, sizeof(output)) == 0);

    assert(spa_stereo_upmix_init(NULL, &config) ==
           SPA_STEREO_UPMIX_ERROR_ARGUMENT);
    assert(spa_stereo_upmix_reset(NULL) ==
           SPA_STEREO_UPMIX_ERROR_ARGUMENT);
}

static void test_output_order_and_dry_fronts(void)
{
    static struct spa_stereo_upmix state;
    const int16_t input[] = {
        INT16_MIN, INT16_MAX, -12345, 23456, 0, -1, 1, 1000
    };
    int16_t output[4 * SPA_STEREO_UPMIX_OUTPUT_CHANNELS];
    size_t frame;

    /* OFF ignores the active profile's attenuation and LFE flag. */
    test_init_lfe(&state, SPA_STEREO_UPMIX_OFF,
                  SPA_STEREO_UPMIX_REAR_Q15_MAX);
    assert(spa_stereo_upmix_process_s16(&state, input, 4, output, 4) ==
           SPA_STEREO_UPMIX_OK);
    for (frame = 0; frame < 4; frame++) {
        assert(output[frame * 6] == input[frame * 2]);
        assert(output[frame * 6 + 1] == input[frame * 2 + 1]);
        assert(output[frame * 6 + 2] == 0);
        assert(output[frame * 6 + 3] == 0);
        assert(output[frame * 6 + 4] == 0);
        assert(output[frame * 6 + 5] == 0);
    }

    test_init(&state, SPA_STEREO_UPMIX_AMBIENT_LITE,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT, 0);
    assert(spa_stereo_upmix_process_s16(&state, input, 4, output, 4) ==
           SPA_STEREO_UPMIX_OK);
    for (frame = 0; frame < 4; frame++) {
        assert(output[frame * 6] == test_q15_scale(
                   input[frame * 2], SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        assert(output[frame * 6 + 1] == test_q15_scale(
                   input[frame * 2 + 1],
                   SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
    }
    test_assert_zero_center(output, 4);
}

static void test_silence_and_mono_cancel(void)
{
    static struct spa_stereo_upmix state;
    uint32_t seed = UINT32_C(0x8f31a2c7);
    size_t frame;

    memset(g_input, 0, TEST_BLOCK_FRAMES * 2 * sizeof(g_input[0]));
    memset(g_output_a, 0x5a, TEST_BLOCK_FRAMES * 6 * sizeof(g_output_a[0]));
    test_init_lfe(&state, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, TEST_BLOCK_FRAMES,
               g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    for (frame = 0; frame < TEST_BLOCK_FRAMES * 6; frame++)
        assert(g_output_a[frame] == 0);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t mono = test_noise_sample(&seed);
        g_input[frame * 2] = mono;
        g_input[frame * 2 + 1] = mono;
    }
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 1024);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + 4] == 0);
        assert(g_output_a[frame * 6 + 5] == 0);
    }
    test_assert_zero_center(g_output_a, TEST_LONG_FRAMES);
}

static void test_static_pan_and_hard_pan_leakage(void)
{
    static struct spa_stereo_upmix state;
    uint32_t seed = UINT32_C(0xd42c91e5);
    double source_energy = 0.0;
    double rear_energy = 0.0;
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int32_t base = (int32_t)test_noise_sample(&seed);
        int16_t right = (int16_t)(base / 2);
        int16_t left = (int16_t)((int32_t)right * 2);
        g_input[frame * 2] = left;
        g_input[frame * 2 + 1] = right;
    }
    test_init(&state, SPA_STEREO_UPMIX_AMBIENT_LITE, 32768,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 19);
    for (frame = 1024; frame < TEST_LONG_FRAMES; frame++) {
        double source = g_input[frame * 2];
        double rear_l = g_output_a[frame * 6 + 4];
        double rear_r = g_output_a[frame * 6 + 5];
        source_energy += source * source;
        rear_energy += rear_l * rear_l + rear_r * rear_r;
    }
    /* -45 dB amplitude leakage is a power ratio of about 3.16e-5. */
    assert(rear_energy < source_energy * 0.0000317);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed);
        g_input[frame * 2 + 1] = 0;
    }
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 23);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + 4] == 0);
        assert(g_output_a[frame * 6 + 5] == 0);
    }
}

static int16_t test_triangle_1khz(size_t frame)
{
    size_t phase = frame % 48;
    int32_t value;

    if (phase < 12)
        value = (int32_t)phase * 1000;
    else if (phase < 36)
        value = 12000 - (int32_t)(phase - 12) * 1000;
    else
        value = -12000 + (int32_t)(phase - 36) * 1000;
    return (int16_t)value;
}

static void test_antiphase_and_diffuse_rears(void)
{
    static struct spa_stereo_upmix state;
    uint32_t seed_l = UINT32_C(0x13579bdf);
    uint32_t seed_r = UINT32_C(0x2468ace1);
    double input_energy = 0.0;
    double rear_l_energy = 0.0;
    double rear_r_energy = 0.0;
    double dot = 0.0;
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t sample = test_triangle_1khz(frame);
        g_input[frame * 2] = sample;
        g_input[frame * 2 + 1] = (int16_t)-sample;
    }
    test_init(&state, SPA_STEREO_UPMIX_AMBIENT_LITE, 32768,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 1024);
    for (frame = 2048; frame < TEST_LONG_FRAMES; frame++) {
        double input = g_input[frame * 2];
        double rear_l = g_output_a[frame * 6 + 4];
        double rear_r = g_output_a[frame * 6 + 5];
        input_energy += input * input;
        rear_l_energy += rear_l * rear_l;
        rear_r_energy += rear_r * rear_r;
    }
    /* The 1 kHz anti-phase component must survive at a controlled level. */
    assert(rear_l_energy > input_energy * 0.0049);
    assert(rear_l_energy < input_energy * 0.0401);
    assert(rear_r_energy > input_energy * 0.0049);
    assert(rear_r_energy < input_energy * 0.0401);
    assert(memcmp(g_output_a + 4, g_output_a + 5, sizeof(int16_t)) != 0 ||
           rear_l_energy != rear_r_energy);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 37);
    rear_l_energy = 0.0;
    rear_r_energy = 0.0;
    for (frame = 2048; frame < TEST_LONG_FRAMES; frame++) {
        double rear_l = g_output_a[frame * 6 + 4];
        double rear_r = g_output_a[frame * 6 + 5];
        rear_l_energy += rear_l * rear_l;
        rear_r_energy += rear_r * rear_r;
        dot += rear_l * rear_r;
    }
    assert(rear_l_energy > 1.0 && rear_r_energy > 1.0);
    /* Equivalent to abs(correlation) < 0.30 without a libm sqrt. */
    assert(dot * dot < 0.09 * rear_l_energy * rear_r_energy);
}

struct test_lfe_response_point {
    unsigned frequency_hz;
    double two_cosine;
    double sine_step;
    double expected_magnitude;
    double tolerance;
};

static double test_measure_lfe_response(
    const struct test_lfe_response_point *point,
    double *phase_correlation_squared, int *phase_is_inverted)
{
    const size_t total_frames = 96000;
    const size_t settle_frames = 24000;
    const double amplitude = 12000.0;
    const double lfe_gain =
        (double)SPA_STEREO_UPMIX_LFE_Q15_DEFAULT / 32768.0;
    static struct spa_stereo_upmix state;
    double previous = -point->sine_step;
    double oscillator = 0.0;
    double input_energy = 0.0;
    double lfe_energy = 0.0;
    double dot = 0.0;
    size_t offset = 0;

    test_init_lfe(&state, SPA_STEREO_UPMIX_AMBIENT_LITE, 0);
    while (offset < total_frames) {
        size_t count = total_frames - offset;
        size_t frame;

        if (count > SPA_STEREO_UPMIX_MAX_FRAMES)
            count = SPA_STEREO_UPMIX_MAX_FRAMES;
        for (frame = 0; frame < count; frame++) {
            double scaled = amplitude * oscillator;
            double next = point->two_cosine * oscillator - previous;
            int32_t rounded = scaled >= 0.0
                            ? (int32_t)(scaled + 0.5)
                            : (int32_t)(scaled - 0.5);
            int16_t sample = (int16_t)rounded;

            g_input[frame * 2] = sample;
            g_input[frame * 2 + 1] = sample;
            previous = oscillator;
            oscillator = next;
        }
        assert(spa_stereo_upmix_process_s16(
                   &state, g_input, count, g_output_a, count) ==
               SPA_STEREO_UPMIX_OK);
        for (frame = 0; frame < count; frame++) {
            int16_t input = g_input[frame * 2];
            int16_t lfe = g_output_a[frame * 6 +
                                     SPA_STEREO_UPMIX_CHANNEL_LFE];

            assert(g_output_a[frame * 6 +
                              SPA_STEREO_UPMIX_CHANNEL_C] == 0);
            assert(g_output_a[frame * 6 +
                              SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
            assert(g_output_a[frame * 6 +
                              SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
            if (offset + frame >= settle_frames) {
                input_energy += (double)input * input;
                lfe_energy += (double)lfe * lfe;
                dot += (double)input * lfe;
            }
        }
        offset += count;
    }

    assert(input_energy > 0.0 && lfe_energy > 0.0);
    if (phase_correlation_squared)
        *phase_correlation_squared =
            dot * dot / (input_energy * lfe_energy);
    if (phase_is_inverted) *phase_is_inverted = dot < 0.0;
    return test_sqrt(lfe_energy / input_energy) / lfe_gain;
}

static void test_lfe_frequency_response(void)
{
    static const struct test_lfe_response_point kPoints[] = {
        { 20,  1.9999931461119695, 0.0026179908874179934,
          0.9961090822, 0.0060 },
        { 40,  1.9999725844948535, 0.0052359638314195796,
          0.9411779884, 0.0060 },
        { 80,  1.9998903387310243, 0.010471784116245792,
          0.5000000000, 0.0060 },
        { 120, 1.9997532649633212, 0.015707317311820672,
          0.1649421599, 0.0040 },
        { 200, 1.9993146499511145, 0.026176948307873153,
          0.0249563280, 0.0020 }
    };
    double measured[sizeof(kPoints) / sizeof(kPoints[0])];
    double phase_correlation_squared[sizeof(kPoints) / sizeof(kPoints[0])];
    int phase_is_inverted[sizeof(kPoints) / sizeof(kPoints[0])];
    size_t point;

    for (point = 0; point < sizeof(kPoints) / sizeof(kPoints[0]); point++) {
        measured[point] = test_measure_lfe_response(
            &kPoints[point], &phase_correlation_squared[point],
            &phase_is_inverted[point]);
        assert(test_absd(measured[point] - kPoints[point].expected_magnitude) <=
               kPoints[point].tolerance);
    }
    /* An LR4 low-pass is -180 degrees at its crossover: the 80 Hz sine is
     * inverted with essentially no quadrature component. */
    assert(phase_is_inverted[2]);
    assert(phase_correlation_squared[2] > 0.999);
    printf("LFE LR4 magnitude: 20=%0.6f 40=%0.6f 80=%0.6f "
           "120=%0.6f 200=%0.6f; corr@80=%0.6f\n",
           measured[0], measured[1], measured[2], measured[3], measured[4],
           -test_sqrt(phase_correlation_squared[2]));
}

static void test_lfe_dc_headroom_and_layout(void)
{
    const double front_gain =
        (double)SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT / 32768.0;
    const double lfe_gain =
        (double)SPA_STEREO_UPMIX_LFE_Q15_DEFAULT / 32768.0;
    const double rear_gain_max =
        (double)SPA_STEREO_UPMIX_REAR_Q15_MAX / 32768.0;
    const double static_left_bound =
        0.794328 * front_gain + 0.398107 * lfe_gain +
        0.707946 * rear_gain_max;
    const double static_centre_bound =
        2.0 * 0.501187 * front_gain + 0.316228 * lfe_gain;
    const double static_lfe_bus_bound = 1.412538 * lfe_gain;
    const double static_surround_bound =
        0.199526 * lfe_gain + 1.995262 * rear_gain_max;
    static struct spa_stereo_upmix state;
    int16_t last_lfe = 0;
    size_t pass;

    assert(SPA_STEREO_UPMIX_CHANNEL_L == 0);
    assert(SPA_STEREO_UPMIX_CHANNEL_R == 1);
    assert(SPA_STEREO_UPMIX_CHANNEL_C == 2);
    assert(SPA_STEREO_UPMIX_CHANNEL_LFE == 3);
    assert(SPA_STEREO_UPMIX_CHANNEL_LS == 4);
    assert(SPA_STEREO_UPMIX_CHANNEL_RS == 5);
    /* Absolute row sums with independently worst-signed bounded inputs. */
    assert(static_left_bound <= 0.994524);
    assert(static_centre_bound <= 0.998526);
    assert(static_lfe_bus_bound <= 0.251445);
    assert(static_surround_bound <= 0.534334);
    assert(static_left_bound > 0.994);
    assert(static_centre_bound > 0.998);
    assert(static_lfe_bus_bound > 0.251);
    assert(static_surround_bound > 0.534);

    test_init_lfe(&state, SPA_STEREO_UPMIX_AMBIENT_LITE, 0);
    for (pass = 0; pass < 96; pass++) {
        size_t frame;

        for (frame = 0; frame < 1000; frame++) {
            g_input[frame * 2] = INT16_MAX;
            g_input[frame * 2 + 1] = INT16_MAX;
        }
        assert(spa_stereo_upmix_process_s16(
                   &state, g_input, 1000, g_output_a, 1000) ==
               SPA_STEREO_UPMIX_OK);
        for (frame = 0; frame < 1000; frame++) {
            int16_t *output = g_output_a + frame * 6;
            double bus_c = 0.501187 *
                ((double)output[SPA_STEREO_UPMIX_CHANNEL_L] / 32768.0 +
                 (double)output[SPA_STEREO_UPMIX_CHANNEL_R] / 32768.0) +
                0.316228 *
                ((double)output[SPA_STEREO_UPMIX_CHANNEL_LFE] / 32768.0);

            assert(output[SPA_STEREO_UPMIX_CHANNEL_L] ==
                   test_q15_scale(INT16_MAX,
                                  SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT));
            assert(output[SPA_STEREO_UPMIX_CHANNEL_R] ==
                   output[SPA_STEREO_UPMIX_CHANNEL_L]);
            assert(output[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
            assert(output[SPA_STEREO_UPMIX_CHANNEL_LFE] <=
                   SPA_STEREO_UPMIX_LFE_Q15_MAX);
            assert(output[SPA_STEREO_UPMIX_CHANNEL_LFE] >=
                   -SPA_STEREO_UPMIX_LFE_Q15_MAX);
            assert(output[SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
            assert(output[SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
            assert(test_absd(bus_c) <= 1.0);
            last_lfe = output[SPA_STEREO_UPMIX_CHANNEL_LFE];
        }
    }
    assert(last_lfe == test_q15_scale(
               INT16_MAX, SPA_STEREO_UPMIX_LFE_Q15_DEFAULT));

    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    for (pass = 0; pass < 48; pass++) {
        size_t frame;

        for (frame = 0; frame < 1000; frame++) {
            g_input[frame * 2] = INT16_MIN;
            g_input[frame * 2 + 1] = INT16_MIN;
        }
        assert(spa_stereo_upmix_process_s16(
                   &state, g_input, 1000, g_output_a, 1000) ==
               SPA_STEREO_UPMIX_OK);
        for (frame = 0; frame < 1000; frame++) {
            int16_t *output = g_output_a + frame * 6;
            double bus_c = 0.501187 *
                ((double)output[SPA_STEREO_UPMIX_CHANNEL_L] / 32768.0 +
                 (double)output[SPA_STEREO_UPMIX_CHANNEL_R] / 32768.0) +
                0.316228 *
                ((double)output[SPA_STEREO_UPMIX_CHANNEL_LFE] / 32768.0);

            assert(output[SPA_STEREO_UPMIX_CHANNEL_LFE] <=
                   SPA_STEREO_UPMIX_LFE_Q15_MAX);
            assert(output[SPA_STEREO_UPMIX_CHANNEL_LFE] >=
                   -SPA_STEREO_UPMIX_LFE_Q15_MAX);
            assert(test_absd(bus_c) <= 1.0);
            last_lfe = output[SPA_STEREO_UPMIX_CHANNEL_LFE];
        }
    }
    assert(last_lfe == -SPA_STEREO_UPMIX_LFE_Q15_DEFAULT);
}

static void test_lfe_antiphase_reset_and_denormal_tail(void)
{
    static struct spa_stereo_upmix state;
    size_t frame;
    size_t block;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t sample = test_triangle_1khz(frame);
        g_input[frame * 2] = sample;
        g_input[frame * 2 + 1] = (int16_t)-sample;
    }
    test_init_lfe(&state, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 1024);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++)
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LFE] == 0);

    /* Seed both LR4 sections, then prove reset cuts the bass tail exactly. */
    memset(g_input, 0, TEST_BLOCK_FRAMES * 2 * sizeof(g_input[0]));
    g_input[0] = INT16_MAX;
    g_input[1] = INT16_MAX;
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, TEST_BLOCK_FRAMES,
               g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    memset(g_input, 0, TEST_BLOCK_FRAMES * 2 * sizeof(g_input[0]));
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, TEST_BLOCK_FRAMES,
               g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    for (frame = 0; frame < TEST_BLOCK_FRAMES * 6; frame++)
        assert(g_output_a[frame] == 0);

    /* The sub-audible IIR tail is snapped before ARMv7 reaches denormals. */
    g_input[0] = INT16_MAX;
    g_input[1] = INT16_MAX;
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, TEST_BLOCK_FRAMES,
               g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    memset(g_input, 0, TEST_BLOCK_FRAMES * 2 * sizeof(g_input[0]));
    for (block = 0; block < 256; block++)
        assert(spa_stereo_upmix_process_s16(
                   &state, g_input, TEST_BLOCK_FRAMES,
                   g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(state._runtime._lfe_biquad_state[0][0] == 0.0);
    assert(state._runtime._lfe_biquad_state[0][1] == 0.0);
    assert(state._runtime._lfe_biquad_state[1][0] == 0.0);
    assert(state._runtime._lfe_biquad_state[1][1] == 0.0);
}

static void test_saturation_and_soundamp_headroom(void)
{
    static struct spa_stereo_upmix state;
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        switch (frame & 3u) {
        case 0:
            g_input[frame * 2] = INT16_MAX;
            g_input[frame * 2 + 1] = INT16_MAX;
            break;
        case 1:
            g_input[frame * 2] = INT16_MAX;
            g_input[frame * 2 + 1] = INT16_MIN;
            break;
        case 2:
            g_input[frame * 2] = INT16_MIN;
            g_input[frame * 2 + 1] = INT16_MAX;
            break;
        default:
            g_input[frame * 2] = INT16_MIN;
            g_input[frame * 2 + 1] = INT16_MIN;
            break;
        }
    }

    test_init(&state, SPA_STEREO_UPMIX_AMBIENT_LITE,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_MAX);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 1024);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        double left = (double)g_output_a[frame * 6] / 32768.0;
        double right = (double)g_output_a[frame * 6 + 1] / 32768.0;
        double rear_l = (double)g_output_a[frame * 6 + 4] / 32768.0;
        double rear_r = (double)g_output_a[frame * 6 + 5] / 32768.0;
        double bus_l = 0.794328 * left + 0.707946 * rear_l;
        double bus_r = 0.794328 * right + 0.707946 * rear_r;
        double bus_c = 0.501187 * (left + right);
        double bus_ls = 1.995262 * rear_l;
        double bus_rs = 1.995262 * rear_r;

        assert(g_output_a[frame * 6 + 4] <= SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(g_output_a[frame * 6 + 4] >= -SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(g_output_a[frame * 6 + 5] <= SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(g_output_a[frame * 6 + 5] >= -SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(test_absd(bus_l) <= 0.9700);
        assert(test_absd(bus_r) <= 0.9700);
        assert(test_absd(bus_c) <= 1.0000);
        assert(test_absd(bus_ls) <= 0.5000);
        assert(test_absd(bus_rs) <= 0.5000);
    }
    test_assert_zero_center(g_output_a, TEST_LONG_FRAMES);
}

static void test_reset_determinism_and_tail_cut(void)
{
    static struct spa_stereo_upmix state;
    uint32_t seed_l = UINT32_C(0x77553311);
    uint32_t seed_r = UINT32_C(0xaaccee29);
    size_t frame;

    for (frame = 0; frame < TEST_BLOCK_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }
    test_init_lfe(&state, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, TEST_BLOCK_FRAMES,
               g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, TEST_BLOCK_FRAMES,
               g_output_b, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(memcmp(g_output_a, g_output_b,
                  TEST_BLOCK_FRAMES * 6 * sizeof(g_output_a[0])) == 0);

    memset(g_input, 0, TEST_BLOCK_FRAMES * 2 * sizeof(g_input[0]));
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    memset(g_output_b, 0x5a,
           TEST_BLOCK_FRAMES * 6 * sizeof(g_output_b[0]));
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, TEST_BLOCK_FRAMES,
               g_output_b, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    for (frame = 0; frame < TEST_BLOCK_FRAMES * 6; frame++)
        assert(g_output_b[frame] == 0);
}

static void test_chunk_and_instance_invariance(void)
{
    static struct spa_stereo_upmix whole;
    static struct spa_stereo_upmix chunked;
    static struct spa_stereo_upmix isolated_a;
    static struct spa_stereo_upmix isolated_b;
    static struct spa_stereo_upmix interleaved_a;
    static struct spa_stereo_upmix interleaved_b;
    uint32_t seed_l = UINT32_C(0x31415926);
    uint32_t seed_r = UINT32_C(0x27182818);
    size_t frame;

    for (frame = 0; frame < TEST_BLOCK_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }
    test_init_lfe(&whole, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init_lfe(&chunked, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_process_s16(
               &whole, g_input, TEST_BLOCK_FRAMES,
               g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&chunked, g_input, TEST_BLOCK_FRAMES, g_output_b, 3);
    assert(memcmp(g_output_a, g_output_b,
                  TEST_BLOCK_FRAMES * 6 * sizeof(g_output_a[0])) == 0);

    test_init_lfe(&isolated_a, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init_lfe(&isolated_b, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init_lfe(&interleaved_a, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init_lfe(&interleaved_b, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_process_s16(
               &isolated_a, g_input, 512, g_output_a, 512) ==
           SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &isolated_b, g_input + 1024, 512,
               g_output_a + 512 * 6, 512) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &interleaved_a, g_input, 256, g_output_b, 256) ==
           SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &interleaved_b, g_input + 1024, 256,
               g_output_b + 512 * 6, 256) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &interleaved_a, g_input + 512, 256,
               g_output_b + 256 * 6, 256) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &interleaved_b, g_input + 1536, 256,
               g_output_b + 768 * 6, 256) == SPA_STEREO_UPMIX_OK);
    assert(memcmp(g_output_a, g_output_b,
                  TEST_BLOCK_FRAMES * 6 * sizeof(g_output_a[0])) == 0);
}

static void test_in_place_widening(void)
{
    static struct spa_stereo_upmix reference;
    static struct spa_stereo_upmix in_place;
    static int16_t buffer[TEST_BLOCK_FRAMES *
                          SPA_STEREO_UPMIX_OUTPUT_CHANNELS];
    uint32_t seed_l = UINT32_C(0xf00dcafe);
    uint32_t seed_r = UINT32_C(0x1234abcd);
    size_t frame;

    for (frame = 0; frame < TEST_BLOCK_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }
    memcpy(buffer, g_input, TEST_BLOCK_FRAMES * 2 * sizeof(buffer[0]));
    test_init_lfe(&reference, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init_lfe(&in_place, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_process_s16(
               &reference, g_input, TEST_BLOCK_FRAMES,
               g_output_a, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &in_place, buffer, TEST_BLOCK_FRAMES,
               buffer, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(memcmp(g_output_a, buffer,
                  TEST_BLOCK_FRAMES * 6 * sizeof(buffer[0])) == 0);

    /* The full source is staged before the first store, so a non-identical
     * overlap is safe as well. */
    assert(spa_stereo_upmix_reset(&in_place) == SPA_STEREO_UPMIX_OK);
    memmove(buffer + 17, g_input,
            TEST_BLOCK_FRAMES * 2 * sizeof(buffer[0]));
    assert(spa_stereo_upmix_process_s16(
               &in_place, buffer + 17, TEST_BLOCK_FRAMES,
               buffer, TEST_BLOCK_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(memcmp(g_output_a, buffer,
                  TEST_BLOCK_FRAMES * 6 * sizeof(buffer[0])) == 0);
}

static void test_hq_contract_latency_chunking_and_reset(void)
{
    static struct spa_stereo_upmix reference;
    static struct spa_stereo_upmix chunked;
    static struct spa_stereo_upmix lite;
    uint32_t seed_l = UINT32_C(0x51f15e5d);
    uint32_t seed_r = UINT32_C(0x0ddba11a);
    int have_rear = 0;
    size_t frame;

    assert(SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES == 1024);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }

    test_init_lfe(&reference, SPA_STEREO_UPMIX_AMBIENT_HQ,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init_lfe(&chunked, SPA_STEREO_UPMIX_AMBIENT_HQ,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_fixed_blocks(&reference, g_input, TEST_LONG_FRAMES,
                              g_output_a, TEST_BLOCK_FRAMES);
    test_process_blocks(&chunked, g_input, TEST_LONG_FRAMES, g_output_b, 255);

    /* Hop scheduling is a property of the stream position, never of how the
     * decoder happened to split its callbacks. */
    assert(memcmp(g_output_a, g_output_b,
                  sizeof(g_output_a)) == 0);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        const int16_t *output = g_output_a + frame * 6;

        assert(output[SPA_STEREO_UPMIX_CHANNEL_L] == test_q15_scale(
                   g_input[frame * 2],
                   SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT));
        assert(output[SPA_STEREO_UPMIX_CHANNEL_R] == test_q15_scale(
                   g_input[frame * 2 + 1],
                   SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT));
        assert(output[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_LS] <=
               SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_LS] >=
               -SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_RS] <=
               SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_RS] >=
               -SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
        if (frame < SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES) {
            assert(output[SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
            assert(output[SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
        } else if (output[SPA_STEREO_UPMIX_CHANNEL_LS] != 0 ||
                   output[SPA_STEREO_UPMIX_CHANNEL_RS] != 0) {
            have_rear = 1;
        }
    }
    assert(have_rear);

    /* The adapter is intentionally backend-independent: changing only the
     * ambience extractor may not move fronts, centre or bass assist. */
    test_init_lfe(&lite, SPA_STEREO_UPMIX_AMBIENT_LITE,
                  SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_fixed_blocks(&lite, g_input, TEST_LONG_FRAMES,
                              g_output_b, TEST_BLOCK_FRAMES);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_C] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_C]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LFE] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LFE]);
    }

    /* Reset discards the analysis window, overlap-add queue, smoothed
     * statistics, decorrelator and limiter together. */
    assert(spa_stereo_upmix_reset(&chunked) == SPA_STEREO_UPMIX_OK);
    test_process_fixed_blocks(&chunked, g_input, TEST_LONG_FRAMES,
                              g_output_b, TEST_BLOCK_FRAMES);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);
    assert(spa_stereo_upmix_reset(&chunked) == SPA_STEREO_UPMIX_OK);
    memset(g_input, 0, sizeof(g_input));
    memset(g_output_b, 0x5a, sizeof(g_output_b));
    test_process_blocks(&chunked, g_input, TEST_LONG_FRAMES, g_output_b, 257);
    for (frame = 0; frame < TEST_LONG_FRAMES * 6; frame++)
        assert(g_output_b[frame] == 0);
}

static void test_hq_mono_and_static_pan_rejection(void)
{
    static struct spa_stereo_upmix state;
    uint32_t seed = UINT32_C(0x90abcdef);
    double source_energy;
    double rear_energy;
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t mono = test_noise_sample(&seed);

        g_input[frame * 2] = mono;
        g_input[frame * 2 + 1] = mono;
    }
    test_init(&state, SPA_STEREO_UPMIX_AMBIENT_HQ,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 1);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LFE] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
    }

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t right = (int16_t)(test_noise_sample(&seed) / 2);

        g_input[frame * 2] = (int16_t)((int32_t)right * 2);
        g_input[frame * 2 + 1] = right;
    }
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 255);
    source_energy = 0.0;
    rear_energy = 0.0;
    for (frame = SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES + 8192;
         frame < TEST_LONG_FRAMES; frame++) {
        double source = g_input[frame * 2];
        double rear_l = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        source_energy += source * source;
        rear_energy += rear_l * rear_l + rear_r * rear_r;
    }
    assert(source_energy > 0.0);
    assert(rear_energy < source_energy * 0.0000317);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed);
        g_input[frame * 2 + 1] = 0;
    }
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 257);
    source_energy = 0.0;
    rear_energy = 0.0;
    for (frame = SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES + 8192;
         frame < TEST_LONG_FRAMES; frame++) {
        double source = g_input[frame * 2];
        double rear_l = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        source_energy += source * source;
        rear_energy += rear_l * rear_l + rear_r * rear_r;
    }
    assert(source_energy > 0.0);
    assert(rear_energy < source_energy * 0.0000317);
}

static double test_hq_antiphase_tone_energy(double two_cosine,
                                             double sine_step)
{
    static struct spa_stereo_upmix state;
    double previous = -sine_step;
    double oscillator = 0.0;
    double rear_energy = 0.0;
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        double scaled = 12000.0 * oscillator;
        double next = two_cosine * oscillator - previous;
        int32_t rounded = scaled >= 0.0
                        ? (int32_t)(scaled + 0.5)
                        : (int32_t)(scaled - 0.5);
        int16_t sample = (int16_t)rounded;

        g_input[frame * 2] = sample;
        g_input[frame * 2 + 1] = (int16_t)-sample;
        previous = oscillator;
        oscillator = next;
    }
    test_init(&state, SPA_STEREO_UPMIX_AMBIENT_HQ,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 31);
    for (frame = SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES + 8192;
         frame < TEST_LONG_FRAMES; frame++) {
        double rear_l = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LFE] == 0);
        rear_energy += rear_l * rear_l + rear_r * rear_r;
    }
    return rear_energy;
}

static void test_hq_antiphase_passband_and_diffuse_field(void)
{
    static struct spa_stereo_upmix state;
    const double low_energy = test_hq_antiphase_tone_energy(
        1.9998494036782890, 0.012271538285719925); /* 93.75 Hz, bin 2 */
    const double mid_energy = test_hq_antiphase_tone_energy(
        1.9615705608064609, 0.19509032201612825);  /* 1.5 kHz, bin 32 */
    const double high_energy = test_hq_antiphase_tone_energy(
        0.76536686473017967, 0.92387953251128674); /* 9 kHz, bin 192 */
    uint32_t seed_l = UINT32_C(0x10293847);
    uint32_t seed_r = UINT32_C(0xabcdef01);
    double rear_l_energy = 0.0;
    double rear_r_energy = 0.0;
    double dot = 0.0;
    size_t frame;

    assert(mid_energy > 1.0);
    assert(low_energy < mid_energy * 0.05);
    assert(high_energy < mid_energy * 0.05);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }
    test_init(&state, SPA_STEREO_UPMIX_AMBIENT_HQ,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 1023);
    for (frame = SPA_STEREO_UPMIX_HQ_LATENCY_FRAMES + 8192;
         frame < TEST_LONG_FRAMES; frame++) {
        double rear_l = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        rear_l_energy += rear_l * rear_l;
        rear_r_energy += rear_r * rear_r;
        dot += rear_l * rear_r;
    }
    assert(rear_l_energy > 1.0);
    assert(rear_r_energy > 1.0);
    assert(dot * dot < 0.09 * rear_l_energy * rear_r_energy);
}

static void test_hq_channel_swap_energy_equivariance(void)
{
    enum { SIGNAL_FRAMES = TEST_LONG_FRAMES / 2 };
    static struct spa_stereo_upmix original;
    static struct spa_stereo_upmix swapped;
    uint32_t seed_l = UINT32_C(0x6d2b79f5);
    uint32_t seed_r = UINT32_C(0xb5297a4d);
    double original_l_energy = 0.0;
    double original_r_energy = 0.0;
    double swapped_l_energy = 0.0;
    double swapped_r_energy = 0.0;
    double pair_scale;
    size_t frame;

    /* A strongly left-panned coherent component plus quiet independent room
     * energy makes the covariance diagonals very unequal.  This is the
     * numerically awkward case for an eigenvector written unconditionally as
     * [|C|, lambda_max-P_L].  The second half is silence so WOLA and both
     * unequal Schroeder networks have ample time to drain before energies are
     * compared. */
    for (frame = 0; frame < SIGNAL_FRAMES; frame++) {
        int32_t direct = test_triangle_1khz(frame);
        int32_t diffuse_l = (int32_t)test_noise_sample(&seed_l) / 16;
        int32_t diffuse_r = (int32_t)test_noise_sample(&seed_r) / 16;

        g_input[frame * 2] = (int16_t)(direct + diffuse_l);
        g_input[frame * 2 + 1] =
            (int16_t)(direct / 32 + diffuse_r);
    }
    memset(g_input + SIGNAL_FRAMES * 2, 0,
           (TEST_LONG_FRAMES - SIGNAL_FRAMES) * 2 * sizeof(g_input[0]));

    test_init(&original, SPA_STEREO_UPMIX_AMBIENT_HQ,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&original, g_input, TEST_LONG_FRAMES, g_output_a, 257);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t temporary = g_input[frame * 2];

        g_input[frame * 2] = g_input[frame * 2 + 1];
        g_input[frame * 2 + 1] = temporary;
    }
    test_init(&swapped, SPA_STEREO_UPMIX_AMBIENT_HQ,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&swapped, g_input, TEST_LONG_FRAMES, g_output_b, 1023);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        double original_l =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double original_r =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];
        double swapped_l =
            g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double swapped_r =
            g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        original_l_energy += original_l * original_l;
        original_r_energy += original_r * original_r;
        swapped_l_energy += swapped_l * swapped_l;
        swapped_r_energy += swapped_r * swapped_r;
    }

    assert(original_l_energy > 1.0);
    assert(original_r_energy > 1.0);
    assert(swapped_l_energy > 1.0);
    assert(swapped_r_energy > 1.0);

    /* The decorrelators intentionally have different coefficients and are
     * not waveform-swap equivariant.  Stable all-pass networks do preserve
     * long-run energy, so compare the crossed channel energies with a modest
     * 0.5% allowance for S16 quantization and the finite drain. */
    pair_scale = original_l_energy > swapped_r_energy
               ? original_l_energy : swapped_r_energy;
    assert(test_absd(original_l_energy - swapped_r_energy) <
           0.005 * pair_scale);
    pair_scale = original_r_energy > swapped_l_energy
               ? original_r_energy : swapped_l_energy;
    assert(test_absd(original_r_energy - swapped_l_energy) <
           0.005 * pair_scale);
}

static void test_backend_saturation_and_soundamp_headroom(
    enum spa_stereo_upmix_backend backend)
{
    static struct spa_stereo_upmix state;
    uint32_t seed = UINT32_C(0xa5a55a5a);
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = (test_prng(&seed) & 1u)
                           ? INT16_MAX : INT16_MIN;
        g_input[frame * 2 + 1] = (test_prng(&seed) & 1u)
                               ? INT16_MAX : INT16_MIN;
    }
    test_init_lfe(&state, backend, SPA_STEREO_UPMIX_REAR_Q15_MAX);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 256);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        const int16_t *output = g_output_a + frame * 6;
        double left = (double)output[SPA_STEREO_UPMIX_CHANNEL_L] / 32768.0;
        double right = (double)output[SPA_STEREO_UPMIX_CHANNEL_R] / 32768.0;
        double lfe = (double)output[SPA_STEREO_UPMIX_CHANNEL_LFE] / 32768.0;
        double rear_l = (double)output[SPA_STEREO_UPMIX_CHANNEL_LS] / 32768.0;
        double rear_r = (double)output[SPA_STEREO_UPMIX_CHANNEL_RS] / 32768.0;
        double bus_l = 0.794328 * left + 0.398107 * lfe +
                       0.707946 * rear_l;
        double bus_r = 0.794328 * right + 0.398107 * lfe +
                       0.707946 * rear_r;
        double bus_c = 0.501187 * (left + right) + 0.316228 * lfe;
        double bus_lfe = 1.412538 * lfe;
        double bus_ls = 0.199526 * lfe + 1.995262 * rear_l;
        double bus_rs = 0.199526 * lfe + 1.995262 * rear_r;

        assert(output[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_LFE] <=
               SPA_STEREO_UPMIX_LFE_Q15_MAX);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_LFE] >=
               -SPA_STEREO_UPMIX_LFE_Q15_MAX);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_LS] <=
               SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_LS] >=
               -SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_RS] <=
               SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_RS] >=
               -SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(test_absd(bus_l) <= 1.0);
        assert(test_absd(bus_r) <= 1.0);
        assert(test_absd(bus_c) <= 1.0);
        assert(test_absd(bus_lfe) <= 1.0);
        assert(test_absd(bus_ls) <= 1.0);
        assert(test_absd(bus_rs) <= 1.0);
    }
}

static void test_hsr_td_contract_chunking_and_reset(void)
{
    static struct spa_stereo_upmix reference;
    static struct spa_stereo_upmix chunked;
    uint32_t seed_l = UINT32_C(0x7f4a21c3);
    uint32_t seed_r = UINT32_C(0xb862d905);
    double input_l_energy = 0.0;
    double input_r_energy = 0.0;
    double rear_l_energy = 0.0;
    double rear_r_energy = 0.0;
    double rear_dot = 0.0;
    int early_rear = 0;
    size_t frame;

    assert(SPA_STEREO_UPMIX_TD_CONTROL_STRIDE == 8);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }

    test_init(&reference, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init(&chunked, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_fixed_blocks(&reference, g_input, TEST_LONG_FRAMES,
                              g_output_a, TEST_BLOCK_FRAMES);
    test_process_blocks(&chunked, g_input, TEST_LONG_FRAMES, g_output_b, 3);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        const int16_t *output = g_output_a + frame * 6;

        assert(output[SPA_STEREO_UPMIX_CHANNEL_L] == test_q15_scale(
                   g_input[frame * 2], SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        assert(output[SPA_STEREO_UPMIX_CHANNEL_R] == test_q15_scale(
                   g_input[frame * 2 + 1],
                   SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        assert(output[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(output[SPA_STEREO_UPMIX_CHANNEL_LFE] == 0);
        if (frame < SPA_STEREO_UPMIX_TD_CONTROL_STRIDE &&
            (output[SPA_STEREO_UPMIX_CHANNEL_LS] != 0 ||
             output[SPA_STEREO_UPMIX_CHANNEL_RS] != 0))
            early_rear = 1;
        if (frame >= 2048) {
            double input_l = g_input[frame * 2];
            double input_r = g_input[frame * 2 + 1];
            double rear_l = output[SPA_STEREO_UPMIX_CHANNEL_LS];
            double rear_r = output[SPA_STEREO_UPMIX_CHANNEL_RS];

            input_l_energy += input_l * input_l;
            input_r_energy += input_r * input_r;
            rear_l_energy += rear_l * rear_l;
            rear_r_energy += rear_r * rear_r;
            rear_dot += rear_l * rear_r;
        }
    }
    /* Causal control: independent spatial energy appears immediately rather
     * than waiting for an FFT frame.  It remains bounded by the configured
     * rear gain and preserves the input's low inter-channel coherence. */
    assert(early_rear);
    assert(rear_l_energy > input_l_energy * 0.001);
    assert(rear_l_energy < input_l_energy * 0.04);
    assert(rear_r_energy > input_r_energy * 0.001);
    assert(rear_r_energy < input_r_energy * 0.04);
    assert(rear_dot * rear_dot <
           0.02 * rear_l_energy * rear_r_energy);

    assert(spa_stereo_upmix_reset(&chunked) == SPA_STEREO_UPMIX_OK);
    test_process_fixed_blocks(&chunked, g_input, TEST_LONG_FRAMES,
                              g_output_b, 257);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);
    assert(spa_stereo_upmix_reset(&chunked) == SPA_STEREO_UPMIX_OK);
    memset(g_input, 0, sizeof(g_input));
    test_process_blocks(&chunked, g_input, TEST_LONG_FRAMES, g_output_b, 31);
    for (frame = 0; frame < TEST_LONG_FRAMES * 6; frame++)
        assert(g_output_b[frame] == 0);
}

static void test_hsr_td_coherent_rejection_and_swap(void)
{
    static struct spa_stereo_upmix original;
    static struct spa_stereo_upmix swapped;
    uint32_t seed = UINT32_C(0x19c4ea72);
    uint32_t seed_l = UINT32_C(0x531d4c87);
    uint32_t seed_r = UINT32_C(0xe297a40b);
    size_t frame;

    /* A fixed positive-amplitude pan is one coherent source, not ambience. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t right = (int16_t)(test_noise_sample(&seed) / 2);

        g_input[frame * 2] = (int16_t)((int32_t)right * 2);
        g_input[frame * 2 + 1] = right;
    }
    test_init(&original, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&original, g_input, TEST_LONG_FRAMES, g_output_a, 7);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
    }

    /* A diffuse pair must render, and swapping the source channels must swap
     * every corresponding front/rear result exactly. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }
    assert(spa_stereo_upmix_reset(&original) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&original, g_input, TEST_LONG_FRAMES, g_output_a, 255);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t temporary = g_input[frame * 2];

        g_input[frame * 2] = g_input[frame * 2 + 1];
        g_input[frame * 2 + 1] = temporary;
    }
    test_init(&swapped, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&swapped, g_input, TEST_LONG_FRAMES, g_output_b, 1023);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS]);
    }
}

static void test_hsr_td_antiphase_passband(void)
{
    static struct spa_stereo_upmix state;
    double input_energy = 0.0;
    double rear_l_energy = 0.0;
    double rear_r_energy = 0.0;
    double dot = 0.0;
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t sample = test_triangle_1khz(frame);

        g_input[frame * 2] = sample;
        g_input[frame * 2 + 1] = (int16_t)-sample;
    }
    test_init(&state, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 1);
    for (frame = 2048; frame < TEST_LONG_FRAMES; frame++) {
        double input = g_input[frame * 2];
        double rear_l = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r = g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        input_energy += input * input;
        rear_l_energy += rear_l * rear_l;
        rear_r_energy += rear_r * rear_r;
        dot += rear_l * rear_r;
    }
    assert(rear_l_energy > input_energy * 0.005);
    assert(rear_l_energy < input_energy * 0.04);
    assert(rear_r_energy > input_energy * 0.005);
    assert(rear_r_energy < input_energy * 0.04);
    assert(dot < 0.0);
    assert(dot * dot > 0.95 * rear_l_energy * rear_r_energy);
}

static void test_hsr_td_dialogue_transition_and_near_coherence(void)
{
    enum { TRANSITION_FRAME = 8192 };
    static struct spa_stereo_upmix state;
    uint32_t seed_l = UINT32_C(0xb43f19a7);
    uint32_t seed_r = UINT32_C(0x72d0ce51);
    double direct_energy = 0.0;
    double rear_energy = 0.0;
    double common_input_energy = 0.0;
    double common_rear_energy = 0.0;
    double side_input_energy = 0.0;
    double side_rear_energy = 0.0;
    size_t frame;

    /* A diffuse field followed by exact mono exercises the covariance release
     * direction.  Within 20 ms of the transition, and throughout the next
     * 80 ms, the old rear field must no longer pull centered dialogue behind
     * the listener. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        if (frame < TRANSITION_FRAME) {
            g_input[frame * 2] = test_noise_sample(&seed_l);
            g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
        } else {
            int16_t direct = test_triangle_1khz(frame);

            g_input[frame * 2] = direct;
            g_input[frame * 2 + 1] = direct;
        }
    }
    test_init(&state, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 19);
    for (frame = TRANSITION_FRAME + 960;
         frame < TRANSITION_FRAME + 4800; frame++) {
        double direct = g_input[frame * 2];
        double rear_l =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        direct_energy += direct * direct;
        rear_energy += rear_l * rear_l + rear_r * rear_r;
    }
    assert(rear_energy < direct_energy * 0.0002);

    /* A realistic center is not bit-identical mono.  Add a small anti-phase
     * stochastic component (about 0.9 overall correlation): the common rear
     * leakage must stay below -37 dB in energy terms while the spatial side
     * component remains active. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int32_t common = test_triangle_1khz(frame);
        int32_t difference = (int32_t)test_noise_sample(&seed_l) / 6;

        g_input[frame * 2] = (int16_t)(common + difference);
        g_input[frame * 2 + 1] = (int16_t)(common - difference);
    }
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 251);
    for (frame = 2048; frame < TEST_LONG_FRAMES; frame++) {
        double input_l = g_input[frame * 2];
        double input_r = g_input[frame * 2 + 1];
        double rear_l =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];
        double common_input = 0.5 * (input_l + input_r);
        double common_rear = 0.5 * (rear_l + rear_r);
        double side_input = 0.5 * (input_l - input_r);
        double side_rear = 0.5 * (rear_l - rear_r);

        common_input_energy += common_input * common_input;
        common_rear_energy += common_rear * common_rear;
        side_input_energy += side_input * side_input;
        side_rear_energy += side_rear * side_rear;
    }
    assert(common_rear_energy < common_input_energy * 0.0002);
    assert(side_rear_energy > side_input_energy * 0.004);
    assert(side_rear_energy < side_input_energy * 0.010);
}

static void test_hsr_td_active_transfer_and_guards(void)
{
    static struct spa_stereo_upmix reference;
    static struct spa_stereo_upmix active;
    static struct spa_stereo_upmix chunked;
    uint32_t seed = UINT32_C(0xc71d5a29);
    uint32_t seed_l = UINT32_C(0x39e82b61);
    uint32_t seed_r = UINT32_C(0xad4713f5);
    double reference_rear_energy = 0.0;
    double active_rear_energy = 0.0;
    int moved_front = 0;
    size_t frame;

    /* Exact mono and a fixed positive pan are classified as direct and must
     * remain bit-identical dry fronts even in the more assertive profile. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t right = (int16_t)(test_noise_sample(&seed) / 2);

        g_input[frame * 2] = (int16_t)((int32_t)right * 2);
        g_input[frame * 2 + 1] = right;
    }
    test_init(&active, SPA_STEREO_UPMIX_HSR_TD_ACTIVE,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&active, g_input, TEST_LONG_FRAMES, g_output_a, 37);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L] ==
               test_q15_scale(g_input[frame * 2],
                              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R] ==
               test_q15_scale(g_input[frame * 2 + 1],
                              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
    }

    /* Independent channels exercise the active level adapter.  Relative to
     * reference HSR it must make accepted rear detail materially louder.  A
     * -3 dB fold of that detail reconstructs the original dry front exactly
     * at this non-clipping test level, proving energy was redistributed. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        g_input[frame * 2] = test_noise_sample(&seed_l);
        g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
    }
    test_init(&reference, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_reset(&active) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&reference, g_input, TEST_LONG_FRAMES, g_output_a, 1);
    test_process_blocks(&active, g_input, TEST_LONG_FRAMES, g_output_b, 1024);
    for (frame = 4096; frame < TEST_LONG_FRAMES; frame++) {
        int16_t reference_l = test_q15_scale(
            g_input[frame * 2], SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT);
        int16_t reference_r = test_q15_scale(
            g_input[frame * 2 + 1], SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT);
        int16_t active_l = g_output_b[frame * 6 +
                                      SPA_STEREO_UPMIX_CHANNEL_L];
        int16_t active_r = g_output_b[frame * 6 +
                                      SPA_STEREO_UPMIX_CHANNEL_R];
        int16_t active_rear_l = g_output_b[frame * 6 +
                                           SPA_STEREO_UPMIX_CHANNEL_LS];
        int16_t active_rear_r = g_output_b[frame * 6 +
                                           SPA_STEREO_UPMIX_CHANNEL_RS];
        double normal_rear_l = g_output_a[frame * 6 +
                                          SPA_STEREO_UPMIX_CHANNEL_LS];
        double normal_rear_r = g_output_a[frame * 6 +
                                          SPA_STEREO_UPMIX_CHANNEL_RS];

        reference_rear_energy += normal_rear_l * normal_rear_l +
                                 normal_rear_r * normal_rear_r;
        active_rear_energy += (double)active_rear_l * active_rear_l +
                              (double)active_rear_r * active_rear_r;
        if (active_l != reference_l || active_r != reference_r)
            moved_front = 1;
        assert((int32_t)active_l +
                   test_q15_scale(active_rear_l, UINT32_C(23170)) ==
               reference_l);
        assert((int32_t)active_r +
                   test_q15_scale(active_rear_r, UINT32_C(23170)) ==
               reference_r);
    }
    assert(moved_front);
    assert(active_rear_energy > reference_rear_energy * 2.5);
    assert(active_rear_energy < reference_rear_energy * 4.1);

    /* The new four envelope states are deterministic across block sizes and
     * reset boundaries just like the underlying HSR covariance state. */
    test_init(&chunked, SPA_STEREO_UPMIX_HSR_TD_ACTIVE,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_reset(&active) == SPA_STEREO_UPMIX_OK);
    test_process_fixed_blocks(&active, g_input, TEST_LONG_FRAMES,
                              g_output_a, 256);
    test_process_blocks(&chunked, g_input, TEST_LONG_FRAMES, g_output_b, 3);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);
    assert(spa_stereo_upmix_reset(&chunked) == SPA_STEREO_UPMIX_OK);
    test_process_fixed_blocks(&chunked, g_input, TEST_LONG_FRAMES,
                              g_output_b, 1024);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t temporary = g_input[frame * 2];

        g_input[frame * 2] = g_input[frame * 2 + 1];
        g_input[frame * 2 + 1] = temporary;
    }
    assert(spa_stereo_upmix_reset(&chunked) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&chunked, g_input, TEST_LONG_FRAMES, g_output_b, 17);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS]);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS] ==
               g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS]);
    }
}

static void test_hsr_td_active_dialogue_transition(void)
{
    enum { TRANSITION_FRAME = 8192 };
    static struct spa_stereo_upmix state;
    uint32_t seed_l = UINT32_C(0x6b1f2359);
    uint32_t seed_r = UINT32_C(0xdf904a17);
    double direct_energy = 0.0;
    double rear_energy = 0.0;
    double common_input_energy = 0.0;
    double common_rear_energy = 0.0;
    double side_input_energy = 0.0;
    double side_rear_energy = 0.0;
    size_t frame;

    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        if (frame < TRANSITION_FRAME) {
            g_input[frame * 2] = test_noise_sample(&seed_l);
            g_input[frame * 2 + 1] = test_noise_sample(&seed_r);
        } else {
            int16_t direct = test_triangle_1khz(frame);

            g_input[frame * 2] = direct;
            g_input[frame * 2 + 1] = direct;
        }
    }
    test_init(&state, SPA_STEREO_UPMIX_HSR_TD_ACTIVE,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 19);
    for (frame = TRANSITION_FRAME + 960;
         frame < TRANSITION_FRAME + 4800; frame++) {
        double direct = g_input[frame * 2];
        double rear_l =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];

        direct_energy += direct * direct;
        rear_energy += rear_l * rear_l + rear_r * rear_r;
    }
    assert(rear_energy < direct_energy * 0.0004);

    /* The assertive gain is allowed to make the existing side information
     * louder, but must not turn a nearly centered source into common-mode
     * rear energy. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int32_t common = test_triangle_1khz(frame);
        int32_t difference = (int32_t)test_noise_sample(&seed_l) / 6;

        g_input[frame * 2] = (int16_t)(common + difference);
        g_input[frame * 2 + 1] = (int16_t)(common - difference);
    }
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&state, g_input, TEST_LONG_FRAMES, g_output_a, 251);
    for (frame = 2048; frame < TEST_LONG_FRAMES; frame++) {
        double input_l = g_input[frame * 2];
        double input_r = g_input[frame * 2 + 1];
        double rear_l =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS];
        double rear_r =
            g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS];
        double common_input = 0.5 * (input_l + input_r);
        double common_rear = 0.5 * (rear_l + rear_r);
        double side_input = 0.5 * (input_l - input_r);
        double side_rear = 0.5 * (rear_l - rear_r);

        common_input_energy += common_input * common_input;
        common_rear_energy += common_rear * common_rear;
        side_input_energy += side_input * side_input;
        side_rear_energy += side_rear * side_rear;
    }
    assert(common_rear_energy < common_input_energy * 0.0004);
    assert(side_rear_energy > side_input_energy * 0.007);
    assert(side_rear_energy < side_input_energy * 0.040);
}

static void test_music_stage_panned_source_and_guards(void)
{
    static struct spa_stereo_upmix hsr;
    static struct spa_stereo_upmix stage;
    static struct spa_stereo_upmix chunked;
    uint32_t seed = UINT32_C(0x81c6f42b);
    double stage_l_energy = 0.0;
    double stage_r_energy = 0.0;
    size_t frame;

    /* A coherent 2:1 pan is intentionally direct in reference HSR.  The music
     * profile must expose it behind the listener with the same dominant side,
     * while its compensated stereo fold remains the original dry front. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t right = (int16_t)(test_noise_sample(&seed) / 2);

        g_input[frame * 2] = (int16_t)((int32_t)right * 2);
        g_input[frame * 2 + 1] = right;
    }
    test_init(&hsr, SPA_STEREO_UPMIX_HSR_TD,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_init(&stage, SPA_STEREO_UPMIX_MUSIC_STAGE,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    test_process_blocks(&hsr, g_input, TEST_LONG_FRAMES, g_output_a, 7);
    test_process_blocks(&stage, g_input, TEST_LONG_FRAMES, g_output_b, 1024);
    for (frame = 2048; frame < TEST_LONG_FRAMES; frame++) {
        int16_t rear_l = g_output_b[frame * 6 +
                                    SPA_STEREO_UPMIX_CHANNEL_LS];
        int16_t rear_r = g_output_b[frame * 6 +
                                    SPA_STEREO_UPMIX_CHANNEL_RS];
        int16_t folded_l = (int16_t)(
            (int32_t)g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L] +
            test_q15_scale(rear_l, UINT32_C(23170)));
        int16_t folded_r = (int16_t)(
            (int32_t)g_output_b[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R] +
            test_q15_scale(rear_r, UINT32_C(23170)));

        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
        assert(folded_l == test_q15_scale(
                   g_input[frame * 2], SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        assert(folded_r == test_q15_scale(
                   g_input[frame * 2 + 1],
                   SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        stage_l_energy += (double)rear_l * rear_l;
        stage_r_energy += (double)rear_r * rear_r;
    }
    assert(stage_l_energy > 1.0e8);
    assert(stage_r_energy > 1.0e8);
    assert(stage_l_energy > stage_r_energy * 3.95);
    assert(stage_l_energy < stage_r_energy * 4.05);

    /* Exact centre remains a hard zero-rear invariant. */
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        int16_t mono = test_noise_sample(&seed);

        g_input[frame * 2] = mono;
        g_input[frame * 2 + 1] = mono;
    }
    assert(spa_stereo_upmix_reset(&stage) == SPA_STEREO_UPMIX_OK);
    test_process_blocks(&stage, g_input, TEST_LONG_FRAMES, g_output_a, 29);
    for (frame = 0; frame < TEST_LONG_FRAMES; frame++) {
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_LS] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_RS] == 0);
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_L] ==
               test_q15_scale(g_input[frame * 2],
                              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
        assert(g_output_a[frame * 6 + SPA_STEREO_UPMIX_CHANNEL_R] ==
               test_q15_scale(g_input[frame * 2 + 1],
                              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT));
    }

    /* Streaming block boundaries and reset cannot change the scene. */
    test_init(&chunked, SPA_STEREO_UPMIX_MUSIC_STAGE,
              SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT,
              SPA_STEREO_UPMIX_REAR_Q15_DEFAULT);
    assert(spa_stereo_upmix_reset(&stage) == SPA_STEREO_UPMIX_OK);
    test_process_fixed_blocks(&stage, g_input, TEST_LONG_FRAMES,
                              g_output_a, 256);
    test_process_blocks(&chunked, g_input, TEST_LONG_FRAMES, g_output_b, 3);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);
}

int main(void)
{
    test_default_and_validation();
    test_output_order_and_dry_fronts();
    test_silence_and_mono_cancel();
    test_static_pan_and_hard_pan_leakage();
    test_antiphase_and_diffuse_rears();
    test_lfe_frequency_response();
    test_lfe_dc_headroom_and_layout();
    test_lfe_antiphase_reset_and_denormal_tail();
    test_saturation_and_soundamp_headroom();
    test_reset_determinism_and_tail_cut();
    test_chunk_and_instance_invariance();
    test_in_place_widening();
    test_hq_contract_latency_chunking_and_reset();
    test_hq_mono_and_static_pan_rejection();
    test_hq_antiphase_passband_and_diffuse_field();
    test_hq_channel_swap_energy_equivariance();
    test_backend_saturation_and_soundamp_headroom(
        SPA_STEREO_UPMIX_AMBIENT_HQ);
    test_hsr_td_contract_chunking_and_reset();
    test_hsr_td_coherent_rejection_and_swap();
    test_hsr_td_antiphase_passband();
    test_hsr_td_dialogue_transition_and_near_coherence();
    test_backend_saturation_and_soundamp_headroom(
        SPA_STEREO_UPMIX_HSR_TD);
    test_hsr_td_active_transfer_and_guards();
    test_hsr_td_active_dialogue_transition();
    test_backend_saturation_and_soundamp_headroom(
        SPA_STEREO_UPMIX_HSR_TD_ACTIVE);
    test_music_stage_panned_source_and_guards();
    test_backend_saturation_and_soundamp_headroom(
        SPA_STEREO_UPMIX_MUSIC_STAGE);
    puts("stereo_upmix_test: OK");
    return 0;
}
