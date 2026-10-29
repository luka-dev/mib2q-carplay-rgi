/*
 * scene_engine_bench.c — the cost of the C scene upmix engine (hook/audio/scene_engine.c), measured the
 * way this directory measures the Universal upmix: host wall time, or under scene_qnx_qemu_bench.sh
 * (QEMU -icount) ClockCycles() calibrated against two exact ARM instruction loops.
 *
 * Pushes N seconds (default 60) of a deterministic synthetic stereo signal (two panned decaying plucks
 * over a noise floor) in 768-frame blocks (one QSA period) through scene_engine_push(). --mask installs
 * a synthetic mask row per 512-sample control period, so the semantic stage and the duck run. Only the
 * push (and the set_mask the hook would pay) is timed; signal and mask generation are outside. Prints
 * frames processed and a checksum of the S16-quantised output. Measurement only; no allocation anywhere.
 * --mask --handoff routes the row through the scene mask worker's mailboxes instead (scene_mask_worker.h,
 * no thread: the worker is stepped between pushes with the net stubbed to the same synthetic row), so the
 * difference against --mask is the audio thread's cost of the hand-off: the 512-frame submit copy, the
 * receive copy and the atomics. Rows then arrive one control late, as on the unit, so the checksum differs.
 *
 * host:  cc -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -Ihook/audio \
 *            tools/stereo-upmix-render/bench/scene_engine_bench.c hook/audio/scene_engine.c \
 *            hook/audio/scene_engine_math.c hook/audio/scene_clusters.c \
 *            hook/audio/scene_mask.c hook/audio/scene_mask_worker.c -lm -lpthread -o build/scene_port/scene_engine_bench
 * QNX:   tools/stereo-upmix-render/bench/scene_build_qnx.sh, run by scene_qnx_qemu_bench.sh
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#if defined(__QNX__)
#include <sys/neutrino.h>
#endif
#include "scene_engine.h"   /* before <math.h>: the QNX 6.5 math.h sets GCC's _SIZE_T guard without the typedef */
#include "scene_mask_worker.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { BLOCK = 768, HOPS_PER_BLOCK = BLOCK / SCENE_HOP, MAX_BLOCKS = 4096, MASK_BINS = 257 };

static struct scene_state g_state;                       /* 265 KB, static like the hook's */
static float g_in[BLOCK * 2];
static float g_out[(BLOCK + SCENE_HOP) * 6];
static float g_elig[MASK_BINS], g_prot[MASK_BINS];
static uint64_t g_elapsed[MAX_BLOCKS];

/* ---- --handoff: the worker without its thread and without its net (stubbed to the synthetic row) */
static struct spa_scene_mask_worker g_worker;
static struct scene_mask_weights g_blob;                 /* magic and dimensions only: the stub never reads weights */
static struct spa_scene_mask_row g_row;

static int stub_control(struct scene_mask *net, const float *in512, float *elig257, float *prot257)
{
    (void)in512; (void)elig257; (void)prot257;
    memcpy(net->elig, g_elig, sizeof g_elig);
    memcpy(net->prot, g_prot, sizeof g_prot);
    return 1;
}

static const struct spa_scene_mask_provider *handoff_start(void)
{
    memset(&g_blob, 0, sizeof g_blob);
    memcpy(g_blob.magic, SMASK_MAGIC, sizeof g_blob.magic);
    g_blob.ch = SMASK_CH; g_blob.hidden = SMASK_HID; g_blob.n_out = SMASK_OUT;
    g_blob.feat_std[0] = g_blob.feat_std[1] = g_blob.feat_std[2] = g_blob.feat_std[3] = 1.0f;
    spa_scene_mask_worker_test_control = stub_control;
    if (spa_scene_mask_worker_start(&g_worker, &g_blob, 1u, SPA_SCENE_MASK_WORKER_NO_THREAD) != 0) return NULL;
    return spa_scene_mask_worker_provider(&g_worker);
}

/* ---- clock: ClockCycles under the QEMU -icount runner, monotonic time otherwise (the retired stereo_upmix_bench.c did the same) */
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

/* Exact 2- and 4-instruction loops: their difference gives ticks per instruction, their ratio proves it. */
#if defined(__QNX__) && defined(__arm__)
static void spin2(uint32_t n) { __asm__ volatile("1:\n\tsubs %0, %0, #1\n\tbne 1b\n\t" : "+r"(n) : : "cc"); }
static void spin4(uint32_t n) { __asm__ volatile("1:\n\tnop\n\tnop\n\tsubs %0, %0, #1\n\tbne 1b\n\t" : "+r"(n) : : "cc"); }
#endif

static double calibrate_icount(void)        /* ticks per instruction, 0 when ClockCycles is not -icount */
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

