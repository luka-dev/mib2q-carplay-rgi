#include "spatial_downmix.h"

#include <limits.h>

static int16_t spa_saturate_s16(int32_t value)
{
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

size_t spa_reorder_5_1_decoder_to_declared_s16(const int16_t *input, size_t frames,
                                               int16_t *output)
{
    /* decoder C L R Ls Rs LFE  ->  declared L R C LFE Ls Rs */
    static const int kFromDeclared[SPA_PCM_5_1_CHANNELS] = { 1, 2, 0, 5, 3, 4 };
    size_t frame;

    if (!input || !output) return 0;

    for (frame = 0; frame < frames; frame++) {
        const int16_t *src = input + frame * SPA_PCM_5_1_CHANNELS;
        int16_t *dst = output + frame * SPA_PCM_5_1_CHANNELS;
        int16_t tmp[SPA_PCM_5_1_CHANNELS];
        int ch;

        for (ch = 0; ch < SPA_PCM_5_1_CHANNELS; ch++)
            tmp[ch] = src[kFromDeclared[ch]];
        for (ch = 0; ch < SPA_PCM_5_1_CHANNELS; ch++)
            dst[ch] = tmp[ch];
    }

    return frames * SPA_PCM_5_1_CHANNELS;
}

size_t spa_downmix_5_1_2_s16(const int16_t *input, size_t frames, int16_t *output)
{
    size_t frame;

    if (!input || !output) return 0;

    for (frame = 0; frame < frames; frame++) {
        const int16_t *src = input + frame * SPA_PCM_5_1_2_CHANNELS;
        int16_t *dst = output + frame * SPA_PCM_5_1_CHANNELS;
        const int32_t left = src[0];
        const int32_t right = src[1];
        const int32_t center = src[2];
        const int32_t lfe = src[3];
        const int32_t surroundLeft = src[4];
        const int32_t surroundRight = src[5];
        const int32_t heightLeft = src[6];
        const int32_t heightRight = src[7];

        dst[0] = (int16_t)left;
        dst[1] = (int16_t)right;
        dst[2] = (int16_t)center;
        dst[3] = (int16_t)(lfe / 2);
        dst[4] = spa_saturate_s16(surroundLeft + heightLeft / 2);
        dst[5] = spa_saturate_s16(surroundRight + heightRight / 2);
    }

    return frames * SPA_PCM_5_1_CHANNELS;
}
