/*
 * altscreen_render — QNX cluster sink: OMX AVC decode -> EGLImage -> RGBA present.
 *
 * Standalone receiver output path, matched against stock libairplay 210 and
 * verified directly by the MU1316 runtime logs, with no libairplay wrapper:
 *   - no CFRuntime/ulog/require_* — plain struct + fprintf + goto.
 *   - INPUT IS ALREADY ANNEX-B: the hook (altscreen_av.c) converts AVCC→Annex-B and
 *     forwards SPS/PPS first, then frames.  So NO avcC handling / H264 helpers here —
 *     we feed the received bytes straight to OMX_EmptyThisBuffer (first buffer flagged
 *     CODECCONFIG, the SPS/PPS header the hook sends before the first frame).
 *   - GEOMETRY comes from the cluster size WE advertised in /info (env ALTSCREEN_W/H,
 *     default 1440x542).  The MU1316/APQ8064 decoder reaches Idle only with native
 *     Qualcomm 32m tiled output (OMX 0x7f000004 / Screen 0x1000c).  OMX remains
 *     bound to native Screen WINDOW buffers exactly like stock libairplay 210.
 *     After FillBufferDone, EGL imports that exact Screen buffer as an external
 *     image. Adreno de-tiles and converts YUV->RGB while drawing into the proven
 *     RGBA8888 plane-99 EGL window.
 *
 * BUILD: needs the QNX Screen headers/lib + OMX (MHI2Q BSP) — links on the unit/VM,
 * NOT in the generic docker SDP. A host syntax-check (make hostcheck) compiles this
 * against stub headers to catch C errors offline. The native 32m decode and the
 * EGL external-image draw have both run on-unit; exact-usage chroma is the next test.
 *
 * QNX 6.5 ARMv7 gcc: no __thread; globals + pthread.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "sink.h"
#include "log_stamp.h"

#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_Video.h"

#include "screen_egl_present.h"
#include "pipeline_timing.h"
#include "../common/alt_timing_stats.h"
#include "loader/loader_layout.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stddef.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#if defined(__QNX__)
#include <sys/neutrino.h>   /* setprio() */
#endif

#define kOMXDecoderName   "OMX.qcom.video.decoder.avc"
#define kInPortIndex      0
#define kOutPortIndex     1

/* Native output is mandatory on this decoder.  Requesting linear NV12 (0x27)
 * is accepted by SetParameter but the Loaded->Idle transition never completes. */
#define kQOMX_ColorFmt_32m            0x7F000004
#define kQOMX_ExtIndex_DisplayPicBuf  "OMX.QCOM.index.config.video.DisplayPictureBuffer"

typedef struct {
    OMX_TICKS stamp;
    uint64_t submitted_us;
    alt_frame_meta_t meta;
} frame_trace_t;
#define FRAME_TRACE_CAPACITY 128u

typedef struct {
    OMX_HANDLETYPE           decoder;
    OMX_BUFFERHEADERTYPE   **inBufs;   OMX_U32 nInBufs;
    OMX_BUFFERHEADERTYPE   **outBufs;  OMX_U32 nOutBufs;
    OMX_U32                 *freeIn;   OMX_U32 freeInHead, freeInTail;

    int32_t                  width, height;
    screen_egl_present_t     *present;   /* tiled Screen windows + GPU RGBA output */
    OMX_U32                  presentOutCount;
    int32_t                  presentWidth, presentHeight;
    volatile int             winReady;   /* read lock-free on the OMX callback thread */

    pthread_mutex_t          lock;
    pthread_cond_t           cond;
    volatile OMX_STATETYPE   state;
    volatile int             stateReached;
    volatile int             omxError;
    /* Vendor Timeout(0x80001011)/Hardware(0x80001009)/Invalid(0x8000100A)
     * while starting or decoding: the core itself reported, teardown is
     * pointless and every further OMX call risks the wedged-CS hang. */
    volatile int             coreFatal;
    volatile int             running;
    int                      callbacksClosing;
    unsigned int             callbacksActive;
    /* Each slot is owned by OMX OR queued/in flight on the presenter, never
     * both. Capacity equals the finite native output pool; no frame copies. */
    pthread_t                presentThread;
    int                      presentThreadStarted, presentStop, presentExited;
    OMX_U32                 *presentQueue, presentHead, presentCount;
    unsigned char           *presentOwned;
    uint64_t *presentQueuedUs;
    frame_trace_t trace[FRAME_TRACE_CAPACITY];
    unsigned trace_next;
    uint64_t trace_last_stamp;
    alt_frame_meta_t incoming_meta;
    alt_timing_stat_t input_wait_timing, decode_timing, present_queue_timing;
    alt_timing_stat_t recycle_timing, hook_to_post_timing;
    uint64_t timing_last_sequence;
    uint32_t timing_generation;
    int timing_enabled;
    int                      omxInitialized;
    int mailbox;
    unsigned long outputSuperseded;
    int                      started;   /* decoder brought up on first frame */
    int                      sessionEverStarted;
    int                      startFails; /* consecutive _StartDecoder failures; capped to protect OMX pool */
    unsigned long long       retryAfterMs;
    int                      fedConfig; /* first buffer (SPS/PPS) flagged CODECCONFIG */
    int                      needsIDR;  /* every new decoder/config needs random access */

    /* Decisive pipeline diagnostics.  Decoder creation alone proves neither that
     * input was accepted nor that a decoded Screen buffer was posted. */
    unsigned long            inputSubmitted;
    unsigned long            inputReturned;
    unsigned long            outputFilled;
    unsigned long            outputPosted;
    unsigned long            outputPostFailed;
    uint64_t                 newestInputMs; /* protected by lock; QNX is 32-bit */
    int                      presentationHealthy;
    int                      presentConsecutiveFails;
    int                      presentRecoveryPending;
    int                      presentRecoveryAttempts;
    int                      presentNeedsFullRebuild;
    unsigned long long       presentRecoveryAfterMs;
    unsigned long            bitstreamErrors;
    int                      bitstreamStallArmed;
    unsigned long            bitstreamStallInputBase;
    unsigned long            bitstreamStallInFlight;
    unsigned long long       bitstreamStallSinceMs;
    uint8_t                  config[4096];
    uint32_t                 configLen;
    volatile int             viewAreaMode; /* control thread -> loader: 0=fullscreen, 1=smallscreen */
    volatile int             layoutStyle;  /* 0=Sport/driving-profile, 1=Classic */
} ScreenSink;

static ScreenSink g_ss;
static unsigned long long _NowMs(void);
static void _AbsTimeoutMs(struct timespec *ts, long ms);
static void _FatalNotePresented(void); /* first presented frame resets deaths */

/* Data callbacks dereference OMX/Screen arrays which teardown frees.  Close the
 * callback gate first, then join the presenter before changing OMX state or
 * releasing arrays. Event callbacks remain enabled for _WaitState. */
static int _CallbackEnter(ScreenSink *me) {
    int allowed;
    pthread_mutex_lock(&me->lock);
    allowed = !me->callbacksClosing;
    if (allowed) me->callbacksActive++;
    pthread_mutex_unlock(&me->lock);
    return allowed;
}

static void _CallbackLeave(ScreenSink *me) {
    pthread_mutex_lock(&me->lock);
    if (me->callbacksActive > 0) me->callbacksActive--;
    if (me->callbacksActive == 0) pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
}

