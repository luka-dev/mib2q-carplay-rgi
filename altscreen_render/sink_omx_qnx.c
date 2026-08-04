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
    volatile int             winReady;   /* read lock-free on the OMX callback thread */

    pthread_mutex_t          lock;
    pthread_cond_t           cond;
    volatile OMX_STATETYPE   state;
    volatile int             stateReached;
    volatile int             omxError;
    volatile int             running;
    int                      omxInitialized;
    int                      started;   /* decoder brought up on first frame */
    int                      startFails; /* consecutive _StartDecoder failures; capped to protect OMX pool */
    int                      fedConfig; /* first buffer (SPS/PPS) flagged CODECCONFIG */

    /* Decisive pipeline diagnostics.  Decoder creation alone proves neither that
     * input was accepted nor that a decoded Screen buffer was posted. */
    unsigned long            inputSubmitted;
    unsigned long            inputReturned;
    unsigned long            outputFilled;
    unsigned long            outputPosted;
    unsigned long            outputPostFailed;
} ScreenSink;

static ScreenSink g_ss;

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
    pthread_mutex_lock(&me->lock);
    for (i = 0; i < me->nInBufs; ++i)
        if (me->inBufs[i] == b) { me->freeIn[me->freeInTail % me->nInBufs] = i; me->freeInTail++; break; }
    me->inputReturned++;
    if (me->inputReturned <= 3 || (me->inputReturned % 600) == 0)
        fprintf(stderr, "altscreen_render[qnx]: EmptyBufferDone #%lu header=%p matched=%s\n",
                me->inputReturned, (void *)b, (i < me->nInBufs) ? "yes" : "NO");
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    return OMX_ErrorNone;
}

/* The OMX output pointer belongs to a tiled Screen WINDOW buffer.  The GPU
 * external-image draw consumes that exact buffer before this callback recycles it. */
static int _PresentGpu(ScreenSink *me, OMX_BUFFERHEADERTYPE *b) {
    OMX_U32 i;
    if (!me->winReady || b->nFilledLen == 0) return -1;
    for (i = 0; i < me->nOutBufs; ++i) {
        if (me->outBufs[i] != b) continue;
        if (screen_egl_present_frame(me->present, (unsigned int)i,
                                     b->nFilledLen) == 0) {
            me->outputPosted++;
            return 0;
        }
        me->outputPostFailed++;
        fprintf(stderr,
                "altscreen_render[qnx]: EGLImage GPU present FAILED #%lu src=%u filled=%u\n",
                me->outputPostFailed, (unsigned)i, (unsigned)b->nFilledLen);
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
    return OMX_ErrorNone;
}

static OMX_CALLBACKTYPE kCallbacks = { _OnEvent, _OnEmptyDone, _OnFillDone };

/* ---- helpers -------------------------------------------------------------- */
/* OMX callbacks come from the component's own thread; if it wedges or dies we must
 * NOT wait forever (would hang startup/teardown of the whole service).
 * Bound every CmdComplete/state wait; a timeout is a hard error → latch omxError. */
#define kOmxWaitMs   10000L      /* generous — only fires on a genuine OMX hang */

static void _AbsTimeoutMs(struct timespec *ts, long ms) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec  += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) { ts->tv_sec++; ts->tv_nsec -= 1000000000L; }
}

static int _WaitState(ScreenSink *me, OMX_STATETYPE want) {
    struct timespec ts; int err = 0;
    _AbsTimeoutMs(&ts, kOmxWaitMs);
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
}

