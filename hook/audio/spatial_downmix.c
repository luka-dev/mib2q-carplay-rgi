/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "spatial_downmix.h"

#include <limits.h>

static int16_t spa_saturate_s16(int32_t value)
{
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

enum spa_decoded_pcm_kind spa_classify_decoded_pcm(int codec_is_apac,
                                                    uint32_t channels,
                                                    uint32_t sample_rate,
                                                    uint32_t channel_layout_tag)
{
    if (sample_rate != 48000u) return SPA_DECODED_PCM_OTHER;
    if (channels == 2u) return SPA_DECODED_PCM_STEREO;
    if (!codec_is_apac) return SPA_DECODED_PCM_OTHER;
    if (channels == SPA_PCM_5_1_CHANNELS &&
        channel_layout_tag == SPA_APAC_5_1_LAYOUT_TAG)
        return SPA_DECODED_PCM_APAC_5_1;
    if (channels == SPA_PCM_5_1_2_CHANNELS &&
        channel_layout_tag == SPA_APAC_5_1_2_LAYOUT_TAG)
        return SPA_DECODED_PCM_APAC_5_1_2;
    return SPA_DECODED_PCM_OTHER;
}

enum spa_decoded_pcm_action spa_select_decoded_pcm_action(
    enum spa_decoded_pcm_kind kind, uint32_t source_channels,
    uint32_t output_channels, uint32_t sample_rate)
{
    if (kind == SPA_DECODED_PCM_STEREO && source_channels == 2u &&
        output_channels == SPA_PCM_5_1_CHANNELS && sample_rate == 48000u)
        return SPA_DECODED_PCM_ACTION_STEREO_UPMIX;
    if (kind == SPA_DECODED_PCM_APAC_5_1 &&
        source_channels == SPA_PCM_5_1_CHANNELS &&
        output_channels >= SPA_PCM_5_1_CHANNELS)
        return SPA_DECODED_PCM_ACTION_REORDER_5_1;
    return SPA_DECODED_PCM_ACTION_GENERIC;
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

size_t spa_drop_5_1_2_s16(const int16_t *input, size_t frames, int16_t *output)
{
    size_t frame;

    if (!input || !output) return 0;
    for (frame = 0; frame < frames; frame++) {
        const int16_t *src = input + frame * SPA_PCM_5_1_2_CHANNELS;
        int16_t *dst = output + frame * SPA_PCM_5_1_CHANNELS;
        int ch;

        for (ch = 0; ch < SPA_PCM_5_1_CHANNELS; ch++)
            dst[ch] = src[ch];
    }
    return frames * SPA_PCM_5_1_CHANNELS;
}