/* ---- OMX callbacks -------------------------------------------------------- */
static OMX_ERRORTYPE _OnEvent(OMX_HANDLETYPE c, OMX_PTR app, OMX_EVENTTYPE ev,
                              OMX_U32 d1, OMX_U32 d2, OMX_PTR edata) {
    ScreenSink *const me = (ScreenSink *)app;
    (void)c; (void)edata;
    pthread_mutex_lock(&me->lock);
    if (ev == OMX_EventCmdComplete && d1 == OMX_CommandStateSet) {
        me->state = (OMX_STATETYPE)d2;
        me->stateReached = 1;
    } else if (ev == OMX_EventPortSettingsChanged && d1 == kOutPortIndex) {
        /* Stock 210 ignores this event.  Width/height and native 32m buffers are
         * configured before Loaded->Idle, so rebuilding live output buffers only
         * creates a FillDone/FreeBuffer race on this component. */
        fprintf(stderr, "altscreen_render[qnx]: PortSettingsChanged(out) ignored (stock-210 parity)\n");
    } else if (ev == OMX_EventError) {
        if (me->running && me->started &&
            ((OMX_ERRORTYPE)d1 == OMX_ErrorStreamCorrupt ||
             (OMX_ERRORTYPE)d1 == OMX_ErrorMbErrorsInFrame)) {
            ++me->bitstreamErrors;
            me->presentationHealthy = 0;
            if (!me->bitstreamStallArmed) {
                me->bitstreamStallArmed = 1;
                me->bitstreamStallInputBase = me->inputSubmitted;
                me->bitstreamStallInFlight = me->inputSubmitted >= me->inputReturned
                    ? me->inputSubmitted - me->inputReturned : 0;
                me->bitstreamStallSinceMs = 0;
            }
            fprintf(stderr,
                    "altscreen_render[qnx]: OMX_EventError 0x%x (bitstream error #%lu) -> keep decoding\n",
                    (unsigned)d1, me->bitstreamErrors);
        } else {
            me->omxError = 1;
            /* Vendor codes only while starting/decoding.  During OUR teardown the
             * component legitimately reports InvalidState etc. as it is forced
             * down — that is renderer-local, never "core is dead".  The teardown
             * helper deadline is the wedge detector on that path. */
            if (!me->callbacksClosing &&
                (d1 == 0x80001011u /* vendor Timeout */ ||
                 d1 == 0x80001009u /* Hardware */ ||
                 d1 == 0x8000100Au /* Invalid */)) {
                me->coreFatal = 1;
                fprintf(stderr,
                        "altscreen_render[qnx]: OMX_EventError 0x%x -> CORE FATAL\n",
                        (unsigned)d1);
            } else {
                fprintf(stderr, "altscreen_render[qnx]: OMX_EventError 0x%x\n",
                        (unsigned)d1);
            }
        }
    }
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE _OnEmptyDone(OMX_HANDLETYPE c, OMX_PTR app, OMX_BUFFERHEADERTYPE *b) {
    ScreenSink *const me = (ScreenSink *)app;
    OMX_U32 i;
    (void)c;
    if (!_CallbackEnter(me)) return OMX_ErrorNone;
    pthread_mutex_lock(&me->lock);
    for (i = 0; i < me->nInBufs; ++i)
        if (me->inBufs[i] == b) { me->freeIn[me->freeInTail % me->nInBufs] = i; me->freeInTail++; break; }
    me->inputReturned++;
    if (me->inputReturned <= 3 || ((me->inputReturned % 600) == 0 && log_verbose_marker()))
        fprintf(stderr, "altscreen_render[qnx]: EmptyBufferDone #%lu header=%p matched=%s\n",
                me->inputReturned, (void *)b, (i < me->nInBufs) ? "yes" : "NO");
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    _CallbackLeave(me);
    return OMX_ErrorNone;
}

/* Timestamp association is bounded and diagnostic: it never controls decode
 * order or rejects an old/final picture. Callers hold me->lock. */
static frame_trace_t *_FindFrameTrace(ScreenSink *me, OMX_TICKS stamp) {
    unsigned i;
    for (i = 0; i < FRAME_TRACE_CAPACITY; ++i)
        if (stamp > 0 && me->trace[i].stamp == stamp)
            return &me->trace[i];
    return NULL;
}

/* The OMX output pointer belongs to a tiled Screen WINDOW buffer.  The GPU
 * external-image draw consumes that exact buffer before the worker recycles it. */
static int _PresentGpu(ScreenSink *me, OMX_BUFFERHEADERTYPE *b) {
    OMX_U32 i;
    int result;
    if (!me->winReady || b->nFilledLen == 0) return -1;
    for (i = 0; i < me->nOutBufs; ++i) {
        if (me->outBufs[i] != b) continue;
        result = screen_egl_present_frame(me->present, (unsigned int)i,
                                          b->nFilledLen);
        pthread_mutex_lock(&me->lock);
        if (result == 0) {
            me->outputPosted++;
            if (me->timing_enabled) {
                frame_trace_t *t = _FindFrameTrace(me, b->nTimeStamp);
                uint64_t now = alt_timing_now_us();
                if (t) {
                    alt_timing_elapsed(&me->hook_to_post_timing, t->meta.received_us, now);
                    me->timing_last_sequence = t->meta.sequence;
                    me->timing_generation = t->meta.generation;
                }
                if (me->outputPosted % 300u == 0) {
                    fprintf(stderr,
                            "altscreen_render[qnx]: timing owner_gen=%u last_seq=%llu mailbox=%d "
                            "superseded=%lu\n",
                            me->timing_generation, (unsigned long long)me->timing_last_sequence,
                            me->mailbox, me->outputSuperseded);
                    alt_timing_report(stderr, "altscreen_render[qnx]", "input_wait",
                                      &me->input_wait_timing);
                    alt_timing_report(stderr, "altscreen_render[qnx]", "decode",
                                      &me->decode_timing);
                    alt_timing_report(stderr, "altscreen_render[qnx]", "present_queue",
                                      &me->present_queue_timing);
                    alt_timing_report(stderr, "altscreen_render[qnx]", "recycle",
                                      &me->recycle_timing);
                    alt_timing_report(stderr, "altscreen_render[qnx]", "hook_to_post",
                                      &me->hook_to_post_timing);
                }
            }
            me->presentationHealthy = 1;
            me->presentConsecutiveFails = 0;
            me->presentRecoveryPending = 0;
            me->presentRecoveryAttempts = 0;
            me->presentRecoveryAfterMs = 0;
            pthread_mutex_unlock(&me->lock);
            _FatalNotePresented(); /* presented frame: the core is alive again */
            return 0;
        }
        me->outputPostFailed++;
        me->presentConsecutiveFails++;
        if (me->presentConsecutiveFails >= 3) {
            if (!me->presentRecoveryPending) {
                me->presentationHealthy = 0;
                me->presentRecoveryPending = 1;
                me->presentRecoveryAfterMs = 0;
                fprintf(stderr,
                        "altscreen_render[qnx]: presentation unhealthy after %d consecutive failures; output-only recovery armed\n",
                        me->presentConsecutiveFails);
            }
        }
        if (me->outputPostFailed <= 3 || (me->outputPostFailed % 120) == 0)
            fprintf(stderr,
                    "altscreen_render[qnx]: EGLImage GPU present FAILED #%lu src=%u filled=%u\n",
                    me->outputPostFailed, (unsigned)i, (unsigned)b->nFilledLen);
        pthread_mutex_unlock(&me->lock);
        return -1;
    }
    fprintf(stderr,
            "altscreen_render[qnx]: decoded output header %p is not bound to a tiled window buffer\n",
            (void *)b);
    return -1;
}

/* Stock libairplay uses a screenRenderThread command queue. Keep the callback
 * short here too: even loader handoff / glFinish / swap cannot block EmptyDone
 * on Qualcomm's component thread. No timer, age cutoff or compressed-frame drop. */
static void *_PresentThread(void *arg) {
    ScreenSink *me = (ScreenSink *)arg;
    pthread_mutex_lock(&me->lock);
    for (;;) {
        OMX_U32 slot;
        OMX_BUFFERHEADERTYPE *b;
        int recycle, timing;
        OMX_ERRORTYPE err;
        while (!me->presentStop && me->presentCount == 0)
            pthread_cond_wait(&me->cond, &me->lock);
        if (me->presentStop) break;
        if (me->mailbox && me->presentCount > 1) {
            OMX_U32 i, keep = me->presentCount - 1, discard;
            int picture = 0;
            /* Snapshot only: a synchronous refill cannot postpone the selected
             * picture forever. Empty outputs never supersede a real picture. */
            for (i = 0; i < me->presentCount; ++i) {
                OMX_U32 queued = me->presentQueue[(me->presentHead + i) % me->nOutBufs];
                if (me->outBufs[queued]->nFilledLen) {
                    keep = i;
                    picture = 1;
                }
            }
            discard = picture ? keep : me->presentCount - 1;
            while (discard && me->presentCount > 1 && !me->presentStop) {
                OMX_U32 old = me->presentQueue[me->presentHead];
                OMX_BUFFERHEADERTYPE *unused_picture = me->outBufs[old];
                me->presentHead = (me->presentHead + 1) % me->nOutBufs;
                --me->presentCount;
                --discard;
                me->presentOwned[old] = 0;
                ++me->outputSuperseded;
                pthread_mutex_unlock(&me->lock);
                /* No GPU has sampled this slot. Ownership returns exactly once;
                 * the in-flight/selected GPU slot is never in this discard set. */
                err = OMX_FillThisBuffer(me->decoder, unused_picture);
                pthread_mutex_lock(&me->lock);
                if (err != OMX_ErrorNone) {
                    me->omxError = 1;
                    me->presentStop = 1;
                    pthread_cond_broadcast(&me->cond);
                    break;
                }
            }
            if (me->presentStop)
                break;
        }
        slot = me->presentQueue[me->presentHead];
        me->presentHead = (me->presentHead + 1) % me->nOutBufs;
        --me->presentCount;
        b = me->outBufs[slot];
        if (me->timing_enabled)
            alt_timing_elapsed(&me->present_queue_timing, me->presentQueuedUs[slot],
                               alt_timing_now_us());
        pthread_mutex_unlock(&me->lock);
        _PresentGpu(me, b); /* completes all GPU reads, including failure paths */
        pthread_mutex_lock(&me->lock);
        recycle = me->running && me->winReady && !me->presentStop;
        timing = me->timing_enabled;
        /* Clear before FillThisBuffer: a component may callback synchronously.
         * Teardown joins this thread before any OMX state/free operation. */
        me->presentOwned[slot] = 0;
        pthread_mutex_unlock(&me->lock);
        if (recycle) {
            uint64_t started = timing ? alt_timing_now_us() : 0;
            err = OMX_FillThisBuffer(me->decoder, b);
            pthread_mutex_lock(&me->lock);
            if (started)
                alt_timing_elapsed(&me->recycle_timing, started, alt_timing_now_us());
            pthread_mutex_unlock(&me->lock);
            if (err != OMX_ErrorNone) {
                pthread_mutex_lock(&me->lock);
                me->omxError = 1;
                me->presentStop = 1;
                pthread_cond_broadcast(&me->cond);
                pthread_mutex_unlock(&me->lock);
                fprintf(stderr, "altscreen_render[qnx]: output recycle failed 0x%x\n", (unsigned)err);
            }
        }
        pthread_mutex_lock(&me->lock);
    }
    /* Pending buffers never reached GPU. Keep them on the client side for the
     * normal Idle->Loaded FreeBuffer sequence; never feed old-session frames. */
    me->presentCount = 0;
    me->presentExited = 1;
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    return NULL;
}

static int _StartPresentThread(ScreenSink *me) {
    if (!me->nOutBufs || me->presentThreadStarted) return -1;
    me->presentQueue = (OMX_U32 *)calloc(me->nOutBufs, sizeof(OMX_U32));
    me->presentOwned = (unsigned char *)calloc(me->nOutBufs, 1);
    me->presentQueuedUs = (uint64_t *)calloc(me->nOutBufs, sizeof(uint64_t));
    if (!me->presentQueue || !me->presentOwned || !me->presentQueuedUs)
        goto fail;
    me->presentHead = me->presentCount = 0;
    me->presentStop = me->presentExited = 0;
    if (pthread_create(&me->presentThread, NULL, _PresentThread, me) != 0) goto fail;
    pthread_mutex_lock(&me->lock);
    me->presentThreadStarted = 1;
    pthread_mutex_unlock(&me->lock);
    return 0;
fail:
    free(me->presentQueue); me->presentQueue = NULL;
    free(me->presentOwned); me->presentOwned = NULL;
    free(me->presentQueuedUs);
    me->presentQueuedUs = NULL;
    return -1;
}

static OMX_ERRORTYPE _OnFillDone(OMX_HANDLETYPE c, OMX_PTR app, OMX_BUFFERHEADERTYPE *b) {
    ScreenSink *const me = (ScreenSink *)app;
    OMX_U32 slot;
    (void)c;
    if (!_CallbackEnter(me)) return OMX_ErrorNone;
    pthread_mutex_lock(&me->lock);
    if (me->running && me->winReady && !me->presentStop && me->presentThreadStarted) {
        for (slot = 0; slot < me->nOutBufs && me->outBufs[slot] != b; ++slot) {}
        if (slot == me->nOutBufs || me->presentOwned[slot] || me->presentCount == me->nOutBufs) {
            /* Never overwrite a queued/live GPU buffer on a broken callback. */
            me->omxError = 1;
            fprintf(stderr, "altscreen_render[qnx]: invalid/duplicate output ownership; decoder restart required\n");
        } else if (b->nFlags & OMX_BUFFERFLAG_DATACORRUPT) {
            /* A component may report a damaged/concealed picture through
             * FillDone without OMX_EventError. Never publish it as healthy
             * video. Keep returned buffers client-owned for normal teardown;
             * the existing health/transport path obtains a fresh config+IDR. */
            me->omxError = 1;
            me->presentationHealthy = 0;
            me->presentStop = 1;
            fprintf(stderr, "altscreen_render[qnx]: corrupt decoded picture flags=0x%x; fresh decoder/IDR required\n",
                    (unsigned)b->nFlags);
        } else {
            uint64_t pictureMs = b->nTimeStamp > 0 ? (uint64_t)b->nTimeStamp / 1000ULL : 0;
            uint64_t nowMs = _NowMs();
            me->presentQueuedUs[slot] = me->timing_enabled ? alt_timing_now_us() : 0;
            if (me->timing_enabled) {
                frame_trace_t *t = _FindFrameTrace(me, b->nTimeStamp);
                if (t)
                    alt_timing_elapsed(&me->decode_timing, t->submitted_us,
                                       me->presentQueuedUs[slot]);
            }
            me->presentOwned[slot] = 1;
            me->presentQueue[(me->presentHead + me->presentCount) % me->nOutBufs] = slot;
            ++me->presentCount;
            ++me->outputFilled;
            if (me->bitstreamStallArmed && b->nFilledLen > 0) {
                if (me->bitstreamStallInFlight > 0) {
                    --me->bitstreamStallInFlight;
                } else if (me->inputSubmitted > me->bitstreamStallInputBase) {
                    me->bitstreamStallArmed = 0;
                    me->bitstreamStallSinceMs = 0;
                    fprintf(stderr, "altscreen_render[qnx]: decoder output continued after bitstream error\n");
                }
            }
            if (me->outputFilled <= 3 || ((me->outputFilled % 600) == 0 && log_verbose_marker()))
                fprintf(stderr, "altscreen_render[qnx]: FillBufferDone #%lu filled=%u queue=%u posted=%lu age=%llu ms\n",
                        me->outputFilled, (unsigned)b->nFilledLen, (unsigned)me->presentCount,
                        me->outputPosted, (unsigned long long)(pictureMs && nowMs >= pictureMs ? nowMs-pictureMs : 0));
        }
        pthread_cond_broadcast(&me->cond);
    }
    pthread_mutex_unlock(&me->lock);
    _CallbackLeave(me);
    return OMX_ErrorNone;
}

static OMX_CALLBACKTYPE kCallbacks = { _OnEvent, _OnEmptyDone, _OnFillDone };

/* ---- helpers -------------------------------------------------------------- */
/* OMX callbacks come from the component's own thread; if it wedges or dies we must
 * NOT wait forever (would hang startup/teardown of the whole service).
 * Bound every CmdComplete/state wait; a timeout is a hard error → latch omxError. */
/* Normal APQ8064 state edges complete in well under a frame.  A wedged OMX
 * transition must not outlive the supervisor's 2 s graceful-stop window and be
 * SIGKILLed with native buffers still bound, so bound each of the two teardown
 * transitions to 750 ms. */
#define kOmxStartIdleWaitMs  10000L  /* APQ8064 may take seconds allocating native 32m */
#define kOmxStartExecWaitMs   2000L
#define kOmxStopWaitMs         900L  /* two stop edges fit the supervisor grace */
#define kCallbackDrainWaitMs  1500L

/* Every timed wait on me->cond uses this deadline, so the condition variable
 * and the deadline share one clock: CLOCK_MONOTONIC where the platform
 * supports it (QNX, Linux), the unit's wall clock jumps from 1970 to the
 * phone's time during a session.  macOS (host tests) keeps CLOCK_REALTIME. */
#if defined(__APPLE__)
#define ALT_SINK_COND_CLOCK CLOCK_REALTIME
#else
#define ALT_SINK_COND_CLOCK CLOCK_MONOTONIC
#endif
static void _CondInit(pthread_cond_t *c) {
#if defined(__APPLE__)
    pthread_cond_init(c, NULL);
#else
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) != 0)
        fprintf(stderr, "altscreen_render[qnx]: monotonic condition unavailable; wall clock waits\n");
    pthread_cond_init(c, &attr);
    pthread_condattr_destroy(&attr);
