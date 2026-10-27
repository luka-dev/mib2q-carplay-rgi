/*
 * Regression for cookie-only AAC/APAC switches inside one type-103 session.
 * The SETUP index is intentionally not an input to render dispatch.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "audio/audio_hook.h"
#include "audio/spatial_downmix.h"
#include "audio/pcm_period.h"
#include "audio/stereo_upmix.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_TRACK_FRAMES = SPA_STEREO_UPMIX_MAX_FRAMES * 2
};

static int16_t g_stereo[TEST_TRACK_FRAMES * 2];
static int16_t g_first_stereo_render[TEST_TRACK_FRAMES * 6];
static int16_t g_second_stereo_render[TEST_TRACK_FRAMES * 6];
static struct spa_stereo_upmix g_upmix;
static void assert_buffered_receiver_sizes(void)
{
    assert(spa_type103_audio_buffer_size_for_index(22) == 5000000);
    assert(spa_type103_audio_buffer_size_for_index(23) == 5000000);
    assert(spa_type103_audio_buffer_size_for_index(40) == 5000000);
    assert(spa_type103_audio_buffer_size_for_index(42) == 5000000);
    assert(spa_type103_audio_buffer_size_for_index(64) == 21168000);
    assert(spa_type103_audio_buffer_size_for_index(65) == 21168000);
    assert(spa_type103_audio_buffer_size_for_index(66) == 21168000);

    /* A type-103 SETUP is performed once even though iOS changes between the
     * two advertised codecs per track.  The selected AAC index must therefore
     * inherit APAC's larger requirement when 65 is also in the offer. */
    assert(spa_type103_audio_buffer_size_for_offer(23, 65, 23) == 21168000);
    assert(spa_type103_audio_buffer_size_for_offer(65, 65, 23) == 21168000);
    assert(spa_type103_audio_buffer_size_for_offer(23, 23, -1) == 5000000);
}

static void assert_whole_period_policy(void)
{
    const size_t codec_frames = 1024;
    const size_t qsa_frames = 768;
    size_t fill = 0;
    size_t take;

    assert(spa_pcm_whole_period_frames(0, qsa_frames) == 0);
    assert(spa_pcm_whole_period_frames(256, qsa_frames) == 0);
    assert(spa_pcm_whole_period_frames(512, qsa_frames) == 0);
    assert(spa_pcm_whole_period_frames(767, qsa_frames) == 0);
    assert(spa_pcm_whole_period_frames(768, qsa_frames) == qsa_frames);
    assert(spa_pcm_whole_period_frames(1024, qsa_frames) == qsa_frames);
    assert(spa_pcm_whole_period_frames(1024, 0) == 0);

    assert(!spa_pcm_prepare_pending_after_flush(-1, -1));
    assert(!spa_pcm_prepare_pending_after_flush(-1, 0));
    assert(!spa_pcm_prepare_pending_after_flush(0, 0));
    assert(spa_pcm_prepare_pending_after_flush(0, -1));
    assert(spa_pcm_prepare_pending_after_flush(0, 7));

    /* Model the actual codec/sink granularity.  A 256- or 512-frame
     * remainder must survive a fallback period; it must never be consumed as
     * the prefix of a partly silent fragment. */
    fill += codec_frames;
    take = spa_pcm_whole_period_frames(fill, qsa_frames);
    assert(take == qsa_frames);
    fill -= take;
    assert(fill == 256);

    take = spa_pcm_whole_period_frames(fill, qsa_frames);
    assert(take == 0);
    assert(fill == 256);

    fill += codec_frames;
    take = spa_pcm_whole_period_frames(fill, qsa_frames);
    assert(take == qsa_frames);
    fill -= take;
    assert(fill == 512);

    take = spa_pcm_whole_period_frames(fill, qsa_frames);
    assert(take == 0);
    assert(fill == 512);
}

