/*
 * Fixed-memory benchmark for the shipping stereo upmix backends.
 *
 * Host and live-QNX runs measure monotonic wall time.  QNX runs under the
 * companion QEMU -icount runner explicitly switch to ClockCycles(), calibrate
 * it against exact ARM instruction loops, and report instruction estimates.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#if defined(__QNX__)
#include <sys/neutrino.h>
#endif

#include "stereo_upmix.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    BENCH_FRAMES = SPA_STEREO_UPMIX_MAX_FRAMES,
    BENCH_DEFAULT_BLOCKS = 128,
    BENCH_DEFAULT_WARMUP_BLOCKS = 8,
    BENCH_MAX_BLOCKS = 512
};

/* Keep every large object out of the small dio_manager receiver stack.  The
 * benchmark, like the hook, performs no allocation in the render loop. */
static struct spa_stereo_upmix g_state;
static int16_t g_pcm2[BENCH_FRAMES * SPA_STEREO_UPMIX_INPUT_CHANNELS];
static int16_t g_pcm6[BENCH_FRAMES * SPA_STEREO_UPMIX_OUTPUT_CHANNELS];
static uint64_t g_elapsed[BENCH_MAX_BLOCKS];
static uint64_t g_sorted[BENCH_MAX_BLOCKS];
static volatile uint64_t g_checksum_sink;

struct bench_stats {
    uint64_t min;
    uint64_t total;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t max;
};

struct icount_calibration {
    double ticks_per_instruction;
    double exact_loop_agreement;
    uint64_t spin2_ticks;
    uint64_t spin4_ticks;
};

#if defined(__QNX__)
static int g_use_qemu_icount_clock;

static uint64_t bench_now(void)
{
    struct timespec value;

    if (g_use_qemu_icount_clock) return (uint64_t)ClockCycles();
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
           (uint64_t)value.tv_nsec;
}

static const char *bench_clock_name(void)
{
    return g_use_qemu_icount_clock ? "ClockCycles ticks" :
                                     "CLOCK_MONOTONIC nanoseconds";
}
#elif defined(__APPLE__)
static uint64_t bench_now(void)
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

static const char *bench_clock_name(void)
{
    return "CLOCK_UPTIME_RAW nanoseconds";
}
#else
static uint64_t bench_now(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
           (uint64_t)value.tv_nsec;
}

static const char *bench_clock_name(void)
{
    return "CLOCK_MONOTONIC nanoseconds";
}
#endif

static uint64_t clock_read_overhead(void)
{
    uint64_t best = UINT64_MAX;
    unsigned iteration;

    for (iteration = 0; iteration < 1024; iteration++) {
        uint64_t before = bench_now();
        uint64_t after = bench_now();
        uint64_t elapsed = after - before;

        if (elapsed < best) best = elapsed;
    }
    return best;
}

/* On ARM these loops retire exactly two and four instructions per iteration.
 * Their difference avoids assuming the fixed call/clock overhead. */
#if defined(__QNX__) && defined(__arm__)
static void spin2(uint32_t iterations)
{
    __asm__ volatile(
        "1:\n\t"
        "subs %0, %0, #1\n\t"
        "bne 1b\n\t"
        : "+r"(iterations) : : "cc");
}

static void spin4(uint32_t iterations)
{
    __asm__ volatile(
        "1:\n\t"
        "nop\n\t"
        "nop\n\t"
        "subs %0, %0, #1\n\t"
        "bne 1b\n\t"
        : "+r"(iterations) : : "cc");
}
#endif

static int calibrate_icount(struct icount_calibration *calibration)
{
#if defined(__QNX__) && defined(__arm__)
    const uint32_t iterations = UINT32_C(2000000);
    uint64_t before;
    uint64_t after;
    double from_spin2;

    before = bench_now();
    spin2(iterations);
    after = bench_now();
    calibration->spin2_ticks = after - before;

    before = bench_now();
    spin4(iterations);
    after = bench_now();
    calibration->spin4_ticks = after - before;

    calibration->ticks_per_instruction =
        ((double)calibration->spin4_ticks -
         (double)calibration->spin2_ticks) /
        (2.0 * (double)iterations);
    if (calibration->ticks_per_instruction <= 0.0) return 0;

    from_spin2 = (double)calibration->spin2_ticks /
                 (2.0 * (double)iterations);
    calibration->exact_loop_agreement =
        from_spin2 / calibration->ticks_per_instruction;
    return calibration->exact_loop_agreement > 0.98 &&
           calibration->exact_loop_agreement < 1.02;
#else
    (void)calibration;
    return 0;
#endif
}