#endif
}

static void _AbsTimeoutMs(struct timespec *ts, long ms) {
    clock_gettime(ALT_SINK_COND_CLOCK, ts);
    ts->tv_sec  += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) { ts->tv_sec++; ts->tv_nsec -= 1000000000L; }
}

static unsigned long long _NowMs(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL
        + (unsigned long long)ts.tv_nsec / 1000000ULL;
}

/* ---- wedged-video-core fatal path ----------------------------------------
 * Field reports + QEMU run of this exact stack: once the core stops answering
 * (Timeout 0x80001011 / Hardware 0x80001009 / Invalid 0x8000100A, or a hung
 * lazy VCD_Init inside the first GetHandle), client-side OMX calls block
 * forever and each abandoned client burns one of ~4 system decoder slots.
 * Process exit is the only clean abandon, so every abnormal death goes
 * through one guarded path: marker -> slog evidence -> _exit.  The marker
 * lives in /tmp (RAM): it survives renderer restarts and is wiped by the
 * reboot that is the only real cure.  Evidence is RAM-only too: the slog ring
 * dies at reboot, while vidc*.dat are the firmware's own job (tpm video_fatal
 * archives them to /var/log/ana_dumps/ramdumps before cleaning). */
#define ALT_FATAL_DEATHS_MAX   2u
#define ALT_FATAL_BACKOFF_MS   60000ULL
#define ALT_FATAL_SLOG_CAP     (256u * 1024u)
#define ALT_FATAL_SLOG_WAIT_MS 3000L
#define ALT_REASON_MAX         96

static const char *g_omx_stage; /* teardown helper stage; NULL when idle */

static pthread_mutex_t _FatalGuardLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  _FatalGuardCond = PTHREAD_COND_INITIALIZER;
static int             _FatalGuardTaken;

static const char *_FatalStatePath(void) {
    const char *p = getenv("ALT_FATAL_STATE");
    return (p && *p) ? p : "/tmp/carplay_altscreen_fatal.txt";
}

static const char *_FatalSlogPath(void) {
    const char *p = getenv("ALT_FATAL_SLOG");
    return (p && *p) ? p : "/tmp/carplay_altscreen_slog.txt";
}

/* Parse "v1 <last_death_ms> <deaths> <reason>"; tolerant of garbage. */
static int _FatalRead(unsigned *deaths, unsigned long long *last_ms,
                      char *reason, size_t rsz) {
    FILE *f = fopen(_FatalStatePath(), "r");
    unsigned long long last = 0;
    unsigned d = 0;
    char tag[8] = {0}, why[ALT_REASON_MAX] = {0};
    int ok;
    if (!f) return -1;
    ok = fscanf(f, "%7s %llu %u %95s", tag, &last, &d, why) == 4
        && strcmp(tag, "v1") == 0;
    fclose(f);
    if (!ok) return -1;
    *deaths = d;
    *last_ms = last;
    if (reason && rsz) snprintf(reason, rsz, "%s", why);
    return 0;
}

static void _FatalWrite(unsigned deaths, unsigned long long last_ms,
                        const char *reason) {
    FILE *f;
    const char *path = _FatalStatePath();
    if (!reason) reason = "unspecified";
    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "altscreen_render[qnx]: fatal marker write FAILED %s\n",
                path);
        return;
    }
    fprintf(f, "v1 %llu %u %s\n", last_ms, deaths, reason);
    fclose(f);
}

const char *av_sink_omx_stage(void) {
    return g_omx_stage;
}

