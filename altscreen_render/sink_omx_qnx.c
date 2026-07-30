/*
 * altscreen_render — QNX cluster sink: OMX AVC decode → zero-copy QNX Screen present.
 *
 * Faithful port of the PROVEN from-source pipeline
 * (Patches/libairplay-mhi2q/src/screenstream.c, see SCREENSTREAM_PARITY.md), with
 * the libairplay wrapper stripped:
 *   - no CFRuntime/ulog/require_* — plain struct + fprintf + goto.
 *   - INPUT IS ALREADY ANNEX-B: the hook (altscreen_av.c) converts AVCC→Annex-B and
 *     forwards SPS/PPS first, then frames.  So NO avcC handling / H264 helpers here —
 *     we feed the received bytes straight to OMX_EmptyThisBuffer (first buffer flagged
 *     CODECCONFIG, the SPS/PPS header the hook sends before the first frame).
 *   - GEOMETRY comes from the cluster size WE advertised in /info (env ALTSCREEN_W/H,
 *     default 1920x720): CarPlay encodes the 111 stream at the advertised display size,
 *     so it is known up front — enabling the zero-copy path (window + N buffers built
 *     before Idle, each Screen buffer's vaddr bound to the OMX output via OMX_UseBuffer;
 *     FillBufferDone posts the Screen buffer directly, no de-tile).
 *
 * BUILD: needs the QNX Screen headers/lib + OMX (MHI2Q BSP) — links on the unit/VM,
 * NOT in the generic docker SDP.  A host syntax-check (make hostcheck) compiles this
 * against stub headers to catch C errors offline.  ON-UNIT UNVALIDATED.
 *
 * QNX 6.5 ARMv7 gcc: no __thread; globals + pthread.
 */
#include "sink.h"

#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_Video.h"

#include <screen/screen.h>
#include "cluster_surface.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define kOMXDecoderName   "OMX.qcom.video.decoder.avc"
#define kInPortIndex      0
#define kOutPortIndex     1

/* qcom output color format the stock decoder emits (COMXVideoDecoder::configure);
 * QNX Screen FORMAT/USAGE literals from dio::OmxVideoImpl::configure
 * (convertColorFormatOMX2Screen(0x7F000004) = 0x1000C tiled-NV12, usage 0x484). */
#define kQOMX_ColorFmt_32m            0x7F000004
#define kQOMX_ExtIndex_DisplayPicBuf  "OMX.QCOM.index.config.video.DisplayPictureBuffer"
#define kScreenFormat_QcomTiledNV12   0x1000C
#define kScreenUsage_QcomVideo        0x484

typedef struct {
    OMX_HANDLETYPE           decoder;
    OMX_BUFFERHEADERTYPE   **inBufs;   OMX_U32 nInBufs;
    OMX_BUFFERHEADERTYPE   **outBufs;  OMX_U32 nOutBufs;
    screen_buffer_t         *outScreenBufs;

    OMX_U32                 *freeIn;   OMX_U32 freeInHead, freeInTail;

    int32_t                  width, height;
    int                      screenFmt;

    cluster_surface_t       *cs;         /* owns screenCtx/win lifecycle */
    screen_context_t         screenCtx;  /* = cluster_surface_context(cs) */
    screen_window_t          win;        /* = cluster_surface_window(cs)  */
    volatile int             winReady;   /* read lock-free on the OMX callback thread; also gates
                                          * buffer resubmit across output-port reconfig (see _OnFillDone) */

    pthread_mutex_t          lock;
    pthread_cond_t           cond;
    volatile OMX_STATETYPE   state;
    volatile int             stateReached;
    volatile int             omxError;
    volatile int             running;
    volatile int             portReconfig;  /* OMX_EventPortSettingsChanged(out) pending */
    volatile int             portCmdDone;   /* Port{Disable,Enable} CmdComplete */

    int                      started;   /* decoder brought up on first frame */
    int                      fedConfig; /* first buffer (SPS/PPS) flagged CODECCONFIG */
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
    } else if (ev == OMX_EventCmdComplete && (d1 == OMX_CommandPortDisable || d1 == OMX_CommandPortEnable)) {
        me->portCmdDone = 1;
    } else if (ev == OMX_EventPortSettingsChanged && d1 == kOutPortIndex) {
        me->portReconfig = 1;   /* decoded geometry ≠ our window → rebuild output port */
        fprintf(stderr, "altscreen_render[qnx]: PortSettingsChanged(out) — reconfiguring output port\n");
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
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    return OMX_ErrorNone;
}

