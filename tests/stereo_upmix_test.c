/* Host-side deterministic tests for the Universal PCM2 -> PCM6 renderer. */

#include "audio/stereo_upmix.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
static struct spa_mask_model g_mask_model;
static int g_model_ready;

struct reset_probe {
    uint32_t resets;
    uint32_t generation;
};

static int reset_probe_submit(
    void *context, uint32_t generation, uint32_t sequence,
    const float features[SPA_HSSR_FEATURES], const float *mask_features)
{
    (void)context;
    (void)generation;
    (void)sequence;
    (void)features;
    (void)mask_features;
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
        const float *matrix = spa_stereo_upmix_input_matrix();
        unsigned bus;
        for (bus = 0; bus < 8; ++bus) {
            double value = 0.0;
            unsigned ch;
            for (ch = 0; ch < 6; ++ch) value += matrix[ch * 8 + bus] * (double)sample[ch];
            assert(fabs(value) <= limit);
        }
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
               (enum spa_stereo_upmix_backend)3));

    memset(&state, 0, sizeof(state));
    memset(output, 0x5a, sizeof(output));
    memcpy(untouched, output, sizeof(output));
    assert(spa_stereo_upmix_process_s16(&state, input, 1, output, 1) ==
           SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED);
    assert(memcmp(output, untouched, sizeof(output)) == 0);

    config.backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.backend = (enum spa_stereo_upmix_backend)3;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_UNSUPPORTED);
    make_universal_config(&config, 1);
    config.sample_rate_hz = 44100;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    /* The static centre-bus worst case is no longer a config error: the
     * per-sample LFE cabin limit is the rail guard (LFE assist +9.5 dB). */
    make_universal_config(&config, 1);
    config.front_gain_q15 = SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
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
    /* the guard and the mask net ride the config; reset() must hand them
     * over again like init() does (the hook resets on every stream start) */
    spa_stereo_upmix_config_set_hssr_guard_model(&config, test_model());
    if (spa_mask_model_load_file(&g_mask_model, "models/mask-full-probe-v21a/hssr_mask.bin") == 0)
        spa_stereo_upmix_config_set_hssr_mask_model(&config, &g_mask_model, 55705u);   /* drive 1.7 */
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    assert(probe.resets == 1u && probe.generation == 1u);
    assert(state._runtime._hssr.control_generation == 1u);
    assert(state._runtime._hssr.guard_model == config.hssr_guard_model);
    assert(state._runtime._hssr.mask_model == config.hssr_mask_model);
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
    assert(state._runtime._hssr.guard_model == config.hssr_guard_model);
    assert(state._runtime._hssr.mask_model == config.hssr_mask_model);
    assert(!config.hssr_mask_model ||
           (state._runtime._hssr.mask_drive > 1.69f && state._runtime._hssr.mask_drive < 1.71f));
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    assert(probe.resets == 3u && probe.generation == 3u);
    spa_mask_model_unload(&g_mask_model);
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

/* Front-only bass trim: -3 dB @ 200 Hz shelf on L/R, sub and rear untouched. */
static void test_front_bass_shelf(void)
{
    static struct spa_stereo_upmix state;
    struct spa_stereo_upmix_config config;
    uint64_t flat_low, trim_low, flat_high, trim_high, flat_lfe, trim_lfe;
    double low_db, high_db;

    make_universal_config(&config, 1);
    config.front_bass_db10 = -30;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);

    fill_sine(60.0f, 0);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_a);
    trim_low = channel_energy(g_output_a, SPA_STEREO_UPMIX_CHANNEL_L, 4096);
    trim_lfe = channel_energy(g_output_a, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    config.front_bass_db10 = 0;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    flat_low = channel_energy(g_output_b, SPA_STEREO_UPMIX_CHANNEL_L, 4096);
    flat_lfe = channel_energy(g_output_b, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    low_db = 10.0 * log10((double)trim_low / (double)flat_low);
    assert(low_db < -2.6 && low_db > -3.4);
    assert(trim_lfe == flat_lfe); /* LFE comes from the dry mix */

    fill_sine(3000.0f, 0);
    config.front_bass_db10 = -30;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_a);
    trim_high = channel_energy(g_output_a, SPA_STEREO_UPMIX_CHANNEL_L, 4096);
    config.front_bass_db10 = 0;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    flat_high = channel_energy(g_output_b, SPA_STEREO_UPMIX_CHANNEL_L, 4096);
    high_db = 10.0 * log10((double)trim_high / (double)flat_high);
    assert(high_db > -0.2 && high_db < 0.2);

    config.front_bass_db10 = 5;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.front_bass_db10 = -130;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_ERROR_CONFIG);
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

    /* The assist corner is 60 Hz (LR4): 96 Hz sits 1.6 octaves above it,
     * -16 dB by design; the retired 95-Hz punch branch would pass 96 Hz. */
    fill_sine(96.0f, 0);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    crossover_energy = channel_energy(
        g_output_b, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    assert(crossover_energy * 20u < low_energy);
    assert_cabin_buses_bounded(g_output_b, TEST_FRAMES);

    fill_sine(500.0f, 0);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    high_energy = channel_energy(
        g_output_b, SPA_STEREO_UPMIX_CHANNEL_LFE, 4096);
    assert(high_energy * 1000u < low_energy);
    assert_cabin_buses_bounded(g_output_b, TEST_FRAMES);
}

