#include "altscreen_sink_fixture.h"

/* ---- wedged-core fatal harness ------------------------------------------- */
static const char *test_fatal_path(void) { return getenv("ALT_FATAL_STATE"); }
static void write_marker(unsigned deaths, unsigned long long last_ms) {
    FILE *f = fopen(test_fatal_path(), "w");
    assert(f);
    fprintf(f, "v1 %llu %u test\n", last_ms, deaths);
    fclose(f);
}
static char marker_reason[96];
static unsigned read_marker_deaths(void) {
    FILE *f = fopen(test_fatal_path(), "r");
    char tag[8] = {0}, *reason = marker_reason;
    memset(marker_reason, 0, sizeof(marker_reason));
    unsigned long long last = 0;
    unsigned deaths = 0;
    int ok;
    if (!f) return 0;
    ok = fscanf(f, "%7s %llu %u %95s", tag, &last, &deaths, reason) == 4;
    fclose(f);
    return ok ? deaths : 0;
}
static void clear_fatal_state(void) {
    unlink(test_fatal_path());
    unlink(getenv("ALT_FATAL_SLOG"));
}
static void *fatal_exit_a(void *unused) {
    (void)unused;
    av_sink_fatal_exit(86, "double_a");
    return NULL;
}
static void *fatal_exit_b(void *unused) {
    (void)unused;
    av_sink_fatal_exit(86, "double_b");
    return NULL;
}

/* The whole wedged-core contract on top of the live sink code.  Saves and
 * restores the sink/counter state: later sections continue from the healthy
 * started-decoder fixture this file set up before calling us. */
static void fatal_hardening(void) {
    const uint8_t config[] = {0,0,0,1,0x67,0x11,0,0,1,0x68,0x22};
    unsigned before_inits, base_submissions;
    int status;
    pid_t child;
    ScreenSink saved = g_ss;
    unsigned saved_inits = init_attempts, saved_submissions = submissions;

    /* (1) Vendor Timeout/Hardware/Invalid classify ONLY while starting or
     * decoding.  During OUR teardown the component is being forced down and
     * its errors are renderer-local (InvalidState 0x8000100A included). */
    g_ss.coreFatal = 0;
    g_ss.callbacksClosing = 1;
    _OnEvent(&component, &g_ss, OMX_EventError, 0x8000100A, 0, NULL);
    _OnEvent(&component, &g_ss, OMX_EventError, 0x8000101A, 0, NULL);
    assert(!g_ss.coreFatal); /* latched as omxError, not classified */
    assert(g_ss.omxError);
    g_ss.callbacksClosing = 0;
    _OnEvent(&component, &g_ss, OMX_EventError, 0x80001011, 0, NULL);
    assert(g_ss.coreFatal);
    g_ss.coreFatal = 0;
    g_ss.omxError = 0;

    /* (2) coreFatal -> guarded fatal exit; never any OMX teardown call.  The
     * fixture's FreeHandle/GetHandle stubs assert(0), so a single component
     * touch would abort the child with SIGABRT instead of a clean 86. */
    clear_fatal_state();
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        g_ss.coreFatal = 1;
        g_ss.decoder = &component;
        g_ss.started = g_ss.running = 0;
        av_sink_frame(config, sizeof(config), 1000);
        _exit(55); /* must not reach */
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 86);
    assert(read_marker_deaths() == 1 && !strcmp(marker_reason, "core_fatal"));
    clear_fatal_state();

    /* (2b) The core dies mid-session and the phone leaves before the next
     * frame: session end must not tear down OMX on it nor forget the death. */
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        g_ss.coreFatal = 0;
        g_ss.callbacksClosing = 0;
        g_ss.decoder = &component;
        g_ss.started = g_ss.running = 1;
        _OnEvent(&component, &g_ss, OMX_EventError, 0x80001011, 0, NULL);
        component.SendCommand = NULL; /* any OMX touch crashes the child */
        av_sink_session_end();
        _exit(55); /* must not reach */
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 86);
    assert(read_marker_deaths() == 1 && !strcmp(marker_reason, "core_fatal"));
    clear_fatal_state();

    /* (2c) Start-phase classification: only a state edge that never comes
     * (-2) is core-class, an ordinary error event (-1) is not. */
    g_ss.omxError = 0; g_ss.stateReached = 0;
    assert(_WaitState(&g_ss, OMX_StateIdle, 20) == -2);
    assert(_WaitState(&g_ss, OMX_StateIdle, 20) == -1); /* error latched */
    g_ss.omxError = 0;

    /* (3) Wedged teardown: the OMX section runs on a helper under the
     * deadline; a blocked SendCommand must end in marker/evidence/_exit(86)
     * with the stage in the reason, and FreeHandle/OMX_Deinit must never run
     * (FreeHandle stub asserts). */
    clear_fatal_state();
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        OMX_BUFFERHEADERTYPE out0 = {0};
        OMX_BUFFERHEADERTYPE *outs[] = {&out0};
        assert(setenv("ALT_TEARDOWN_DEADLINE_MS", "400", 1) == 0);
        block_send_command = 1;
        teardown_freebufs = 0;
        component.SendCommand = stub_send;
        component.FreeBuffer = stub_freebuf;
        g_ss.decoder = &component;
        g_ss.omxInitialized = 1;
        g_ss.started = g_ss.running = 1;
        g_ss.callbacksClosing = 0;
        g_ss.inBufs = inputs; g_ss.nInBufs = 1;
        g_ss.outBufs = outs;  g_ss.nOutBufs = 1;
        _Teardown(&g_ss, 0);
        _exit(55); /* must not reach */
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 86);
    assert(read_marker_deaths() == 1);
    clear_fatal_state();
    block_send_command = 0;
    g_ss.decoder = NULL;
    g_ss.omxInitialized = 0;
    g_ss.started = g_ss.running = 0;

    /* (4) One real death counts once, even when two watchdogs race into the
     * fatal path (88 at 1.5s + 93 at 3.5s on shutdown). */
    clear_fatal_state();
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        pthread_t a, b;
        assert(pthread_create(&a, NULL, fatal_exit_a, NULL) == 0);
        assert(pthread_create(&b, NULL, fatal_exit_b, NULL) == 0);
        pthread_join(a, NULL);
        pthread_join(b, NULL);
        _exit(55);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 86);
    assert(read_marker_deaths() == 1);
    clear_fatal_state();

    /* (5) Backoff / degrade / reboot-reset swallow frames with 0 — the
     * transport, hook IDR requests and STATUS 0 stay alive; only the decoder
     * start is deferred.  Expired backoff and reboot-reset proceed and pay
     * one bounded OMX_Init attempt each. */
    g_ss.started = g_ss.running = 0;
    g_ss.decoder = NULL;
    g_ss.inBufs = NULL; g_ss.freeIn = NULL; g_ss.nInBufs = 0;
    g_ss.outBufs = NULL; g_ss.nOutBufs = 0;
    g_ss.omxInitialized = 0;
    g_ss.startFails = 0; g_ss.retryAfterMs = 0;
    g_ss.coreFatal = 0; g_ss.omxError = 0;
    g_ss.configLen = 0; g_ss.fedConfig = 0; g_ss.needsIDR = 1;
    before_inits = init_attempts;
    base_submissions = submissions;

    write_marker(1, _NowMs());
    assert(av_sink_frame(config, sizeof(config), 1000) == 0);
    assert(init_attempts == before_inits && submissions == base_submissions);

    write_marker(1, _NowMs() - ALT_FATAL_BACKOFF_MS - 1000ULL);
    assert(av_sink_frame(config, sizeof(config), 1000) < 0);
    assert(init_attempts == before_inits + 1);

    write_marker(ALT_FATAL_DEATHS_MAX, _NowMs());
    assert(av_sink_frame(config, sizeof(config), 1000) == 0);
    assert(init_attempts == before_inits + 1);
    clear_fatal_state(); /* degraded is sticky without re-reading the marker */
    assert(av_sink_frame(config, sizeof(config), 1000) == 0);
    assert(init_attempts == before_inits + 1);
    g_fatal_degraded = 0; /* a new process after the reboot */

    /* Future last-death timestamp = CLOCK_MONOTONIC restarted = reboot. */
    write_marker(1, _NowMs() + 86400000ULL);
    assert(av_sink_frame(config, sizeof(config), 1000) < 0);
    assert(init_attempts == before_inits + 2);
    clear_fatal_state();
    g_ss.startFails = 0; g_ss.retryAfterMs = 0;

    /* (6) A presented frame proves recovery: the death budget resets. */
    write_marker(1, _NowMs());
    g_fatal_present_reset_done = 0;
    _FatalNotePresented();
    assert(read_marker_deaths() == 0);
    clear_fatal_state();

    /* Restore the healthy started-decoder fixture for the sections that
     * follow.  The struct copy carries the sync primitives' bytes; since no
     * thread held the lock at snapshot time they are valid, but re-init them
     * anyway. */
    g_ss = saved;
    pthread_mutex_destroy(&g_ss.lock);
    pthread_mutex_init(&g_ss.lock, NULL);
    pthread_cond_destroy(&g_ss.cond);
    _CondInit(&g_ss.cond);
    init_attempts = saved_inits;
    submissions = saved_submissions;
    clear_fatal_state();
}