/* zero-copy present: the OMX output buffer IS a Screen buffer → post it directly. */
static void _PresentZeroCopy(ScreenSink *me, OMX_BUFFERHEADERTYPE *b) {
    OMX_U32 i;
    int rect[4];
    if (!me->winReady || b->nFilledLen == 0) return;
    for (i = 0; i < me->nOutBufs; ++i)
        if (me->outBufs[i] == b) {
            rect[0] = 0; rect[1] = 0; rect[2] = me->width; rect[3] = me->height;
            screen_post_window(me->win, me->outScreenBufs[i], 1, rect, 0);
            return;
        }
}

static OMX_ERRORTYPE _OnFillDone(OMX_HANDLETYPE c, OMX_PTR app, OMX_BUFFERHEADERTYPE *b) {
    ScreenSink *const me = (ScreenSink *)app;
    _PresentZeroCopy(me, b);
    /* Recycle — but NOT during output-port reconfig.  _ReconfigOutputPort (feed thread) drops
     * winReady=0 before OMX_CommandPortDisable + OMX_FreeBuffer; the disable makes the component
     * hand every in-flight output buffer back here on the callback thread.  Resubmitting one via
     * FillThisBuffer while the feed thread frees the same header = UAF / illegal-during-disable.
     * winReady stays 0 for the whole disable→rebuild window (re-armed in _MakeWindow), so gating
     * the resubmit on it is exactly the quiesce we need.  running alone is insufficient (stays 1). */
    if (me->running && me->winReady) OMX_FillThisBuffer(c, b);
    return OMX_ErrorNone;
}

static OMX_CALLBACKTYPE kCallbacks = { _OnEvent, _OnEmptyDone, _OnFillDone };

/* ---- helpers -------------------------------------------------------------- */
/* OMX callbacks come from the component's own thread; if it wedges or dies we must
 * NOT wait forever (would hang startup/reconfig/teardown of the whole service).
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

/* Our cluster displayable id (managed window ID_STRING), distinct from
 * maneuver's 98.  Free id from the 61-99 range.  The DM composites the active
 * cluster context (dc[82]={99,102,101}) and the MOST encoder captures it → VC. */
#define SS_DISPLAYABLE_ID  99

/* Build the managed cluster window via the shared cluster_surface primitive —
 * SAME path as maneuver_render, just opaque full-frame video params (NV12).
 * screen_manage_window + ID_STRING + watchdog all live in cluster_surface. */
static int _MakeWindow(ScreenSink *me, int nbufs) {
    const char *fmtEnv = getenv("SS_SCREEN_FORMAT");
    cluster_surface_cfg cfg;
    int fmt = me->screenFmt ? me->screenFmt
            : (fmtEnv ? (int)strtol(fmtEnv, NULL, 0) : kScreenFormat_QcomTiledNV12);
    me->screenFmt = fmt;

    memset(&cfg, 0, sizeof(cfg));
    cfg.id          = SS_DISPLAYABLE_ID;
    cfg.width       = me->width;
    cfg.height      = me->height;
    cfg.format      = fmt;
    cfg.usage       = kScreenUsage_QcomVideo;
    cfg.nbuffers    = nbufs;
    cfg.transparent = 0;               /* opaque full-frame video */

    me->cs = cluster_surface_create(&cfg);
    if (!me->cs) {
        fprintf(stderr, "altscreen_render[qnx]: cluster_surface_create failed\n");
        me->winReady = 0;
        return -1;
    }
    me->screenCtx = cluster_surface_context(me->cs);
    me->win       = cluster_surface_window(me->cs);
    me->winReady  = 1;
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
    if (OMX_SetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
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

static OMX_U32 _OutputBufferCount(ScreenSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kOutPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd) != OMX_ErrorNone) return 0;
    return pd.nBufferCountActual;
}

