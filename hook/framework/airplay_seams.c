/*
 * Framework-owned stock libairplay interpositions.
 *
 * Three entry points are wanted by more than one module, and an ELF symbol has
 * exactly one definition in this shared object.  They used to be defined in
 * altscreen_hook.c, which therefore called eight audio functions on the audio
 * module's behalf.  Here the seam belongs to no module: it calls stock, then
 * hands the same objects to every module that declared the matching callback
 * in hook_airplay_seams_t, in module priority order.
 *
 * Nothing in this file knows what any module does with them.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "hook_framework.h"
#include "state_trace.h"

DEFINE_LOG_MODULE(SEAM);

/* Assigned before stock runs, so a late completion from a superseded RTSP
 * session can be recognised as stale by any module that caches per-session
 * state (altScreen's display advert, the audio capture files). */
static volatile uint32_t g_setup_sequence;
#if ENABLE_STATE_TRACE
static pthread_mutex_t g_airplay_trace_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_airplay_trace_session;
static uint32_t g_airplay_trace_generation;
static uint32_t g_airplay_trace_setup_ordinal;

static uint32_t seam_trace_note_session(void *session, int *is_new,
                                        uint32_t *setup_ordinal)
{
    uint32_t generation;
    pthread_mutex_lock(&g_airplay_trace_lock);
    *is_new = 0;
    if (session && session != g_airplay_trace_session) {
        g_airplay_trace_session = session;
        g_airplay_trace_generation++;
        if (!g_airplay_trace_generation) g_airplay_trace_generation++;
        g_airplay_trace_setup_ordinal = 0;
        *is_new = 1;
    }
    *setup_ordinal = ++g_airplay_trace_setup_ordinal;
    generation = g_airplay_trace_generation;
    pthread_mutex_unlock(&g_airplay_trace_lock);
    return generation;
}

static uint32_t seam_trace_current_session(void *session)
{
    uint32_t generation;
    pthread_mutex_lock(&g_airplay_trace_lock);
    generation = session && session == g_airplay_trace_session ?
                 g_airplay_trace_generation : 0;
    pthread_mutex_unlock(&g_airplay_trace_lock);
    return generation;
}

static uint32_t seam_trace_end_session(void *session, int full)
{
    uint32_t generation;
    pthread_mutex_lock(&g_airplay_trace_lock);
    generation = session && session == g_airplay_trace_session ?
                 g_airplay_trace_generation : 0;
    if (full && generation) {
        g_airplay_trace_session = 0;
        g_airplay_trace_setup_ordinal = 0;
    }
    pthread_mutex_unlock(&g_airplay_trace_lock);
    return generation;
}
#endif

static uint64_t seam_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* Every seam enters the framework the way the Cinemo boundaries do.  By the
 * time AirPlay runs, iAP2 has long since initialised the framework; calling
 * again is idempotent and guarantees the module table is populated even if
 * some future build reaches AirPlay first. */
static void seam_enter(void)
{
    hook_framework_init();
}

/* ── AirPlayCopyServerInfo ──────────────────────────────────────────────────
 * Returns the mutable /info dictionary stock just built, immediately before
 * dio serialises it into the HTTP response the phone reads.  Modules mutate it
 * in priority order; the _final pass runs after every mutation, for modules
 * that capture the finished object. */
typedef void *(*fn_copyinfo_t)(void *, void *, void *, void *);

HOOK_EXPORT void *AirPlayCopyServerInfo(void *session, void *qualifier, void *macAddr, void *outErr)
{
    static fn_copyinfo_t real;
    void *info;
    size_t i, n;

    seam_enter();
    if (!real) real = (fn_copyinfo_t)dlsym(RTLD_NEXT, "AirPlayCopyServerInfo");
    if (!real) { LOG_ERROR(LOG_MODULE, "AirPlayCopyServerInfo real==NULL"); return 0; }

    info = real(session, qualifier, macAddr, outErr);

    /* Unconditional: this is the ONLY seam that proves the phone reached us over
     * the carplay0 IP link.  Everything a module does below runs after control
     * SETUP, so a generation that never starts CarPlay used to leave no trace
     * here at all.  A failing generation WITH this line means NCM/HTTP are up
     * and the phone read /info and then declined CarPlay (protocol/content —
     * ours); a failing generation WITHOUT it means the phone never reached us
     * (cable/pairing). */
    {
        static int info_calls;
        LOG_INFO(LOG_MODULE, "/info #%d session=%p qualifier=%p out=%p",
                 ++info_calls, session, qualifier, info);
        STATE_TRACE("AIRPLAY_INFO",
                    "air_gen=%u session=%p response=%d call=%d",
                    (unsigned)seam_trace_current_session(session), session,
                    info ? 1 : 0, info_calls);
    }
    if (!info) return info;

    n = hook_framework_module_count();
    for (i = 0; i < n; i++) {
        const hook_module_def_t *def = hook_framework_module_at(i);
        if (def && def->airplay.on_server_info) def->airplay.on_server_info(session, info);
    }
    for (i = 0; i < n; i++) {
        const hook_module_def_t *def = hook_framework_module_at(i);
        if (def && def->airplay.on_server_info_final)
            def->airplay.on_server_info_final(session, info);
    }
    return info;
}