static void test_corrected_guard_and_transfer_extremes(void)
{
    uint32_t seed = 721u;
    unsigned n, ch;
    int16_t sample[6], before[6];
    /* Each newly recovered route must participate, including B7/B8. */
    const float *m = spa_stereo_upmix_input_matrix();
    assert(fabsf(m[3] - .398107171f) < 1e-7f); /* L -> sub */
    assert(fabsf(m[6] - .707945784f) < 1e-7f); /* L -> B7 */
    assert(fabsf(m[4 * 8 + 6] - 2.818382931f) < 1e-7f);
    for (n = 0; n < 30000; ++n) {
        for (ch = 0; ch < 6; ++ch) {
            seed = seed * 1664525u + 1013904223u;
            before[ch] = sample[ch] = (int16_t)(seed >> 16);
        }
        if (n % 3 == 0) sample[3] = before[3] = 0;
        spa_stereo_upmix_test_guard_s16(sample, n % 2);
        assert_cabin_buses_bounded(sample, 1);
        if (before[3] == 0) assert(sample[3] == 0);
        assert((int32_t)sample[3] * before[3] >= 0);
        assert(abs((int)sample[3]) <= abs((int)before[3]));
    }
    assert(spa_stereo_upmix_test_transfer(0, 16384, 81920, 31392) == -31392);
    assert(spa_stereo_upmix_test_transfer(0, -16384, 81920, 31392) == 31392);
    assert(spa_stereo_upmix_test_transfer(10000, 5000, 0, 31392) == 10000);
    assert(spa_stereo_upmix_test_transfer(32767, -16384, 81920, 32768) == 32767);
    assert(spa_stereo_upmix_test_transfer(-32768, 16384, 81920, 32768) == -32768);
}

static void test_s16_front_retention(void)
{
    static struct spa_stereo_upmix state;
    struct spa_stereo_upmix_config c;
    int16_t pcm[2] = {0, 0}, result[3][6];
    unsigned k, ch;
    make_universal_config(&c, 0);
    assert(c.front_retention_q15 == 0);
    for (k = 0; k < 3; ++k) {
        c.front_retention_q15 = k * 16384;
        assert(spa_stereo_upmix_init(&state, &c) == 0);
        state._runtime._hssr.frames_seen = SPA_HSSR_LATENCY_FRAMES;
        state._runtime._hssr.front_delay[0][0] = 2000;
        state._runtime._hssr.source_ola[0][0] = .01f;
        state._runtime._scene_rear_delay[0][0] = .01f;
        assert(spa_stereo_upmix_process_s16(&state, pcm, 1, result[k], 1) == 0);
    }
    assert(result[0][0] < result[1][0] && result[1][0] < result[2][0]);
    assert(abs(2 * result[1][0] - result[0][0] - result[2][0]) <= 2);
    assert(abs(result[2][0] - (int)(2000.0*c.front_gain_q15/32768 + .5)) <= 1);
    for (ch = 2; ch < 6; ++ch)
        assert(result[0][ch] == result[1][ch] && result[1][ch] == result[2][ch]);
    c.front_retention_q15 = 32769;
    assert(spa_stereo_upmix_init(&state, &c) == SPA_STEREO_UPMIX_ERROR_CONFIG);
}

