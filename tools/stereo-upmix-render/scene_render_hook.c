/*
 * scene_render_hook.c — the SCENE backend of hook/audio/stereo_upmix.c on a
 * stereo WAV, through spa_stereo_upmix_process_s16() in 768-frame calls (one
 * QSA period), so the hook path can be compared with scene_upmix.py
 * (port/test_hook_render.py).  48 kHz / 16-bit / stereo in, 6-ch 16-bit out
 * (L R C LFE Ls Rs); the output lags the input by SPA_SCENE_LATENCY_FRAMES
 * (1024) and the last 1024 engine frames are never flushed, exactly as in
 * the hook.
 *
 * Build (host, no FMA like the unit; the run_tests.sh line plus the scene files):
 *   cc -std=gnu99 -O2 -ffp-contract=off -Wall -Wextra -Werror -Ihook -Ihook/audio \
 *      tools/stereo-upmix-render/scene_render_hook.c hook/audio/stereo_upmix.c \
 *      hook/audio/soundcheck.c \
 *      hook/audio/scene_engine_math.c hook/audio/scene_clusters.c hook/audio/scene_engine.c \
 *      hook/audio/scene_mask.c \
 *      -lm -lpthread -o build/scene_port/scene_render_hook
 *
 * Usage: scene_render_hook [--mode scene|off] [--lfe off|assist] [--block 1..1024]
 *        [--alpha A] [--inst-alpha A] [--inst-gamma G] [--duck-depth D]
 *        [--rear-delay-ms MS] [--sigma-min S] [--side-lp-hz HZ] [--bleed-cancel K] [--front-keep-db DB]
 *        [--mask-blob masknet.bin] [--mask-rows rows.f32]
 *        in.wav out.wav
 *
 * --mask-blob installs the fine mask net (scene_mask.h, the SMSKNET1 file of
 * port/export_masknet_weights.py) as AUDIO_SCENE_MASK_BLOB does on the unit.
 * --mask-rows (a -DSPA_STEREO_UPMIX_TESTING build) writes the rows the backend
 * installed, one record of 257 elig + 257 prot float32 per 512-frame control
 * boundary (zeros while the net primes), and checks the cadence (boundaries at
 * 256, 768, 1280, ... pushed frames): record r-1 is scene_upmix.py's row r.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "stereo_upmix.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static uint16_t rd16(const uint8_t *b) { return (uint16_t)(b[0] | b[1] << 8); }

static void wr32(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}

static void wr16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }

/* RIFF walk to the data chunk of a 48 kHz / 16-bit / stereo PCM file (plain or
 * extensible fmt); leaves the stream at the first sample, returns the frame count. */
static long open_stereo_wav(FILE *f)
{
    uint8_t h[12], c[8], fmt[40];
    int have_fmt = 0;

    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
        fprintf(stderr, "error: not a RIFF/WAVE file\n");
        return -1;
    }
    while (fread(c, 1, 8, f) == 8) {
        uint32_t size = rd32(c + 4);
        if (!memcmp(c, "fmt ", 4)) {
            size_t n = size < sizeof fmt ? size : sizeof fmt;
            if (fread(fmt, 1, n, f) != n) break;
            if (fseek(f, (long)(size - n) + (long)(size & 1u), SEEK_CUR)) break;
            if ((rd16(fmt) != 1 && rd16(fmt) != 0xfffe) || rd16(fmt + 2) != 2 ||
                rd32(fmt + 4) != 48000 || rd16(fmt + 14) != 16) {
                fprintf(stderr, "error: input must be 48 kHz, 16-bit, stereo PCM\n");
                return -1;
            }
            have_fmt = 1;
        } else if (!memcmp(c, "data", 4)) {
            if (!have_fmt) { fprintf(stderr, "error: data before fmt\n"); return -1; }
            return (long)(size / 4u);
        } else if (fseek(f, (long)size + (long)(size & 1u), SEEK_CUR)) {
            break;
        }
    }
    fprintf(stderr, "error: WAV is missing its fmt or data chunk\n");
    return -1;
}

/* Plain WAVE_FORMAT_PCM header, 6 channels: what scene_upmix.write_wav6 writes. */
static int write_wav6_header(FILE *f, uint32_t frames)
{
    uint8_t h[44];
    const uint32_t bytes = frames * 12u;

    memcpy(h, "RIFF", 4); wr32(h + 4, bytes + 36u); memcpy(h + 8, "WAVEfmt ", 8);
    wr32(h + 16, 16); wr16(h + 20, 1); wr16(h + 22, 6); wr32(h + 24, 48000);
    wr32(h + 28, 48000u * 12u); wr16(h + 32, 12); wr16(h + 34, 16);
    memcpy(h + 36, "data", 4); wr32(h + 40, bytes);
    return fwrite(h, 1, sizeof h, f) == sizeof h;
}