/* Copy the tail of src into dst, capped.  RAM-only files, best effort. */
static void _FatalCopyTail(const char *src, const char *dst, size_t cap) {
    char buf[4096];
    ssize_t n;
    off_t sz, off;
    int in = open(src, O_RDONLY);
    int out;
    size_t total = 0;
    if (in < 0) return;
    sz = lseek(in, 0, SEEK_END);
    off = (sz > 0 && (size_t)sz > cap) ? (off_t)(sz - (off_t)cap) : 0;
    if (lseek(in, off, SEEK_SET) < 0) { close(in); return; }
    out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { close(in); return; }
    while (total < cap && (n = read(in, buf, sizeof(buf))) > 0) {
        size_t take = (size_t)n;
        if (total + take > cap) take = cap - total;
        if (write(out, buf, take) != (ssize_t)take) break;
        total += take;
    }
    close(in);
    close(out);
}

/* One sloginfo snapshot, hard-bounded: fork+exec, wait with deadline, SIGKILL,
 * then tail-copy into the RAM file the collector picks up.  Never blocks the
 * exit: on any overrun we proceed with partial/absent evidence. */
static void _FatalSlogSnapshot(void) {
    char tmp[256];
    pid_t pid;
    int waited_ms = 0, st;
    snprintf(tmp, sizeof(tmp), "%s.tmp", _FatalSlogPath());
    fflush(NULL);
    pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
#if defined(__QNX__)
        /* Not at the renderer's 15: a full slog dump froze playback on a busy
         * unit (docs/LANDMINES.md, "Never pull a full sloginfo"). */
        setprio(0, 10);
#endif
        execl("/proc/boot/sloginfo", "sloginfo", (char *)0);
        execlp("sloginfo", "sloginfo", (char *)0);
        _exit(127);
    }
    while (waitpid(pid, &st, WNOHANG) == 0 && waited_ms < ALT_FATAL_SLOG_WAIT_MS) {
        struct timespec pause = { 0, 20L * 1000000L };
        nanosleep(&pause, NULL);
        waited_ms += 20;
    }
    if (waited_ms >= ALT_FATAL_SLOG_WAIT_MS) {
        kill(pid, SIGKILL);
        waitpid(pid, &st, 0);
        fprintf(stderr, "altscreen_render[qnx]: slog snapshot timed out; "
                        "keeping partial evidence\n");
    }
    _FatalCopyTail(tmp, _FatalSlogPath(), ALT_FATAL_SLOG_CAP);
    unlink(tmp);
}

void av_sink_fatal_exit(int exit_code, const char *reason) {
    pthread_mutex_lock(&_FatalGuardLock);
    if (_FatalGuardTaken) {
        /* Another thread already owns marker/evidence/_exit.  Park forever —
         * the first entrant is guaranteed to reach _exit (evidence is
         * deadline-bounded), and the process dies with this thread. */
        for (;;)
            pthread_cond_wait(&_FatalGuardCond, &_FatalGuardLock);
    }
    _FatalGuardTaken = 1;
    pthread_mutex_unlock(&_FatalGuardLock);

    if (!reason || !*reason) reason = "unspecified";
    fprintf(stderr, "altscreen_render[qnx]: FATAL %s -> marker/evidence/_exit(%d)\n",
            reason, exit_code);

    {
        unsigned deaths = 0;
        unsigned long long last = 0;
        _FatalRead(&deaths, &last, NULL, 0);
        _FatalWrite(deaths + 1, _NowMs(), reason);
    }
    _FatalSlogSnapshot();
    _exit(exit_code);
}

/* Gate verdicts for new decoder starts.  Returns 0 = go, 1 = backoff (swallow
 * frames, transport stays up), 2 = degraded until reboot.  A marker whose
 * last-death timestamp lies in the future means CLOCK_MONOTONIC restarted —
 * the unit rebooted, the cure happened, the budget is fresh again. */
static unsigned g_fatal_degraded; /* deaths at degrade; the host test re-arms it */
static int _FatalGate(unsigned long long now, unsigned *deaths_out,
                      unsigned long long *retry_in_ms) {
    unsigned deaths = 0;
    unsigned long long last = 0;
    *deaths_out = g_fatal_degraded;
    *retry_in_ms = 0;
    if (g_fatal_degraded) return 2;
    if (_FatalRead(&deaths, &last, NULL, 0) != 0) return 0;
    if (now && last && now < last) {
        _FatalWrite(0, 0, "reset_reboot");
        return 0;
    }
    if (deaths >= ALT_FATAL_DEATHS_MAX) {
        *deaths_out = g_fatal_degraded = deaths;
        return 2;
    }
    if (deaths > 0 && now && last && now - last < ALT_FATAL_BACKOFF_MS) {
        *deaths_out = deaths;
        *retry_in_ms = ALT_FATAL_BACKOFF_MS - (now - last);
        return 1;
    }
    return 0;
}

/* A presented frame proves the core recovered: give the boot its full death
 * budget back.  Once per process is enough — after the reset nothing re-fills
 * the counter except a fresh fatal.  (File-scope flag: the host test suite
 * re-arms it.) */
static unsigned char g_fatal_present_reset_done;
static void _FatalNotePresented(void) {
    unsigned deaths = 0;
    unsigned long long last = 0;
    if (g_fatal_present_reset_done) return;
    g_fatal_present_reset_done = 1;
    if (_FatalRead(&deaths, &last, NULL, 0) == 0 && deaths != 0) {
        _FatalWrite(0, last, "reset_presented");
        fprintf(stderr, "altscreen_render[qnx]: presented after %u prior "
                        "death(s); decoder budget reset\n", deaths);
    }
}

/* ignoreError: a teardown must still wait for the state edge after an error
 * was latched, otherwise StateLoaded and FreeBuffer land on a component that
 * is still leaving Executing (OMX_ErrorIncorrectStateTransition 0x80001017,
 * OMX_ErrorPortUnpopulated 0x8000101C in the 2026-10-09 car log). */
/* Returns 0, -1 on a latched error event, -2 when the edge never came. */
static int _WaitStateEx(ScreenSink *me, OMX_STATETYPE want, long timeoutMs, int ignoreError) {
    struct timespec ts; int err = 0, timedOut = 0;
    _AbsTimeoutMs(&ts, timeoutMs);
    pthread_mutex_lock(&me->lock);
    while (!(me->stateReached && me->state == want) && (ignoreError || !me->omxError)) {
        if (pthread_cond_timedwait(&me->cond, &me->lock, &ts) == ETIMEDOUT) {
            fprintf(stderr, "altscreen_render[qnx]: _WaitState TIMEOUT (want=%d)\n", (int)want);
            me->omxError = 1; timedOut = 1; break;
        }
    }
    if (me->omxError) err = timedOut ? -2 : -1;
    me->stateReached = 0;
    pthread_mutex_unlock(&me->lock);
    return err;
}
static int _WaitState(ScreenSink *me, OMX_STATETYPE want, long timeoutMs) {
    return _WaitStateEx(me, want, timeoutMs, 0);
}

static void _DestroyPresent(ScreenSink *me) {
    me->winReady = 0;
    if (me->present) {
        screen_egl_present_destroy(me->present);
        me->present = NULL;
    }
    me->presentOutCount = 0;
    me->presentWidth = me->presentHeight = 0;
}

static int _CreateEarlyPresent(ScreenSink *me) {
    me->present = screen_egl_present_create_output(me->width, me->height,
                                                   &me->viewAreaMode, &me->layoutStyle);
    if (!me->present) {
        fprintf(stderr,
                "altscreen_render[qnx]: early RGBA plane-99 setup FAILED\n");
        return -1;
    }
    me->presentWidth = me->width;
    me->presentHeight = me->height;
    me->presentOutCount = 0;
    if (screen_egl_present_start_loader(me->present) != 0) {
        _DestroyPresent(me);
        return -1;
    }
    return 0;
}

static int _MakePresent(ScreenSink *me, OMX_U32 count) {
    if (!me->present || me->presentWidth != me->width ||
        me->presentHeight != me->height) {
        if (me->present) _DestroyPresent(me);
        if (_CreateEarlyPresent(me) != 0) return -1;
    }
    if (me->presentOutCount == count) {
        me->winReady = 1;
        fprintf(stderr,
                "altscreen_render[qnx]: reusing persistent EGL/Screen presenter across reconnect (%u buffers)\n",
                (unsigned)count);
        return 0;
    }
    if (me->presentOutCount != 0) {
        /* A live source-count change cannot be detached safely from a presenter
         * whose pointers may have been handed to OMX. Recreate only on this ABI
         * mismatch; the normal path keeps the early RGBA window. */
        fprintf(stderr,
                "altscreen_render[qnx]: source-count changed %u -> %u; rebuilding presenter\n",
                (unsigned)me->presentOutCount, (unsigned)count);
        _DestroyPresent(me);
        if (_CreateEarlyPresent(me) != 0) return -1;
    }
    if (screen_egl_present_attach_sources(me->present,
                                          (unsigned int)count) != 0) {
        fprintf(stderr,
                "altscreen_render[qnx]: EGLImage GPU source attach FAILED\n");
        me->winReady = 0;
        return -1;
    }
    me->presentOutCount = count;
    me->presentWidth = me->width;
    me->presentHeight = me->height;
    me->winReady = 1;
    fprintf(stderr,
            "altscreen_render[qnx]: output path attached to existing plane99: OMX=0x7f000004 -> EGLImage -> RGBA8888\n");
    return 0;
}