static void assert_detached_render_pump_policy(void)
{
    const size_t codec_frames = 1024;
    const size_t qsa_frames = 768;
    const size_t high_frames = 12000;
    size_t fill = 19200;
    size_t iteration;

    assert(!spa_pcm_prime_ready(0, 19200));
    assert(!spa_pcm_prime_ready(19199, 19200));
    assert(spa_pcm_prime_ready(19200, 19200));
    assert(spa_pcm_prime_ready(7680, 7680));
    assert(!spa_pcm_prime_ready(7679, 7680));
    assert(!spa_pcm_prime_ready(19200, 0));

    /* The production cold-start values deliberately have prime > high.  A
     * SETRATE=1 FIFO drain must cross 250 ms and reach the 400-ms bootstrap
     * target before steady-state high-water backpressure can stop it. */
    assert(spa_pcm_queue_should_decode(0, high_frames, 19200, 0));
    assert(spa_pcm_queue_should_decode(high_frames, high_frames, 19200, 0));
    assert(spa_pcm_queue_should_decode(19199, high_frames, 19200, 0));
    assert(!spa_pcm_queue_should_decode(19200, high_frames, 19200, 0));
    assert(spa_pcm_queue_should_decode(high_frames - 1, high_frames,
                                       19200, 1));
    assert(!spa_pcm_queue_should_decode(high_frames, high_frames, 19200, 1));
    assert(spa_pcm_queue_should_decode(0, 0, 0, 0));

    {
        size_t queued_fill = 0;
        while (spa_pcm_queue_should_decode(queued_fill, high_frames,
                                           19200, 0))
            queued_fill += codec_frames;
        assert(queued_fill >= 19200);
        assert(queued_fill < 19200 + codec_frames);
        assert(spa_pcm_prime_ready(queued_fill, 19200));
        assert(!spa_pcm_queue_should_decode(queued_fill, high_frames,
                                            19200, 1));
    }

    assert(spa_pcm_pump_should_read_source(0, high_frames, qsa_frames,
                                            high_frames));
    assert(spa_pcm_pump_should_read_source(1, 0, qsa_frames,
                                            high_frames));
    assert(spa_pcm_pump_should_read_source(1, qsa_frames - 1,
                                            qsa_frames, high_frames));
    assert(spa_pcm_pump_should_read_source(1, high_frames - 1,
                                            qsa_frames, high_frames));
    assert(!spa_pcm_pump_should_read_source(1, high_frames,
                                             qsa_frames, high_frames));
    assert(!spa_pcm_pump_should_read_source(1, high_frames + 1024,
                                             qsa_frames, high_frames));

    assert(spa_pcm_pump_poll_timeout_ms(0, high_frames, qsa_frames, 100) == 100);
    assert(spa_pcm_pump_poll_timeout_ms(1, qsa_frames - 1,
                                        qsa_frames, 100) == 100);
    assert(spa_pcm_pump_poll_timeout_ms(1, qsa_frames,
                                        qsa_frames, 100) == 0);

    assert(spa_pcm_source_should_pace(1, 0, 0));
    assert(!spa_pcm_source_should_pace(1, 0, 1));
    assert(!spa_pcm_source_should_pace(1, 1, 0));
    assert(!spa_pcm_source_should_pace(0, 0, 0));

    /* Never splice the independent realtime rail into a buffered item. */
    assert(spa_pcm_period_source(0, 0) == SPA_PCM_PERIOD_FROM_REALTIME);
    assert(spa_pcm_period_source(0, qsa_frames) ==
           SPA_PCM_PERIOD_FROM_REALTIME);
    assert(spa_pcm_period_source(1, 0) == SPA_PCM_PERIOD_FROM_SILENCE);
    assert(spa_pcm_period_source(1, qsa_frames - 1) ==
           SPA_PCM_PERIOD_FROM_BUFFERED);
    assert(spa_pcm_period_source(1, qsa_frames) ==
           SPA_PCM_PERIOD_FROM_BUFFERED);

    {
        const size_t frame_bytes = 6 * sizeof(int16_t);
        size_t head = 900, ring_fill = qsa_frames * 2, reserved = 0;
        size_t accepted;

        assert(spa_pcm_ring_reserve(ring_fill, qsa_frames, &reserved));
        accepted = spa_pcm_write_accepted_frames(0, frame_bytes, reserved);
        assert(spa_pcm_ring_commit(1024, &head, &ring_fill, &reserved,
                                   accepted) == 0);
        assert(head == 900 && ring_fill == qsa_frames * 2 && reserved == 0);

        /* A retry sees the same head.  A half-period write consumes only that
         * prefix and leaves the exact unwritten frames queued. */
        assert(spa_pcm_ring_reserve(ring_fill, qsa_frames, &reserved));
        accepted = spa_pcm_write_accepted_frames(
            (int64_t)(qsa_frames / 2 * frame_bytes), frame_bytes, reserved);
        assert(spa_pcm_ring_commit(1024, &head, &ring_fill, &reserved,
                                   accepted) == qsa_frames / 2);
        assert(head == 260);
        assert(ring_fill == qsa_frames + qsa_frames / 2);
        assert(reserved == 0);

        assert(spa_pcm_ring_reserve(ring_fill, qsa_frames, &reserved));
        accepted = spa_pcm_write_accepted_frames(
            (int64_t)(qsa_frames * frame_bytes), frame_bytes, reserved);
        assert(spa_pcm_ring_commit(1024, &head, &ring_fill, &reserved,
                                   accepted) == qsa_frames);
        assert(head == 4 && ring_fill == qsa_frames / 2 && reserved == 0);

        assert(spa_pcm_write_accepted_frames(-1, frame_bytes, qsa_frames) == 0);
        assert(spa_pcm_write_accepted_frames((int64_t)frame_bytes - 1,
                                             frame_bytes, qsa_frames) == 0);
    }

    /* Model a permanently-ready source.  Unlike drain-to-empty, one source
     * packet plus at most one sink period per iteration converges to a narrow
     * sawtooth around 250 ms and never consumes the reserve. */
    for (iteration = 0; iteration < 1000; iteration++) {
        if (spa_pcm_pump_should_read_source(1, fill, qsa_frames,
                                            high_frames))
            fill += codec_frames;
        if (fill >= qsa_frames)
            fill -= qsa_frames;
        assert(fill <= 19200);
        if (iteration > 32) {
            assert(fill >= high_frames - qsa_frames);
            assert(fill < high_frames + codec_frames);
        }
    }
}

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

    for (frame = 0; frame < TEST_TRACK_FRAMES; frame++) {
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

static void assert_stereo_synthesis_present(const int16_t *output,
                                            size_t frames)
{
    int have_lfe = 0;
    int have_rear = 0;
    size_t frame;

    for (frame = 0; frame < frames; frame++) {
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

static void render_stereo_track(struct spa_stereo_upmix *upmix,
                                int16_t *output)
{
    size_t offset = 0;

    while (offset < TEST_TRACK_FRAMES) {
        size_t frames = TEST_TRACK_FRAMES - offset;

        if (frames > SPA_STEREO_UPMIX_MAX_FRAMES)
            frames = SPA_STEREO_UPMIX_MAX_FRAMES;
        assert(render_cookie_pcm(
                   upmix, 0, 2, 0, g_stereo + offset * 2, frames,
                   output + offset * 6) ==
               SPA_DECODED_PCM_ACTION_STEREO_UPMIX);
        offset += frames;
    }
    assert_stereo_synthesis_present(output, TEST_TRACK_FRAMES);
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

static void run_switch_sequence(enum spa_stereo_upmix_backend backend)
{
    static const int setup_indices[2] = { 65, 23 };
    struct spa_stereo_upmix_config config;
    size_t sequence;

    spa_stereo_upmix_default_config(&config);
    spa_stereo_upmix_config_set_lfe(&config, 1);
    config.backend = backend;
    assert(spa_stereo_upmix_init(&g_upmix, &config) ==
           SPA_STEREO_UPMIX_OK);

    for (sequence = 0;
         sequence < sizeof(setup_indices) / sizeof(setup_indices[0]);
         sequence++) {
        int16_t *stereo_output = sequence == 0 ?
            g_first_stereo_render : g_second_stereo_render;

        /* Exercise both orders: SETUP=65 begins with AAC, SETUP=23 begins
         * with APAC. Neither stale SETUP value may influence the action. */
        if (setup_indices[sequence] == 65) {
            render_stereo_track(&g_upmix, stereo_output);
            assert(spa_stereo_upmix_reset(&g_upmix) ==
                   SPA_STEREO_UPMIX_OK);
            assert_native_reorder(&g_upmix);
        } else {
            assert_native_reorder(&g_upmix);
            assert(spa_stereo_upmix_reset(&g_upmix) ==
                   SPA_STEREO_UPMIX_OK);
            render_stereo_track(&g_upmix, stereo_output);
        }
        assert(spa_stereo_upmix_reset(&g_upmix) == SPA_STEREO_UPMIX_OK);
    }

    /* Every cookie boundary reset must make the first AAC block independent
     * of which APAC/AAC track preceded it. */
    assert(memcmp(g_first_stereo_render, g_second_stereo_render,
                  sizeof(g_first_stereo_render)) == 0);
}

int main(void)
{
    assert_buffered_receiver_sizes();
    assert_whole_period_policy();
    assert_detached_render_pump_policy();
    fill_stereo();
    run_switch_sequence(SPA_STEREO_UPMIX_SCENE);
    puts("buffered_pcm_switch_test: OK "
         "(receiver sizes, whole QSA periods, detached reserve pump, fast flush drain, "
         "single-owner gap policy, SETUP 65/23, "
         "AAC <-> APAC, scene)");
    return 0;
}