/* Dolby-probe sequence: steady -> x4 amplitude (+12 dB) -> steady gives
 * 0, 1, beta, beta^2, beta^3; thresholds +3/+6/+9 dB give 0 / 0.5 / 1. */
static void test_bed_attack_detector(void)
{
    const float beta = 0.9637f;
    const double rises_db[3] = { 3.0, 6.0, 9.0 };
    const float expected[3] = { 0.0f, 0.5f, 1.0f };
    float history = 0.0f, weight = 0.0f, w[5];
    unsigned k;

    for (k = 0; k < 20; ++k) spa_hssr_test_attack_step(1.0f, &history, &weight);
    w[0] = spa_hssr_test_attack_step(1.0f, &history, &weight);
    w[1] = spa_hssr_test_attack_step(16.0f, &history, &weight);
    w[2] = spa_hssr_test_attack_step(1.0f, &history, &weight);
    w[3] = spa_hssr_test_attack_step(1.0f, &history, &weight);
    w[4] = spa_hssr_test_attack_step(1.0f, &history, &weight);
    assert(w[0] == 0.0f && w[1] == 1.0f);
    assert(fabsf(w[2] - beta) < 1e-5f);
    assert(fabsf(w[3] - beta * beta) < 1e-5f);
    assert(fabsf(w[4] - beta * beta * beta) < 1e-5f);
    for (k = 0; k < 3; ++k) {
        float rise = (float)pow(10.0, rises_db[k] / 10.0), value;
        unsigned n;

        history = 0.0f; weight = 0.0f;
        for (n = 0; n < 20; ++n) spa_hssr_test_attack_step(1.0f, &history, &weight);
        value = spa_hssr_test_attack_step(rise, &history, &weight);
        assert(fabsf(value - expected[k]) < 0.01f);
    }
}

/* Bed impulse in one bin: 0.79 undelayed, 0.60 after the band's Ls delay,
 * -0.14 cross on Rs after the Ls delay, nothing else; ring survives reset. */
static void test_bed_delay_ring(void)
{
    static struct spa_hssr_dsp dsp;
    static float out_re[2][SPA_HSSR_BINS], out_im[2][SPA_HSSR_BINS];
    const unsigned bins[2] = { 40, 3 };        /* 3750 Hz: L1/R2; 281 Hz: L9/R7 */
    const unsigned delay_l[2] = { 1, 9 };
    unsigned which, hop, bin;

    for (which = 0; which < 2; ++which) {
        spa_hssr_dsp_prepare(&dsp);
        dsp.bed_delays = 1;
        for (hop = 0; hop < 16; ++hop) {
            memset(dsp.bed_re, 0, sizeof(dsp.bed_re));
            memset(dsp.bed_im, 0, sizeof(dsp.bed_im));
            if (hop == 0) dsp.bed_re[0][bins[which]] = 1.0f;
            spa_hssr_test_mix_bed(&dsp, out_re, out_im);
            for (bin = 0; bin < SPA_HSSR_BINS; ++bin) {
                float l = out_re[0][bin], r = out_re[1][bin];
                float el = 0.0f, er = 0.0f;

                if (bin == bins[which] && hop == 0) el = 0.79f;
                if (bin == bins[which] && hop == delay_l[which]) { el = 0.60f; er = -0.14f; }
                assert(fabsf(l - el) < 1e-6f && fabsf(r - er) < 1e-6f);
                assert(out_im[0][bin] == 0.0f && out_im[1][bin] == 0.0f);
            }
        }
    }
}

/* Whole renderer with the experiments on: valid, bounded, deterministic after
 * reset, actually different from the legacy rear, and rejected out of range. */