/* ---- signal: two decaying plucks (220 Hz panned left, 330 Hz panned right, retriggered every 0.5 s and
 * 0.7 s) over a -41 dBFS noise floor: two places for the clusters, an ambient bed for the rear. */
static uint32_t g_rng = 0x5ce7e001u;
static float g_phase[2], g_env[2];
static int g_since[2];

static float noise(void)
{
    g_rng = g_rng * 1664525u + 1013904223u;
    return (float)(g_rng >> 8) * (1.0f / 16777216.0f) - 0.5f;
}

static void make_block(void)
{
    static const float hz[2] = { 220.0f, 330.0f }, gl[2] = { 0.8f, 0.3f }, gr[2] = { 0.3f, 0.8f };
    static const int period[2] = { 24000, 33600 };
    int i, v;
    for (i = 0; i < BLOCK; i++) {
        float l = 0.03f * noise(), r = 0.03f * noise();
        for (v = 0; v < 2; v++) {
            float s;
            if (g_since[v] == 0) g_env[v] = 0.4f;
            g_since[v] = g_since[v] + 1 == period[v] ? 0 : g_since[v] + 1;
            g_phase[v] += hz[v] * (6.28318530f / 48000.0f);
            if (g_phase[v] > 6.28318530f) g_phase[v] -= 6.28318530f;
            s = g_env[v] * (sinf(g_phase[v]) + 0.3f * sinf(2.0f * g_phase[v]) + 0.1f * sinf(3.0f * g_phase[v]));
            g_env[v] *= 0.99993f;                              /* -29 dB per second */
            l += gl[v] * s; r += gr[v] * s;
        }
        g_in[2 * i] = l; g_in[2 * i + 1] = r;
    }
}

/* ---- mask: a synthetic control row on the 257-bin (512-point) grid. elig = an instrument bump over
 * 0.9-5.6 kHz whose height swings from row to row (exercises the attack/release smoothers); prot = bass
 * core below 375 Hz plus the complement of elig (the vocal/drum share). Both in 0..1. */
static void make_mask(int row)
{
    const float swing = 0.6f + 0.4f * sinf(0.37f * (float)row);
    int m;
    for (m = 0; m < MASK_BINS; m++) {
        float x = ((float)m - 35.0f) / 25.0f;
        float bump = x > -1.0f && x < 1.0f ? 0.5f + 0.5f * cosf(3.14159265f * x) : 0.0f;
        g_elig[m] = 0.85f * swing * bump;
        g_prot[m] = m < 4 ? 0.95f : 0.9f * (1.0f - g_elig[m]);
    }
}

/* ---- output accounting: FNV-1a over the S16-quantised sample stream (what the hook would emit), and the
 * front / rear RMS so a run shows the rear is non-trivial and the mask changes it. */
static uint64_t g_sum = UINT64_C(14695981039346656037);
static double g_front2, g_rear2;

