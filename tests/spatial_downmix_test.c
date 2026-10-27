/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "audio/spatial_downmix.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_matrix(void)
{
    const int16_t input[8] = { 100, -200, 300, 1000, 400, -500, 600, -800 };
    int16_t output[6] = { 0 };
    const int16_t expected[6] = { 100, -200, 300, 500, 700, -900 };

    assert(spa_downmix_5_1_2_s16(input, 1, output) == 6);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

static void test_saturation(void)
{
    const int16_t input[8] = {
        0, 0, 0, -32768,
        30000, -30000, 10000, -10000
    };
    int16_t output[6] = { 0 };

    assert(spa_downmix_5_1_2_s16(input, 1, output) == 6);
    assert(output[3] == -16384);
    assert(output[4] == 32767);
    assert(output[5] == -32768);
}

static void test_exact_in_place(void)
{
    int16_t samples[16] = {
        1, 2, 3, 4, 5, 6, 8, 10,
        -1, -2, -3, -4, -5, -6, -8, -10
    };
    const int16_t expected[12] = {
        1, 2, 3, 2, 9, 11,
        -1, -2, -3, -2, -9, -11
    };

    assert(spa_downmix_5_1_2_s16(samples, 2, samples) == 12);
    assert(memcmp(samples, expected, sizeof(expected)) == 0);
}

static void test_drop_in_place(void)
{
    int16_t samples[16] = {
        1, 2, 3, 4, 5, 6, 700, 800,
        -1, -2, -3, -4, -5, -6, -700, -800
    };
    const int16_t expected[12] = {
        1, 2, 3, 4, 5, 6,
        -1, -2, -3, -4, -5, -6
    };

    assert(spa_drop_5_1_2_s16(samples, 2, samples) == 12);
    assert(memcmp(samples, expected, sizeof(expected)) == 0);
}

static void test_reorder(void)
{
    /* decoder slots C L R Ls Rs LFE tagged by value */
    const int16_t input[6] = { 300, 100, -200, 400, -500, 1000 };
    const int16_t expected[6] = { 100, -200, 300, 1000, 400, -500 };
    int16_t output[6] = { 0 };

    assert(spa_reorder_5_1_decoder_to_declared_s16(input, 1, output) == 6);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

static void test_reorder_in_place(void)
{
    int16_t samples[12] = {
        300, 100, -200, 400, -500, 1000,
        -300, -100, 200, -400, 500, -1000
    };
    const int16_t expected[12] = {
        100, -200, 300, 1000, 400, -500,
        -100, 200, -300, -1000, -400, 500
    };

    assert(spa_reorder_5_1_decoder_to_declared_s16(samples, 2, samples) == 12);
    assert(memcmp(samples, expected, sizeof(expected)) == 0);
}

/* The reorder feeds the fold: a decoder-order 5.1 frame padded with heights
 * must land on the same speakers the fold expects. */
static void test_reorder_then_downmix(void)
{
    const int16_t decoded[6] = { 300, 100, -200, 400, -500, 1000 };
    int16_t declared[8] = { 0 };
    int16_t folded[6] = { 0 };
    const int16_t expected[6] = { 100, -200, 300, 500, 400, -500 };

    assert(spa_reorder_5_1_decoder_to_declared_s16(decoded, 1, declared) == 6);
    declared[6] = 0;
    declared[7] = 0;
    assert(spa_downmix_5_1_2_s16(declared, 1, folded) == 6);
    assert(memcmp(folded, expected, sizeof(expected)) == 0);
}

static void test_cookie_resolved_pcm_kind(void)
{
    static const int setup_indices[2] = { 65, 23 };
    size_t setup;

    /* The classifier deliberately has no negotiated-index argument.  SETUP can
     * remain 65 while an AAC cookie resolves to stereo, or remain 23 while an
     * APAC cookie resolves to native 5.1. */
    assert(spa_classify_decoded_pcm(0, 2, 48000, 0) ==
           SPA_DECODED_PCM_STEREO);
    assert(spa_classify_decoded_pcm(1, 2, 48000, 0x12345678) ==
           SPA_DECODED_PCM_STEREO);
    assert(spa_classify_decoded_pcm(1, 6, 48000,
                                    SPA_APAC_5_1_LAYOUT_TAG) ==
           SPA_DECODED_PCM_APAC_5_1);
    assert(spa_classify_decoded_pcm(1, 8, 48000,
                                    SPA_APAC_5_1_2_LAYOUT_TAG) ==
           SPA_DECODED_PCM_APAC_5_1_2);

    assert(spa_classify_decoded_pcm(0, 6, 48000,
                                    SPA_APAC_5_1_LAYOUT_TAG) ==
           SPA_DECODED_PCM_OTHER);
    assert(spa_classify_decoded_pcm(1, 6, 48000, 0) ==
           SPA_DECODED_PCM_OTHER);
    assert(spa_classify_decoded_pcm(1, 8, 44100,
                                    SPA_APAC_5_1_2_LAYOUT_TAG) ==
           SPA_DECODED_PCM_OTHER);

    /* Both switch directions use the same cookie-resolved decisions.  The
     * loop's SETUP value is intentionally absent from the classifier/action
     * calls: it may stay stale for the lifetime of the type-103 session. */
    for (setup = 0; setup < sizeof(setup_indices) / sizeof(setup_indices[0]);
         setup++) {
        enum spa_decoded_pcm_kind kind;
        assert(setup_indices[setup] == 65 || setup_indices[setup] == 23);

        kind = spa_classify_decoded_pcm(0, 2, 48000, 0);
        assert(spa_select_decoded_pcm_action(kind, 2, 6, 48000) ==
               SPA_DECODED_PCM_ACTION_STEREO_UPMIX);

        kind = spa_classify_decoded_pcm(1, 6, 48000,
                                        SPA_APAC_5_1_LAYOUT_TAG);
        assert(spa_select_decoded_pcm_action(kind, 6, 6, 48000) ==
               SPA_DECODED_PCM_ACTION_REORDER_5_1);

        kind = spa_classify_decoded_pcm(1, 8, 48000,
                                        SPA_APAC_5_1_2_LAYOUT_TAG);
        assert(spa_select_decoded_pcm_action(kind, 8, 6, 48000) ==
               SPA_DECODED_PCM_ACTION_GENERIC);
    }

    assert(spa_select_decoded_pcm_action(SPA_DECODED_PCM_STEREO,
                                         2, 8, 48000) ==
           SPA_DECODED_PCM_ACTION_GENERIC);
    assert(spa_select_decoded_pcm_action(SPA_DECODED_PCM_STEREO,
                                         2, 6, 44100) ==
           SPA_DECODED_PCM_ACTION_GENERIC);
}

int main(void)
{
    int16_t sink = 0;

    assert(spa_downmix_5_1_2_s16(0, 1, &sink) == 0);
    assert(spa_downmix_5_1_2_s16(&sink, 1, 0) == 0);
    assert(spa_drop_5_1_2_s16(0, 1, &sink) == 0);
    assert(spa_drop_5_1_2_s16(&sink, 1, 0) == 0);
    assert(spa_reorder_5_1_decoder_to_declared_s16(0, 1, &sink) == 0);
    assert(spa_reorder_5_1_decoder_to_declared_s16(&sink, 1, 0) == 0);
    test_matrix();
    test_saturation();
    test_exact_in_place();
    test_drop_in_place();
    test_reorder();
    test_reorder_in_place();
    test_reorder_then_downmix();
    test_cookie_resolved_pcm_kind();
    return 0;
}