static void test_bed_experiments_end_to_end(void)
{
    static struct spa_stereo_upmix legacy, experiment;
    struct spa_stereo_upmix_config config;
    uint64_t rear_difference = 0;
    size_t frame;

    fill_stereo(UINT32_C(0x42454421), 0);
    make_universal_config(&config, 0);
    assert(spa_stereo_upmix_init(&legacy, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&legacy, g_input, g_output_a);
    config.bed_delays = 1;
    config.bed_attack = SPA_HSSR_BED_ATTACK_OWNER;
    config.front_retention_q15 = 16384;
    assert(spa_stereo_upmix_init(&experiment, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&experiment, g_input, g_output_b);
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        const int16_t *a = g_output_a + frame * TEST_CHANNELS;
        const int16_t *b = g_output_b + frame * TEST_CHANNELS;
        int64_t d = (int64_t)a[4] - b[4];

        assert(b[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(b[4] >= -(int32_t)SPA_STEREO_UPMIX_REAR_Q15_MAX && b[4] <= (int32_t)SPA_STEREO_UPMIX_REAR_Q15_MAX);
        rear_difference += (uint64_t)(d * d);
    }
    assert(rear_difference > 0);
    assert(spa_stereo_upmix_reset(&experiment) == SPA_STEREO_UPMIX_OK);
    process_blocks(&experiment, g_input, g_overlap);
    assert(memcmp(g_output_b, g_overlap, sizeof(g_output_b)) == 0);
    config.bed_attack = 3;
    assert(spa_stereo_upmix_init(&experiment, &config) == SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.bed_attack = SPA_HSSR_BED_ATTACK_FRONT;
    config.attack_collapse_q15 = 32769;
    assert(spa_stereo_upmix_init(&experiment, &config) == SPA_STEREO_UPMIX_ERROR_CONFIG);
}

/* The retention taper is a frequency gate on the front object share: body
 * bands keep it, treble bands give the object up entirely, the rear is
 * untouched, and hi <= lo restores the flat behaviour.  Band edges from
 * hssr_dsp.c: band 12 starts at 1000 Hz, band 17 at 2500 Hz. */
static void test_retention_hf_taper(void)
{
    static struct spa_stereo_upmix flat, tapered;
    struct spa_stereo_upmix_config config;
    unsigned band;
    size_t frame;
    int front_differs = 0;

    fill_stereo(UINT32_C(0x48465441), 0);
    make_universal_config(&config, 0);
    config.bed_delays = 1;
    config.bed_attack = SPA_HSSR_BED_ATTACK_OWNER;
    config.front_retention_q15 = 16384;
    config.attack_collapse_q15 = 24576;
    config.retention_hf_lo_hz = 0;
    config.retention_hf_hi_hz = 0;
    assert(spa_stereo_upmix_init(&flat, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&flat, g_input, g_output_a);
    config.retention_hf_lo_hz = 1000;
    config.retention_hf_hi_hz = 2500;
    assert(spa_stereo_upmix_init(&tapered, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&tapered, g_input, g_output_b);
    for (band = 0; band < SPA_HSSR_BANDS; ++band) {
        float keep = tapered._runtime._hssr.band_front_keep[band];

        if (band < 12u)
            assert(keep == flat._runtime._hssr.band_front_keep[band]);
        else if (band >= 17u)
            assert(keep == 1.0f);
        assert(keep >= 0.0f && keep <= 1.0f);
    }
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        const int16_t *a = g_output_a + frame * TEST_CHANNELS;
        const int16_t *b = g_output_b + frame * TEST_CHANNELS;

        /* Only the front transfer may change; rear synthesis must not. */
        assert(a[SPA_STEREO_UPMIX_CHANNEL_LS] == b[SPA_STEREO_UPMIX_CHANNEL_LS]);
        assert(a[SPA_STEREO_UPMIX_CHANNEL_RS] == b[SPA_STEREO_UPMIX_CHANNEL_RS]);
        if (a[SPA_STEREO_UPMIX_CHANNEL_L] != b[SPA_STEREO_UPMIX_CHANNEL_L] ||
            a[SPA_STEREO_UPMIX_CHANNEL_R] != b[SPA_STEREO_UPMIX_CHANNEL_R])
            front_differs = 1;
    }
    assert(front_differs);
    config.retention_hf_hi_hz = 24001;
    assert(spa_stereo_upmix_init(&tapered, &config) == SPA_STEREO_UPMIX_ERROR_CONFIG);
}

/* SCENE smoke: no model and no blob is spatial only; the latency and size
 * contract; C and LFE zero; reset() restores the exact fresh state (tables
 * kept) and the block size changes nothing; the blob setter's validation; a
 * zero-weight blob drives the mask cadence (one control per 512 frames, rows
 * from the third boundary on, prot 1 everywhere so the duck engages) and feeds
 * the LFE confidences. */
static uint32_t g_scene_boundaries, g_scene_rows;
static uint64_t g_scene_last_pushed;

static void scene_observer(uint64_t pushed, const float *elig, const float *prot)
{
    assert(pushed % 512u == 256u);
    assert(g_scene_boundaries == 0 || pushed == g_scene_last_pushed + 512u);
    assert((elig == NULL) == (prot == NULL));
    g_scene_last_pushed = pushed;
    g_scene_boundaries++;
    if (elig) {
        g_scene_rows++;
        assert(elig[0] == 0.0f && prot[0] == 1.0f && elig[SMASK_BINS - 1] == 0.0f);
    }
}

static void test_scene_backend_smoke(void)
{
    static struct spa_stereo_upmix state;
    static struct scene_mask_weights blob;
    struct spa_stereo_upmix_config config;
    size_t frame, count;
    unsigned i;
    int nonzero = 0;

    fill_stereo(UINT32_C(0x5343454e), 0);
    spa_stereo_upmix_default_config(&config);
    config.backend = SPA_STEREO_UPMIX_SCENE;
    assert(config.scene_mask_blob == NULL);
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    assert(state.config.front_gain_q15 == 32768u);
    assert(!state._runtime._scene_mask_on);
    process_blocks(&state, g_input, g_output_a);
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        const int16_t *out = g_output_a + frame * TEST_CHANNELS;

        assert(out[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        assert(out[SPA_STEREO_UPMIX_CHANNEL_LFE] == 0);
        if (frame < SPA_SCENE_LATENCY_FRAMES)
            assert(!out[0] && !out[1] && !out[4] && !out[5]);
        else if (out[0] || out[1] || out[4] || out[5])
            nonzero = 1;
    }
    assert(nonzero);
    assert(spa_stereo_upmix_scene_track_change(&state) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_output_b);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    for (frame = 0; frame < TEST_FRAMES; frame += count) {
        count = TEST_FRAMES - frame < 100 ? TEST_FRAMES - frame : 100;
        assert(spa_stereo_upmix_process_s16(&state, g_input + frame * 2, count,
                                            g_output_b + frame * TEST_CHANNELS, count) ==
               SPA_STEREO_UPMIX_OK);
    }
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) == 0);

    /* the setter: the exact size, magic and dimensions or nothing */
    memset(&blob, 0, sizeof(blob));
    spa_stereo_upmix_config_set_scene_mask_blob(&config, &blob, sizeof(blob));
    assert(config.scene_mask_blob == NULL);
    memcpy(blob.magic, SMASK_MAGIC, sizeof(blob.magic));
    blob.ch = SMASK_CH;
    blob.hidden = SMASK_HID;
    blob.n_out = SMASK_OUT;
    for (i = 0; i < SMASK_FEAT; ++i) blob.feat_std[i] = 1.0f;
    spa_stereo_upmix_config_set_scene_mask_blob(&config, &blob, sizeof(blob) - 1u);
    assert(config.scene_mask_blob == NULL);
    spa_stereo_upmix_config_set_scene_mask_blob(&config, &blob, sizeof(blob));
    assert(config.scene_mask_blob == &blob);
    blob.ch = SMASK_CH + 1u;
    spa_stereo_upmix_config_set_scene_mask_blob(&config, &blob, sizeof(blob));
    assert(config.scene_mask_blob == NULL);
    blob.ch = SMASK_CH;
    spa_stereo_upmix_config_set_scene_mask_blob(&config, &blob, sizeof(blob));
    assert(config.scene_mask_blob == &blob);

    /* zero weights: every posterior sigmoid(0) = 0.5, no eligible output ->
     * elig 0 / prot 1, so the duck engages and the LFE steering sees 0.5 */
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    assert(state._runtime._scene_mask_on);
    spa_stereo_upmix_test_scene_mask_observer = scene_observer;
    process_blocks(&state, g_input, g_output_b);
    assert(g_scene_boundaries == (TEST_FRAMES - 256u) / 512u + 1u);
    assert(g_scene_rows == g_scene_boundaries - 2u);
    assert(memcmp(g_output_a, g_output_b, sizeof(g_output_a)) != 0);
    assert(state._runtime._scene_lfe_bass > 0.4f && state._runtime._scene_lfe_bass <= 0.5f);
    assert(state._runtime._scene_lfe_drum > 0.4f && state._runtime._scene_lfe_drum <= 0.5f);
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        const int16_t *out = g_output_b + frame * TEST_CHANNELS;

        assert(out[SPA_STEREO_UPMIX_CHANNEL_C] == 0 && out[SPA_STEREO_UPMIX_CHANNEL_LFE] == 0);
        if (frame < SPA_SCENE_LATENCY_FRAMES)
            assert(!out[0] && !out[1] && !out[4] && !out[5]);
    }
    /* reset with the net: the same cadence and the same render again */
    g_scene_boundaries = g_scene_rows = 0;
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state, g_input, g_overlap);
    spa_stereo_upmix_test_scene_mask_observer = NULL;
    assert(g_scene_rows == g_scene_boundaries - 2u);
    assert(memcmp(g_overlap, g_output_b, sizeof(g_output_b)) == 0);

    config.backend = SPA_STEREO_UPMIX_OFF;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_scene_track_change(&state) == SPA_STEREO_UPMIX_ERROR_UNSUPPORTED);
    assert(spa_stereo_upmix_scene_track_change(NULL) == SPA_STEREO_UPMIX_ERROR_ARGUMENT);
}

/* SCENE with the mask net on the worker (scene_mask_worker.h), the same
 * zero-weight blob (rows elig 0 / prot 1).  Three runs:
 *  1. driven by hand (no thread, every job stepped after each block): every
 *     row is applied at the boundary after its own, never earlier, from the
 *     fourth boundary on (the audio-thread net: the third), no job refused,
 *     and the two renders agree until the first row lands;
 *  2. a thread whose net sleeps 1 ms, a driver paced at half realtime: no job
 *     is lost (busy 0, every job completed), rows one or two boundaries late
 *     (two when a 768-frame block holds two boundaries), in order, each once;
 *     reset() re-primes the net under a new generation;
 *  3. a net slower than the driver (5 ms per control, the driver unpaced):
 *     jobs are refused instead of waited for (busy > 0), the rows that come
 *     are still in order and late, nothing is lost from the accounting. */
static struct spa_stereo_upmix g_worker_state;    /* the observer reads its sequence numbers */
static struct spa_stereo_upmix g_sync_state;
static struct spa_scene_mask_worker g_worker;
static uint32_t g_worker_rows, g_worker_boundaries, g_worker_last_sequence;
static uint32_t g_worker_first_row_boundary, g_worker_max_lateness;
static unsigned g_worker_sleep_ms;

static void worker_observer(uint64_t pushed, const float *elig, const float *prot)
{
    const struct spa_stereo_upmix_runtime *rt = &g_worker_state._runtime;
    uint32_t lateness;

    assert(pushed % 512u == 256u);
    assert((elig == NULL) == (prot == NULL));
    g_worker_boundaries++;
    if (!elig) return;
    /* the row of job s is applied at a boundary strictly after the one that submitted s */
    assert(rt->_scene_mask_row.sequence < rt->_scene_mask_sequence);
    assert(rt->_scene_mask_row.sequence > g_worker_last_sequence);   /* in order, each once */
    lateness = rt->_scene_mask_sequence - rt->_scene_mask_row.sequence;
    if (lateness > g_worker_max_lateness) g_worker_max_lateness = lateness;
    g_worker_last_sequence = rt->_scene_mask_row.sequence;
    if (!g_worker_rows) g_worker_first_row_boundary = g_worker_boundaries;
    g_worker_rows++;
    assert(elig[0] == 0.0f && prot[0] == 1.0f && elig[SMASK_BINS - 1] == 0.0f);
}

static int slow_control(struct scene_mask *net, const float *in512, float *elig, float *prot)
{
    struct timespec pause;

    pause.tv_sec = 0;
    pause.tv_nsec = (long)g_worker_sleep_ms * 1000000L;
    if (g_worker_sleep_ms) (void)nanosleep(&pause, NULL);
    return scene_mask_control(net, in512, elig, prot);
}

static void worker_counters_reset(void)
{
    g_worker_rows = g_worker_boundaries = g_worker_last_sequence = 0;
    g_worker_first_row_boundary = g_worker_max_lateness = 0;
}

/* block = 512 (one boundary per block) or 768 (the realtime period, one or
 * two); pace_ms > 0 sleeps between blocks */
static void worker_drive(int16_t *output, size_t block, unsigned pace_ms, int step_by_hand)
{
    size_t frame, count;

    for (frame = 0; frame < TEST_FRAMES; frame += count) {
        count = TEST_FRAMES - frame < block ? TEST_FRAMES - frame : block;
        assert(spa_stereo_upmix_process_s16(&g_worker_state, g_input + frame * 2, count,
                                            output + frame * TEST_CHANNELS, count) ==
               SPA_STEREO_UPMIX_OK);
        if (step_by_hand)
            while (spa_scene_mask_worker_step(&g_worker)) {}
        if (pace_ms) {
            struct timespec pause;

            pause.tv_sec = 0;
            pause.tv_nsec = (long)pace_ms * 1000000L;
            (void)nanosleep(&pause, NULL);
        }
    }
}

static void worker_wait_completed(struct spa_scene_mask_worker_stats *stats)
{
    unsigned attempt;

    for (attempt = 0; attempt < 2000u; ++attempt) {
        struct timespec pause;

        spa_scene_mask_worker_get_stats(&g_worker, stats);
        if (stats->completed == stats->submitted) return;
        pause.tv_sec = 0;
        pause.tv_nsec = 1000000L;
        (void)nanosleep(&pause, NULL);
    }
}

static void test_scene_mask_worker(void)
{
    static struct scene_mask_weights blob;
    struct spa_stereo_upmix_config config;
    struct spa_scene_mask_worker_stats stats;
    size_t frame, first_diff = TEST_FRAMES;
    unsigned i;

    fill_stereo(UINT32_C(0x574f524b), 0);
    memset(&blob, 0, sizeof(blob));
    memcpy(blob.magic, SMASK_MAGIC, sizeof(blob.magic));
    blob.ch = SMASK_CH;
    blob.hidden = SMASK_HID;
    blob.n_out = SMASK_OUT;
    for (i = 0; i < SMASK_FEAT; ++i) blob.feat_std[i] = 1.0f;
    spa_stereo_upmix_default_config(&config);
    config.backend = SPA_STEREO_UPMIX_SCENE;
    spa_stereo_upmix_config_set_scene_mask_blob(&config, &blob, sizeof(blob));
    assert(config.scene_mask_blob == &blob);

    /* the audio-thread reference */
    assert(spa_stereo_upmix_init(&g_sync_state, &config) == SPA_STEREO_UPMIX_OK);
    process_blocks(&g_sync_state, g_input, g_output_a);

    /* 1. by hand */
    assert(spa_scene_mask_worker_start(NULL, &blob, 1u, 0) == -1);
    assert(spa_scene_mask_worker_start(&g_worker, &blob, 1u, SPA_SCENE_MASK_WORKER_NO_THREAD) == 0);
    spa_stereo_upmix_config_set_scene_mask_provider(&config, spa_scene_mask_worker_provider(&g_worker));
    assert(config.scene_mask_provider != NULL);
    assert(spa_stereo_upmix_init(&g_worker_state, &config) == SPA_STEREO_UPMIX_OK);
    assert(g_worker_state._runtime._scene_mask_on);
    spa_stereo_upmix_test_scene_mask_observer = worker_observer;
    worker_counters_reset();
    worker_drive(g_output_b, 512, 0, 1);
    spa_scene_mask_worker_get_stats(&g_worker, &stats);
    assert(g_worker_boundaries == (TEST_FRAMES - 256u) / 512u + 1u);
    assert(stats.submitted == g_worker_boundaries && stats.completed == g_worker_boundaries);
    assert(stats.busy == 0 && stats.stale_discarded == 0 && stats.superseded == 0);
    assert(stats.resets == 1);
    assert(g_worker_rows == g_worker_boundaries - 3u);      /* sync: boundaries - 2 */
    assert(g_worker_first_row_boundary == 4u);              /* sync: 3 */
    assert(g_worker_max_lateness == 1u);
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        const int16_t *a = g_output_a + frame * TEST_CHANNELS;
        const int16_t *b = g_output_b + frame * TEST_CHANNELS;

        assert(b[SPA_STEREO_UPMIX_CHANNEL_C] == 0 && b[SPA_STEREO_UPMIX_CHANNEL_LFE] == 0);
        if (first_diff == TEST_FRAMES && memcmp(a, b, TEST_CHANNELS * sizeof(*a)) != 0)
            first_diff = frame;
    }
    /* the audio-thread row lands at 1280 pushed frames, 1024 of latency later
     * in the output: identical before that, different after (the duck starts
     * one control later on the worker) */
    assert(first_diff >= 1280u - SPA_SCENE_LATENCY_FRAMES && first_diff < TEST_FRAMES);
    spa_scene_mask_worker_stop(&g_worker);
    assert(spa_scene_mask_worker_provider(&g_worker) == NULL);

    /* 2. a slow net that keeps up */
    g_worker_sleep_ms = 1;
    spa_scene_mask_worker_test_control = slow_control;
    assert(spa_scene_mask_worker_start(&g_worker, &blob, 1u, 0) == 0);
    assert(spa_stereo_upmix_init(&g_worker_state, &config) == SPA_STEREO_UPMIX_OK);
    worker_counters_reset();
    worker_drive(g_output_b, 768, 8, 0);
    worker_wait_completed(&stats);
    assert(stats.submitted == g_worker_boundaries);
    assert(stats.completed == stats.submitted);             /* no job lost */
    assert(stats.busy == 0 && stats.stale_discarded == 0);
    assert(g_worker_rows >= 1u && g_worker_max_lateness >= 1u && g_worker_max_lateness <= 2u);
    /* every completed row past priming was either installed, superseded by a
     * newer one at the same boundary, or is still in the mailbox (at most two) */
    assert(g_worker_rows + stats.superseded <= stats.completed - 2u);
    assert(g_worker_rows + stats.superseded + 2u >= stats.completed - 2u);
    /* reset(): a new generation, the net re-primes, rows come again */
    assert(spa_stereo_upmix_reset(&g_worker_state) == SPA_STEREO_UPMIX_OK);
    worker_counters_reset();
    worker_drive(g_output_b, 768, 8, 0);
    worker_wait_completed(&stats);
    assert(stats.resets >= 2u);
    assert(stats.busy == 0);
    assert(g_worker_rows >= 1u && g_worker_first_row_boundary >= 4u);
    spa_scene_mask_worker_stop(&g_worker);

    /* 3. slower than the driver */
    g_worker_sleep_ms = 5;
    assert(spa_scene_mask_worker_start(&g_worker, &blob, 1u, 0) == 0);
    assert(spa_stereo_upmix_init(&g_worker_state, &config) == SPA_STEREO_UPMIX_OK);
    worker_counters_reset();
    worker_drive(g_output_b, 768, 0, 0);
    spa_scene_mask_worker_stop(&g_worker);                  /* joins: the in-flight job finishes */
    spa_scene_mask_worker_get_stats(&g_worker, &stats);
    assert(stats.busy > 0);                                 /* refused, never waited for */
    assert(stats.submitted + stats.busy == g_worker_boundaries);
    assert(stats.submitted >= stats.completed && stats.submitted - stats.completed <= 2u);
    spa_scene_mask_worker_test_control = NULL;
    spa_stereo_upmix_test_scene_mask_observer = NULL;
    g_worker_sleep_ms = 0;
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
    test_front_bass_shelf();
    test_corrected_guard_and_transfer_extremes();
    test_s16_front_retention();
    test_bed_attack_detector();
    test_bed_delay_ring();
    test_bed_experiments_end_to_end();
    test_retention_hf_taper();
    test_scene_backend_smoke();
    test_scene_mask_worker();
    puts("stereo_upmix_test: OK (Universal + rear shelf + LFE V2 + SCENE smoke + mask worker)");
    return 0;
}
