/* Copyright (c) 2026, original project author. */
/* Adapted real decoder core; output_backend implementation is intentionally separate.
 * This core is for an isolated renderer process. A stalled data callback can
 * force process exit rather than freeing buffers still used by that callback.
 */
#define _POSIX_C_SOURCE 200809L
#include "sink.h"
#include "output_backend.h"
#include "OMX_Video.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

typedef struct {
    output_config outputConfig;
    OMX_HANDLETYPE           decoder;
    OMX_BUFFERHEADERTYPE   **inBufs;   OMX_U32 nInBufs;
    OMX_BUFFERHEADERTYPE   **outBufs;  OMX_U32 nOutBufs;
    OMX_U32                 *freeIn;   OMX_U32 freeInHead, freeInTail;

    int32_t                  width, height;
    output_backend_t     *present;   
    volatile int             winReady;   

    pthread_mutex_t          lock;
    pthread_cond_t           cond;
    volatile OMX_STATETYPE   state;
    volatile int             stateReached;
    volatile int             omxError;
    volatile int             running;
    int                      callbacksClosing;
    unsigned int             callbacksActive;
    int                      omxInitialized;
    int                      started;   
    int                      sessionEverStarted;
    int                      startFails; 
    unsigned long long       retryAfterMs;
    int                      fedConfig; 

    unsigned long            inputSubmitted;
    unsigned long            inputReturned;
    unsigned long            outputFilled;
    unsigned long            outputPosted;
    unsigned long            outputPostFailed;
    uint8_t                  config[4096];
    uint32_t                 configLen;
} VideoSink;

static VideoSink g_ss;
#define kOmxStartIdleWaitMs 10000L
#define kOmxStartExecWaitMs 2000L
#define kOmxStopWaitMs 900L
#define kCallbackDrainWaitMs 1500L

static int _CallbackEnter(VideoSink *me) {
    int allowed;
    pthread_mutex_lock(&me->lock);
    allowed = !me->callbacksClosing;
    if (allowed) me->callbacksActive++;
    pthread_mutex_unlock(&me->lock);
    return allowed;
}

static void _CallbackLeave(VideoSink *me) {
    pthread_mutex_lock(&me->lock);
    if (me->callbacksActive > 0) me->callbacksActive--;
    if (me->callbacksActive == 0) pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
}