static int _MakePresent(ScreenSink *me, OMX_U32 count) {
    if (me->present && me->presentOutCount == count) {
        me->winReady = 1;
        fprintf(stderr,
                "altscreen_render[qnx]: reusing persistent EGL/Screen presenter across reconnect (%u buffers)\n",
                (unsigned)count);
        return 0;
    }
    if (me->present) _DestroyPresent(me);
    me->present = screen_egl_present_create(me->width, me->height, count);
    if (!me->present) {
        fprintf(stderr, "altscreen_render[qnx]: EGLImage GPU path setup FAILED\n");
        me->winReady = 0;
        return -1;
    }
    me->presentOutCount = count;
    me->winReady = 1;
    fprintf(stderr,
            "altscreen_render[qnx]: output path OMX=0x7f000004 tiled Screen buffer -> external EGLImage -> RGBA8888 plane=99\n");
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
        if (!vaddr) goto fail;
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
    me->running = 0;
    me->winReady = 0;
    pthread_mutex_lock(&me->lock); pthread_cond_broadcast(&me->cond); pthread_mutex_unlock(&me->lock);
    if (me->decoder) {
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        _WaitState(me, OMX_StateIdle);
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        for (i = 0; i < me->nInBufs;  ++i) OMX_FreeBuffer(me->decoder, kInPortIndex,  me->inBufs[i]);
        for (i = 0; i < me->nOutBufs; ++i) OMX_FreeBuffer(me->decoder, kOutPortIndex, me->outBufs[i]);
        _WaitState(me, OMX_StateLoaded);
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
}

/* Bring up native 32m OMX output + post-decode EGL external-image conversion. */
static int _StartDecoder(ScreenSink *me) {
    OMX_U32 i, outCount;
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
    if (_WaitState(me, OMX_StateIdle)) goto fail;

    me->freeIn = (OMX_U32 *)calloc(me->nInBufs, sizeof(OMX_U32));
    if (!me->freeIn) goto fail;
    for (i = 0; i < me->nInBufs; ++i) me->freeIn[i] = i;
    me->freeInHead = 0; me->freeInTail = me->nInBufs;

    if (OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)) goto fail;
    if (_WaitState(me, OMX_StateExecuting)) goto fail;
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
    while (me->freeInHead == me->freeInTail && me->running && !me->omxError)
        pthread_cond_wait(&me->cond, &me->lock);
    if (!me->running || me->omxError) { pthread_mutex_unlock(&me->lock); return; }
    idx = me->freeIn[me->freeInHead % me->nInBufs]; me->freeInHead++;
    pthread_mutex_unlock(&me->lock);

    b = me->inBufs[idx];
    if (len > b->nAllocLen) {
        fprintf(stderr,
                "altscreen_render[qnx]: INPUT CLIPPED requested=%u alloc=%u (frame is invalid after clipping)\n",
                (unsigned)len, (unsigned)b->nAllocLen);
        len = b->nAllocLen;
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
    return 0;
}

void av_sink_frame(const uint8_t *annexb, uint32_t len) {
    if (!annexb || len == 0) return;
    if (!g_ss.started) {
        /* FAIL-SAFE: a repeatedly-failing decoder start must NOT loop — each failed OMX GetHandle can
         * hold a hardware decoder slot, and starving the pool made dio's own decoder fail
         * (kBadReferenceErr) → CarPlay crash-loop.  Cap at 3 attempts, then go idle (black, but the
         * OMX pool and dio stay healthy).  Reset per session in av_sink_session_end. */
        if (g_ss.startFails >= 3) return;
        if (_StartDecoder(&g_ss) != 0) {
            if (++g_ss.startFails == 3)
                fprintf(stderr, "altscreen_render[qnx]: decoder start failed 3x — GIVING UP (idle) to protect the OMX pool / dio\n");
            return;
        }
        g_ss.startFails = 0;
    }
    /* first message from the hook is the Annex-B SPS/PPS header → CODECCONFIG. */
    OMX_U32 flags = OMX_BUFFERFLAG_ENDOFFRAME;
    if (!g_ss.fedConfig) { flags |= OMX_BUFFERFLAG_CODECCONFIG; g_ss.fedConfig = 1; }
    _FeedInput(&g_ss, annexb, len, flags);
}

void av_sink_session_end(void) {
    fprintf(stderr, "altscreen_render[qnx]: session end → teardown decoder; keep EGL/Screen presenter for reconnect\n");
    _Teardown(&g_ss, 0);
    g_ss.startFails = 0;     /* fresh attempts on the next connection */
}

void av_sink_close(void) {
    _Teardown(&g_ss, 1);
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
}
