/* Host-side deterministic tests for the Universal PCM2 -> PCM6 renderer. */

#include "audio/stereo_upmix.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_FRAMES = 8192,
    TEST_CHANNELS = SPA_STEREO_UPMIX_OUTPUT_CHANNELS
};

static int16_t g_input[TEST_FRAMES * SPA_STEREO_UPMIX_INPUT_CHANNELS];
static int16_t g_output_a[TEST_FRAMES * TEST_CHANNELS];
static int16_t g_output_b[TEST_FRAMES * TEST_CHANNELS];
static int16_t g_overlap[TEST_FRAMES * TEST_CHANNELS];
static union {
    float align;
    uint8_t bytes[SPA_HSSR_MODEL_PAYLOAD_BYTES];
} g_model_payload;
static struct spa_hssr_model g_model;
static int g_model_ready;

static uint32_t test_prng(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static const struct spa_hssr_model *test_model(void)
{
    float *floats;
    unsigned index;

    if (g_model_ready) return &g_model;
    memset(&g_model_payload, 0, sizeof(g_model_payload));
    floats = (float *)(void *)(g_model_payload.bytes + SPA_HSSR_WEIGHT_BYTES);
    for (index = 0; index < SPA_HSSR_FEATURES; ++index)
        floats[SPA_HSSR_FEATURES + index] = 1.0f;
    for (index = SPA_HSSR_FEATURES * 2;
         index < SPA_HSSR_FEATURES * 2 +
                     SPA_HSSR_GATE_ROWS * 4 + SPA_HSSR_OUTPUTS;
         ++index)
        floats[index] = 1.0f / 127.0f;
    assert(spa_hssr_model_view_payload(
               &g_model, g_model_payload.bytes,
               sizeof(g_model_payload.bytes), 0) == SPA_HSSR_OK);
    g_model_ready = 1;
    return &g_model;
}

static void fill_stereo(uint32_t seed, int mono)
{
    size_t frame;

    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        int16_t left = (int16_t)((int32_t)(test_prng(&seed) >> 16) - 32768);
        int16_t right = mono ? left :
            (int16_t)((int32_t)(test_prng(&seed) >> 16) - 32768);

        g_input[frame * 2] = (int16_t)(left / 2);
        g_input[frame * 2 + 1] = (int16_t)(right / 2);
    }
}

static void make_universal_config(struct spa_stereo_upmix_config *config,
                                  int lfe)
{
    spa_stereo_upmix_default_config(config);
    config->backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    config->rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_MAX;
    spa_stereo_upmix_config_set_lfe(config, lfe);
    spa_stereo_upmix_config_set_hssr_model(config, test_model());
}

static void process_pattern(struct spa_stereo_upmix *state,
                            const int16_t *input, int16_t *output)
{
    static const size_t pattern[] = { 1, 7, 31, 257, 13, 509, 3, 211 };
    size_t frame = 0;
    size_t pattern_index = 0;

    while (frame < TEST_FRAMES) {
        size_t count = pattern[pattern_index %
            (sizeof(pattern) / sizeof(pattern[0]))];

        if (count > TEST_FRAMES - frame) count = TEST_FRAMES - frame;
        assert(spa_stereo_upmix_process_s16(
                   state, input + frame * 2, count,
                   output + frame * TEST_CHANNELS, count) ==
               SPA_STEREO_UPMIX_OK);
        frame += count;
        ++pattern_index;
    }
}

static void process_blocks(struct spa_stereo_upmix *state,
                           const int16_t *input, int16_t *output)
{
    size_t frame = 0;

    while (frame < TEST_FRAMES) {
        size_t count = TEST_FRAMES - frame;

        if (count > SPA_STEREO_UPMIX_MAX_FRAMES)
            count = SPA_STEREO_UPMIX_MAX_FRAMES;
        assert(spa_stereo_upmix_process_s16(
                   state, input + frame * 2, count,
                   output + frame * TEST_CHANNELS, count) ==
               SPA_STEREO_UPMIX_OK);
        frame += count;
    }
}