static int _ConfigurePorts(ScreenSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kInPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    pd.format.video.xFramerate = 30u << 16;
    pd.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    pd.format.video.eColorFormat       = OMX_COLOR_FormatUnused;
    if (me->width > 0 && me->height > 0) {
        pd.format.video.nFrameWidth  = (OMX_U32)me->width;
        pd.format.video.nFrameHeight = (OMX_U32)me->height;
    }
    if (OMX_SetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;

    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kOutPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    pd.format.video.xFramerate = 30u << 16;
    pd.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)kQOMX_ColorFmt_32m;
    if (me->width > 0 && me->height > 0) {
        pd.format.video.nFrameWidth  = (OMX_U32)me->width;
        pd.format.video.nFrameHeight = (OMX_U32)me->height;
    }
    if (OMX_SetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kOutPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    if ((OMX_U32)pd.format.video.eColorFormat != (OMX_U32)kQOMX_ColorFmt_32m) {
        fprintf(stderr,
                "altscreen_render[qnx]: decoder rejected native 32m: requested=0x7f000004 actual=0x%x\n",
                (unsigned)pd.format.video.eColorFormat);
        return -1;
    }
    for (OMX_U32 port = kInPortIndex; port <= kOutPortIndex; ++port) {
        memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
        pd.nPortIndex = port;
        if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
        if (pd.format.video.xFramerate != (30u << 16)) {
            fprintf(stderr, "altscreen_render[qnx]: port %u fps readback mismatch requested=30 actual=0x%x\n",
                    (unsigned)port, (unsigned)pd.format.video.xFramerate);
            return -1;
        }
    }
    fprintf(stderr, "altscreen_render[qnx]: input/output xFramerate=30 verified\n");
    return 0;
}

/* MU1316 libairplay 0x86ea8 and Qcom extension 0x7f100008: the name says
 * "config", but OEM uses Get/SetParameter and a 20-byte payload. Word +8 is
 * preserved from GetParameter; its vendor meaning is intentionally not guessed. */
typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 vendorWord;
    OMX_U32 enabled;
    OMX_U32 mode;
} DecoderOrderParam;
typedef char DecoderOrderAbi[(sizeof(DecoderOrderParam) == 20 &&
    offsetof(DecoderOrderParam, enabled) == 12 &&
    offsetof(DecoderOrderParam, mode) == 16) ? 1 : -1];

static int _EnableDecoderOrderMode(ScreenSink *me) {
    OMX_INDEXTYPE idx;
    DecoderOrderParam on;
    OMX_ERRORTYPE err;
    const char *stage = "GetExtensionIndex";
    err = OMX_GetExtensionIndex(me->decoder, (OMX_STRING)kQOMX_ExtIndex_DisplayPicBuf, &idx);
    if (err != OMX_ErrorNone) goto fail;
    memset(&on, 0, sizeof(on)); on.nSize = sizeof(on); on.nVersion.nVersion = 0x101u /* OEM 1.1.0.0 */;
    stage = "GetParameter";
    err = OMX_GetParameter(me->decoder, idx, &on);
    if (err != OMX_ErrorNone) goto fail;
    on.enabled = 1; on.mode = 1;
    stage = "SetParameter";
    err = OMX_SetParameter(me->decoder, idx, &on);
    if (err != OMX_ErrorNone) goto fail;
    memset(&on, 0, sizeof(on)); on.nSize = sizeof(on); on.nVersion.nVersion = 0x101u /* OEM 1.1.0.0 */;
    stage = "readback";
    err = OMX_GetParameter(me->decoder, idx, &on);
    if (err != OMX_ErrorNone) goto fail;
    if (on.enabled != 1 || on.mode != 1) {
        fprintf(stderr, "altscreen_render[qnx]: decoder order readback mismatch enabled=%u mode=%u\n",
                (unsigned)on.enabled, (unsigned)on.mode);
        return -1;
    }
    fprintf(stderr, "altscreen_render[qnx]: decoder order enabled (20-byte Get/SetParameter + readback)\n");
    return 0;
fail:
    fprintf(stderr, "altscreen_render[qnx]: decoder order %s failed 0x%x\n", stage, (unsigned)err);
    return -1;
}

static OMX_U32 _ReadOutputDefinition(ScreenSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kOutPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd) != OMX_ErrorNone) return 0;
    if ((OMX_U32)pd.format.video.eColorFormat != (OMX_U32)kQOMX_ColorFmt_32m) {
        fprintf(stderr,
                "altscreen_render[qnx]: output definition is not Qualcomm 32m (0x%x)\n",
                (unsigned)pd.format.video.eColorFormat);
        return 0;
    }
    if (pd.format.video.nFrameWidth)  me->width  = (int32_t)pd.format.video.nFrameWidth;
    if (pd.format.video.nFrameHeight) me->height = (int32_t)pd.format.video.nFrameHeight;
    fprintf(stderr,
            "altscreen_render[qnx]: output definition frame=%ux%u stride=%d slice=%u size=%u count=%u\n",
            (unsigned)pd.format.video.nFrameWidth,
            (unsigned)pd.format.video.nFrameHeight,
            (int)pd.format.video.nStride,
            (unsigned)pd.format.video.nSliceHeight,
            (unsigned)pd.nBufferSize,
            (unsigned)pd.nBufferCountActual);
    return pd.nBufferCountActual;
}

static int _AllocInput(ScreenSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd; OMX_U32 i, count;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kInPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    count = pd.nBufferCountActual;
    me->inBufs = (OMX_BUFFERHEADERTYPE **)calloc(count, sizeof(*me->inBufs));
    if (!me->inBufs) return -1;
    me->nInBufs = 0;
    for (i = 0; i < count; ++i) {
        if (OMX_AllocateBuffer(me->decoder, &me->inBufs[i], kInPortIndex, me,
                               pd.nBufferSize)) return -1;
        ++me->nInBufs;
    }
    return 0;
}

/* Bind pointers from the native tiled Screen WINDOW buffers to OMX.  This is the
 * allocator contract used by stock libairplay 210 and the only path proven to
 * complete Loaded->Idle on the APQ8064 decoder. */
static int _UseScreenBuffers(ScreenSink *me, OMX_U32 count) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    OMX_U32 i;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kOutPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) goto fail;

    me->outBufs       = (OMX_BUFFERHEADERTYPE **)calloc(count, sizeof(*me->outBufs));
    if (!me->outBufs) goto fail;
    me->nOutBufs = 0;

    for (i = 0; i < count; ++i) {
        void *vaddr = screen_egl_present_source_pointer(me->present,
                                                         (unsigned int)i);
        unsigned int capacity = screen_egl_present_source_capacity(me->present,
                                                                    (unsigned int)i);
        /* MU1316's qcom AVC component reports nBufferSize=1278000 for the
         * 1440x542 port, while its native 32m Screen geometry and every actual
         * FillDone payload are 1277952 bytes.  OMX_UseBuffer accepts that exact
         * Screen allocation (proven over 19k frames); the extra 48 is component
         * bookkeeping/alignment, not decoded pixel storage.  Reject unknown
         * mismatches, but preserve this tightly-bounded Qualcomm ABI slack. */
        if (!vaddr || capacity == 0
                || ((unsigned int)pd.nBufferSize > capacity
                    && (unsigned int)pd.nBufferSize - capacity > 64u)) {
            fprintf(stderr,
                    "altscreen_render[qnx]: source buffer %u too small ptr=%p capacity=%u OMX=%u\n",
                    (unsigned)i, vaddr, capacity, (unsigned)pd.nBufferSize);
            goto fail;
        }
        if (i == 0 && capacity < (unsigned int)pd.nBufferSize)
            fprintf(stderr,
                    "altscreen_render[qnx]: accepting known Qcom output-size slack payload=%u OMX=%u delta=%u\n",
                    capacity, (unsigned)pd.nBufferSize,
                    (unsigned)pd.nBufferSize - capacity);
        if (OMX_UseBuffer(me->decoder, &me->outBufs[i], kOutPortIndex, me, pd.nBufferSize, (OMX_U8 *)vaddr)) goto fail;
        ++me->nOutBufs;
    }
    return 0;
fail:
    return -1;
}

static void _DrainCallbacksAndPresenter(ScreenSink *me) {
    pthread_mutex_lock(&me->lock);
    me->running = 0;
    me->winReady = 0;
    me->callbacksClosing = 1;
    me->presentStop = 1;
    pthread_cond_broadcast(&me->cond);
    {
        struct timespec callbackDeadline;
        int waitRc = 0;
        _AbsTimeoutMs(&callbackDeadline, kCallbackDrainWaitMs);
        while ((me->callbacksActive > 0 || (me->presentThreadStarted && !me->presentExited)) && waitRc != ETIMEDOUT)
            waitRc = pthread_cond_timedwait(&me->cond, &me->lock,
                                            &callbackDeadline);
        if (me->callbacksActive > 0 || (me->presentThreadStarted && !me->presentExited)) {
            fprintf(stderr,
                    "altscreen_render[qnx]: callback/presenter drain TIMEOUT active=%u; exiting without freeing live OMX/EGL storage\n",
                    me->callbacksActive);
            pthread_mutex_unlock(&me->lock);
            /* A GPU presenter or data callback is wedged inside this renderer. Continuing would
             * either block forever or free buffers under the worker/callback. Exit the
             * isolated renderer; the persistent supervisor restarts it after
             * backoff, while dio_manager and the HMI remain alive. */
            av_sink_fatal_exit(86, "drain_timeout");
        }
    }
    pthread_mutex_unlock(&me->lock);
    if (me->presentThreadStarted) {
        pthread_join(me->presentThread, NULL);
        me->presentThreadStarted = 0;
    }
    free(me->presentQueue); me->presentQueue = NULL;
    free(me->presentOwned); me->presentOwned = NULL;
    free(me->presentQueuedUs);
    me->presentQueuedUs = NULL;
}

