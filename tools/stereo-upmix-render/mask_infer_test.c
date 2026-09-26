/*
 * Host parity test for hook/audio/hssr_mask.c:
 *   mask_infer_test BLOB FEATURES.feat OUT.elig
 * FEATURES: u32 T, then T x 257 x 4 float32 (mask_net.py dump-features);
 * OUT: T x 257 float32 eligibilities to compare with the .ref dump.
 *
 * Build: cc -std=c99 -O2 -ffp-contract=off -Wall -Wextra -Werror -I../../hook/audio \
 *        mask_infer_test.c ../../hook/audio/hssr_mask.c -lm -o mask_infer_test
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "hssr_mask.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct spa_mask_model model;
    static struct spa_mask_state state;
    FILE *in, *out;
    uint32_t t, n;
    float *feat, *elig;
    unsigned char head[4];

    if (argc != 4) {
        fprintf(stderr, "usage: %s blob features.feat out.elig\n", argv[0]);
        return 2;
    }
    if (spa_mask_model_load_file(&model, argv[1]) != 0) {
        fprintf(stderr, "blob load failed\n");
        return 1;
    }
    in = fopen(argv[2], "rb");
    out = fopen(argv[3], "wb");
    if (!in || !out || fread(head, 1, 4, in) != 4) {
        fprintf(stderr, "feature file\n");
        return 1;
    }
    n = (uint32_t)head[0] | ((uint32_t)head[1] << 8) | ((uint32_t)head[2] << 16) | ((uint32_t)head[3] << 24);
    feat = (float *)malloc(sizeof(float) * SPA_MASK_BINS * SPA_MASK_FEATURES);
    elig = (float *)malloc(sizeof(float) * SPA_MASK_BINS);
    spa_mask_state_reset(&state);
    for (t = 0; t < n; ++t) {
        if (fread(feat, sizeof(float), SPA_MASK_BINS * SPA_MASK_FEATURES, in) != SPA_MASK_BINS * SPA_MASK_FEATURES) {
            fprintf(stderr, "short read at control %u\n", t);
            return 1;
        }
        if (spa_mask_infer(&model, &state, feat, elig, NULL) != 0) {
            fprintf(stderr, "infer failed\n");
            return 1;
        }
        fwrite(elig, sizeof(float), SPA_MASK_BINS, out);
    }
    fclose(in);
    fclose(out);
    printf("%u controls, %zu blob floats\n", n, model.float_count);
    spa_mask_model_unload(&model);
    free(feat);
    free(elig);
    return 0;
}