static OMX_ERRORTYPE _OnEvent(OMX_HANDLETYPE c, OMX_PTR app, OMX_EVENTTYPE event,
                               OMX_U32 d1, OMX_U32 d2, OMX_PTR extra)
{
    VideoSink *me=(VideoSink *)app;
    (void)c; (void)extra;
    pthread_mutex_lock(&me->lock);
    if (event==OMX_EventCmdComplete && d1==OMX_CommandStateSet) {
        me->state=(OMX_STATETYPE)d2;
        me->stateReached=1;
    } else if (event==OMX_EventError ||
               (event==OMX_EventPortSettingsChanged && d1==me->outputConfig.output_port)) {
        me->omxError=1;
        fprintf(stderr,"video_decoder: component error or unsupported output reconfiguration\n");
    }
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE _OnEmptyDone(OMX_HANDLETYPE c, OMX_PTR app, OMX_BUFFERHEADERTYPE *b) {
    VideoSink *const me = (VideoSink *)app;
    OMX_U32 i;
    (void)c;
    if (!_CallbackEnter(me)) return OMX_ErrorNone;
    pthread_mutex_lock(&me->lock);
    for (i = 0; i < me->nInBufs; ++i)
        if (me->inBufs[i] == b) { me->freeIn[me->freeInTail % me->nInBufs] = i; me->freeInTail++; break; }
    me->inputReturned++;
    if (me->inputReturned <= 3 || (me->inputReturned % 600) == 0)
        fprintf(stderr, "video_decoder: EmptyBufferDone #%lu header=%p matched=%s\n",
                me->inputReturned, (void *)b, (i < me->nInBufs) ? "yes" : "NO");
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    _CallbackLeave(me);
    return OMX_ErrorNone;
}

static int _PresentOutput(VideoSink *me, OMX_BUFFERHEADERTYPE *header)
{
    OMX_U32 i;
    if (!me->winReady || !header->nFilledLen) return -1;
    for (i=0;i<me->nOutBufs;++i) {
        if (me->outBufs[i]!=header) continue;
        int result=output_backend_present(me->present,(unsigned int)i,header);
        pthread_mutex_lock(&me->lock);
        if (result==0) ++me->outputPosted;
        else { ++me->outputPostFailed; me->omxError=1; }
        pthread_mutex_unlock(&me->lock);
        return result;
    }
    return -1;
}

static OMX_ERRORTYPE _OnFillDone(OMX_HANDLETYPE c, OMX_PTR app, OMX_BUFFERHEADERTYPE *b) {
    VideoSink *const me = (VideoSink *)app;
    OMX_ERRORTYPE err;
    if (!_CallbackEnter(me)) return OMX_ErrorNone;
    me->outputFilled++;
    if (me->outputFilled <= 3 || (me->outputFilled % 600) == 0)
        fprintf(stderr,
                "video_decoder: FillBufferDone #%lu filled=%u offset=%u flags=0x%x winReady=%d\n",
                me->outputFilled, (unsigned)b->nFilledLen, (unsigned)b->nOffset,
                (unsigned)b->nFlags, me->winReady);
    _PresentOutput(me, b);
    
    if (me->running && me->winReady) {
        err = OMX_FillThisBuffer(c, b);
        if (err != OMX_ErrorNone)
            fprintf(stderr, "video_decoder: OMX_FillThisBuffer(recycle) failed 0x%x\n",
                    (unsigned)err);
    }
    _CallbackLeave(me);
    return OMX_ErrorNone;
}

static OMX_CALLBACKTYPE kCallbacks={_OnEvent,_OnEmptyDone,_OnFillDone};

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

static int _WaitState(VideoSink *me, OMX_STATETYPE want, long timeoutMs) {
    struct timespec ts; int err = 0;
    _AbsTimeoutMs(&ts, timeoutMs);
    pthread_mutex_lock(&me->lock);
    while (!(me->stateReached && me->state == want) && !me->omxError) {
        if (pthread_cond_timedwait(&me->cond, &me->lock, &ts) == ETIMEDOUT) {
            fprintf(stderr, "video_decoder: _WaitState TIMEOUT (want=%d)\n", (int)want);
            me->omxError = 1; break;
        }
    }
    if (me->omxError) err = -1;
    me->stateReached = 0;
    pthread_mutex_unlock(&me->lock);
    return err;
}

static int _ConfigurePorts(VideoSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = me->outputConfig.input_port;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    pd.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    pd.format.video.eColorFormat       = OMX_COLOR_FormatUnused;
    if (me->width > 0 && me->height > 0) {
        pd.format.video.nFrameWidth  = (OMX_U32)me->width;
        pd.format.video.nFrameHeight = (OMX_U32)me->height;
    }
    if (OMX_SetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;

    return output_backend_configure_decoder(me->decoder,&me->outputConfig);
}

static OMX_U32 _MakeOutput(VideoSink *me)
{
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    memset(&pd,0,sizeof(pd)); pd.nSize=sizeof(pd); pd.nVersion.nVersion=OMX_VERSION;
    pd.nPortIndex=me->outputConfig.output_port;
    if (OMX_GetParameter(me->decoder,OMX_IndexParamPortDefinition,&pd)) return 0;
    if (!pd.nBufferCountActual || !pd.nBufferSize) return 0;
    me->present=output_backend_create(&me->outputConfig,pd.nBufferCountActual,pd.nBufferSize);
    if (!me->present) return 0;
    me->winReady=1;
    return pd.nBufferCountActual;
}

static int _AllocInput(VideoSink *me) {
    OMX_PARAM_PORTDEFINITIONTYPE pd; OMX_U32 i, count;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = me->outputConfig.input_port;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) return -1;
    count = pd.nBufferCountActual;
    if (!count || !pd.nBufferSize) return -1;
    me->inBufs = (OMX_BUFFERHEADERTYPE **)calloc(count, sizeof(*me->inBufs));
    if (!me->inBufs) return -1;
    me->nInBufs = 0;
    for (i = 0; i < count; ++i) {
        if (OMX_AllocateBuffer(me->decoder, &me->inBufs[i], me->outputConfig.input_port, me,
                               pd.nBufferSize)) return -1;
        ++me->nInBufs;
    }
    return 0;
}

static int _UseOutputBuffers(VideoSink *me, OMX_U32 count) {
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    OMX_U32 i;
    memset(&pd, 0, sizeof(pd)); pd.nSize = sizeof(pd); pd.nVersion.nVersion = OMX_VERSION;
    pd.nPortIndex = me->outputConfig.output_port;
    if (OMX_GetParameter(me->decoder, OMX_IndexParamPortDefinition, &pd)) goto fail;

    me->outBufs       = (OMX_BUFFERHEADERTYPE **)calloc(count, sizeof(*me->outBufs));
    if (!me->outBufs) goto fail;
    me->nOutBufs = 0;

    for (i = 0; i < count; ++i) {
        void *vaddr = output_backend_pointer(me->present,
                                                         (unsigned int)i);
        unsigned int capacity = output_backend_capacity(me->present,
                                                                    (unsigned int)i);
        
        if (!vaddr || capacity < pd.nBufferSize) goto fail;
        if (OMX_UseBuffer(me->decoder, &me->outBufs[i], me->outputConfig.output_port, me, pd.nBufferSize, (OMX_U8 *)vaddr)) goto fail;
        ++me->nOutBufs;
    }
    return 0;
fail:
    return -1;
}

static void _Teardown(VideoSink *me) {
    OMX_U32 i;
    if (!me->decoder && !me->present && !me->omxInitialized) return;
    pthread_mutex_lock(&me->lock);
    me->running = 0;
    me->winReady = 0;
    me->callbacksClosing = 1;
    pthread_cond_broadcast(&me->cond);
    {
        struct timespec callbackDeadline;
        int waitRc = 0;
        _AbsTimeoutMs(&callbackDeadline, kCallbackDrainWaitMs);
        while (me->callbacksActive > 0 && waitRc != ETIMEDOUT)
            waitRc = pthread_cond_timedwait(&me->cond, &me->lock,
                                            &callbackDeadline);
        if (me->callbacksActive > 0) {
            fprintf(stderr,
                    "video_decoder: callback drain TIMEOUT active=%u; exiting without freeing live component storage\n",
                    me->callbacksActive);
            pthread_mutex_unlock(&me->lock);
            
            _exit(86);
        }
    }
    pthread_mutex_unlock(&me->lock);
    if (me->decoder) {
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        _WaitState(me, OMX_StateIdle, kOmxStopWaitMs);
        OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        for (i = 0; i < me->nInBufs;  ++i) OMX_FreeBuffer(me->decoder, me->outputConfig.input_port,  me->inBufs[i]);
        for (i = 0; i < me->nOutBufs; ++i) OMX_FreeBuffer(me->decoder, me->outputConfig.output_port, me->outBufs[i]);
        _WaitState(me, OMX_StateLoaded, kOmxStopWaitMs);
        OMX_FreeHandle(me->decoder); me->decoder = NULL;
    }
    if (me->omxInitialized) {
        OMX_Deinit();
        me->omxInitialized = 0;
    }
    if (me->present) { output_backend_destroy(me->present); me->present=NULL; }
    free(me->inBufs);        me->inBufs = NULL;        me->nInBufs = 0;
    free(me->outBufs);       me->outBufs = NULL;       me->nOutBufs = 0;
    free(me->freeIn);        me->freeIn = NULL;
    me->winReady = 0; me->started = 0; me->fedConfig = 0;
    me->stateReached = 0; me->omxError = 0;
    me->inputSubmitted = me->inputReturned = 0;
    me->outputFilled = me->outputPosted = me->outputPostFailed = 0;

}

static int _StartDecoder(VideoSink *me) {
    OMX_U32 i, outCount;
    pthread_mutex_lock(&me->lock);
    me->callbacksClosing = 0;
    pthread_mutex_unlock(&me->lock);
    if (OMX_Init()) goto fail;
    me->omxInitialized = 1;
    if (OMX_GetHandle(&me->decoder, (OMX_STRING)me->outputConfig.component_name, me, &kCallbacks)) goto fail;

    if (_ConfigurePorts(me)) goto fail;

    outCount = _MakeOutput(me);
    if (outCount == 0) goto fail;

    if (OMX_SendCommand(me->decoder, OMX_CommandStateSet, OMX_StateIdle, NULL)) goto fail;
    if (_AllocInput(me)) goto fail;
    if (_UseOutputBuffers(me, outCount)) goto fail;
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
            fprintf(stderr, "video_decoder: OMX_FillThisBuffer(prime %u) failed 0x%x\n",
                    (unsigned)i, (unsigned)err);
            goto fail;
        }
    }

    me->started = 1;
    me->sessionEverStarted = 1;
    fprintf(stderr, "video_decoder: decoder started %dx%d, %u adapter output buffers\n",
            me->width, me->height, (unsigned)me->nOutBufs);
    return 0;