static int _AllocInput(ScreenSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd; OMX_U32 i;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kInPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    me->nInBufs = pd.nBufferCountActual;
    me->inBufs  = (OMX_BUFFERHEADERTYPE **)calloc(me->nInBufs, sizeof(*me->inBufs));
    if (!me->inBufs) return -1;
    for (i = 0; i < me->nInBufs; ++i)
        if (OMX_AllocateBuffer(me->decoder, &me->inBufs[i], kInPortIndex, me, pd.nBufferSize)) return -1;
    return 0;
}

/* zero-copy output bind: window owns `count` Screen buffers; pass each buffer's CPU
 * vaddr to OMX_UseBuffer so the decoder writes straight into the display buffers. */
static int _UseScreenBuffers(ScreenSink *me, OMX_U32 count) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    screen_buffer_t *sbufs = NULL;
    OMX_U32 i;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kOutPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) goto fail;

    sbufs = (screen_buffer_t *)calloc(count, sizeof(*sbufs));
    if (!sbufs) goto fail;
    if (screen_get_window_property_pv(me->win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)sbufs) != 0) goto fail;

    me->outBufs       = (OMX_BUFFERHEADERTYPE **)calloc(count, sizeof(*me->outBufs));
    me->outScreenBufs = (screen_buffer_t *)      calloc(count, sizeof(*me->outScreenBufs));
    if (!me->outBufs || !me->outScreenBufs) goto fail;

    for (i = 0; i < count; ++i) {
        void *vaddr = NULL;
        if (screen_get_buffer_property_pv(sbufs[i], SCREEN_PROPERTY_POINTER, &vaddr) != 0 || !vaddr) goto fail;
        if (OMX_UseBuffer(me->decoder, &me->outBufs[i], kOutPortIndex, me, pd.nBufferSize, (OMX_U8 *)vaddr)) goto fail;
        me->outScreenBufs[i] = sbufs[i];
    }
    me->nOutBufs = count;
    free(sbufs);
    return 0;
fail:
    free(sbufs);
    return -1;
}

static void _Teardown(ScreenSink *me) {
    OMX_U32 i;
    if (!me->decoder && !me->screenCtx) return;
    me->running = 0;
    pthread_mutex_lock(&me->lock); pthread_cond_broadcast(&me->cond); pthread_mutex_unlock(&me->lock);
    if (me->decoder) {
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        _WaitState(me, OMX_StateIdle);
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        for (i = 0; i < me->nInBufs;  ++i) OMX_FreeBuffer(me->decoder, kInPortIndex,  me->inBufs[i]);
        for (i = 0; i < me->nOutBufs; ++i) OMX_FreeBuffer(me->decoder, kOutPortIndex, me->outBufs[i]);
        _WaitState(me, OMX_StateLoaded);
        OMX_FreeHandle(me->decoder); me->decoder = NULL;
        OMX_Deinit();
    }
    if (me->cs) { cluster_surface_destroy(me->cs); me->cs = NULL; }
    me->win = NULL; me->screenCtx = NULL;
    free(me->inBufs);        me->inBufs = NULL;        me->nInBufs = 0;
    free(me->outBufs);       me->outBufs = NULL;       me->nOutBufs = 0;
    free(me->outScreenBufs); me->outScreenBufs = NULL;
    free(me->freeIn);        me->freeIn = NULL;
    me->winReady = 0; me->started = 0; me->fedConfig = 0;
    me->stateReached = 0; me->omxError = 0;
    me->portReconfig = 0; me->portCmdDone = 0;
}

