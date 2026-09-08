/* Host-side deterministic tests for the Universal PCM2 -> PCM6 renderer. */

#include "audio/stereo_upmix.h"

#include <assert.h>
#include <math.h>
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
static int16_t g_front_a[TEST_FRAMES * 2];
static int16_t g_front_b[TEST_FRAMES * 2];
static float g_rear_a[TEST_FRAMES * 2];
static float g_rear_b[TEST_FRAMES * 2];
static float g_lfe_control[TEST_FRAMES * 2];
static union {
    float align;
    uint8_t bytes[SPA_HSSR_MODEL_PAYLOAD_BYTES];
} g_model_payload;
static struct spa_hssr_model g_model;
static int g_model_ready;

struct reset_probe {
    uint32_t resets;
    uint32_t generation;
};

static int reset_probe_submit(
    void *context, uint32_t generation, uint32_t sequence,
    const float features[SPA_HSSR_FEATURES])
{
    (void)context;
    (void)generation;
    (void)sequence;
    (void)features;
    return 1;
}

static int reset_probe_receive(
    void *context, uint32_t generation, uint32_t maximum_sequence,
    float control[SPA_HSSR_OUTPUTS], uint32_t *sequence)
{
    (void)context;
    (void)generation;
    (void)maximum_sequence;
    (void)control;
    (void)sequence;
    return 0;
}

static void reset_probe_reset(void *context, uint32_t generation)
{
    struct reset_probe *probe = (struct reset_probe *)context;

    probe->resets++;
    probe->generation = generation;
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

static void fill_sine(float frequency_hz, int anti_phase)
{
    const double phase_step = 6.28318530717958647693 *
        (double)frequency_hz / (double)SPA_STEREO_UPMIX_SAMPLE_RATE;
    size_t frame;

    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        int16_t sample = (int16_t)lrint(32700.0 *
            sin(phase_step * (double)frame));

        g_input[frame * 2] = sample;
        g_input[frame * 2 + 1] = anti_phase ? (int16_t)-sample : sample;
    }
}

static uint64_t channel_energy(
    const int16_t *pcm6, unsigned channel, size_t first_frame)
{
    uint64_t energy = 0;
    size_t frame;

    for (frame = first_frame; frame < TEST_FRAMES; ++frame) {
        int64_t sample = pcm6[frame * TEST_CHANNELS + channel];

        energy += (uint64_t)(sample * sample);
    }
    return energy;
}

static double rear_shelf_gain_db(double frequency_hz)
{
    const double phase_step = 6.28318530717958647693 * frequency_hz /
        (double)SPA_STEREO_UPMIX_SAMPLE_RATE;
    float state[2] = { 0.0f, 0.0f };
    double input_energy = 0.0;
    double output_energy = 0.0;
    size_t frame;

    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        float input = (float)(0.25 * sin(phase_step * (double)frame));
        float output = spa_stereo_upmix_test_rear_high_shelf(input, state);

        if (frame >= 1024) {
            input_energy += (double)input * input;
            output_energy += (double)output * output;
        }
    }
    assert(input_energy > 0.0 && output_energy > 0.0);
    return 10.0 * log10(output_energy / input_energy);
}

static void test_rear_high_shelf_response(void)
{
    double low_db = rear_shelf_gain_db(1000.0);
    double midpoint_db = rear_shelf_gain_db(4000.0);
    double high_db = rear_shelf_gain_db(12000.0);

    assert(low_db >= 0.0 && low_db < 0.03);
    assert(midpoint_db > 1.48 && midpoint_db < 1.52);
    assert(high_db > 2.96 && high_db < 3.02);
}