fail:
    fprintf(stderr, "video_decoder: decoder start FAILED\n");
    _Teardown(me);
    return -1;
}

static void _FeedInput(VideoSink *me, const uint8_t *data, uint32_t len, OMX_U32 flags) {
    OMX_BUFFERHEADERTYPE *b; OMX_U32 idx;
    OMX_ERRORTYPE err;
    uint32_t requestedLen = len;
    pthread_mutex_lock(&me->lock);
    while (me->freeInHead == me->freeInTail && me->running && !me->omxError) {
        struct timespec ts;
        _AbsTimeoutMs(&ts, 500L);
        if (pthread_cond_timedwait(&me->cond, &me->lock, &ts) == ETIMEDOUT) {
            fprintf(stderr,
                    "video_decoder: input-buffer wait TIMEOUT; marking decoder unhealthy\n");
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
                "video_decoder: INPUT DROPPED requested=%u alloc=%u (never submit a clipped access unit)\n",
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
    b->nTimeStamp = 0;                            
    err = OMX_EmptyThisBuffer(me->decoder, b);
    if (err == OMX_ErrorNone) {
        me->inputSubmitted++;
        if (me->inputSubmitted <= 3 || (me->inputSubmitted % 600) == 0)
            fprintf(stderr,
                    "video_decoder: EmptyThisBuffer OK #%lu requested=%u submitted=%u flags=0x%x in=%u\n",
                    me->inputSubmitted, (unsigned)requestedLen, (unsigned)len,
                    (unsigned)flags, (unsigned)idx);
    } else {
        fprintf(stderr,
                "video_decoder: OMX_EmptyThisBuffer FAILED 0x%x requested=%u submitted=%u flags=0x%x in=%u\n",
                (unsigned)err, (unsigned)requestedLen, (unsigned)len,
                (unsigned)flags, (unsigned)idx);
        
        pthread_mutex_lock(&me->lock);
        me->freeIn[me->freeInTail % me->nInBufs] = idx; me->freeInTail++;
        pthread_cond_broadcast(&me->cond);
        pthread_mutex_unlock(&me->lock);
    }
}

int av_sink_open(void)
{
    memset(&g_ss,0,sizeof(g_ss));
    if (output_backend_get_config(&g_ss.outputConfig)!=0 || !g_ss.outputConfig.component_name ||
        !*g_ss.outputConfig.component_name || g_ss.outputConfig.width<=0 || g_ss.outputConfig.height<=0 ||
        g_ss.outputConfig.input_port==g_ss.outputConfig.output_port) return -1;
    if (pthread_mutex_init(&g_ss.lock,NULL)!=0) return -1;
    if (pthread_cond_init(&g_ss.cond,NULL)!=0) { pthread_mutex_destroy(&g_ss.lock); return -1; }
    g_ss.width=g_ss.outputConfig.width; g_ss.height=g_ss.outputConfig.height;
    return 0;
}

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

void av_sink_frame(const uint8_t *annexb, uint32_t len) {
    int isConfig;
    unsigned long long nowMs;
    if (!annexb || len == 0) return;
    isConfig = _HasParameterSet(annexb, len);
    if (isConfig && len <= sizeof(g_ss.config)) {
        memcpy(g_ss.config, annexb, len);
        g_ss.configLen = len;
    }
    /* A latched runtime error requires a new session in this excerpt. */
    if (g_ss.started && g_ss.omxError) return;
    if (!g_ss.started) {
        
        nowMs = _NowMs();
        if (g_ss.startFails >= 3) {
            if (nowMs == 0 || nowMs < g_ss.retryAfterMs) return;
            fprintf(stderr,
                    "video_decoder: decoder retry cooldown expired; allowing a new bounded attempt set\n");
            g_ss.startFails = 0;
        }
        if (_StartDecoder(&g_ss) != 0) {
            if (++g_ss.startFails == 3) {
                g_ss.retryAfterMs = _NowMs() + 30000ULL;
                fprintf(stderr,
                        "video_decoder: decoder start failed 3x — cooling down 30s to protect component resources\n");
            }
            return;
        }
        g_ss.startFails = 0;
        g_ss.retryAfterMs = 0;
    }
    
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

void av_sink_session_end(void)
{
    int worked=g_ss.sessionEverStarted;
    _Teardown(&g_ss);
    g_ss.configLen=0;
    g_ss.sessionEverStarted=0;
    if (worked) { g_ss.startFails=0; g_ss.retryAfterMs=0; }
}

void av_sink_close(void)
{
    _Teardown(&g_ss);
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
}
