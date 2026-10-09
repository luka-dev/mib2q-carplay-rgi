/*
 * scene_mask_bench.c — the cost of the fine mask net runtime (hook/audio/scene_mask.c): scene_mask_control()
 * per 512-sample control on a deterministic synthetic stereo signal (the scene_engine_bench signal), timed
 * the same way (host wall time, or ClockCycles calibrated against two exact ARM instruction loops under
 * scene_qnx_qemu_bench.sh --bench mask). Loads the SMSKNET1 weight blob (export_masknet_weights.py).
 * Prints controls processed and a checksum of the S16-quantised eligibility rows. No allocation.
 *
 * host:  cc -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -Ihook/audio \
 *            tools/stereo-upmix-render/bench/scene_mask_bench.c hook/audio/scene_mask.c -lm -o build/scene_port/scene_mask_bench
 *        build/scene_port/scene_mask_bench --weights build/scene_port/masknet_c.bin
 * QNX:   tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --bench mask --weights build/scene_port/masknet_c.bin
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#if defined(__QNX__)
#include <sys/neutrino.h>
#endif
#include "scene_mask.h"   /* before <math.h>: the QNX 6.5 math.h sets GCC's _SIZE_T guard without the typedef */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { MAX_CONTROLS = 8192 };

static struct scene_mask g_mask;                          /* ~170 KB, static like the hook's */
static struct scene_mask_weights g_weights;
static float g_in[SMASK_HOP * 2], g_elig[SMASK_BINS], g_prot[SMASK_BINS];
static uint64_t g_elapsed[MAX_CONTROLS];