#ifdef SPA_STEREO_UPMIX_TESTING
static FILE *rows_file;
static uint64_t rows_expected_pushed = SPA_SCENE_MASK_PREFILL_FRAMES, rows_written;
static int rows_cadence_ok = 1;

static void observe_rows(uint64_t pushed, const float *elig, const float *prot)
{
    static const float zeros[SMASK_BINS];

    if (pushed != rows_expected_pushed) rows_cadence_ok = 0;
    rows_expected_pushed = pushed + SCENE_MASK_HOP;
    if (!rows_file) return;
    if (fwrite(elig ? elig : zeros, sizeof(float), SMASK_BINS, rows_file) != SMASK_BINS ||
        fwrite(prot ? prot : zeros, sizeof(float), SMASK_BINS, rows_file) != SMASK_BINS) {
        fprintf(stderr, "error: cannot write the mask rows\n");
        exit(1);
    }
    rows_written++;
}
#endif

static int parse_float(const char *text, float *value)
{
    char *end = 0;
    double v;

    errno = 0;
    v = strtod(text, &end);
    if (errno || end == text || *end || v != v) return 0;
    *value = (float)v;
    return 1;
}

int main(int argc, char **argv)
{
    static struct spa_stereo_upmix state;          /* 700 KB: never on the stack */
    static uint8_t in_bytes[SPA_STEREO_UPMIX_MAX_FRAMES * 4];
    static uint8_t out_bytes[SPA_STEREO_UPMIX_MAX_FRAMES * 12];
    static int16_t pcm2[SPA_STEREO_UPMIX_MAX_FRAMES * 2];
    static int16_t pcm6[SPA_STEREO_UPMIX_MAX_FRAMES * 6];
    static struct scene_mask_weights blob;
    struct spa_stereo_upmix_config config;
    const char *blob_path = NULL;
    const char *paths[2] = { NULL, NULL };
    FILE *in = NULL, *out = NULL;
    long total, done = 0, block = 768;
    int i, n_paths = 0, status;

    spa_stereo_upmix_default_config(&config);
    config.backend = SPA_STEREO_UPMIX_SCENE;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        float x = 0.0f;

        if (a[0] != '-' || !a[1]) {
            if (n_paths < 2) paths[n_paths++] = a; else goto usage;
            continue;
        }
        if (!v) goto usage;
        i++;
        if (!strcmp(a, "--mode")) {
            if (!strcmp(v, "scene")) config.backend = SPA_STEREO_UPMIX_SCENE;
            else if (!strcmp(v, "off")) config.backend = SPA_STEREO_UPMIX_OFF;
            else goto usage;
        } else if (!strcmp(a, "--lfe")) {
            if (!strcmp(v, "assist")) spa_stereo_upmix_config_set_lfe(&config, 1);
            else if (!strcmp(v, "off")) spa_stereo_upmix_config_set_lfe(&config, 0);
            else goto usage;
        } else if (!strcmp(a, "--block")) {
            block = strtol(v, NULL, 10);
            if (block < 1 || block > SPA_STEREO_UPMIX_MAX_FRAMES) goto usage;
        } else if (!strcmp(a, "--mask-blob")) {
            FILE *f = fopen(v, "rb");
            size_t got = f ? fread(&blob, 1, sizeof blob, f) : 0;
            int extra = f ? fgetc(f) : EOF;

            if (f) fclose(f);
            spa_stereo_upmix_config_set_scene_mask_blob(&config, &blob, got);
            if (!f || extra != EOF || !config.scene_mask_blob) {
                fprintf(stderr, "error: %s is not a %u-byte %s mask blob\n", v, (unsigned)sizeof blob, SMASK_MAGIC);
                return 1;
            }
            blob_path = v;
        } else if (!strcmp(a, "--mask-rows")) {
#ifdef SPA_STEREO_UPMIX_TESTING
            rows_file = fopen(v, "wb");
            if (!rows_file) { fprintf(stderr, "error: cannot create %s: %s\n", v, strerror(errno)); return 1; }
            spa_stereo_upmix_test_scene_mask_observer = observe_rows;
#else
            fprintf(stderr, "error: --mask-rows needs a -DSPA_STEREO_UPMIX_TESTING build\n");
            return 2;
#endif
        } else if (!parse_float(v, &x)) {
            goto usage;
        } else if (!strcmp(a, "--alpha")) {
            spa_stereo_upmix_config_set_scene_alpha(&config, x);
        } else if (!strcmp(a, "--inst-alpha")) {
            spa_stereo_upmix_config_set_scene_inst_alpha(&config, x);
        } else if (!strcmp(a, "--inst-gamma")) {
            spa_stereo_upmix_config_set_scene_inst_gamma(&config, x);
        } else if (!strcmp(a, "--duck-depth")) {
            spa_stereo_upmix_config_set_scene_duck_depth(&config, x);
        } else if (!strcmp(a, "--rear-delay-ms")) {
            spa_stereo_upmix_config_set_scene_rear_delay_ms(&config, x);
        } else if (!strcmp(a, "--sigma-min")) {
            spa_stereo_upmix_config_set_scene_sigma_min(&config, x);
        } else if (!strcmp(a, "--side-lp-hz")) {
            spa_stereo_upmix_config_set_scene_side_lp_hz(&config, x);
        } else if (!strcmp(a, "--bleed-cancel")) {
            spa_stereo_upmix_config_set_scene_bleed_cancel(&config, x);
        } else if (!strcmp(a, "--front-keep-db")) {
            spa_stereo_upmix_config_set_scene_front_keep_db(&config, x);
        } else {
            goto usage;
        }
    }
    if (n_paths != 2) goto usage;
    if (config.backend == SPA_STEREO_UPMIX_OFF) {      /* the exact bypass, as audio_hook.c arms it */
        spa_stereo_upmix_config_set_lfe(&config, 0);
        config.front_gain_q15 = 32768u;
    }

    in = fopen(paths[0], "rb");
    if (!in) { fprintf(stderr, "error: cannot open %s: %s\n", paths[0], strerror(errno)); return 1; }
    total = open_stereo_wav(in);
    if (total < 0) return 1;
    status = spa_stereo_upmix_init(&state, &config);
    if (status != SPA_STEREO_UPMIX_OK) {
        fprintf(stderr, "error: upmixer initialisation failed (%d)\n", status);
        return 1;
    }
    out = fopen(paths[1], "wb");
    if (!out || !write_wav6_header(out, (uint32_t)total)) {
        fprintf(stderr, "error: cannot create %s: %s\n", paths[1], strerror(errno));
        return 1;
    }
    while (done < total) {
        size_t frames = (size_t)(total - done < block ? total - done : block), s;

        if (fread(in_bytes, 4, frames, in) != frames) { fprintf(stderr, "error: WAV data truncated\n"); return 1; }
        for (s = 0; s < frames * 2; s++) pcm2[s] = (int16_t)rd16(in_bytes + 2 * s);
        status = spa_stereo_upmix_process_s16(&state, pcm2, frames, pcm6, frames);
        if (status != SPA_STEREO_UPMIX_OK) { fprintf(stderr, "error: upmixer failed (%d)\n", status); return 1; }
        for (s = 0; s < frames * 6; s++) wr16(out_bytes + 2 * s, (uint16_t)pcm6[s]);
        if (fwrite(out_bytes, 12, frames, out) != frames) { fprintf(stderr, "error: cannot write %s\n", paths[1]); return 1; }
        done += (long)frames;
    }
    if (fclose(out)) { fprintf(stderr, "error: cannot finish %s\n", paths[1]); return 1; }
    fclose(in);