int main(void) {
    const uint8_t config[] = {0,0,0,1,0x67,0x11,0,0,1,0x68,0x22};
    const uint8_t next_config[] = {0,0,0,1,0x67,0x33,0,0,1,0x68,0x44};
    const uint8_t idr[] = {0,0,0,1,0x65,0x55};
    const uint8_t pframe[] = {0,0,0,1,0x41,0x66};
    uint8_t mixed[sizeof(next_config) + sizeof(idr)];
    unsigned before;
    alarm(15);
    assert(setenv("ALT_FATAL_STATE", "/tmp/altscreen_sink_test_fatal.txt", 1) == 0);
    assert(setenv("ALT_FATAL_SLOG", "/tmp/altscreen_sink_test_slog.txt", 1) == 0);
    clear_fatal_state();
    callback_thread = pthread_self();
    pthread_mutex_init(&g_ss.lock, NULL);
    _CondInit(&g_ss.cond); /* same clock as the sink's deadlines (CLOCK_MONOTONIC off macOS) */
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
    assert(!g_ss.coreFatal); /* an ETB ErrorHardware RETURN is not a core event */
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
    fatal_hardening();

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
    clear_fatal_state();
    g_ss.started = g_ss.running = 0;
    g_ss.decoder = NULL;
    g_ss.inBufs = NULL; g_ss.freeIn = NULL; g_ss.nInBufs = 0;
    for (unsigned i = 0; i < 3; ++i) {
        assert(av_sink_frame(config, sizeof(config), 1000) < 0);
        av_sink_session_end();
    }
    assert(init_attempts == 3 && g_ss.startFails == 3);
    assert(av_sink_frame(config, sizeof(config), 1000) < 0 && init_attempts == 3);
    assert(!g_ss.coreFatal);
    clear_fatal_state();
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
    puts("altscreen_sink_test: config/IDR, rejection, cooldown, delayed EmptyDone, steady decoder latency, native OMX ABI, async ownership/stop/faults, core-fatal marker/backoff/degrade/one-shot-guard PASS");
    return 0;
}