#if defined(__QNX__)
static int g_icount;
static uint64_t now_ticks(void)
{
    struct timespec ts;
    if (g_icount) return (uint64_t)ClockCycles();
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
#elif defined(__APPLE__)
static uint64_t now_ticks(void) { return clock_gettime_nsec_np(CLOCK_UPTIME_RAW); }
#else
static uint64_t now_ticks(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
#endif

#if defined(__QNX__) && defined(__arm__)
static void spin2(uint32_t n) { __asm__ volatile("1:\n\tsubs %0, %0, #1\n\tbne 1b\n\t" : "+r"(n) : : "cc"); }
static void spin4(uint32_t n) { __asm__ volatile("1:\n\tnop\n\tnop\n\tsubs %0, %0, #1\n\tbne 1b\n\t" : "+r"(n) : : "cc"); }
#endif

static double calibrate_icount(void)
{
#if defined(__QNX__) && defined(__arm__)
    const uint32_t n = 2000000u;
    uint64_t a, b, c;
    double tpi, agreement;
    a = now_ticks(); spin2(n); b = now_ticks(); spin4(n); c = now_ticks();
    tpi = ((double)(c - b) - (double)(b - a)) / (2.0 * (double)n);
    if (tpi <= 0.0) return 0.0;
    agreement = (double)(b - a) / (2.0 * (double)n) / tpi;
    printf("icount_calibration spin2_ticks=%llu spin4_ticks=%llu ticks_per_instruction=%.9f exact_loop_agreement=%.6f\n",
           (unsigned long long)(b - a), (unsigned long long)(c - b), tpi, agreement);
    return agreement > 0.98 && agreement < 1.02 ? tpi : 0.0;
#else
    return 0.0;
#endif
}

/* the scene_engine_bench signal: two decaying plucks over a -41 dBFS noise floor */
static uint32_t g_rng = 0x5ce7e001u;
static float g_phase[2], g_env[2];
static int g_since[2];

static float noise(void)
{
    g_rng = g_rng * 1664525u + 1013904223u;
    return (float)(g_rng >> 8) * (1.0f / 16777216.0f) - 0.5f;
}

static void make_control(void)
{
    static const float hz[2] = { 220.0f, 330.0f }, gl[2] = { 0.8f, 0.3f }, gr[2] = { 0.3f, 0.8f };
    static const int period[2] = { 24000, 33600 };
    int i, v;
    for (i = 0; i < SMASK_HOP; i++) {
        float l = 0.03f * noise(), r = 0.03f * noise();
        for (v = 0; v < 2; v++) {
            float s;
            if (g_since[v] == 0) g_env[v] = 0.4f;
            g_since[v] = g_since[v] + 1 == period[v] ? 0 : g_since[v] + 1;
            g_phase[v] += hz[v] * (6.28318530f / 48000.0f);
            if (g_phase[v] > 6.28318530f) g_phase[v] -= 6.28318530f;
            s = g_env[v] * (sinf(g_phase[v]) + 0.3f * sinf(2.0f * g_phase[v]) + 0.1f * sinf(3.0f * g_phase[v]));
            g_env[v] *= 0.99993f;
            l += gl[v] * s; r += gr[v] * s;
        }
        g_in[2 * i] = l; g_in[2 * i + 1] = r;
    }
}

static uint64_t g_sum = UINT64_C(14695981039346656037);
static double g_elig2;

static void account(void)
{
    int i;
    for (i = 0; i < SMASK_BINS; i++) {
        int v = (int)floorf(g_elig[i] * 32767.0f + 0.5f);
        uint16_t u;
        if (v > 32767) v = 32767; else if (v < 0) v = 0;
        u = (uint16_t)v;
        g_sum ^= (uint8_t)u;        g_sum *= UINT64_C(1099511628211);
        g_sum ^= (uint8_t)(u >> 8); g_sum *= UINT64_C(1099511628211);
        g_elig2 += (double)g_elig[i] * (double)g_elig[i];
    }
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

static double pct(unsigned n, unsigned p)
{
    unsigned rank = (p * n + 99u) / 100u;
    return (double)g_elapsed[(rank ? rank : 1u) - 1u];
}

int main(int argc, char **argv)
{
    const char *weights = NULL;
    unsigned seconds = 60, controls, k, icount = 0, done = 0;
    double scale = 1.0, total = 0.0, mean;
    FILE *f;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--qemu-icount")) icount = 1;
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--weights") && i + 1 < argc) weights = argv[++i];
        else seconds = 0;
    }
    controls = seconds * 48000u / SMASK_HOP;
    if (!weights || seconds < 1 || controls > MAX_CONTROLS) {
        fprintf(stderr, "usage: %s --weights SMSKNET1.bin [--seconds 1..87] [--qemu-icount]\n", argv[0]);
        return 2;
    }
#if defined(__QNX__)
    g_icount = (int)icount;
#else
    if (icount) { fprintf(stderr, "error: --qemu-icount is valid only in the QNX guest\n"); return 2; }
#endif
    f = fopen(weights, "rb");
    if (!f || fread(&g_weights, 1, sizeof g_weights, f) != sizeof g_weights) {
        fprintf(stderr, "error: cannot read %lu weight bytes from %s\n", (unsigned long)sizeof g_weights, weights);
        return 1;
    }
    fclose(f);
    if (scene_mask_init(&g_mask, &g_weights) != 0) { fprintf(stderr, "error: scene_mask_init rejected the blob\n"); return 1; }

    printf("scene_mask_bench frames_per_control=%d seconds=%u controls=%u sizeof_mask=%lu\n",
           SMASK_HOP, seconds, controls, (unsigned long)sizeof g_mask);
    if (icount) {
        double tpi = calibrate_icount();
        if (tpi <= 0.0) {
            fprintf(stderr, "error: ClockCycles did not validate as QEMU -icount; refusing to label ticks as instructions\n");
            return 1;
        }
        scale = 1.0 / tpi;
    }
    for (k = 0; k < controls; k++) {
        uint64_t t0, t1;
        int got;
        make_control();
        t0 = now_ticks();
        got = scene_mask_control(&g_mask, g_in, g_elig, g_prot);
        t1 = now_ticks();
        g_elapsed[k] = t1 - t0;
        total += (double)(t1 - t0);
        if (got) { done++; account(); }
    }
    qsort(g_elapsed, controls, sizeof g_elapsed[0], cmp_u64);
    mean = total / (double)controls;
    printf("metric=%s unit=%s per=control_of_%d_frames n=%u min=%.0f mean=%.0f p50=%.0f p95=%.0f p99=%.0f max=%.0f\n",
           icount ? "qemu_icount" : "host_wall_time", icount ? "estimated_instructions" : "ns", SMASK_HOP, controls,
           (double)g_elapsed[0] * scale, mean * scale, pct(controls, 50) * scale, pct(controls, 95) * scale,
           pct(controls, 99) * scale, (double)g_elapsed[controls - 1] * scale);
    printf("per_hop_mean=%.0f per_second_of_audio=%.0f\n", mean * scale / 2.0, mean * scale * 48000.0 / SMASK_HOP);
    if (icount)
        printf("note=qemu_icount_is_target_instruction_retirement_not_wall_time_or_krait_cycles\n");
    printf("controls_in=%u controls_out=%u checksum_fnv1a64_s16=0x%016llx elig_rms=%.5f\n",
           controls, done, (unsigned long long)g_sum, done ? sqrt(g_elig2 / ((double)done * SMASK_BINS)) : 0.0);
    return 0;
}