#ifdef SPA_STEREO_UPMIX_TESTING
    if (rows_file && fclose(rows_file)) { fprintf(stderr, "error: cannot finish the mask rows\n"); return 1; }
    if (!rows_cadence_ok) { fprintf(stderr, "error: mask control boundaries off the 256 + 512 r cadence\n"); return 1; }
    fprintf(stderr, "mask rows: %llu records\n", (unsigned long long)rows_written);
#endif
    fprintf(stderr, "rendered %ld frames (%.3f s) mode=%s block=%ld latency=%d frames lfe=%s mask=%s "
            "front_q15=%u alpha=%.3f inst_alpha=%.3f inst_gamma=%.3f duck_depth=%.3f "
            "rear_delay_ms=%.2f sigma_min=%.3f side_lp_hz=%.0f bleed_cancel=%.3f\n%s\n",
            total, (double)total / 48000.0,
            state.config.backend == SPA_STEREO_UPMIX_SCENE ? "scene" : "off", block,
            state.config.backend == SPA_STEREO_UPMIX_SCENE ? (int)SPA_SCENE_LATENCY_FRAMES : 0,
            state.config.lfe_enabled ? "assist" : "off", blob_path ? blob_path : "none",
            (unsigned)state.config.front_gain_q15,
            (double)state.config.scene.alpha, (double)state.config.scene.inst_alpha,
            (double)state.config.scene.inst_gamma, (double)state.config.scene.duck_depth,
            (double)state.config.scene.rear_delay_ms, (double)state.config.scene.sigma_min,
            (double)state.config.scene.side_lp_hz, (double)state.config.scene.bleed_cancel, paths[1]);
    return 0;

usage:
    fprintf(stderr, "usage: %s [--mode scene|off] [--lfe off|assist] [--block 1..1024] "
            "[--alpha A] [--inst-alpha A] [--inst-gamma G] [--duck-depth D] [--rear-delay-ms MS] "
            "[--sigma-min S] [--side-lp-hz HZ] [--bleed-cancel K] [--front-keep-db DB] [--mask-blob masknet.bin] "
            "[--mask-rows rows.f32] in.wav out.wav\n", argv[0]);
    return 2;
}