/* The whole OMX teardown section, executed on a helper thread so a wedged
 * core cannot take the caller with it.  Every step publishes its stage for the
 * watchdog reason strings; the global is cleared when the section completes.
 * Per QNX/Qualcomm stack (verified in IDA and live in QEMU): Idle->Idle edge
 * issues a sync MMI_CMD_STOP ioctl, Loaded edge a sync MMI_CMD_RELEASE_RESOURCES,
 * FreeHandle ends in QVD_Close + thread-join — all plain MsgSends with no
 * client-side timeout. */
static void _TeardownOmxSection(ScreenSink *me) {
    OMX_U32 i;
    if (me->decoder) {
        g_omx_stage = "idle_stop";
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        _WaitStateEx(me, OMX_StateIdle, kOmxStopWaitMs, 1);
        g_omx_stage = "loaded_release";
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        g_omx_stage = "free_buffers";
        for (i = 0; i < me->nInBufs;  ++i) OMX_FreeBuffer(me->decoder, kInPortIndex,  me->inBufs[i]);
        for (i = 0; i < me->nOutBufs; ++i) OMX_FreeBuffer(me->decoder, kOutPortIndex, me->outBufs[i]);
        _WaitStateEx(me, OMX_StateLoaded, kOmxStopWaitMs, 1);
        g_omx_stage = "free_handle";
        OMX_FreeHandle(me->decoder); me->decoder = NULL;
    }
    if (me->omxInitialized) {
        g_omx_stage = "deinit";
        OMX_Deinit();
        me->omxInitialized = 0;
    }
    g_omx_stage = NULL;
}

typedef struct {
    ScreenSink *me;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int done;
} _TeardownJob;

static void *_TeardownHelper(void *arg) {
    _TeardownJob *job = (_TeardownJob *)arg;
    _TeardownOmxSection(job->me);
    pthread_mutex_lock(&job->lock);
    job->done = 1;
    pthread_cond_broadcast(&job->cond);
    pthread_mutex_unlock(&job->lock);
    return NULL;
}

static long _TeardownDeadlineMs(void) {
    const char *e = getenv("ALT_TEARDOWN_DEADLINE_MS");
    long v;
    if (e && (v = atol(e)) > 0) return v;
    return 4000L; /* server-side waits are 3 s bounded; 4 s covers one + slack */
}

/* Runs the OMX section on a helper under a hard deadline.  Returns 1 when the
 * section completed, 0 when it wedged (the helper thread is abandoned — it
 * dies with the process when the caller exits through the fatal path). */
static int _TeardownOmxJoined(ScreenSink *me) {
    /* Heap, not stack: an abandoned helper still signals the job when its OMX
     * call returns late, while the caller is already in the fatal path. */
    _TeardownJob *job = (_TeardownJob *)calloc(1, sizeof(*job));
    pthread_t th;
    struct timespec ts;
    int rc = 0, done;
    if (!job) {
        _TeardownOmxSection(me);
        return 1;
    }
    job->me = me;
    pthread_mutex_init(&job->lock, NULL);
    _CondInit(&job->cond);
    if (pthread_create(&th, NULL, _TeardownHelper, job) != 0) {
        /* No helper available: inline is the historical behavior. */
        _TeardownOmxSection(me);
        done = 1;
    } else {
        _AbsTimeoutMs(&ts, _TeardownDeadlineMs());
        pthread_mutex_lock(&job->lock);
        while (!job->done && rc != ETIMEDOUT)
            rc = pthread_cond_timedwait(&job->cond, &job->lock, &ts);
        done = job->done;
        pthread_mutex_unlock(&job->lock);
        if (!done)
            return 0; /* ponytail: job leaks on purpose, the caller exits */
        pthread_join(th, NULL);
    }
    pthread_cond_destroy(&job->cond);
    pthread_mutex_destroy(&job->lock);
    free(job);
    return 1;
}

static void _Teardown(ScreenSink *me, int destroyPresent) {
    if (!me->decoder && (!destroyPresent || !me->present) && !me->omxInitialized) return;
    if (me->coreFatal) {
        /* The core reported Timeout/Hardware/Invalid while starting or
         * decoding: every further OMX call risks the wedged hang and the core
         * stays dead until reboot.  Start failure, session end, frame error
         * and close all reach here; mark, leave evidence, exit. */
        fprintf(stderr,
                "altscreen_render[qnx]: core-fatal latched -> no OMX teardown, marker/evidence/exit\n");
        av_sink_fatal_exit(86, "core_fatal");
    }
    _DrainCallbacksAndPresenter(me);
    if (me->decoder || me->omxInitialized) {
        if (!_TeardownOmxJoined(me)) {
            char reason[ALT_REASON_MAX];
            const char *stage = g_omx_stage ? g_omx_stage : "unknown";
            /* Never touch that component again: Idle->Loaded, FreeBuffer and
             * FreeHandle are MsgSends into the wedged core and block forever.
             * Snapshot the stage while it is still valid, then exit. */
            snprintf(reason, sizeof(reason), "teardown_deadline:%s", stage);
            fprintf(stderr,
                    "altscreen_render[qnx]: teardown OMX stage '%s' exceeded deadline; abandoning component\n",
                    stage);
            av_sink_fatal_exit(86, reason);
        }
    }
    if (destroyPresent) _DestroyPresent(me);
    free(me->inBufs);        me->inBufs = NULL;        me->nInBufs = 0;
    free(me->outBufs);       me->outBufs = NULL;       me->nOutBufs = 0;
    free(me->freeIn);        me->freeIn = NULL;
    me->winReady = 0; me->started = 0; me->fedConfig = 0; me->needsIDR = 1;
    me->stateReached = 0; me->omxError = 0; me->coreFatal = 0;
    me->inputSubmitted = me->inputReturned = 0;
    me->outputFilled = me->outputPosted = me->outputPostFailed = 0;
    me->outputSuperseded = 0;
    me->presentationHealthy = 0;
    me->presentConsecutiveFails = 0;
    me->presentRecoveryPending = 0;
    me->presentRecoveryAttempts = 0;
    me->presentNeedsFullRebuild = 0;
    me->presentRecoveryAfterMs = 0;
    me->bitstreamErrors = 0;
    me->bitstreamStallArmed = 0;
    me->bitstreamStallInputBase = 0;
    me->bitstreamStallInFlight = 0;
    me->bitstreamStallSinceMs = 0;
    me->newestInputMs = 0;
    memset(me->trace, 0, sizeof(me->trace));
    me->trace_next = 0;
    me->trace_last_stamp = 0;
    me->timing_generation = 0;
    me->timing_last_sequence = 0;
    memset(&me->input_wait_timing, 0, sizeof(me->input_wait_timing));
    memset(&me->decode_timing, 0, sizeof(me->decode_timing));
    memset(&me->present_queue_timing, 0, sizeof(me->present_queue_timing));
    memset(&me->recycle_timing, 0, sizeof(me->recycle_timing));
    memset(&me->hook_to_post_timing, 0, sizeof(me->hook_to_post_timing));
    if (!destroyPresent && me->present) {
        screen_egl_present_reset_timing(me->present);
        screen_egl_present_set_stream_pending(me->present, 0);
        screen_egl_present_start_loader(me->present);
    }
}

/* Bring up native 32m OMX output + post-decode EGL external-image conversion. */
static int _StartDecoder(ScreenSink *me) {
    OMX_U32 i, outCount;
    int waitRc;
    uint64_t begin = _NowMs(), sourcesReady;
    fprintf(stderr, "altscreen_render[qnx]: decoder start begin at %llu ms\n",
            (unsigned long long)begin);
    screen_egl_present_set_stream_pending(me->present, 1);
    pthread_mutex_lock(&me->lock);
    me->callbacksClosing = 0;
    pthread_mutex_unlock(&me->lock);
    if (OMX_Init()) goto fail;
    me->omxInitialized = 1;
    if (OMX_GetHandle(&me->decoder, (OMX_STRING)kOMXDecoderName, me, &kCallbacks)) goto fail;

    if (_EnableDecoderOrderMode(me)) goto fail;
    if (_ConfigurePorts(me)) goto fail;

    outCount = _ReadOutputDefinition(me);
    if (outCount == 0 || _MakePresent(me, outCount) != 0) goto fail;
    screen_egl_present_set_stream_pending(me->present, 1);
    sourcesReady = _NowMs();
    fprintf(stderr, "altscreen_render[qnx]: native sources ready after %llu ms\n",
            (unsigned long long)(sourcesReady - begin));

    if (OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL)) goto fail;
    if (_AllocInput(me)) goto fail;
    if (_UseScreenBuffers(me, outCount)) goto fail;
    /* A start-phase state edge that never completes is core-class: the core
     * accepted the session but cannot run it (field: "never reaches
     * Executing").  An ordinary error event is not; _Teardown exits on it. */
    if ((waitRc = _WaitState(me, OMX_StateIdle, kOmxStartIdleWaitMs)) != 0) {
        if (waitRc == -2) me->coreFatal = 1;
        goto fail;
    }

    me->freeIn = (OMX_U32 *)calloc(me->nInBufs, sizeof(OMX_U32));
    if (!me->freeIn) goto fail;
    for (i = 0; i < me->nInBufs; ++i) me->freeIn[i] = i;
    me->freeInHead = 0; me->freeInTail = me->nInBufs;

    if (OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)) goto fail;
    if ((waitRc = _WaitState(me, OMX_StateExecuting, kOmxStartExecWaitMs)) != 0) {
        if (waitRc == -2) me->coreFatal = 1;
        goto fail;
    }
    me->running = 1;
    if (_StartPresentThread(me)) goto fail;
    for (i = 0; i < me->nOutBufs; ++i) {
        OMX_ERRORTYPE err = OMX_FillThisBuffer(me->decoder, me->outBufs[i]);
        if (err != OMX_ErrorNone) {
            fprintf(stderr, "altscreen_render[qnx]: OMX_FillThisBuffer(prime %u) failed 0x%x\n",
                    (unsigned)i, (unsigned)err);
            goto fail;
        }
    }

    pthread_mutex_lock(&me->lock);
    me->started = 1;
    pthread_mutex_unlock(&me->lock);
    me->needsIDR = 1;
    me->sessionEverStarted = 1;
    fprintf(stderr, "altscreen_render[qnx]: decoder startup total=%llu ms OMX states/buffers=%llu ms\n",
            (unsigned long long)(_NowMs() - begin),
            (unsigned long long)(_NowMs() - sourcesReady));
    fprintf(stderr, "altscreen_render[qnx]: decoder started %dx%d, native-32m %u Screen window bufs -> EGL GPU RGBA\n",
            me->width, me->height, (unsigned)me->nOutBufs);
    return 0;