static void test_default_validation_and_fail_closed(void)
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
    assert(config.backend == SPA_STEREO_UPMIX_OFF);
    assert(config.hssr_model == NULL);
    assert(spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_OFF));
    assert(spa_stereo_upmix_backend_supported(
               SPA_STEREO_UPMIX_HSSR_UNIVERSAL));
    assert(!spa_stereo_upmix_backend_supported(
               (enum spa_stereo_upmix_backend)2));

    memset(&state, 0, sizeof(state));
    memset(output, 0x5a, sizeof(output));
    memcpy(untouched, output, sizeof(output));
    assert(spa_stereo_upmix_process_s16(&state, input, 1, output, 1) ==
           SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED);
    assert(memcmp(output, untouched, sizeof(output)) == 0);

    config.backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.backend = (enum spa_stereo_upmix_backend)2;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_UNSUPPORTED);
    make_universal_config(&config, 1);
    config.sample_rate_hz = 44100;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    make_universal_config(&config, 1);
    config.front_gain_q15 = SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    make_universal_config(&config, 1);
    config.rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_MAX + 1u;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);

    make_universal_config(&config, 1);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    before = state;
    memset(output, 0x5a, sizeof(output));
    memcpy(untouched, output, sizeof(output));
    assert(spa_stereo_upmix_process_s16(
               &state, input, SPA_STEREO_UPMIX_MAX_FRAMES + 1u,
               output, SPA_STEREO_UPMIX_MAX_FRAMES + 1u) ==
           SPA_STEREO_UPMIX_ERROR_BLOCK_TOO_LARGE);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(memcmp(output, untouched, sizeof(output)) == 0);
    assert(spa_stereo_upmix_process_s16(&state, input, 1, output, 0) ==
           SPA_STEREO_UPMIX_ERROR_OUTPUT_TOO_SMALL);
    assert(spa_stereo_upmix_process_s16(&state, NULL, 1, output, 1) ==
           SPA_STEREO_UPMIX_ERROR_ARGUMENT);
    assert(spa_stereo_upmix_process_s16(&state, input, 1, NULL, 1) ==
           SPA_STEREO_UPMIX_ERROR_ARGUMENT);
    assert(spa_stereo_upmix_process_s16(&state, NULL, 0, NULL, 0) ==
           SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &state, input, 1, (int16_t *)(void *)&state, 1) ==
           SPA_STEREO_UPMIX_ERROR_STATE_OVERLAP);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
}

static void test_off_is_exact_and_overlap_safe(void)
{
    static struct spa_stereo_upmix state;
    struct spa_stereo_upmix_config config;
    size_t frame;

    fill_stereo(UINT32_C(0x4f464621), 0);
    spa_stereo_upmix_default_config(&config);
    config.front_gain_q15 = 1;
    config.rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_MAX;
    config.lfe_enabled = 1;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_a);
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        const int16_t *out = g_output_a + frame * TEST_CHANNELS;

        assert(out[0] == g_input[frame * 2]);
        assert(out[1] == g_input[frame * 2 + 1]);
        assert(out[2] == 0 && out[3] == 0 && out[4] == 0 && out[5] == 0);
    }

    memcpy(g_overlap, g_input,
           SPA_STEREO_UPMIX_MAX_FRAMES * 2 * sizeof(g_input[0]));
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &state, g_overlap, SPA_STEREO_UPMIX_MAX_FRAMES,
               g_overlap, SPA_STEREO_UPMIX_MAX_FRAMES) ==
           SPA_STEREO_UPMIX_OK);
    assert(memcmp(g_overlap, g_output_a,
                  SPA_STEREO_UPMIX_MAX_FRAMES * TEST_CHANNELS *
                      sizeof(g_output_a[0])) == 0);
}

