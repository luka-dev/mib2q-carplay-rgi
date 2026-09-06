/* Host diagnostic for feature parity and exported-model inference parity. */
#include "hssr_dsp.h"
#include "hssr_neon.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct spa_hssr_dsp g_dsp;
static struct spa_hssr_state g_network;

static int dump_features(const struct spa_hssr_model *model,
                         FILE *input, FILE *output)
{
    uint8_t bytes[4];
    uint32_t previous_count = 0;

    spa_hssr_dsp_prepare(&g_dsp);
    while (fread(bytes, 1, sizeof(bytes), input) == sizeof(bytes)) {
        int16_t input_l = (int16_t)((uint16_t)bytes[0] |
                                    ((uint16_t)bytes[1] << 8));
        int16_t input_r = (int16_t)((uint16_t)bytes[2] |
                                    ((uint16_t)bytes[3] << 8));
        int16_t front_l;
        int16_t front_r;
        float rear_l;
        float rear_r;

        if (spa_hssr_dsp_process_sample(
                &g_dsp, model, input_l, input_r,
                &front_l, &front_r, &rear_l, &rear_r) != SPA_HSSR_OK)
            return 0;
        if (g_dsp.network.inference_count != previous_count) {
            if (fwrite(g_dsp.features, sizeof(float), SPA_HSSR_FEATURES,
                       output) != SPA_HSSR_FEATURES)
                return 0;
            previous_count = g_dsp.network.inference_count;
        }
    }
    return feof(input) && !ferror(input) && !ferror(output);
}

static int dump_inference(const struct spa_hssr_model *model,
                          FILE *input, FILE *output)
{
    float features[SPA_HSSR_FEATURES];
    float controls[SPA_HSSR_OUTPUTS];

    spa_hssr_state_reset(&g_network);
    for (;;) {
        size_t count = fread(features, sizeof(float), SPA_HSSR_FEATURES,
                             input);

        if (count == 0 && feof(input)) break;
        if (count != SPA_HSSR_FEATURES) return 0;
        if (spa_hssr_infer(model, &g_network, features, controls) !=
                SPA_HSSR_OK)
            return 0;
        if (fwrite(controls, sizeof(float), SPA_HSSR_OUTPUTS, output) !=
                SPA_HSSR_OUTPUTS)
            return 0;
    }
    return !ferror(input) && !ferror(output);
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s MODEL.bin --pcm INPUT.s16le FEATURES.f32le\n"
            "       %s MODEL.bin --features FEATURES.f32le OUTPUTS.f32le\n",
            program, program);
}

int main(int argc, char **argv)
{
    struct spa_hssr_model model;
    FILE *input;
    FILE *output;
    int ok;

    if (argc != 5 || (strcmp(argv[2], "--pcm") != 0 &&
                      strcmp(argv[2], "--features") != 0)) {
        usage(argv[0]);
        return 2;
    }
    memset(&model, 0, sizeof(model));
    if (spa_hssr_model_load_file(&model, argv[1]) != SPA_HSSR_OK) {
        fprintf(stderr, "error: invalid model %s\n", argv[1]);
        return 1;
    }
    input = fopen(argv[3], "rb");
    output = fopen(argv[4], "wb");
    if (!input || !output) {
        fprintf(stderr, "error: cannot open diagnostic input/output\n");
        if (input) fclose(input);
        if (output) fclose(output);
        spa_hssr_model_unload(&model);
        return 1;
    }
    ok = strcmp(argv[2], "--pcm") == 0
        ? dump_features(&model, input, output)
        : dump_inference(&model, input, output);
    if (fclose(input) != 0 || fclose(output) != 0) ok = 0;
    spa_hssr_model_unload(&model);
    if (!ok) {
        fprintf(stderr, "error: diagnostic conversion failed\n");
        return 1;
    }
    return 0;
}