fail:
    fprintf(stderr, "altscreen_render[qnx]: decoder start FAILED\n");
    _Teardown(me, 0);
    return -1;
}

/* feed one Annex-B buffer to the decoder (blocks for a free input buffer). */
static int _FeedInput(ScreenSink *me, const uint8_t *data, uint32_t len, OMX_U32 flags,
                      uint64_t received_ms) {
    OMX_BUFFERHEADERTYPE *b; OMX_U32 idx;
    OMX_ERRORTYPE err;
    uint32_t requestedLen = len;
    uint64_t wait_started;
    struct timespec deadline;
    _AbsTimeoutMs(&deadline, ALT_INPUT_WAIT_MS);
    pthread_mutex_lock(&me->lock);
    if (me->inputSubmitted % 300u == 0)
        me->timing_enabled = log_verbose_marker();
    wait_started = me->timing_enabled ? alt_timing_now_us() : 0;
    while (me->freeInHead == me->freeInTail && me->running && !me->omxError) {
        int waited = pthread_cond_timedwait(&me->cond, &me->lock, &deadline);
        if (waited == ETIMEDOUT && me->freeInHead == me->freeInTail) {
            fprintf(stderr,
                    "altscreen_render[qnx]: input-buffer wait TIMEOUT (%d ms); marking decoder unhealthy\n",
                    ALT_INPUT_WAIT_MS);
            me->omxError = 1;
            break;
        }
    }
    if (!me->running || me->omxError) { pthread_mutex_unlock(&me->lock); return -1; }
    if (wait_started)
        alt_timing_elapsed(&me->input_wait_timing, wait_started, alt_timing_now_us());
    idx = me->freeIn[me->freeInHead % me->nInBufs]; me->freeInHead++;
    pthread_mutex_unlock(&me->lock);

    b = me->inBufs[idx];
    if (len > b->nAllocLen) {
        fprintf(stderr,
                "altscreen_render[qnx]: INPUT DROPPED requested=%u alloc=%u (never submit a clipped access unit)\n",
                (unsigned)len, (unsigned)b->nAllocLen);
        pthread_mutex_lock(&me->lock);
        me->freeIn[me->freeInTail % me->nInBufs] = idx; me->freeInTail++;
        pthread_cond_broadcast(&me->cond);
        pthread_mutex_unlock(&me->lock);
        return -1;
    }
    memcpy(b->pBuffer, data, len);
    b->nOffset = 0; b->nFilledLen = len;
    b->nFlags = flags;
    /* OMX timestamps are microseconds. Preserve socket age through decode;
     * decoder-order mode still determines ordering, never a wall-clock PTS. */
    b->nTimeStamp = (OMX_TICKS)(received_ms * 1000ULL);
    if (me->incoming_meta.version) {
        frame_trace_t *t;
        uint64_t stamp = alt_timing_now_us();
        if (stamp <= me->trace_last_stamp)
            stamp = me->trace_last_stamp + 1u;
        me->trace_last_stamp = stamp;
        b->nTimeStamp = (OMX_TICKS)stamp;
        pthread_mutex_lock(&me->lock);
        t = &me->trace[me->trace_next++ % FRAME_TRACE_CAPACITY];
        t->stamp = b->nTimeStamp;
        t->submitted_us = stamp;
        t->meta = me->incoming_meta;
        /* Different/invalid monotonic clocks must not become huge durations. */
        if (t->meta.received_us > stamp)
            t->meta.received_us = 0;
        pthread_mutex_unlock(&me->lock);
    }
    err = OMX_EmptyThisBuffer(me->decoder, b);
    if (err == OMX_ErrorNone) {
        pthread_mutex_lock(&me->lock);
        if (received_ms > me->newestInputMs) me->newestInputMs = received_ms;
        me->inputSubmitted++;
        if (me->bitstreamStallArmed && !(flags & OMX_BUFFERFLAG_CODECCONFIG)) {
            unsigned long long nowMs = _NowMs();
            if (me->bitstreamStallSinceMs == 0) {
                me->bitstreamStallSinceMs = nowMs;
            } else if (nowMs >= me->bitstreamStallSinceMs + 2000ULL) {
                me->omxError = 1;
                fprintf(stderr,
                        "altscreen_render[qnx]: bitstream error stall >2000 ms after input #%lu; decoder restart required\n",
                        me->inputSubmitted);
            }
        }
        pthread_mutex_unlock(&me->lock);
        if (me->inputSubmitted <= 3 || ((me->inputSubmitted % 600) == 0 && log_verbose_marker()))
            fprintf(stderr,
                    "altscreen_render[qnx]: EmptyThisBuffer OK #%lu requested=%u submitted=%u flags=0x%x in=%u\n",
                    me->inputSubmitted, (unsigned)requestedLen, (unsigned)len,
                    (unsigned)flags, (unsigned)idx);
    } else {
        fprintf(stderr,
                "altscreen_render[qnx]: OMX_EmptyThisBuffer FAILED 0x%x requested=%u submitted=%u flags=0x%x in=%u\n",
                (unsigned)err, (unsigned)requestedLen, (unsigned)len,
                (unsigned)flags, (unsigned)idx);
        /* Component rejected the buffer → it will NOT call _OnEmptyDone for it.
         * Return the index to the free ring so the pool isn't permanently drained
         * (else _FeedInput eventually blocks forever waiting for a free buffer). */
        pthread_mutex_lock(&me->lock);
        me->freeIn[me->freeInTail % me->nInBufs] = idx; me->freeInTail++;
        pthread_cond_broadcast(&me->cond);
        pthread_mutex_unlock(&me->lock);
    }
    return err == OMX_ErrorNone ? 0 : -1;
}

/* ---- sink API ------------------------------------------------------------- */
int av_sink_open(void) {
    const char *ew = getenv("ALTSCREEN_W"), *eh = getenv("ALTSCREEN_H");
    uint64_t begin = _NowMs();
    int result;
    memset(&g_ss, 0, sizeof(g_ss));
    {
        const char *fifo = getenv("ALTSCREEN_PRESENT_FIFO");
        g_ss.mailbox = !(fifo && !strcmp(fifo, "1"));
    }
    pthread_mutex_init(&g_ss.lock, NULL);
    _CondInit(&g_ss.cond);
    g_ss.width  = ew ? atoi(ew) : 1440;           /* = advertised cluster size = Audi Kombi panel */
    g_ss.height = eh ? atoi(eh) : 542;            /* qcdisplaycfg_MMX2.xml: "Audi 1440x542 Kombi panel" */
    fprintf(stderr, "altscreen_render[qnx]: sink open (%dx%d, OMX 32m Screen buffer -> EGL GPU RGBA)\n",
            g_ss.width, g_ss.height);
    result = _CreateEarlyPresent(&g_ss);
    fprintf(stderr, "altscreen_render[qnx]: Screen/EGL open took %llu ms result=%d\n",
            (unsigned long long)(_NowMs() - begin), result);
    return result;
}

enum { NAL_PARAMETER_SET = 1, NAL_PICTURE = 2, NAL_IDR = 4 };

/* A picture may contain in-band SPS/PPS as well as AUD/SEI. Recognise the
 * entire access unit; any parameter set alone does not make it config-only. */
static unsigned _AnnexBFlags(const uint8_t *p, uint32_t len) {
    uint32_t i;
    unsigned flags = 0;
    if (!p) return 0;
    for (i = 0; i + 3 < len; ++i) {
        uint32_t start = 0;
        if (i + 3 < len && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1)
            start = i + 3;
        else if (i + 4 < len && p[i] == 0 && p[i + 1] == 0
                 && p[i + 2] == 0 && p[i + 3] == 1) start = i + 4;
        if (start) {
            unsigned type = p[start] & 0x1f;
            if (type == 7 || type == 8) flags |= NAL_PARAMETER_SET;
            if (type >= 1 && type <= 5) flags |= NAL_PICTURE;
            if (type == 5) flags |= NAL_IDR;
        }
    }
    return flags;
}

/* Recovery is intentionally driven by the decode-input worker. The presenter only
 * marks presentation unhealthy; destroying an EGLSurface from that callback
 * would race OMX buffer recycling and teardown. */