static uint32_t next_random(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

/* Deterministic correlated-plus-difference stereo excites both the direct and
 * ambient estimators.  Generation is outside the timed region. */
static void make_input(uint32_t *random_state)
{
    size_t frame;

    for (frame = 0; frame < BENCH_FRAMES; frame++) {
        int32_t common =
            (int32_t)((next_random(random_state) >> 16) & UINT32_C(0xffff)) -
            INT32_C(32768);
        int32_t difference =
            (int32_t)((next_random(random_state) >> 16) & UINT32_C(0xffff)) -
            INT32_C(32768);

        g_pcm2[frame * 2] = (int16_t)((3 * common + difference) / 5);
        g_pcm2[frame * 2 + 1] =
            (int16_t)((3 * common - difference) / 5);
    }
}

static uint64_t checksum_output(uint64_t checksum)
{
    size_t sample;

    /* FNV-1a over a canonical little-endian sample stream. */
    for (sample = 0;
         sample < BENCH_FRAMES * SPA_STEREO_UPMIX_OUTPUT_CHANNELS;
         sample++) {
        uint16_t value = (uint16_t)g_pcm6[sample];

        checksum ^= (uint8_t)value;
        checksum *= UINT64_C(1099511628211);
        checksum ^= (uint8_t)(value >> 8);
        checksum *= UINT64_C(1099511628211);
    }
    return checksum;
}

static void sort_elapsed(size_t count)
{
    size_t index;

    memcpy(g_sorted, g_elapsed, count * sizeof(g_sorted[0]));
    for (index = 1; index < count; index++) {
        uint64_t value = g_sorted[index];
        size_t position = index;

        while (position != 0 && g_sorted[position - 1] > value) {
            g_sorted[position] = g_sorted[position - 1];
            position--;
        }
        g_sorted[position] = value;
    }
}

static uint64_t percentile(size_t count, unsigned percent)
{
    /* Nearest-rank percentile: ceil(percent * count / 100) - 1. */
    size_t rank = ((size_t)percent * count + 99u) / 100u;

    if (rank == 0) rank = 1;
    if (rank > count) rank = count;
    return g_sorted[rank - 1];
}

static struct bench_stats summarize(size_t count)
{
    struct bench_stats statistics;
    size_t index;

    memset(&statistics, 0, sizeof(statistics));
    sort_elapsed(count);
    statistics.min = g_sorted[0];
    statistics.max = g_sorted[count - 1];
    statistics.p50 = percentile(count, 50);
    statistics.p95 = percentile(count, 95);
    statistics.p99 = percentile(count, 99);
    for (index = 0; index < count; index++)
        statistics.total += g_elapsed[index];
    return statistics;
}

static int parse_count(const char *text, size_t maximum, size_t *result)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 ||
        value > maximum)
        return 0;
    *result = (size_t)value;
    return 1;
}

static int parse_backend(const char *text,
                         enum spa_stereo_upmix_backend *backend)
{
    if (strcmp(text, "ambient-hq") == 0) {
        *backend = SPA_STEREO_UPMIX_AMBIENT_HQ;
        return 1;
    }
    if (strcmp(text, "hsr-td") == 0) {
        *backend = SPA_STEREO_UPMIX_HSR_TD;
        return 1;
    }
    if (strcmp(text, "hsr-td-active") == 0) {
        *backend = SPA_STEREO_UPMIX_HSR_TD_ACTIVE;
        return 1;
    }
    if (strcmp(text, "music-stage") == 0) {
        *backend = SPA_STEREO_UPMIX_MUSIC_STAGE;
        return 1;
    }
    if (strcmp(text, "hssr-neon") == 0 || strcmp(text, "hssr") == 0) {
        *backend = SPA_STEREO_UPMIX_HSSR_NEON;
        return 1;
    }
    if (strcmp(text, "hssr-active") == 0 ||
        strcmp(text, "hssr-neon-active") == 0) {
        *backend = SPA_STEREO_UPMIX_HSSR_ACTIVE;
        return 1;
    }
    if (strcmp(text, "hssr-object") == 0 ||
        strcmp(text, "hssr-neon-object") == 0) {
        *backend = SPA_STEREO_UPMIX_HSSR_OBJECT;
        return 1;
    }
    if (strcmp(text, "hssr-object-v2") == 0 ||
        strcmp(text, "hssr-neon-object-v2") == 0) {
        *backend = SPA_STEREO_UPMIX_HSSR_OBJECT_V2;
        return 1;
    }
    if (strcmp(text, "hssr-object-focus") == 0 ||
        strcmp(text, "hssr-object-v3") == 0) {
        *backend = SPA_STEREO_UPMIX_HSSR_OBJECT_FOCUS;
        return 1;
    }
    return 0;
}