static void test_universal_latency_chunking_reset_and_lfe(void)
{
    static struct spa_stereo_upmix block_state;
    static struct spa_stereo_upmix chunk_state;
    struct spa_stereo_upmix_config config;
    int have_rear = 0;
    int have_lfe = 0;
    size_t frame;

    fill_stereo(UINT32_C(0x554e4956), 1);
    make_universal_config(&config, 1);
    assert(spa_stereo_upmix_init(&block_state, &config) ==
           SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_init(&chunk_state, &config) ==
           SPA_STEREO_UPMIX_OK);
    process_blocks(&block_state, g_input, g_output_a);
    process_pattern(&chunk_state, g_input, g_output_b);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);

    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        const int16_t *out = g_output_a + frame * TEST_CHANNELS;

        assert(out[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(out[SPA_STEREO_UPMIX_CHANNEL_LFE] >=
               -(int32_t)SPA_STEREO_UPMIX_LFE_Q15_MAX);
        assert(out[SPA_STEREO_UPMIX_CHANNEL_LFE] <=
               (int32_t)SPA_STEREO_UPMIX_LFE_Q15_MAX);
        assert(out[SPA_STEREO_UPMIX_CHANNEL_LS] >=
               -(int32_t)SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(out[SPA_STEREO_UPMIX_CHANNEL_LS] <=
               (int32_t)SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(out[SPA_STEREO_UPMIX_CHANNEL_RS] >=
               -(int32_t)SPA_STEREO_UPMIX_REAR_Q15_MAX);
        assert(out[SPA_STEREO_UPMIX_CHANNEL_RS] <=
               (int32_t)SPA_STEREO_UPMIX_REAR_Q15_MAX);
        if (frame < SPA_HSSR_LATENCY_FRAMES) {
            assert(out[0] == 0 && out[1] == 0 && out[3] == 0);
            assert(out[4] == 0 && out[5] == 0);
        } else {
            if (out[3] != 0) have_lfe = 1;
            if (out[4] != 0 || out[5] != 0) have_rear = 1;
        }
    }
    assert(have_rear);
    assert(have_lfe);

    assert(spa_stereo_upmix_reset(&chunk_state) == SPA_STEREO_UPMIX_OK);
    process_blocks(&chunk_state, g_input, g_output_b);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);
}

static void test_universal_overlap_and_signal_derived_fallback(void)
{
    static struct spa_stereo_upmix reference;
    static struct spa_stereo_upmix overlap;
    struct spa_stereo_upmix_config config;
    uint64_t rear_energy = 0;
    uint64_t difference_energy = 0;
    size_t frame;

    fill_stereo(UINT32_C(0x52454152), 1);
    make_universal_config(&config, 0);
    assert(spa_stereo_upmix_init(&reference, &config) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_init(&overlap, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&reference, g_input, g_output_a);
    memcpy(g_overlap, g_input,
           SPA_STEREO_UPMIX_MAX_FRAMES * 2 * sizeof(g_input[0]));
    assert(spa_stereo_upmix_process_s16(
               &overlap, g_overlap, SPA_STEREO_UPMIX_MAX_FRAMES,
               g_overlap, SPA_STEREO_UPMIX_MAX_FRAMES) ==
           SPA_STEREO_UPMIX_OK);
    assert(memcmp(g_overlap, g_output_a,
                  SPA_STEREO_UPMIX_MAX_FRAMES * TEST_CHANNELS *
                      sizeof(g_output_a[0])) == 0);

    for (frame = 4096; frame < TEST_FRAMES; ++frame) {
        int32_t left = g_output_a[frame * TEST_CHANNELS +
                                  SPA_STEREO_UPMIX_CHANNEL_LS];
        int32_t right = g_output_a[frame * TEST_CHANNELS +
                                   SPA_STEREO_UPMIX_CHANNEL_RS];
        int64_t difference = (int64_t)left - right;

        rear_energy += (uint64_t)((int64_t)left * left) +
                       (uint64_t)((int64_t)right * right);
        difference_energy += (uint64_t)(difference * difference);
    }
    assert(rear_energy > UINT64_C(1000000));
    assert(difference_energy > rear_energy / 20u);

    memset(g_input, 0, sizeof(g_input));
    assert(spa_stereo_upmix_reset(&reference) == SPA_STEREO_UPMIX_OK);
    process_blocks(&reference, g_input, g_output_a);
    for (frame = 0; frame < TEST_FRAMES * TEST_CHANNELS; ++frame)
        assert(g_output_a[frame] == 0);
}

int main(void)
{
    test_default_validation_and_fail_closed();
    test_off_is_exact_and_overlap_safe();
    test_universal_latency_chunking_reset_and_lfe();
    test_universal_overlap_and_signal_derived_fallback();
    puts("stereo_upmix_test: OK (off + hssr-universal only)");
    return 0;
}
