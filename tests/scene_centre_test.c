/* The centre stage of the scene engine (scene_engine_set_vocal): a centred tone the mask calls vocal
 * moves from L/R to C with its power kept, half of it at frac 0.5, none of a hard-left tone, and
 * frac 0 leaves C exactly silent. */
#include "audio/scene_engine.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static struct scene_state st;
static float in[2 * 48000], out[6 * 50000];

/* RMS per channel over the second half second; the mask says "all vocal, protected" every control */
static void run(float frac, double rms[6])
{
    struct scene_config cfg;
    float elig[257], prot[257], voc[257];
    size_t n = 0, f;
    int c, b, i;

    scene_config_defaults(&cfg);
    assert(scene_engine_init(&st, &cfg) == 0);
    for (i = 0; i < 257; i++) { elig[i] = 0.0f; prot[i] = 1.0f; voc[i] = 1.0f; }
    for (b = 0; b < 48000 / 512; b++) {
        scene_engine_set_mask(&st, elig, prot, 257);
        scene_engine_set_vocal(&st, voc, 257, frac);
        n += scene_engine_push(&st, in + 2 * 512 * b, 512, out + 6 * n, 50000 - n);
    }
    for (c = 0; c < 6; c++) rms[c] = 0.0;
    for (f = 24000; f < n; f++)
        for (c = 0; c < 6; c++) rms[c] += (double)out[6 * f + c] * out[6 * f + c];
    for (c = 0; c < 6; c++) rms[c] = sqrt(rms[c] / (double)(n - 24000));
}

int main(void)
{
    double off[6], on[6], half[6], p_off;
    int i;

    for (i = 0; i < 48000; i++)
        in[2 * i] = in[2 * i + 1] = 0.3f * sinf(2.0f * 3.14159265f * 1000.0f * (float)i / 48000.0f);
    run(0.0f, off); run(1.0f, on); run(0.5f, half);
    p_off = off[0] * off[0] + off[1] * off[1];
    assert(off[2] == 0.0);                                        /* off: C exactly silent */
    assert(on[0] < 0.05 * off[0] && on[1] < 0.05 * off[1]);       /* 1: the centred tone leaves L/R */
    assert(fabs(on[2] * on[2] - p_off) < 0.02 * p_off);           /* ... with its power */
    assert(fabs(half[0] - 0.5 * off[0]) < 0.03 * off[0]);         /* 0.5: L/R at -6 dB */
    assert(fabs(half[0] * half[0] + half[1] * half[1] + half[2] * half[2] - p_off) < 0.02 * p_off);
    for (i = 0; i < 48000; i++) in[2 * i + 1] = 0.0f;            /* hard left: not centred, stays in L */
    run(1.0f, on);
    assert(on[2] < 0.05 * on[0]);
    printf("scene_centre_test: OK (off silent, centred vocal to C power-kept, half share, panned stays)\n");
    return 0;
}