/* ── AirPlayReceiverSessionSetup ────────────────────────────────────────────
 * Modules see the request before stock (the only chance to record what stock
 * is about to reject), then the response.  `result` is threaded through every
 * callback exactly the way the old single wrapper threaded its local `ret`: a
 * module gates its own work on setup->result == 0 and writes a non-zero error
 * into it when it cannot fulfil the contract it just advertised. */
typedef int (*fn_sess_setup_t)(void *, void *, void **, int);

HOOK_EXPORT int AirPlayReceiverSessionSetup(void *session, void *request, void **respOut, int a4)
{
    static fn_sess_setup_t real;
    hook_setup_ctx_t setup;
    uint64_t beginMs, realDoneMs, doneMs;
    unsigned long realMs, hookMs;
#if ENABLE_STATE_TRACE
    uint32_t airplayGeneration, setupOrdinal;
    int newSession;
#endif
    size_t i, n;

    seam_enter();
    if (!real) real = (fn_sess_setup_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionSetup");
    if (!real) { LOG_ERROR(LOG_MODULE, "SessionSetup real==NULL"); return -6740; }

    memset(&setup, 0, sizeof(setup));
    setup.session = session;
    setup.request = request;
    setup.sequence = __sync_add_and_fetch(&g_setup_sequence, 1);
#if ENABLE_STATE_TRACE
    airplayGeneration = seam_trace_note_session(session, &newSession,
                                                &setupOrdinal);
#endif
    STATE_TRACE("AIRPLAY_SETUP_BEGIN",
                "setup_seq=%u air_gen=%u setup_ordinal=%u session_edge=%s "
                "session=%p request=%d",
                (unsigned)setup.sequence, (unsigned)airplayGeneration,
                (unsigned)setupOrdinal, newSession ? "new" : "existing",
                session, request ? 1 : 0);

    n = hook_framework_module_count();
    for (i = 0; i < n; i++) {
        const hook_module_def_t *def = hook_framework_module_at(i);
        if (def && def->airplay.on_setup_request) def->airplay.on_setup_request(&setup);
    }

    beginMs = seam_now_ms();
    setup.result = real(session, request, respOut, a4);
    realDoneMs = seam_now_ms();

    /* Run even when stock produced no response: recording that stock rejected
     * a stream outright is exactly what the type-103 work needed.  A callback
     * that needs the dictionary checks setup->response itself. */
    setup.response = (respOut && *respOut) ? *respOut : NULL;
    for (i = 0; i < n; i++) {
        const hook_module_def_t *def = hook_framework_module_at(i);
        if (def && def->airplay.on_setup_response) def->airplay.on_setup_response(&setup);
    }
    for (i = 0; i < n; i++) {
        const hook_module_def_t *def = hook_framework_module_at(i);
        if (def && def->airplay.on_setup_response_final)
            def->airplay.on_setup_response_final(&setup);
    }

    doneMs = seam_now_ms();
    realMs = (beginMs && realDoneMs >= beginMs) ? (unsigned long)(realDoneMs - beginMs) : 0;
    hookMs = (realDoneMs && doneMs >= realDoneMs) ? (unsigned long)(doneMs - realDoneMs) : 0;
    LOG_INFO(LOG_MODULE,
             "STARTUP SETUP timing seq=%u real=%lums hook=%lums total=%lums ret=%d",
             (unsigned)setup.sequence, realMs, hookMs, realMs + hookMs, setup.result);
    STATE_TRACE("AIRPLAY_SETUP_DONE",
                "setup_seq=%u air_gen=%u setup_ordinal=%u result=%d "
                "response=%d real_ms=%lu hook_ms=%lu",
                (unsigned)setup.sequence, (unsigned)airplayGeneration,
                (unsigned)setupOrdinal, setup.result,
                setup.response ? 1 : 0, realMs, hookMs);
    return setup.result;
}

/* ── AirPlayReceiverSessionTearDown ─────────────────────────────────────────
 * A non-NULL request is a per-stream teardown and a module must check whether
 * the request names its own stream; a NULL request tears down the whole
 * session.  Every module runs BEFORE stock, because stock frees the session
 * fields the modules' receiver threads are still reading.
 *
 * Deliberately NO HOOK_EVENT_DISCONNECT here.  A full AirPlay teardown is an
 * AirPlay-layer event; the iAP2 link (and thus RGD component_valid) survives
 * it.  Firing DISCONNECT would clear RGD state that only a new iAP2 Identify
 * restores, stranding RGI dead across an AirPlay restart.  The authoritative
 * per-session reset stays on IAP2_MSG_IDENTIFY_START. */
typedef int (*fn_sess_teardown_t)(void *, void *, int, int);

HOOK_EXPORT int AirPlayReceiverSessionTearDown(void *session, void *request, int reason, int a4)
{
    static fn_sess_teardown_t real;
    size_t i, n;

    seam_enter();
    if (!real) real = (fn_sess_teardown_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionTearDown");

    STATE_TRACE("AIRPLAY_TEARDOWN",
                "air_gen=%u scope=%s session=%p reason=%d request=%d",
                (unsigned)seam_trace_end_session(session, request == NULL),
                request ? "stream" : "full", session, reason,
                request ? 1 : 0);

    n = hook_framework_module_count();
    for (i = 0; i < n; i++) {
        const hook_module_def_t *def = hook_framework_module_at(i);
        if (def && def->airplay.on_teardown) def->airplay.on_teardown(session, request, reason);
    }

    if (!real) return reason;
    return real(session, request, reason, a4);
}