static void _RecoverPresentationIfNeeded(ScreenSink *me) {
    int pending;
    int attempt;
    int result;
    unsigned long long nowMs;

    pthread_mutex_lock(&me->lock);
    pending = me->started && !me->callbacksClosing &&
        me->presentRecoveryPending;
    nowMs = _NowMs();
    if (pending && me->presentRecoveryAfterMs != 0 && nowMs != 0 &&
        nowMs < me->presentRecoveryAfterMs) pending = 0;
    attempt = me->presentRecoveryAttempts + 1;
    if (pending) {
        /* Claim this attempt before releasing the state lock.  A presenter that
         * swaps immediately after recovery is then free to restore healthy=1,
         * and the server must not overwrite that newer result. */
        me->presentRecoveryPending = 0;
        me->presentationHealthy = 0;
    }
    pthread_mutex_unlock(&me->lock);
    if (!pending) return;

    result = screen_egl_present_recover_output(me->present);
    pthread_mutex_lock(&me->lock);
    if (!me->started || me->callbacksClosing) {
        pthread_mutex_unlock(&me->lock);
        return;
    }
    if (result == 0) {
        me->presentRecoveryPending = 0;
        me->presentRecoveryAttempts = 0;
        me->presentRecoveryAfterMs = 0;
        me->presentConsecutiveFails = 0;
        fprintf(stderr,
                "altscreen_render[qnx]: output-only recovery OK; waiting for first new RGBA swap\n");
    } else {
        me->presentRecoveryAttempts = attempt;
        me->presentRecoveryAfterMs = _NowMs() + 250ULL;
        if (attempt >= 3) {
            me->presentRecoveryPending = 0;
            me->presentNeedsFullRebuild = 1;
            me->omxError = 1;
            fprintf(stderr,
                    "altscreen_render[qnx]: output-only recovery failed 3x; escalating to bounded decoder/presenter restart\n");
        } else {
            me->presentRecoveryPending = 1;
            fprintf(stderr,
                    "altscreen_render[qnx]: output-only recovery attempt %d failed; retrying after 250 ms\n",
                    attempt);
        }
    }
    pthread_mutex_unlock(&me->lock);
}

int av_sink_frame(const uint8_t *annexb, uint32_t len, uint64_t received_ms) {
    int isConfig;
    unsigned nalFlags;
    unsigned long long nowMs;
    if (!annexb || len == 0) return -1;
    nalFlags = _AnnexBFlags(annexb, len);
    isConfig = (nalFlags & NAL_PARAMETER_SET) && !(nalFlags & NAL_PICTURE);
    if (isConfig) {
        if (len > sizeof(g_ss.config)) return -1;
        if (g_ss.configLen == len && memcmp(g_ss.config, annexb, len) == 0 &&
            g_ss.fedConfig) {
            /* Repeated avcC is not a new random-access boundary. Keep the
             * already-fed codec and IDR state unchanged. */
        } else if (g_ss.started && g_ss.fedConfig && g_ss.configLen != 0) {
            fprintf(stderr,
                    "altscreen_render[qnx]: codec config changed midstream; require fresh decoder/IDR\n");
            return -1;
        } else {
            memcpy(g_ss.config, annexb, len);
            g_ss.configLen = len;
            g_ss.fedConfig = 0;
            g_ss.needsIDR = 1;
        }
    }
    _RecoverPresentationIfNeeded(&g_ss);
    if (g_ss.started && g_ss.omxError) {
        int rebuildPresent;
        pthread_mutex_lock(&g_ss.lock);
        rebuildPresent = g_ss.presentNeedsFullRebuild;
        pthread_mutex_unlock(&g_ss.lock);
        fprintf(stderr,
                "altscreen_render[qnx]: decoder error latched -> VIDEO_OFF + bounded restart (present=%s)\n",
                rebuildPresent ? "rebuild" : "keep");
        _Teardown(&g_ss, rebuildPresent);
        /* Rebuilding on the current P-frame loses its reference chain. End
         * the transport; the hook already owns bounded ForceKeyFrame retries. */
        return -1;
    }
    if (!g_ss.started) {
        unsigned gate_deaths = 0;
        unsigned long long gate_retry = 0;
        int gate;
        /* FAIL-SAFE: a repeatedly-failing decoder start must NOT loop — each failed OMX GetHandle can
         * hold a hardware decoder slot, and starving the pool made dio's own decoder fail
         * (kBadReferenceErr) → CarPlay crash-loop.  Cap at 3 attempts, then go idle (black, but the
         * OMX pool and dio stay healthy).  Reset per session in av_sink_session_end. */
        nowMs = _NowMs();
        if (g_ss.startFails >= 3) {
            if (nowMs == 0 || nowMs < g_ss.retryAfterMs) return -1;
            fprintf(stderr,
                    "altscreen_render[qnx]: decoder retry cooldown expired; allowing a new bounded attempt set\n");
            g_ss.startFails = 0;
        }
        /* Wedged-core memory (marker survives renderer restarts, /tmp = RAM:
         * reboot clears it).  Swallow frames with 0 — the transport, hook IDR
         * requests and STATUS 0 stay alive; only the decoder start waits. */
        gate = _FatalGate(nowMs, &gate_deaths, &gate_retry);
        if (gate != 0) {
            static unsigned long long last_gate_log;
            if (nowMs - last_gate_log >= 10000ULL || last_gate_log == 0) {
                last_gate_log = nowMs;
                if (gate == 2)
                    fprintf(stderr,
                            "altscreen_render[qnx]: video core fatal x%u this boot; degraded to stock map until reboot\n",
                            gate_deaths);
                else
                    fprintf(stderr,
                            "altscreen_render[qnx]: video core fatal backoff; decoder retry in %llu ms\n",
                            gate_retry);
            }
            return 0;
        }
        if (_StartDecoder(&g_ss) != 0) {
            if (++g_ss.startFails == 3) {
                g_ss.retryAfterMs = _NowMs() + 30000ULL;
                fprintf(stderr,
                        "altscreen_render[qnx]: decoder start failed 3x — cooling down 30s to protect the OMX pool / dio\n");
            }
            return -1;
        }
        g_ss.startFails = 0;
        g_ss.retryAfterMs = 0;
    }
    /* Preserve SPS/PPS across a failed decoder-start attempt.  Otherwise the
     * next IDR would be mislabeled CODECCONFIG and the retry could never decode. */
    if (!g_ss.fedConfig) {
        if (g_ss.configLen == 0) return -1;
        if (_FeedInput(&g_ss, g_ss.config, g_ss.configLen,
                       OMX_BUFFERFLAG_ENDOFFRAME | OMX_BUFFERFLAG_CODECCONFIG, 0) != 0)
            return -1;
        g_ss.fedConfig = 1;
    }
    if (!isConfig) {
        if ((nalFlags & NAL_PICTURE) && g_ss.needsIDR && !(nalFlags & NAL_IDR))
            return -1;
        if (_FeedInput(&g_ss, annexb, len, OMX_BUFFERFLAG_ENDOFFRAME,
                       (nalFlags & NAL_PICTURE) ? received_ms : 0) != 0)
            return -1;
        if (nalFlags & NAL_IDR) g_ss.needsIDR = 0;
    }
    return 0;
}

int av_sink_video_ready(void) {
    int ready;
    pthread_mutex_lock(&g_ss.lock);
    /* The control thread also samples during startup/teardown. Closing is
     * published before teardown resets fields outside the callback lock. */
    ready = !g_ss.callbacksClosing && g_ss.started &&
        !g_ss.omxError && g_ss.presentationHealthy;
    pthread_mutex_unlock(&g_ss.lock);
    return ready;
}

void av_sink_set_layout(int mode, int style, int rawSkin) {
    const char *layoutName;
    mode = mode == 1 ? 1 : 0;
    g_ss.viewAreaMode = mode;
    if (style == LOADER_STYLE_SPORT || style == LOADER_STYLE_CLASSIC)
        g_ss.layoutStyle = style;
    layoutName = !mode ? "fullscreen"
        : (g_ss.layoutStyle == LOADER_STYLE_CLASSIC
            ? "classicscreen" : "sportscreen");
    fprintf(stderr,
            "altscreen_render[loader]: layout -> view=%s style=%s rawSkin=%d\n",
            layoutName,
            g_ss.layoutStyle == LOADER_STYLE_CLASSIC ? "classic" : "sport",
            rawSkin);
}

int av_sink_frame_ex(const uint8_t *annexb, uint32_t len, uint64_t received_ms,
                     const alt_frame_meta_t *meta) {
    int rc;
    /* Only the serialized input worker calls this seam. Callbacks read the
     * per-picture timestamp table under lock, never this transient value. */
    memset(&g_ss.incoming_meta, 0, sizeof(g_ss.incoming_meta));
    if (meta)
        g_ss.incoming_meta = *meta;
    rc = av_sink_frame(annexb, len, received_ms);
    memset(&g_ss.incoming_meta, 0, sizeof(g_ss.incoming_meta));
    return rc;
}

void av_sink_session_end(void) {
    int hadWorkingDecoder = g_ss.sessionEverStarted;
    fprintf(stderr, "altscreen_render[qnx]: session end → teardown decoder; keep EGL/Screen presenter for reconnect\n");
    _Teardown(&g_ss, 0);
    g_ss.configLen = 0;
    g_ss.sessionEverStarted = 0;
    if (hadWorkingDecoder) {
        /* A decoder which reached Executing and produced this session is not an
         * OMX-pool leak.  Give the next phone connection a fresh attempt budget.
         * Sessions that never reached started retain the global 30 s guard. */
        g_ss.startFails = 0;
        g_ss.retryAfterMs = 0;
        fprintf(stderr,
                "altscreen_render[qnx]: working session ended cleanly; decoder attempt budget refreshed\n");
    } else if (g_ss.startFails > 0) {
        fprintf(stderr,
                "altscreen_render[qnx]: session never started; preserving OMX cooldown fails=%d\n",
                g_ss.startFails);
    }
}

void av_sink_close(void) {
    _Teardown(&g_ss, 1);
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
}