static const char *backend_name(enum spa_stereo_upmix_backend backend)
{
    if (backend == SPA_STEREO_UPMIX_HSR_TD) return "hsr-td";
    if (backend == SPA_STEREO_UPMIX_HSR_TD_ACTIVE) return "hsr-td-active";
    if (backend == SPA_STEREO_UPMIX_MUSIC_STAGE) return "music-stage";
    if (backend == SPA_STEREO_UPMIX_HSSR_NEON) return "hssr-neon";
    if (backend == SPA_STEREO_UPMIX_HSSR_ACTIVE) return "hssr-active";
    if (backend == SPA_STEREO_UPMIX_HSSR_OBJECT) return "hssr-object";
    if (backend == SPA_STEREO_UPMIX_HSSR_OBJECT_V2) return "hssr-object-v2";
    if (backend == SPA_STEREO_UPMIX_HSSR_OBJECT_FOCUS)
        return "hssr-object-focus";
    return "ambient-hq";
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--backend ambient-hq|hsr-td|hsr-td-active|music-stage|hssr-neon|hssr-active|hssr-object|hssr-object-v2|hssr-object-focus] "
            "[--model HSSR.bin] "
            "[--blocks 1..%u] [--warmup 1..%u] "
            "[--qemu-icount]\n",
            program, (unsigned)BENCH_MAX_BLOCKS,
            (unsigned)BENCH_MAX_BLOCKS);
}

static void print_wall_stats(const struct bench_stats *statistics,
                             size_t blocks)
{
    const double mean = (double)statistics->total / (double)blocks;
    const double budget_ns =
        (double)BENCH_FRAMES * 1000000000.0 /
        (double)SPA_STEREO_UPMIX_SAMPLE_RATE;

    printf("metric=host_wall_time unit=ns n=%lu "
           "min=%llu mean=%.0f p50=%llu p95=%llu p99=%llu max=%llu\n",
           (unsigned long)blocks,
           (unsigned long long)statistics->min, mean,
           (unsigned long long)statistics->p50,
           (unsigned long long)statistics->p95,
           (unsigned long long)statistics->p99,
           (unsigned long long)statistics->max);
    printf("realtime_budget_ns=%.0f mean_budget_fraction=%.6f "
           "max_budget_fraction=%.6f\n",
           budget_ns, mean / budget_ns,
           (double)statistics->max / budget_ns);
}

static void print_icount_stats(const struct bench_stats *statistics,
                               size_t blocks,
                               const struct icount_calibration *calibration)
{
    const double scale = 1.0 / calibration->ticks_per_instruction;
    const double mean_ticks =
        (double)statistics->total / (double)blocks;

    printf("icount_calibration spin2_ticks=%llu spin4_ticks=%llu "
           "ticks_per_instruction=%.9f exact_loop_agreement=%.6f\n",
           (unsigned long long)calibration->spin2_ticks,
           (unsigned long long)calibration->spin4_ticks,
           calibration->ticks_per_instruction,
           calibration->exact_loop_agreement);
    printf("metric=qemu_icount unit=estimated_instructions n=%lu "
           "min=%.0f mean=%.0f p50=%.0f p95=%.0f p99=%.0f max=%.0f\n",
           (unsigned long)blocks,
           (double)statistics->min * scale, mean_ticks * scale,
           (double)statistics->p50 * scale,
           (double)statistics->p95 * scale,
           (double)statistics->p99 * scale,
           (double)statistics->max * scale);
    printf("note=qemu_icount_is_target_instruction_retirement_not_wall_time_"
           "or_krait_cycles\n");
}

