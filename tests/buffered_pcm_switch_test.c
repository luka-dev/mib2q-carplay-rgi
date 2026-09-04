/*
 * Regression for cookie-only AAC/APAC switches inside one type-103 session.
 * The SETUP index is intentionally not an input to render dispatch.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "audio/spatial_downmix.h"
#include "audio/stereo_upmix.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { TEST_FRAMES = SPA_STEREO_UPMIX_MAX_FRAMES };

static int16_t g_stereo[TEST_FRAMES * 2];
static int16_t g_first_stereo_render[TEST_FRAMES * 6];
static int16_t g_second_stereo_render[TEST_FRAMES * 6];

static enum spa_decoded_pcm_action render_cookie_pcm(
    struct spa_stereo_upmix *upmix, int codec_is_apac, uint32_t channels,
    uint32_t layout_tag, const int16_t *input, size_t frames, int16_t *output)
{
    enum spa_decoded_pcm_kind kind = spa_classify_decoded_pcm(
        codec_is_apac, channels, 48000, layout_tag);
    enum spa_decoded_pcm_action action = spa_select_decoded_pcm_action(
        kind, channels, 6, 48000);

    if (action == SPA_DECODED_PCM_ACTION_STEREO_UPMIX) {
        assert(spa_stereo_upmix_process_s16(upmix, input, frames,
                                            output, frames) ==
               SPA_STEREO_UPMIX_OK);
    } else if (action == SPA_DECODED_PCM_ACTION_REORDER_5_1) {
        assert(spa_reorder_5_1_decoder_to_declared_s16(
                   input, frames, output) == frames * 6);
    } else {
        assert(!"unexpected action in cookie-switch regression");
    }
    return action;
}

static void fill_stereo(void)
{
    uint32_t left_noise = UINT32_C(0x12345678);
    uint32_t right_noise = UINT32_C(0x9abcdef0);
    size_t frame;

    for (frame = 0; frame < TEST_FRAMES; frame++) {
        int32_t left;
        int32_t right;

        left_noise = left_noise * UINT32_C(1664525) + UINT32_C(1013904223);
        right_noise = right_noise * UINT32_C(22695477) + UINT32_C(1);
        left = 8000 + (int32_t)((left_noise >> 20) & 0xfff) - 2048;
        right = 8000 + (int32_t)((right_noise >> 20) & 0xfff) - 2048;
        g_stereo[frame * 2] = (int16_t)left;
        g_stereo[frame * 2 + 1] = (int16_t)right;
    }
}

static void assert_stereo_synthesis_present(const int16_t *output)
{
    int have_lfe = 0;
    int have_rear = 0;
    size_t frame;

    for (frame = 0; frame < TEST_FRAMES; frame++) {
        const int16_t *sample = output + frame * 6;
        assert(sample[SPA_STEREO_UPMIX_CHANNEL_C] == 0);
        if (sample[SPA_STEREO_UPMIX_CHANNEL_LFE] != 0) have_lfe = 1;
        if (sample[SPA_STEREO_UPMIX_CHANNEL_LS] != 0 ||
            sample[SPA_STEREO_UPMIX_CHANNEL_RS] != 0)
            have_rear = 1;
    }
    assert(have_lfe);
    assert(have_rear);
}

static void assert_native_reorder(struct spa_stereo_upmix *upmix)
{
    const int16_t decoder_order[6] = { 300, 100, -200, 400, -500, 1000 };
    const int16_t declared_order[6] = { 100, -200, 300, 1000, 400, -500 };
    int16_t output[6] = { 0 };

    assert(render_cookie_pcm(upmix, 1, 6, SPA_APAC_5_1_LAYOUT_TAG,
                             decoder_order, 1, output) ==
           SPA_DECODED_PCM_ACTION_REORDER_5_1);
    assert(memcmp(output, declared_order, sizeof(output)) == 0);
}

int main(void)
{
    static const int setup_indices[2] = { 65, 23 };
    struct spa_stereo_upmix_config config;
    struct spa_stereo_upmix upmix;
    size_t sequence;

    fill_stereo();
    spa_stereo_upmix_default_config(&config);
    spa_stereo_upmix_config_set_lfe(&config, 1);
    assert(spa_stereo_upmix_init(&upmix, &config) == SPA_STEREO_UPMIX_OK);

    for (sequence = 0;
         sequence < sizeof(setup_indices) / sizeof(setup_indices[0]);
         sequence++) {
        int16_t *stereo_output = sequence == 0 ?
            g_first_stereo_render : g_second_stereo_render;

        /* Exercise both orders: SETUP=65 begins with AAC, SETUP=23 begins
         * with APAC. Neither stale SETUP value may influence the action. */
        if (setup_indices[sequence] == 65) {
            assert(render_cookie_pcm(&upmix, 0, 2, 0, g_stereo,
                                     TEST_FRAMES, stereo_output) ==
                   SPA_DECODED_PCM_ACTION_STEREO_UPMIX);
            assert_stereo_synthesis_present(stereo_output);
            assert(spa_stereo_upmix_reset(&upmix) == SPA_STEREO_UPMIX_OK);
            assert_native_reorder(&upmix);
        } else {
            assert_native_reorder(&upmix);
            assert(spa_stereo_upmix_reset(&upmix) == SPA_STEREO_UPMIX_OK);
            assert(render_cookie_pcm(&upmix, 0, 2, 0, g_stereo,
                                     TEST_FRAMES, stereo_output) ==
                   SPA_DECODED_PCM_ACTION_STEREO_UPMIX);
            assert_stereo_synthesis_present(stereo_output);
        }
        assert(spa_stereo_upmix_reset(&upmix) == SPA_STEREO_UPMIX_OK);
    }

    /* Every cookie boundary reset must make the first AAC block independent
     * of which APAC/AAC track preceded it. */
    assert(memcmp(g_first_stereo_render, g_second_stereo_render,
                  sizeof(g_first_stereo_render)) == 0);
    puts("buffered_pcm_switch_test: OK (SETUP 65/23, AAC <-> APAC)");
    return 0;
}
