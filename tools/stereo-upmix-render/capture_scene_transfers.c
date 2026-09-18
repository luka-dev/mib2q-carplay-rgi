/* Host-only: per-bin complex rear transfer of the SCENE path (main + optional guard model), no vehicle I/O.
 * usage: capture_scene_transfers MODEL [GUARD|-] INPUT.s16 TRANSFERS.f32 REAR.f32
 * TRANSFERS: 512 analysis window, 512 synthesis window, then per hop 257 x 8 floats (re/im of LL, LR, RL, RR). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hssr_dsp.h"
static FILE *transfers;
static unsigned expected_bin;
void spa_hssr_capture_transfer(unsigned bin, const float matrix[8])
{
    if (bin != expected_bin || fwrite(matrix, sizeof(float), 8, transfers) != 8) exit(3);
    expected_bin = (bin + 1u) % SPA_HSSR_BINS;
}
int main(int argc, char **argv)
{
    static struct spa_hssr_dsp dsp;
    static struct spa_hssr_model model, guard;
    FILE *input, *output;
    int16_t pcm[256], front[256];
    float rear[256], lfe[256];
    size_t count, i;
    if (argc != 6) { fprintf(stderr, "usage: %s MODEL GUARD|- INPUT.s16 TRANSFERS.f32 REAR.f32\n", argv[0]); return 2; }
    if (spa_hssr_model_load_file(&model, argv[1]) != SPA_HSSR_OK) return 3;
    input = fopen(argv[3], "rb"); transfers = fopen(argv[4], "wb"); output = fopen(argv[5], "wb");
    if (!input || !transfers || !output) return 3;
    spa_hssr_dsp_prepare(&dsp);
    dsp.cabin_policy = 0;
    if (strcmp(argv[2], "-") != 0) {
        if (spa_hssr_model_load_file(&guard, argv[2]) != SPA_HSSR_OK) return 3;
        dsp.guard_model = &guard;
        spa_hssr_state_reset(&dsp.guard_network);
    }
    if (fwrite(dsp.analysis_window, sizeof(float), 512, transfers) != 512 ||
        fwrite(dsp.synthesis_window, sizeof(float), 512, transfers) != 512) return 3;
    while ((count = fread(pcm, sizeof(int16_t), 256, input)) != 0) {
        if (count % 2) return 3;
        if (spa_hssr_dsp_process_block(&dsp, &model, NULL, pcm, count / 2, front, rear, lfe) != SPA_HSSR_OK) return 3;
        for (i = 0; i < count; i += 2) {
            const float frame[2] = { rear[i], rear[i + 1] };
            if (fwrite(frame, sizeof(float), 2, output) != 2) return 3;
        }
    }
    fclose(output); fclose(transfers); fclose(input);
    return 0;
}
