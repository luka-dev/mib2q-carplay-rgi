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
 */
#include "sink.h"

#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_Video.h"

#include "screen_egl_present.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define kOMXDecoderName   "OMX.qcom.video.decoder.avc"
#define kInPortIndex      0
#define kOutPortIndex     1

/* Native output is mandatory on this decoder.  Requesting linear NV12 (0x27)
 * is accepted by SetParameter but the Loaded->Idle transition never completes. */
#define kQOMX_ColorFmt_32m            0x7F000004
#define kQOMX_ExtIndex_DisplayPicBuf  "OMX.QCOM.index.config.video.DisplayPictureBuffer"

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
    volatile int             running;
    int                      callbacksClosing;
    unsigned int             callbacksActive;
    int                      omxInitialized;
    int                      started;   /* decoder brought up on first frame */
    int                      sessionEverStarted;
    int                      startFails; /* consecutive _StartDecoder failures; capped to protect OMX pool */
    unsigned long long       retryAfterMs;
    int                      fedConfig; /* first buffer (SPS/PPS) flagged CODECCONFIG */

    /* Decisive pipeline diagnostics.  Decoder creation alone proves neither that
     * input was accepted nor that a decoded Screen buffer was posted. */
    unsigned long            inputSubmitted;
    unsigned long            inputReturned;
    unsigned long            outputFilled;
    unsigned long            outputPosted;
    unsigned long            outputPostFailed;
    int                      presentationHealthy;
    int                      presentConsecutiveFails;
    int                      presentRecoveryPending;
    int                      presentRecoveryAttempts;
    int                      presentNeedsFullRebuild;
    unsigned long long       presentRecoveryAfterMs;
    uint8_t                  config[4096];
    uint32_t                 configLen;
} ScreenSink;

static ScreenSink g_ss;

/* Data callbacks dereference OMX/Screen arrays which teardown frees.  Close the
 * callback gate first, then wait for an already-running GPU presentation to
 * leave before changing OMX state or releasing those arrays.  Event callbacks
 * remain enabled because _WaitState depends on them. */
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
        me->omxError = 1;
        fprintf(stderr, "altscreen_render[qnx]: OMX_EventError 0x%x\n", (unsigned)d1);
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
    if (me->inputReturned <= 3 || (me->inputReturned % 600) == 0)
        fprintf(stderr, "altscreen_render[qnx]: EmptyBufferDone #%lu header=%p matched=%s\n",
                me->inputReturned, (void *)b, (i < me->nInBufs) ? "yes" : "NO");
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    _CallbackLeave(me);
    return OMX_ErrorNone;
}

/* The OMX output pointer belongs to a tiled Screen WINDOW buffer.  The GPU
 * external-image draw consumes that exact buffer before this callback recycles it. */
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
            me->presentationHealthy = 1;
            me->presentConsecutiveFails = 0;
            me->presentRecoveryPending = 0;
            me->presentRecoveryAttempts = 0;
            me->presentRecoveryAfterMs = 0;
            pthread_mutex_unlock(&me->lock);
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

