/* Host-side deterministic tests for the stereo upmix: the SCENE PCM2 -> PCM6 renderer,
 * the OFF bypass, the LFE V2 assist and the cabin guard. */

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
static uint32_t test_prng(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
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

static void make_scene_config(struct spa_stereo_upmix_config *config, int lfe)
{
    spa_stereo_upmix_default_config(config);
    config->backend = SPA_STEREO_UPMIX_SCENE;
    spa_stereo_upmix_config_set_lfe(config, lfe);
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
    assert(config.lfe_gain_q15 == SPA_STEREO_UPMIX_LFE_Q15_DEFAULT);
    assert(config.lfe_enabled == 0);
    assert(config.backend == SPA_STEREO_UPMIX_OFF);
    assert(config.scene_mask_blob == NULL);
    assert(config.scene_mask_provider == NULL);
    assert(config.scene.n_fft == SCENE_NFFT && config.scene.hop == SCENE_HOP);
    assert(spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_OFF));
    assert(spa_stereo_upmix_backend_supported(SPA_STEREO_UPMIX_SCENE));
    assert(!spa_stereo_upmix_backend_supported(
               (enum spa_stereo_upmix_backend)2));

    memset(&state, 0, sizeof(state));
    memset(output, 0x5a, sizeof(output));
    memcpy(untouched, output, sizeof(output));
    assert(spa_stereo_upmix_process_s16(&state, input, 1, output, 1) ==
           SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED);
    assert(memcmp(output, untouched, sizeof(output)) == 0);

    /* an out-of-range scene knob is a config error, not a clamp */
    config.backend = SPA_STEREO_UPMIX_SCENE;
    config.scene.alpha = 2.0f;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.backend = (enum spa_stereo_upmix_backend)2;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_UNSUPPORTED);
    make_scene_config(&config, 1);
    config.sample_rate_hz = 44100;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);
    /* The static centre-bus worst case is no longer a config error: the
     * per-sample LFE cabin limit is the rail guard (LFE assist +9.5 dB). */
    make_scene_config(&config, 1);
    config.front_gain_q15 = SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    make_scene_config(&config, 1);
    config.lfe_gain_q15 = SPA_STEREO_UPMIX_LFE_Q15_MAX + 1u;
    assert(spa_stereo_upmix_init(&state, &config) ==
           SPA_STEREO_UPMIX_ERROR_CONFIG);

    make_scene_config(&config, 1);
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

    make_scene_config(&config, 1);

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

static void test_corrected_guard_extremes(void)
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
}

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

/* MMI levels: OFF is the bypass, input SPA_SCENE_LATENCY_FRAMES late; to/from OFF
 * crossfades and between levels the tuning glides, both over SPA_UPMIX_LEVEL_FADE_MS;
 * init and reset land on the level. */