/* bring up OMX + zero-copy Screen window (geometry already known = advertised size). */
static int _StartDecoder(ScreenSink *me) {
    OMX_U32 i, outCount;
    if (OMX_Init()) goto fail;
    if (OMX_GetHandle(&me->decoder, (OMX_STRING)kOMXDecoderName, me, &kCallbacks)) goto fail;

    _EnableDecoderOrderMode(me);
    if (_ConfigurePorts(me)) goto fail;

    outCount = _OutputBufferCount(me);
    if (outCount == 0 || _MakeWindow(me, (int)outCount) != 0) goto fail;

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
    for (i = 0; i < me->nOutBufs; ++i) OMX_FillThisBuffer(me->decoder, me->outBufs[i]);

    me->started = 1;
    fprintf(stderr, "altscreen_render[qnx]: decoder started %dx%d, zero-copy %u bufs\n",
            me->width, me->height, (unsigned)me->nOutBufs);
    return 0;
fail:
    fprintf(stderr, "altscreen_render[qnx]: decoder start FAILED\n");
    _Teardown(me);
    return -1;
}

/* feed one Annex-B buffer to the decoder (blocks for a free input buffer). */
static void _FeedInput(ScreenSink *me, const uint8_t *data, uint32_t len, OMX_U32 flags) {
    OMX_BUFFERHEADERTYPE *b; OMX_U32 idx;
    pthread_mutex_lock(&me->lock);
    while (me->freeInHead == me->freeInTail && me->running && !me->omxError)
        pthread_cond_wait(&me->cond, &me->lock);
    if (!me->running || me->omxError) { pthread_mutex_unlock(&me->lock); return; }
    idx = me->freeIn[me->freeInHead % me->nInBufs]; me->freeInHead++;
    pthread_mutex_unlock(&me->lock);

    b = me->inBufs[idx];
    if (len > b->nAllocLen) len = b->nAllocLen;   /* ponytail: oversized frames clipped */
    memcpy(b->pBuffer, data, len);
    b->nOffset = 0; b->nFilledLen = len;
    b->nFlags = flags;
    b->nTimeStamp = 0;                            /* decoder-order mode → pts not needed */
    if (OMX_EmptyThisBuffer(me->decoder, b) != OMX_ErrorNone) {
        /* Component rejected the buffer → it will NOT call _OnEmptyDone for it.
         * Return the index to the free ring so the pool isn't permanently drained
         * (else _FeedInput eventually blocks forever waiting for a free buffer). */
        pthread_mutex_lock(&me->lock);
        me->freeIn[me->freeInTail % me->nInBufs] = idx; me->freeInTail++;
        pthread_cond_broadcast(&me->cond);
        pthread_mutex_unlock(&me->lock);
    }
}

static int _WaitPortCmd(ScreenSink *me) {
    struct timespec ts; int err = 0;
    _AbsTimeoutMs(&ts, kOmxWaitMs);
    pthread_mutex_lock(&me->lock);
    while (!me->portCmdDone && !me->omxError) {
        if (pthread_cond_timedwait(&me->cond, &me->lock, &ts) == ETIMEDOUT) {
            fprintf(stderr, "altscreen_render[qnx]: _WaitPortCmd TIMEOUT\n");
            me->omxError = 1; break;
        }
    }
    if (me->omxError) err = -1;
    me->portCmdDone = 0;
    pthread_mutex_unlock(&me->lock);
    return err;
}

/* Output-port reconfig on OMX_EventPortSettingsChanged (decoded geometry differs
 * from the window we built up front).  Runs on the FEED thread (NOT the OMX
 * callback thread), so waiting for the Port{Disable,Enable} CmdComplete events
 * (delivered on the callback thread) is safe.  Standard OMX IL dance:
 *   disable out port → free its buffers → rebuild the Screen window at the new
 *   size → enable → re-bind (OMX_UseBuffer) the new window buffers → re-prime.
 * winReady is dropped for the duration so _PresentZeroCopy skips the half-built
 * window; the port-disable quiesces FillBufferDone for the old buffers. */
