#include "altscreen_sink_fixture.h"

int main(void) {
    const uint8_t config[] = {0,0,0,1,0x67,0x11,0,0,1,0x68,0x22};
    const uint8_t next_config[] = {0,0,0,1,0x67,0x33,0,0,1,0x68,0x44};
    const uint8_t idr[] = {0,0,0,1,0x65,0x55};
    const uint8_t pframe[] = {0,0,0,1,0x41,0x66};
    uint8_t mixed[sizeof(next_config) + sizeof(idr)];
    unsigned before;
    alarm(15);
    callback_thread = pthread_self();
    pthread_mutex_init(&g_ss.lock, NULL);
    pthread_cond_init(&g_ss.cond, NULL);
    component.EmptyThisBuffer = submit;
    g_ss.decoder = &component;
    g_ss.started = g_ss.running = 1;
    g_ss.inBufs = inputs;
    g_ss.nInBufs = 1;
    g_ss.freeIn = free_input;
    g_ss.freeInTail = 1;
    input.pBuffer = storage;
    input.nAllocLen = sizeof(storage);

    assert(av_sink_frame(config, sizeof(config), 1000) == 0);
    assert(submissions == 1 && submitted_len == sizeof(config));
    assert(submitted_flags & OMX_BUFFERFLAG_CODECCONFIG);
    assert(submitted_time == 0 && g_ss.newestInputMs == 0);
    assert(g_ss.needsIDR);
    assert(av_sink_frame(pframe, sizeof(pframe), 1000) < 0 && submissions == 1);
    assert(av_sink_frame(idr, sizeof(idr), 1000) == 0 && !g_ss.needsIDR);
    assert(submitted_time == 1000000 && g_ss.newestInputMs == 1000);

    /* A repeated avcC must not disturb the decoder after its first IDR. A
     * genuinely changed codec needs a fresh transport/decoder, never a live
     * CODECCONFIG submission into Qualcomm's reference chain. */
    before = submissions;
    assert(av_sink_frame(config, sizeof(config), 1000) == 0);
    assert(submissions == before && !g_ss.needsIDR);
    assert(av_sink_frame(next_config, sizeof(next_config), 1000) < 0);
    assert(submissions == before && !g_ss.needsIDR);
    /* A new generation receives the new config before its first IDR. */
    g_ss.configLen = 0; g_ss.fedConfig = 0; g_ss.needsIDR = 1;
    assert(av_sink_frame(next_config, sizeof(next_config), 1000) == 0);
    assert(submissions == before + 1 && submitted_len == sizeof(next_config));
    assert(!memcmp(submitted, next_config, sizeof(next_config)));
    assert(submitted_flags & OMX_BUFFERFLAG_CODECCONFIG);

    /* Parameter sets + picture is a complete picture, not a config-only record. */
    memcpy(mixed, next_config, sizeof(next_config));
    memcpy(mixed + sizeof(next_config), idr, sizeof(idr));
    before = submissions;
    assert(av_sink_frame(mixed, sizeof(mixed), 1000) == 0);
    assert(submissions == before + 1 && submitted_len == sizeof(mixed));
    assert(!memcmp(submitted, mixed, sizeof(mixed)));
    assert(!(submitted_flags & OMX_BUFFERFLAG_CODECCONFIG) && !g_ss.needsIDR);

    submit_error = OMX_ErrorHardware;
    before = submissions;
    assert(av_sink_frame(pframe, sizeof(pframe), 1000) < 0 && submissions == before);
    assert(g_ss.freeInTail - g_ss.freeInHead == 1); /* rejected buffer is returned */
    submit_error = OMX_ErrorNone;
    input.nAllocLen = 4;
    assert(av_sink_frame(pframe, sizeof(pframe), 1000) < 0 && submissions == before);
    assert(g_ss.freeInTail - g_ss.freeInHead == 1);

    /* EmptyDone can sit behind the first frame's healthy <750 ms loader join.
     * The old 500 ms input deadline killed this working decoder. */
    input.nAllocLen = sizeof(storage);
    g_ss.freeInHead = g_ss.freeInTail;
    pthread_t returning_input;
    assert(!pthread_create(&returning_input, NULL, delayed_empty, NULL));
    assert(av_sink_frame(pframe, sizeof(pframe), 1200) == 0);
    pthread_join(returning_input, NULL);
    assert(!g_ss.omxError && submitted_time == 1200000);
    {
        alt_frame_meta_t meta = {0};
        frame_trace_t *trace;
        meta.version = ALT_WIRE_VERSION;
        meta.generation = 7;
        meta.sequence = 123;
        meta.sender_pts = UINT64_C(0x123456789abcdef0);
        meta.received_us = alt_timing_now_us();
        assert(av_sink_frame_ex(pframe, sizeof(pframe), 1200, &meta) == 0);
        trace = _FindFrameTrace(&g_ss, submitted_time);
        assert(trace && trace->meta.generation == 7 && trace->meta.sequence == 123);
        assert(trace->meta.sender_pts == meta.sender_pts && !g_ss.incoming_meta.version);
    }
    omx_contract();
    presentation_latency();
    presentation_queue();
    presentation_faults();

    /* State notifications without a returned buffer must not extend the input
     * deadline indefinitely. GPU ownership still has its separate watchdog. */
    g_ss.freeInHead = g_ss.freeInTail;
    uint64_t wait_begin = _NowMs();
    assert(!pthread_create(&returning_input, NULL, spurious_wakes, NULL));
    assert(av_sink_frame(pframe, sizeof(pframe), 1300) < 0 && g_ss.omxError);
    uint64_t waited = _NowMs() - wait_begin;
    assert(waited >= 2900 && waited < 3800); /* ALT_INPUT_WAIT_MS = 3000 */
    pthread_join(returning_input, NULL);

    /* Failed starts report loss of continuity and preserve the bounded retry
     * budget across transport sessions that never produced a working decoder. */
    g_ss.started = g_ss.running = 0;
    g_ss.decoder = NULL;
    g_ss.inBufs = NULL; g_ss.freeIn = NULL; g_ss.nInBufs = 0;
    for (unsigned i = 0; i < 3; ++i) {
        assert(av_sink_frame(config, sizeof(config), 1000) < 0);
        av_sink_session_end();
    }
    assert(init_attempts == 3 && g_ss.startFails == 3);
    assert(av_sink_frame(config, sizeof(config), 1000) < 0 && init_attempts == 3);
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
    puts("altscreen_sink_test: config/IDR, rejection, cooldown, delayed EmptyDone, steady decoder latency, native OMX ABI, async ownership/stop/faults PASS");
    return 0;
}