static void account(size_t frames)
{
    size_t i;
    for (i = 0; i < frames * 6; i++) {
        float x = g_out[i];
        int v = (int)floorf(x * 32767.0f + 0.5f);
        uint16_t u;
        if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
        u = (uint16_t)(int16_t)v;
        g_sum ^= (uint8_t)u;        g_sum *= UINT64_C(1099511628211);
        g_sum ^= (uint8_t)(u >> 8); g_sum *= UINT64_C(1099511628211);
        if (i % 6 < 2) g_front2 += (double)x * (double)x;
        else if (i % 6 >= 4) g_rear2 += (double)x * (double)x;
    }
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

static double pct(unsigned n, unsigned p)   /* nearest rank on the sorted g_elapsed */
{
    unsigned rank = (p * n + 99u) / 100u;
    return (double)g_elapsed[(rank ? rank : 1u) - 1u];
}

static double rms_db(double sum2, double count)
{
    return count > 0.0 && sum2 > 0.0 ? 10.0 * log10(sum2 / count) : -200.0;
}

int main(int argc, char **argv)
{
    struct scene_config cfg;
    unsigned seconds = 60, blocks, k, mask = 0, icount = 0, nopush = 0, handoff = 0;
    size_t frames_in = 0, frames_out = 0;
    double scale = 1.0, total = 0.0, mean;
    int row = -1, i;
    const struct spa_scene_mask_provider *provider = NULL;
    uint32_t sequence = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mask")) mask = 1;
        else if (!strcmp(argv[i], "--handoff")) handoff = 1;      /* the row through the worker mailboxes (--mask only) */
        else if (!strcmp(argv[i], "--qemu-icount")) icount = 1;
        else if (!strcmp(argv[i], "--no-push")) nopush = 1;      /* everything but the engine: the L1D plugin counts the whole guest run, this is what to subtract */
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = (unsigned)atoi(argv[++i]);
        else seconds = 0;
    }
    blocks = seconds * SCENE_SR / BLOCK;
    if (seconds < 1 || blocks > MAX_BLOCKS || (handoff && !mask)) {
        fprintf(stderr, "usage: %s [--seconds 1..65] [--mask [--handoff]] [--no-push] [--qemu-icount]\n", argv[0]);
        return 2;
    }
    if (handoff && !(provider = handoff_start())) { fprintf(stderr, "error: scene mask worker start failed\n"); return 1; }
#if defined(__QNX__)
    g_icount = (int)icount;
#else
    if (icount) { fprintf(stderr, "error: --qemu-icount is valid only in the QNX guest\n"); return 2; }
#endif
    scene_config_defaults(&cfg);
    if (scene_engine_init(&g_state, &cfg) != 0) { fprintf(stderr, "error: scene_engine_init failed\n"); return 1; }

    printf("scene_engine_bench mask=%s handoff=%s push=%s frames_per_push=%d hops_per_push=%d seconds=%u blocks=%u sizeof_state=%lu\n",
           mask ? "on" : "off", handoff ? "on" : "off", nopush ? "off" : "on", BLOCK, HOPS_PER_BLOCK, seconds, blocks,
           (unsigned long)sizeof g_state);
    if (icount) {
        double tpi = calibrate_icount();
        if (tpi <= 0.0) {
            fprintf(stderr, "error: ClockCycles did not validate as QEMU -icount; refusing to label ticks as instructions\n");
            return 1;
        }
        scale = 1.0 / tpi;
    }

    for (k = 0; k < blocks; k++) {
        /* row of the push's first frame (push k computes frames 3k..3k+2, row r covers frames 2r, 2r+1): the
         * push's later frames may run one row behind the Python's inst_mask indexing, which costs nothing */
        int r = (int)(frames_in / SCENE_MASK_HOP), install = mask && r != row;
        uint64_t t0, t1;
        size_t n;
        if (install) { make_mask(r); row = r; }
        make_block();
        t0 = now_ticks();
        if (nopush) n = k ? BLOCK : 0;                              /* what push returns: the accounting walks the same addresses */
        else {
            if (install && handoff) {
                /* what the hook's boundary does: the newest finished row in, the 512 frames out */
                if (provider->receive(provider->context, 1u, &g_row))
                    scene_engine_set_mask(&g_state, g_row.elig, g_row.prot, MASK_BINS);
                (void)provider->submit(provider->context, 1u, ++sequence, g_in);
            } else if (install) {
                scene_engine_set_mask(&g_state, g_elig, g_prot, MASK_BINS);
            }
            n = scene_engine_push(&g_state, g_in, BLOCK, g_out, sizeof g_out / (6 * sizeof g_out[0]));
        }
        t1 = now_ticks();
        g_elapsed[k] = t1 - t0;
        total += (double)(t1 - t0);
        if (handoff) while (spa_scene_mask_worker_step(&g_worker)) {}   /* the worker's turn, outside the timing */
        frames_in += BLOCK;
        frames_out += n;
        account(n);
    }

    qsort(g_elapsed, blocks, sizeof g_elapsed[0], cmp_u64);
    mean = total / (double)blocks;
    printf("metric=%s unit=%s per=push_of_%d_frames n=%u min=%.0f mean=%.0f p50=%.0f p95=%.0f p99=%.0f max=%.0f\n",
           icount ? "qemu_icount" : "host_wall_time", icount ? "estimated_instructions" : "ns", BLOCK, blocks,
           (double)g_elapsed[0] * scale, mean * scale, pct(blocks, 50) * scale, pct(blocks, 95) * scale,
           pct(blocks, 99) * scale, (double)g_elapsed[blocks - 1] * scale);
    printf("per_hop_mean=%.0f per_hop_max=%.0f per_second_of_audio=%.0f\n",
           mean * scale / HOPS_PER_BLOCK, (double)g_elapsed[blocks - 1] * scale / HOPS_PER_BLOCK,
           mean * scale * (double)SCENE_SR / BLOCK);
    if (icount)
        printf("note=qemu_icount_is_target_instruction_retirement_not_wall_time_or_krait_cycles\n");
    else
        printf("audio_period_ns=%.0f mean_period_fraction=%.6f max_period_fraction=%.6f\n",
               (double)BLOCK * 1e9 / SCENE_SR, mean / ((double)BLOCK * 1e9 / SCENE_SR),
               (double)g_elapsed[blocks - 1] / ((double)BLOCK * 1e9 / SCENE_SR));
    printf("frames_in=%lu frames_out=%lu checksum_fnv1a64_s16=0x%016llx front_rms_db=%.2f rear_rms_db=%.2f\n",
           (unsigned long)frames_in, (unsigned long)frames_out, (unsigned long long)g_sum,
           rms_db(g_front2, 2.0 * (double)frames_out), rms_db(g_rear2, 2.0 * (double)frames_out));
    return 0;
}