static int _ReconfigOutputPort(ScreenSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    OMX_U32 i, newCount;

    pthread_mutex_lock(&me->lock);
    me->winReady = 0;
    me->portReconfig = 0;
    pthread_mutex_unlock(&me->lock);

    /* 1. disable out port + free its (UseBuffer) headers */
    if (OMX_SendCommand(me->decoder, OMX_CommandPortDisable, kOutPortIndex, NULL)) return -1;
    for (i = 0; i < me->nOutBufs; ++i)
        if (me->outBufs[i]) OMX_FreeBuffer(me->decoder, kOutPortIndex, me->outBufs[i]);
    if (_WaitPortCmd(me)) return -1;

    /* 2. tear down the old window (frees its Screen buffers) + arrays */
    if (me->cs) { cluster_surface_destroy(me->cs); me->cs = NULL; }
    me->win = NULL; me->screenCtx = NULL;
    free(me->outBufs);       me->outBufs = NULL;
    free(me->outScreenBufs); me->outScreenBufs = NULL;
    me->nOutBufs = 0;

    /* 3. new geometry + buffer count from the port */
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = kOutPortIndex;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    if (pd.format.video.nFrameWidth)  me->width  = (int32_t)pd.format.video.nFrameWidth;
    if (pd.format.video.nFrameHeight) me->height = (int32_t)pd.format.video.nFrameHeight;
    newCount = pd.nBufferCountActual;
    fprintf(stderr, "altscreen_render[qnx]: reconfig → %dx%d, %u bufs\n",
            me->width, me->height, (unsigned)newCount);
    if (newCount == 0) return -1;

    /* 4. rebuild the Screen window at the new size */
    if (_MakeWindow(me, (int)newCount) != 0) return -1;

    /* 5. re-enable the port, re-bind the new window buffers, wait for completion */
    if (OMX_SendCommand(me->decoder, OMX_CommandPortEnable, kOutPortIndex, NULL)) return -1;
    if (_UseScreenBuffers(me, newCount)) return -1;
    if (_WaitPortCmd(me)) return -1;

    /* 6. prime the new output buffers */
    for (i = 0; i < me->nOutBufs; ++i) OMX_FillThisBuffer(me->decoder, me->outBufs[i]);
    return 0;
}

/* ---- sink API ------------------------------------------------------------- */
int av_sink_open(void) {
    const char *ew = getenv("ALTSCREEN_W"), *eh = getenv("ALTSCREEN_H");
    memset(&g_ss, 0, sizeof(g_ss));
    pthread_mutex_init(&g_ss.lock, NULL);
    pthread_cond_init(&g_ss.cond, NULL);
    g_ss.width  = ew ? atoi(ew) : 1440;           /* = advertised cluster size = Audi Kombi panel */
    g_ss.height = eh ? atoi(eh) : 542;            /* qcdisplaycfg_MMX2.xml: "Audi 1440x542 Kombi panel" */
    fprintf(stderr, "altscreen_render[qnx]: sink open (%dx%d, OMX zero-copy)\n", g_ss.width, g_ss.height);
    return 0;
}

void av_sink_frame(const uint8_t *annexb, uint32_t len) {
    if (!annexb || len == 0) return;
    if (!g_ss.started && _StartDecoder(&g_ss) != 0) return;
    if (g_ss.portReconfig && _ReconfigOutputPort(&g_ss) != 0) {
        fprintf(stderr, "altscreen_render[qnx]: output reconfig FAILED → teardown\n");
        _Teardown(&g_ss);
        return;
    }
    /* first message from the hook is the Annex-B SPS/PPS header → CODECCONFIG. */
    OMX_U32 flags = OMX_BUFFERFLAG_ENDOFFRAME;
    if (!g_ss.fedConfig) { flags |= OMX_BUFFERFLAG_CODECCONFIG; g_ss.fedConfig = 1; }
    _FeedInput(&g_ss, annexb, len, flags);
}

void av_sink_session_end(void) {
    fprintf(stderr, "altscreen_render[qnx]: session end → teardown decoder\n");
    _Teardown(&g_ss);
}

void av_sink_close(void) {
    _Teardown(&g_ss);
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
}