static void assert_cabin_buses_bounded(
    const int16_t *pcm6, size_t frames)
{
    const double limit = 32275.01;
    size_t frame;

    for (frame = 0; frame < frames; ++frame) {
        const int16_t *sample = pcm6 + frame * TEST_CHANNELS;
        double buses[6];
        unsigned bus;

        buses[0] = 0.794328 * sample[0] + 0.398107 * sample[3] +
                   0.707946 * sample[4];
        buses[1] = 0.794328 * sample[1] + 0.398107 * sample[3] +
                   0.707946 * sample[5];
        buses[2] = 0.501187 * (sample[0] + sample[1]) +
                   0.316228 * sample[3];
        buses[3] = 1.412538 * sample[3];
        buses[4] = 0.199526 * sample[3] + 1.995262 * sample[4];
        buses[5] = 0.199526 * sample[3] + 1.995262 * sample[5];
        for (bus = 0; bus < 6; ++bus)
            assert(fabs(buses[bus]) <= limit);
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
    assert(config.hssr_control_provider == NULL);
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

static void test_hssr_block_matches_sample_entry(void)
{
    static struct spa_hssr_dsp block_state;
    static struct spa_hssr_dsp sample_state;
    static const size_t pattern[] = { 1, 127, 3, 509, 128, 17, 1024 };
    const struct spa_hssr_model *model = test_model();
    size_t offset = 0;
    size_t pattern_index = 0;
    size_t frame;

    fill_stereo(UINT32_C(0x424c4f43), 0);
    spa_hssr_dsp_prepare(&block_state);
    spa_hssr_dsp_prepare(&sample_state);
    while (offset < TEST_FRAMES) {
        size_t count = pattern[pattern_index %
            (sizeof(pattern) / sizeof(pattern[0]))];

        if (count > TEST_FRAMES - offset) count = TEST_FRAMES - offset;
        assert(spa_hssr_dsp_process_block(
                   &block_state, model, NULL, g_input + offset * 2,
                   count, g_front_a + offset * 2,
                   g_rear_a + offset * 2,
                   g_lfe_control + offset * 2) == SPA_HSSR_OK);
        offset += count;
        pattern_index++;
    }
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        assert(spa_hssr_dsp_process_sample(
                   &sample_state, model,
                   g_input[frame * 2], g_input[frame * 2 + 1],
                   &g_front_b[frame * 2], &g_front_b[frame * 2 + 1],
                   &g_rear_b[frame * 2], &g_rear_b[frame * 2 + 1]) ==
               SPA_HSSR_OK);
    }
    assert(memcmp(g_front_a, g_front_b, sizeof(g_front_a)) == 0);
    assert(memcmp(g_rear_a, g_rear_b, sizeof(g_rear_a)) == 0);
}

static void test_reset_advances_control_generation(void)
{
    static struct spa_stereo_upmix state;
    struct spa_stereo_upmix_config config;
    struct reset_probe probe;
    struct spa_hssr_control_provider provider;

    memset(&probe, 0, sizeof(probe));
    provider.context = &probe;
    provider.submit = reset_probe_submit;
    provider.receive = reset_probe_receive;
    provider.reset = reset_probe_reset;
    make_universal_config(&config, 0);
    spa_stereo_upmix_config_set_hssr_control_provider(&config, &provider);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    assert(probe.resets == 1u && probe.generation == 1u);
    assert(state._runtime._hssr.control_generation == 1u);
    fill_stereo(UINT32_C(0x4c415445), 0);
    assert(spa_stereo_upmix_process_s16(
               &state, g_input, SPA_STEREO_UPMIX_MAX_FRAMES,
               g_output_a, SPA_STEREO_UPMIX_MAX_FRAMES) ==
           SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &state, g_input + SPA_STEREO_UPMIX_MAX_FRAMES * 2,
               SPA_STEREO_UPMIX_MAX_FRAMES,
               g_output_a + SPA_STEREO_UPMIX_MAX_FRAMES * TEST_CHANNELS,
               SPA_STEREO_UPMIX_MAX_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_process_s16(
               &state, g_input + SPA_STEREO_UPMIX_MAX_FRAMES * 4,
               SPA_STEREO_UPMIX_MAX_FRAMES,
               g_output_a + SPA_STEREO_UPMIX_MAX_FRAMES * 2 * TEST_CHANNELS,
               SPA_STEREO_UPMIX_MAX_FRAMES) == SPA_STEREO_UPMIX_OK);
    assert(state._runtime._hssr.control_deadlines_missed >= 3u);
    assert(state._runtime._hssr.inference_failures > 0u);
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    assert(probe.resets == 2u && probe.generation == 2u);
    assert(state._runtime._hssr.control_generation == 2u);
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    assert(probe.resets == 3u && probe.generation == 3u);
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

static void test_lfe_v2_phase_frequency_and_cabin_bounds(void)
{
    static struct spa_stereo_upmix state;
    struct spa_stereo_upmix_config config;
    uint64_t low_energy;
    uint64_t anti_energy;
    uint64_t crossover_energy;
    uint64_t high_energy;
    int32_t low_peak = 0;
    size_t frame;

    make_universal_config(&config, 1);

    fill_sine(40.0f, 0);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_a);
    low_energy = channel_energy(
        g_output_a, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    for (frame = 4096; frame < TEST_FRAMES; ++frame) {
        int32_t sample = g_output_a[frame * TEST_CHANNELS +
                                    SPA_STEREO_UPMIX_CHANNEL_LFE];

        if (sample < 0) sample = -sample;
        if (sample > low_peak) low_peak = sample;
    }
    assert(low_energy > UINT64_C(10000000000));
    assert(low_peak > 2750);
    assert_cabin_buses_bounded(g_output_a, TEST_FRAMES);

    fill_sine(40.0f, 1);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    anti_energy = channel_energy(
        g_output_b, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    assert(anti_energy > low_energy / 2u);
    assert_cabin_buses_bounded(g_output_b, TEST_FRAMES);

    /* The retired 95-Hz punch branch would fail this guard.  The shipping
     * path stays below the conservative 48-Hz SoundAmp sub-band boundary. */
    fill_sine(96.0f, 0);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    crossover_energy = channel_energy(
        g_output_b, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    assert(crossover_energy * 50u < low_energy);
    assert_cabin_buses_bounded(g_output_b, TEST_FRAMES);

    fill_sine(500.0f, 0);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    high_energy = channel_energy(
        g_output_b, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    assert(high_energy * 1000u < low_energy);
    assert_cabin_buses_bounded(g_output_b, TEST_FRAMES);
}

int main(void)
{
    test_default_validation_and_fail_closed();
    test_hssr_block_matches_sample_entry();
    test_rear_high_shelf_response();
    test_reset_advances_control_generation();
    test_off_is_exact_and_overlap_safe();
    test_universal_latency_chunking_reset_and_lfe();
    test_universal_overlap_and_signal_derived_fallback();
    test_lfe_v2_phase_frequency_and_cabin_bounds();
    puts("stereo_upmix_test: OK (Universal + rear shelf + LFE V2)");
    return 0;
}