int main(int argc, char **argv)
{
    struct spa_stereo_upmix_config config;
    struct spa_hssr_model hssr_model;
    struct bench_stats statistics;
    struct icount_calibration calibration;
    size_t blocks = BENCH_DEFAULT_BLOCKS;
    size_t warmup_blocks = BENCH_DEFAULT_WARMUP_BLOCKS;
    enum spa_stereo_upmix_backend backend = SPA_STEREO_UPMIX_AMBIENT_HQ;
    uint32_t random_state = UINT32_C(0x4d484932);
    uint64_t checksum = UINT64_C(14695981039346656037);
    int qemu_icount = 0;
    const char *model_path = NULL;
    size_t index;
    int argument;

    memset(&calibration, 0, sizeof(calibration));
    memset(&hssr_model, 0, sizeof(hssr_model));
    for (argument = 1; argument < argc; argument++) {
        if (strcmp(argv[argument], "--backend") == 0 &&
            argument + 1 < argc) {
            if (!parse_backend(argv[++argument], &backend)) {
                print_usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[argument], "--blocks") == 0 &&
            argument + 1 < argc) {
            if (!parse_count(argv[++argument], BENCH_MAX_BLOCKS, &blocks)) {
                print_usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[argument], "--warmup") == 0 &&
                   argument + 1 < argc) {
            if (!parse_count(argv[++argument], BENCH_MAX_BLOCKS,
                             &warmup_blocks)) {
                print_usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[argument], "--qemu-icount") == 0) {
            qemu_icount = 1;
        } else if (strcmp(argv[argument], "--model") == 0 &&
                   argument + 1 < argc) {
            model_path = argv[++argument];
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

#if defined(__QNX__)
    g_use_qemu_icount_clock = qemu_icount;
#endif

#if !defined(__QNX__)
    if (qemu_icount) {
        fprintf(stderr,
                "error: --qemu-icount is valid only in the QNX guest\n");
        return 2;
    }
#endif

    spa_stereo_upmix_default_config(&config);
    config.backend = backend;
    spa_stereo_upmix_config_set_lfe(&config, 1);
    if (backend == SPA_STEREO_UPMIX_HSSR_NEON ||
        backend == SPA_STEREO_UPMIX_HSSR_ACTIVE ||
        backend == SPA_STEREO_UPMIX_HSSR_OBJECT ||
        backend == SPA_STEREO_UPMIX_HSSR_OBJECT_V2 ||
        backend == SPA_STEREO_UPMIX_HSSR_OBJECT_FOCUS) {
        if (!model_path) {
            fprintf(stderr, "error: selected HSSR backend requires --model HSSR.bin\n");
            return 2;
        }
        if (spa_hssr_model_load_file(&hssr_model, model_path) !=
                SPA_HSSR_OK) {
            fprintf(stderr, "error: failed to load HSSR model %s\n",
                    model_path);
            return 1;
        }
        spa_stereo_upmix_config_set_hssr_model(&config, &hssr_model);
    }
    if (spa_stereo_upmix_init(&g_state, &config) != SPA_STEREO_UPMIX_OK) {
        fprintf(stderr, "error: %s init failed\n", backend_name(backend));
        spa_hssr_model_unload(&hssr_model);
        return 1;
    }

    printf("stereo_upmix_bench backend=%s frames_per_call=%u "
           "warmup=%lu blocks=%lu\n",
           backend_name(backend), (unsigned)BENCH_FRAMES,
           (unsigned long)warmup_blocks,
           (unsigned long)blocks);
    printf("sizeof_state=%lu sizeof_runtime=%lu static_pcm_bytes=%lu\n",
           (unsigned long)sizeof(g_state),
           (unsigned long)sizeof(g_state._runtime),
           (unsigned long)(sizeof(g_pcm2) + sizeof(g_pcm6)));
    printf("clock=%s clock_read_overhead=%llu\n", bench_clock_name(),
           (unsigned long long)clock_read_overhead());

    if (qemu_icount && !calibrate_icount(&calibration)) {
        fprintf(stderr,
                "error: ClockCycles did not validate as QEMU -icount; "
                "refusing to label ticks as instructions\n");
        spa_hssr_model_unload(&hssr_model);
        return 1;
    }

    for (index = 0; index < warmup_blocks; index++) {
        make_input(&random_state);
        if (spa_stereo_upmix_process_s16(
                &g_state, g_pcm2, BENCH_FRAMES, g_pcm6,
                BENCH_FRAMES) != SPA_STEREO_UPMIX_OK) {
            fprintf(stderr, "error: %s warmup failed\n",
                    backend_name(backend));
            spa_hssr_model_unload(&hssr_model);
            return 1;
        }
    }

    for (index = 0; index < blocks; index++) {
        uint64_t before;
        uint64_t after;
        int status;

        make_input(&random_state);
        before = bench_now();
        status = spa_stereo_upmix_process_s16(
            &g_state, g_pcm2, BENCH_FRAMES, g_pcm6, BENCH_FRAMES);
        after = bench_now();
        if (status != SPA_STEREO_UPMIX_OK) {
            fprintf(stderr, "error: %s process failed at block %lu "
                    "with status %d\n", backend_name(backend),
                    (unsigned long)index, status);
            spa_hssr_model_unload(&hssr_model);
            return 1;
        }
        g_elapsed[index] = after - before;
        checksum = checksum_output(checksum);
    }

    statistics = summarize(blocks);
    if (qemu_icount)
        print_icount_stats(&statistics, blocks, &calibration);
    else
        print_wall_stats(&statistics, blocks);

    g_checksum_sink = checksum;
    printf("checksum_fnv1a64_le=0x%016llx\n",
           (unsigned long long)g_checksum_sink);
    spa_hssr_model_unload(&hssr_model);
    return 0;
}