static void test_scene_levels(void)
{
    enum { N = 48000, BLOCK = 768 };
    static struct spa_stereo_upmix state;
    static int16_t in[N * 2], out[N * TEST_CHANNELS];
    struct spa_stereo_upmix_config config;
    const size_t fade = (size_t)SPA_UPMIX_LEVEL_FADE_MS * 48u;
    uint32_t seed = UINT32_C(0x4c564c53);
    size_t i, f;
    double rear_early = 0.0, rear_late = 0.0;

    for (i = 0; i < N; ++i) {                     /* quiet noise + a wide offset: guard never binds */
        in[2 * i] = (int16_t)((int32_t)(test_prng(&seed) % 4001u) - 2000);
        in[2 * i + 1] = (int16_t)((int32_t)(test_prng(&seed) % 4001u) - 2000) / 3;
    }
    spa_stereo_upmix_default_config(&config);
    config.backend = SPA_STEREO_UPMIX_SCENE;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    spa_stereo_upmix_set_level(&state, 99);                       /* ignored */
    assert(state._level_req == SPA_UPMIX_LEVEL_NORMAL);
    spa_stereo_upmix_set_level(&state, SPA_UPMIX_LEVEL_OFF);
    assert(spa_stereo_upmix_process_s16(&state, in, 1, out, 1) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_reset(&state) == SPA_STEREO_UPMIX_OK);
    assert(state._wet == 0.0f);                                   /* a new stream starts at OFF */
    for (f = 0; f < N; f += BLOCK)
        assert(spa_stereo_upmix_process_s16(&state, in + f * 2, BLOCK < N - f ? BLOCK : N - f,
                                            out + f * TEST_CHANNELS, N) == SPA_STEREO_UPMIX_OK);
    for (f = 0; f < N; ++f) {
        const int16_t *o = out + f * TEST_CHANNELS;
        const int16_t want_l = f < SPA_SCENE_LATENCY_FRAMES ? 0 : in[2 * (f - SPA_SCENE_LATENCY_FRAMES)];
        const int16_t want_r = f < SPA_SCENE_LATENCY_FRAMES ? 0 : in[2 * (f - SPA_SCENE_LATENCY_FRAMES) + 1];
        assert(o[0] == want_l && o[1] == want_r && !o[2] && !o[3] && !o[4] && !o[5]);
    }

    /* OFF -> INTENSE mid-stream: tuning switched, rears fade in, fully wet after the fade */
    spa_stereo_upmix_set_level(&state, SPA_UPMIX_LEVEL_INTENSE);
    for (f = 0; f < N; f += BLOCK)
        assert(spa_stereo_upmix_process_s16(&state, in + f * 2, BLOCK < N - f ? BLOCK : N - f,
                                            out + f * TEST_CHANNELS, N) == SPA_STEREO_UPMIX_OK);
    assert(state._runtime._scene.cfg.alpha == 1.0f && state._runtime._scene.cfg.side_lp_hz == 12000.0f);
    assert(state._wet == 1.0f);
    for (f = 0; f < fade / 4; ++f) rear_early += fabs((double)out[f * TEST_CHANNELS + 4]);
    for (f = fade; f < fade + fade / 4; ++f) rear_late += fabs((double)out[f * TEST_CHANNELS + 4]);
    assert(rear_late > 4.0 * rear_early);                          /* eased in, not stepped */

    /* a state initialised at OFF starts dry, no fade from the scene */
    config.level = SPA_UPMIX_LEVEL_OFF;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    assert(state._wet == 0.0f && state._level_applied == SPA_UPMIX_LEVEL_OFF);
    config.level = SPA_UPMIX_LEVEL_COUNT;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_ERROR_CONFIG);
    config.level = SPA_UPMIX_LEVEL_INTENSE;
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    assert(state._wet == 1.0f && state._runtime._scene.cfg.alpha == 1.0f);
    config.level = SPA_UPMIX_LEVEL_NORMAL;

    /* NORMAL -> INTENSE glides (one block in: between), then lands on INTENSE */
    assert(spa_stereo_upmix_init(&state, &config) == SPA_STEREO_UPMIX_OK);
    spa_stereo_upmix_set_level(&state, SPA_UPMIX_LEVEL_INTENSE);
    assert(spa_stereo_upmix_process_s16(&state, in, BLOCK, out, N) == SPA_STEREO_UPMIX_OK);
    assert(state._runtime._scene.cfg.alpha > config.scene.alpha && state._runtime._scene.cfg.alpha < 1.0f);
    assert(state._runtime._scene.cfg.side_lp_hz > 7000.0f && state._runtime._scene.cfg.side_lp_hz < 12000.0f);
    for (f = 0; f < N; f += BLOCK)
        assert(spa_stereo_upmix_process_s16(&state, in + f * 2, BLOCK < N - f ? BLOCK : N - f,
                                            out + f * TEST_CHANNELS, N) == SPA_STEREO_UPMIX_OK);
    assert(state._runtime._scene.cfg.alpha == 1.0f && state._runtime._scene.cfg.side_lp_hz == 12000.0f);

    /* NORMAL restores the configured scene */
    spa_stereo_upmix_set_level(&state, SPA_UPMIX_LEVEL_NORMAL);
    for (f = 0; f < N; f += BLOCK)
        assert(spa_stereo_upmix_process_s16(&state, in + f * 2, BLOCK < N - f ? BLOCK : N - f,
                                            out + f * TEST_CHANNELS, N) == SPA_STEREO_UPMIX_OK);
    assert(state._runtime._scene.cfg.alpha == config.scene.alpha &&
           state._runtime._scene.cfg.side_lp_hz == config.scene.side_lp_hz &&
           state._runtime._scene.cfg.duck_depth == config.scene.duck_depth);
}