static OMX_ERRORTYPE _OnFillDone(OMX_HANDLETYPE c, OMX_PTR app, OMX_BUFFERHEADERTYPE *b) {
    ScreenSink *const me = (ScreenSink *)app;
    OMX_ERRORTYPE err;
    if (!_CallbackEnter(me)) return OMX_ErrorNone;
    me->outputFilled++;
    if (me->outputFilled <= 3 || (me->outputFilled % 600) == 0)
        fprintf(stderr,
                "altscreen_render[qnx]: FillBufferDone #%lu filled=%u offset=%u flags=0x%x winReady=%d\n",
                me->outputFilled, (unsigned)b->nFilledLen, (unsigned)b->nOffset,
                (unsigned)b->nFlags, me->winReady);
    _PresentGpu(me, b);
    /* The GPU path finishes sampling this exact Screen buffer before returning;
     * only now may it return to the decoder.  winReady drops before teardown. */
    if (me->running && me->winReady) {
        err = OMX_FillThisBuffer(c, b);
        if (err != OMX_ErrorNone)
            fprintf(stderr, "altscreen_render[qnx]: OMX_FillThisBuffer(recycle) failed 0x%x\n",
                    (unsigned)err);
    }
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

static void _AbsTimeoutMs(struct timespec *ts, long ms) {
    clock_gettime(CLOCK_REALTIME, ts);
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

static int _WaitState(ScreenSink *me, OMX_STATETYPE want, long timeoutMs) {
    struct timespec ts; int err = 0;
    _AbsTimeoutMs(&ts, timeoutMs);
    pthread_mutex_lock(&me->lock);
    while (!(me->stateReached && me->state == want) && !me->omxError) {
        if (pthread_cond_timedwait(&me->cond, &me->lock, &ts) == ETIMEDOUT) {
            fprintf(stderr, "altscreen_render[qnx]: _WaitState TIMEOUT (want=%d)\n", (int)want);
            me->omxError = 1; break;
        }
    }
    if (me->omxError) err = -1;
    me->stateReached = 0;
    pthread_mutex_unlock(&me->lock);
    return err;
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
    me->present = screen_egl_present_create_output(me->width, me->height);
    if (!me->present) {
        fprintf(stderr,
                "altscreen_render[qnx]: early RGBA plane-99 setup FAILED\n");
        return -1;
    }
    me->presentWidth = me->width;
    me->presentHeight = me->height;
    me->presentOutCount = 0;
    if (screen_egl_present_start_loader(me->present) != 0) {
        fprintf(stderr,
                "altscreen_render[qnx]: early RGBA loader start FAILED\n");
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
    return 0;
}

static void _EnableDecoderOrderMode(ScreenSink *me) {
    OMX_INDEXTYPE idx;
    OMX_CONFIG_BOOLEANTYPE on;
    if (OMX_GetExtensionIndex(me->decoder, (OMX_STRING)kQOMX_ExtIndex_DisplayPicBuf, &idx) != OMX_ErrorNone) return;
    memset(&on, 0, sizeof(on)); on.nSize = sizeof(on); on.nVersion.nVersion = OMX_VERSION;
    on.bEnabled = OMX_TRUE;
    OMX_SetConfig(me->decoder, idx, &on);
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

static void _Teardown(ScreenSink *me, int destroyPresent) {
    OMX_U32 i;
    if (!me->decoder && (!destroyPresent || !me->present) && !me->omxInitialized) return;
    pthread_mutex_lock(&me->lock);
    me->running = 0;
    me->winReady = 0;
    me->callbacksClosing = 1;
    pthread_cond_broadcast(&me->cond);
    while (me->callbacksActive > 0) {
        /* Never free OMX/Screen storage underneath a callback.  In the normal
         * path this lasts at most one GPU swap; if the GPU itself is wedged the
         * supervisor's SIGKILL backstop is safer than an in-process UAF. */
        pthread_cond_wait(&me->cond, &me->lock);
    }
    pthread_mutex_unlock(&me->lock);
    if (me->decoder) {
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        _WaitState(me, OMX_StateIdle, kOmxStopWaitMs);
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        for (i = 0; i < me->nInBufs;  ++i) OMX_FreeBuffer(me->decoder, kInPortIndex,  me->inBufs[i]);
        for (i = 0; i < me->nOutBufs; ++i) OMX_FreeBuffer(me->decoder, kOutPortIndex, me->outBufs[i]);
        _WaitState(me, OMX_StateLoaded, kOmxStopWaitMs);
        OMX_FreeHandle(me->decoder); me->decoder = NULL;
    }
    if (me->omxInitialized) {
        OMX_Deinit();
        me->omxInitialized = 0;
    }
    if (destroyPresent) _DestroyPresent(me);
    free(me->inBufs);        me->inBufs = NULL;        me->nInBufs = 0;
    free(me->outBufs);       me->outBufs = NULL;       me->nOutBufs = 0;
    free(me->freeIn);        me->freeIn = NULL;
    me->winReady = 0; me->started = 0; me->fedConfig = 0;
    me->stateReached = 0; me->omxError = 0;
    me->inputSubmitted = me->inputReturned = 0;
    me->outputFilled = me->outputPosted = me->outputPostFailed = 0;
    me->presentationHealthy = 0;
    me->presentConsecutiveFails = 0;
    me->presentRecoveryPending = 0;
    me->presentRecoveryAttempts = 0;
    me->presentNeedsFullRebuild = 0;
    me->presentRecoveryAfterMs = 0;
    if (!destroyPresent && me->present)
        screen_egl_present_start_loader(me->present);
}

/* Bring up native 32m OMX output + post-decode EGL external-image conversion. */
static int _StartDecoder(ScreenSink *me) {
    OMX_U32 i, outCount;
    pthread_mutex_lock(&me->lock);
    me->callbacksClosing = 0;
    pthread_mutex_unlock(&me->lock);
    if (OMX_Init()) goto fail;
    me->omxInitialized = 1;
    if (OMX_GetHandle(&me->decoder, (OMX_STRING)kOMXDecoderName, me, &kCallbacks)) goto fail;

    _EnableDecoderOrderMode(me);
    if (_ConfigurePorts(me)) goto fail;

    outCount = _ReadOutputDefinition(me);
    if (outCount == 0 || _MakePresent(me, outCount) != 0) goto fail;

    if (OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL)) goto fail;
    if (_AllocInput(me)) goto fail;
    if (_UseScreenBuffers(me, outCount)) goto fail;
    if (_WaitState(me, OMX_StateIdle, kOmxStartIdleWaitMs)) goto fail;

    me->freeIn = (OMX_U32 *)calloc(me->nInBufs, sizeof(OMX_U32));
    if (!me->freeIn) goto fail;
    for (i = 0; i < me->nInBufs; ++i) me->freeIn[i] = i;
    me->freeInHead = 0; me->freeInTail = me->nInBufs;

    if (OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)) goto fail;
    if (_WaitState(me, OMX_StateExecuting, kOmxStartExecWaitMs)) goto fail;
    me->running = 1;
    for (i = 0; i < me->nOutBufs; ++i) {
        OMX_ERRORTYPE err = OMX_FillThisBuffer(me->decoder, me->outBufs[i]);
        if (err != OMX_ErrorNone) {
            fprintf(stderr, "altscreen_render[qnx]: OMX_FillThisBuffer(prime %u) failed 0x%x\n",
                    (unsigned)i, (unsigned)err);
            goto fail;
        }
    }

    me->started = 1;
    me->sessionEverStarted = 1;
    fprintf(stderr, "altscreen_render[qnx]: decoder started %dx%d, native-32m %u Screen window bufs -> EGL GPU RGBA\n",
            me->width, me->height, (unsigned)me->nOutBufs);
    return 0;
fail:
    fprintf(stderr, "altscreen_render[qnx]: decoder start FAILED\n");
    _Teardown(me, 0);
    return -1;
}

/* feed one Annex-B buffer to the decoder (blocks for a free input buffer). */
static void _FeedInput(ScreenSink *me, const uint8_t *data, uint32_t len, OMX_U32 flags) {
    OMX_BUFFERHEADERTYPE *b; OMX_U32 idx;
    OMX_ERRORTYPE err;
    uint32_t requestedLen = len;
    pthread_mutex_lock(&me->lock);
    while (me->freeInHead == me->freeInTail && me->running && !me->omxError) {
        struct timespec ts;
        _AbsTimeoutMs(&ts, 500L);
        if (pthread_cond_timedwait(&me->cond, &me->lock, &ts) == ETIMEDOUT) {
            fprintf(stderr,
                    "altscreen_render[qnx]: input-buffer wait TIMEOUT; marking decoder unhealthy\n");
            me->omxError = 1;
            break;
        }
    }
    if (!me->running || me->omxError) { pthread_mutex_unlock(&me->lock); return; }
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
        return;
    }
    memcpy(b->pBuffer, data, len);
    b->nOffset = 0; b->nFilledLen = len;
    b->nFlags = flags;
    b->nTimeStamp = 0;                            /* decoder-order mode → pts not needed */
    err = OMX_EmptyThisBuffer(me->decoder, b);
    if (err == OMX_ErrorNone) {
        me->inputSubmitted++;
        if (me->inputSubmitted <= 3 || (me->inputSubmitted % 600) == 0)
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
}

/* ---- sink API ------------------------------------------------------------- */
int av_sink_open(void) {
    const char *ew = getenv("ALTSCREEN_W"), *eh = getenv("ALTSCREEN_H");
    memset(&g_ss, 0, sizeof(g_ss));
    pthread_mutex_init(&g_ss.lock, NULL);
    pthread_cond_init(&g_ss.cond, NULL);
    g_ss.width  = ew ? atoi(ew) : 1440;           /* = advertised cluster size = Audi Kombi panel */
    g_ss.height = eh ? atoi(eh) : 542;            /* qcdisplaycfg_MMX2.xml: "Audi 1440x542 Kombi panel" */
    fprintf(stderr, "altscreen_render[qnx]: sink open (%dx%d, OMX 32m Screen buffer -> EGL GPU RGBA)\n",
            g_ss.width, g_ss.height);
    return _CreateEarlyPresent(&g_ss);
}

/* Minimal Annex-B scan used only to recognise the cached SPS/PPS message. */
static int _HasParameterSet(const uint8_t *p, uint32_t len) {
    uint32_t i;
    if (!p) return 0;
    for (i = 0; i + 3 < len; ++i) {
        uint32_t start = 0;
        if (i + 3 < len && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1)
            start = i + 3;
        else if (i + 4 < len && p[i] == 0 && p[i + 1] == 0
                 && p[i + 2] == 0 && p[i + 3] == 1) start = i + 4;
        if (start && ((p[start] & 0x1f) == 7 || (p[start] & 0x1f) == 8)) return 1;
    }
    return 0;
}

/* Recovery is intentionally driven by the TCP/server thread.  FillDone only
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
        /* Claim this attempt before releasing the state lock.  A FillDone that
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

void av_sink_frame(const uint8_t *annexb, uint32_t len) {
    int isConfig;
    unsigned long long nowMs;
    if (!annexb || len == 0) return;
    isConfig = _HasParameterSet(annexb, len);
    if (isConfig && len <= sizeof(g_ss.config)) {
        memcpy(g_ss.config, annexb, len);
        g_ss.configLen = len;
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
    }
    if (!g_ss.started) {
        /* FAIL-SAFE: a repeatedly-failing decoder start must NOT loop — each failed OMX GetHandle can
         * hold a hardware decoder slot, and starving the pool made dio's own decoder fail
         * (kBadReferenceErr) → CarPlay crash-loop.  Cap at 3 attempts, then go idle (black, but the
         * OMX pool and dio stay healthy).  Reset per session in av_sink_session_end. */
        nowMs = _NowMs();
        if (g_ss.startFails >= 3) {
            if (nowMs == 0 || nowMs < g_ss.retryAfterMs) return;
            fprintf(stderr,
                    "altscreen_render[qnx]: decoder retry cooldown expired; allowing a new bounded attempt set\n");
            g_ss.startFails = 0;
        }
        if (_StartDecoder(&g_ss) != 0) {
            if (++g_ss.startFails == 3) {
                g_ss.retryAfterMs = _NowMs() + 30000ULL;
                fprintf(stderr,
                        "altscreen_render[qnx]: decoder start failed 3x — cooling down 30s to protect the OMX pool / dio\n");
            }
            return;
        }
        g_ss.startFails = 0;
        g_ss.retryAfterMs = 0;
    }
    /* Preserve SPS/PPS across a failed decoder-start attempt.  Otherwise the
     * next IDR would be mislabeled CODECCONFIG and the retry could never decode. */
    if (!g_ss.fedConfig) {
        if (g_ss.configLen == 0) return;
        _FeedInput(&g_ss, g_ss.config, g_ss.configLen,
                   OMX_BUFFERFLAG_ENDOFFRAME | OMX_BUFFERFLAG_CODECCONFIG);
        if (g_ss.omxError) return;
        g_ss.fedConfig = 1;
    }
    if (!isConfig)
        _FeedInput(&g_ss, annexb, len, OMX_BUFFERFLAG_ENDOFFRAME);
}

int av_sink_video_ready(void) {
    int ready;
    pthread_mutex_lock(&g_ss.lock);
    ready = g_ss.started && !g_ss.omxError && g_ss.presentationHealthy;
    pthread_mutex_unlock(&g_ss.lock);
    return ready;
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
