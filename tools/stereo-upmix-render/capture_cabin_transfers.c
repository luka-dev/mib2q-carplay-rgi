/* Host-only mixture-conditioned routing diagnostic, no vehicle I/O. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hssr_dsp.h"

static FILE *transfers;
static unsigned expected_bin;
void spa_hssr_capture_transfer(unsigned bin, const float matrix[8])
{
    if (bin != expected_bin || fwrite(matrix, sizeof(float), 8, transfers) != 8)
        exit(3);
    expected_bin = (bin + 1u) % SPA_HSSR_BINS;
}

int main(int argc, char **argv)
{
    static struct spa_hssr_dsp dsp;
    static struct spa_hssr_model model;
    FILE *input, *output;
    int16_t pcm[256], front[256];
    float rear[256], source[256], lfe[256];
    size_t count, i;
    const int stdio_mode = argc == 3 && strcmp(argv[2], "--stdio") == 0;
    if (argc != 5 && !stdio_mode) {
        fprintf(stderr, "usage: %s MODEL INPUT.s16 TRANSFERS.f32 OUTPUT.f32\n", argv[0]);
        fprintf(stderr, "       %s MODEL --stdio (PCM stdin, transfers stdout, output stderr)\n", argv[0]);
        return 2;
    }
    if (spa_hssr_model_load_file(&model, argv[1]) != SPA_HSSR_OK) return 3;
    input = stdio_mode ? stdin : fopen(argv[2], "rb");
    transfers = stdio_mode ? stdout : fopen(argv[3], "wbx");
    output = stdio_mode ? stderr : fopen(argv[4], "wbx");
    if (!input || !transfers || !output) return 3;
    spa_hssr_dsp_prepare(&dsp);
    dsp.cabin_policy = 1;
    if (fwrite(dsp.analysis_window, sizeof(float), 512, transfers) != 512 ||
        fwrite(dsp.synthesis_window, sizeof(float), 512, transfers) != 512) return 3;
    while ((count = fread(pcm, sizeof(int16_t), 256, input)) != 0) {
        if (count % 2) return 3;
        if (spa_hssr_dsp_process_block_sources(&dsp, &model, NULL, pcm, count/2,
                front, rear, lfe, source) != SPA_HSSR_OK) return 3;
        for (i = 0; i < count; i += 2) {
            const float frame[4] = {rear[i], rear[i+1], source[i], source[i+1]};
            if (fwrite(frame, sizeof(float), 4, output) != 4) return 3;
        }
    }
    if (ferror(input) || fclose(output) || fclose(transfers)) return 3;
    fclose(input);
    spa_hssr_model_unload(&model);
    return 0;
}