static void test_scene_vehicle_refinements(void)
{
    static struct spa_stereo_upmix state_a, state_b;
    struct spa_stereo_upmix_config cfg_a, cfg_b;
    float elig[257], prot[257];
    uint64_t rear_lo_a, rear_lo_b, rear_sub_bleed;
    double bus4_no_bc = 0.0, bus4_bc = 0.0;
    size_t frame;
    int i;

    /* 1. Setter validation and NORMAL <-> INTENSE glide for front_keep_db + wide_pair */
    spa_stereo_upmix_default_config(&cfg_a);
    cfg_a.backend = SPA_STEREO_UPMIX_SCENE;
    spa_stereo_upmix_config_set_scene_front_keep_db(&cfg_a, 1.5f);
    spa_stereo_upmix_config_set_scene_wide_pair(&cfg_a, 0.35f);
    spa_stereo_upmix_config_set_scene_inst_gamma_lo(&cfg_a, 1.5f);
    spa_stereo_upmix_config_set_scene_bleed_cancel(&cfg_a, 0.15f);
    assert(spa_stereo_upmix_init(&state_a, &cfg_a) == SPA_STEREO_UPMIX_OK);
    assert(fabsf(state_a._runtime._scene.cfg.front_keep_db - 1.5f) < 1e-6f);
    assert(state_a._runtime._scene.cfg.wide_pair == 1);
    assert(fabsf(state_a._runtime._scene.cfg.wide_pair_alpha - 0.35f) < 1e-6f);
    assert(fabsf(state_a._runtime._scene.cfg.inst_gamma_lo - 1.5f) < 1e-6f);
    assert(fabsf(state_a._runtime._scene.cfg.bleed_cancel - 0.15f) < 1e-6f);

    fill_stereo(UINT32_C(0x56454831), 0);
    spa_stereo_upmix_set_level(&state_a, SPA_UPMIX_LEVEL_INTENSE);
    process_blocks(&state_a, g_input, g_output_a);
    /* 8192 frames (~170 ms) is halfway through the 300 ms glide */
    assert(state_a._runtime._scene.cfg.front_keep_db > 1.5f &&
           state_a._runtime._scene.cfg.front_keep_db < 2.8f);
    assert(state_a._runtime._scene.cfg.wide_pair_alpha > 0.35f &&
           state_a._runtime._scene.cfg.wide_pair_alpha < 0.55f);
    process_blocks(&state_a, g_input, g_output_a);
    assert(fabsf(state_a._runtime._scene.cfg.front_keep_db - 2.8f) < 1e-5f);
    assert(fabsf(state_a._runtime._scene.cfg.wide_pair_alpha - 0.55f) < 1e-5f);

    /* 2. Midband front_anchor: heavy 50 Hz sub-bass must not blind midrange protection */
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        double sub50 = 14000.0 * sin(6.283185307179586 * 50.0 * (double)frame / 48000.0);
        double mid1k = 4000.0 * sin(6.283185307179586 * 1000.0 * (double)frame / 48000.0);
        g_input[frame * 2]     = (int16_t)lrint(sub50 + mid1k);
        g_input[frame * 2 + 1] = (int16_t)lrint(sub50); /* 1 kHz hard-panned left */
    }
    for (i = 0; i < 257; ++i) { elig[i] = 0.9f; prot[i] = 0.0f; }
    spa_stereo_upmix_default_config(&cfg_a);
    cfg_a.backend = SPA_STEREO_UPMIX_SCENE;
    spa_stereo_upmix_config_set_scene_front_keep_db(&cfg_a, 1.5f);
    assert(spa_stereo_upmix_init(&state_a, &cfg_a) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_scene_set_mask(&state_a, elig, prot, 257) == SPA_STEREO_UPMIX_OK);
    for (i = 0; i < 4; ++i) process_blocks(&state_a, g_input, g_output_a);
    /* Anchor must engage (< 0.50) despite the loud 50 Hz sub-bass */
    assert(state_a._runtime._scene.anchor < 0.50f);

    /* 3. inst_gamma_lo < 700 Hz sharpens partial eligibility at 350 Hz */
    fill_sine(350.0f, 0);
    for (i = 0; i < 257; ++i) { elig[i] = 0.45f; prot[i] = 0.0f; }
    spa_stereo_upmix_default_config(&cfg_a);
    cfg_a.backend = SPA_STEREO_UPMIX_SCENE;
    cfg_b = cfg_a;
    spa_stereo_upmix_config_set_scene_inst_gamma_lo(&cfg_b, 1.5f);
    assert(spa_stereo_upmix_init(&state_a, &cfg_a) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_init(&state_b, &cfg_b) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_scene_set_mask(&state_a, elig, prot, 257) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_scene_set_mask(&state_b, elig, prot, 257) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state_a, g_input, g_output_a);
    process_blocks(&state_b, g_input, g_output_b);
    rear_lo_a = channel_energy(g_output_a, SPA_STEREO_UPMIX_CHANNEL_LS, 4096);
    rear_lo_b = channel_energy(g_output_b, SPA_STEREO_UPMIX_CHANNEL_LS, 4096);
    assert(rear_lo_b < (rear_lo_a * 3u) / 4u);

    /* 4. bleed_cancel (0.15) reduces J525 Bus 4 centered 1 kHz bleed without injecting 50 Hz into Ls/Rs */
    fill_sine(1000.0f, 0);
    spa_stereo_upmix_default_config(&cfg_a);
    cfg_a.backend = SPA_STEREO_UPMIX_SCENE;
    cfg_b = cfg_a;
    spa_stereo_upmix_config_set_scene_bleed_cancel(&cfg_b, 0.15f);
    assert(spa_stereo_upmix_init(&state_a, &cfg_a) == SPA_STEREO_UPMIX_OK);
    assert(spa_stereo_upmix_init(&state_b, &cfg_b) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state_a, g_input, g_output_a);
    process_blocks(&state_b, g_input, g_output_b);
    for (frame = 4096; frame < TEST_FRAMES; ++frame) {
        const int16_t *sa = g_output_a + frame * TEST_CHANNELS;
        const int16_t *sb = g_output_b + frame * TEST_CHANNELS;
        double b4a = 0.707945784 * sa[0] + 1.995262315 * sa[4];
        double b4b = 0.707945784 * sb[0] + 1.995262315 * sb[4];
        bus4_no_bc += b4a * b4a;
        bus4_bc += b4b * b4b;
    }
    assert(bus4_bc < bus4_no_bc * 0.45);
    fill_sine(50.0f, 0);
    assert(spa_stereo_upmix_init(&state_b, &cfg_b) == SPA_STEREO_UPMIX_OK);
    process_blocks(&state_b, g_input, g_output_b);
    rear_sub_bleed = channel_energy(g_output_b, SPA_STEREO_UPMIX_CHANNEL_LS, 4096);
    assert(rear_sub_bleed * 100000u < channel_energy(g_output_b, SPA_STEREO_UPMIX_CHANNEL_L, 4096));
}

int main(void)
{
    test_default_validation_and_fail_closed();
    test_off_is_exact_and_overlap_safe();
    test_lfe_v2_phase_frequency_and_cabin_bounds();
    test_corrected_guard_extremes();
    test_scene_backend_smoke();
    test_scene_mask_worker();
    test_scene_levels();
    test_scene_vehicle_refinements();
    puts("stereo_upmix_test: OK (validation, OFF bypass, LFE V2 + cabin guard, SCENE smoke, mask worker, levels, vehicle refinements)");
    return 0;
}
