/* SPDX-License-Identifier: GPL-3.0-or-later
 * Hardware H.264 decode for the cluster player - see omxdec.h. Built from src/omx_probe.c (the car run that
 * proved the component): same library loading, port setup, output reconfiguration and buffer flow.
 * Output buffers: the newest decoded frame is held for the presenter (omxdec_take de-tiles it into planar
 * YUV and hands the buffer back); an older undisplayed one is returned to the decoder at once, so frames are
 * replaced, never shown late - the same policy as the FFmpeg path.
 */
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_Video.h"
#include "omxdec.h"
#include "detile.h"

#define MAXBUF 32
#define QCOM_TILED 0x7f000004u   /* OMX_QCOM_COLOR_FormatYVU420PackedSemiPlanar32m4ka... 64x32 tiled NV12 */
#define LOG(...) do { fprintf(stderr, "omx: " __VA_ARGS__); fputc('\n', stderr); } while (0)
#define INIT(x) do { memset(&(x), 0, sizeof(x)); (x).nSize = sizeof(x); (x).nVersion.s.nVersionMajor = 1; \
                     (x).nVersion.s.nVersionMinor = 1; (x).nVersion.s.nRevision = 2; } while (0)

typedef OMX_ERRORTYPE (*init_fn)(void);
typedef OMX_ERRORTYPE (*gethandle_fn)(OMX_HANDLETYPE *, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE *);
typedef OMX_ERRORTYPE (*freehandle_fn)(OMX_HANDLETYPE);

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;     /* flags, free lists, held buffer */
static pthread_mutex_t use_mu = PTHREAD_MUTEX_INITIALIZER; /* output buffers in use (de-tile vs. free) */
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static OMX_HANDLETYPE comp;
static freehandle_fn ofree;
static OMX_BUFFERHEADERTYPE *inb[MAXBUF], *outb[MAXBUF], *held;
static int nin, nout, in_free[MAXBUF];
static int state_reached = -1, cmd_done_port = -1, port_changed, errors, reconf, closing, opened;
static unsigned frames, fed;
static int out_w = 1920, out_h = 1080, out_stride = 1920, out_slice = 1088;
static unsigned out_color = QCOM_TILED;

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static OMX_ERRORTYPE on_event(OMX_HANDLETYPE h, OMX_PTR app, OMX_EVENTTYPE e, OMX_U32 d1, OMX_U32 d2, OMX_PTR data)
{
    (void)h; (void)app; (void)data;
    pthread_mutex_lock(&mu);
    if (e == OMX_EventCmdComplete && d1 == OMX_CommandStateSet) state_reached = (int)d2;
    else if (e == OMX_EventCmdComplete && (d1 == OMX_CommandPortDisable || d1 == OMX_CommandPortEnable)) cmd_done_port = (int)d2;
    /* a crop-only change needs no new buffers */
    else if (e == OMX_EventPortSettingsChanged && d2 != OMX_IndexConfigCommonOutputCrop) port_changed = 1;
    else if (e == OMX_EventError) errors++;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mu);
    if (e != OMX_EventCmdComplete) LOG("event type=%d data1=0x%08x data2=0x%08x", (int)e, (unsigned)d1, (unsigned)d2);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE on_empty(OMX_HANDLETYPE h, OMX_PTR app, OMX_BUFFERHEADERTYPE *b)
{
    int i;
    (void)h; (void)app;
    pthread_mutex_lock(&mu);
    for (i = 0; i < nin; i++) if (inb[i] == b) in_free[i] = 1;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mu);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE on_fill(OMX_HANDLETYPE h, OMX_PTR app, OMX_BUFFERHEADERTYPE *b)
{
    OMX_BUFFERHEADERTYPE *ret = NULL;
    (void)app;
    pthread_mutex_lock(&mu);
    if (reconf || closing) ret = NULL;                       /* port going down: the component keeps nothing */
    else if (b->nFilledLen > 0) { frames++; ret = held; held = b; }
    else ret = b;
    pthread_mutex_unlock(&mu);
    if (ret) OMX_FillThisBuffer(h, ret);
    return OMX_ErrorNone;
}

static int wait_for(int *var, int want, int ms)
{
    struct timespec ts;
    int rc = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000; ts.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&mu);
    while (*var != want && rc != ETIMEDOUT) rc = pthread_cond_timedwait(&cv, &mu, &ts);
    rc = *var == want;
    pthread_mutex_unlock(&mu);
    return rc;
}

static int read_port(int port, OMX_PARAM_PORTDEFINITIONTYPE *d)
{
    INIT(*d); d->nPortIndex = (OMX_U32)port;
    if (OMX_GetParameter(comp, OMX_IndexParamPortDefinition, d) != OMX_ErrorNone) return -1;
    LOG("port=%d count=%u size=%u dims=%ux%u stride=%d slice=%u color=0x%08x", port, (unsigned)d->nBufferCountActual,
        (unsigned)d->nBufferSize, (unsigned)d->format.video.nFrameWidth, (unsigned)d->format.video.nFrameHeight,
        (int)d->format.video.nStride, (unsigned)d->format.video.nSliceHeight, (unsigned)d->format.video.eColorFormat);
    if (port == 1) {
        out_w = (int)d->format.video.nFrameWidth; out_h = (int)d->format.video.nFrameHeight;
        out_stride = (int)d->format.video.nStride; out_slice = (int)d->format.video.nSliceHeight;
        out_color = (unsigned)d->format.video.eColorFormat;
        if (out_stride < out_w) out_stride = out_w;
        if (out_slice < out_h) out_slice = out_h;
    }
    return 0;
}

static int alloc_port(int port, OMX_BUFFERHEADERTYPE **arr, int *n)
{
    OMX_PARAM_PORTDEFINITIONTYPE d;
    int i;
    if (read_port(port, &d)) return -1;
    *n = (int)d.nBufferCountActual > MAXBUF ? MAXBUF : (int)d.nBufferCountActual;
    for (i = 0; i < *n; i++) {
        OMX_ERRORTYPE r = OMX_AllocateBuffer(comp, &arr[i], (OMX_U32)port, NULL, d.nBufferSize);
        if (r != OMX_ErrorNone) { LOG("AllocateBuffer port=%d index=%d result=0x%08x", port, i, (unsigned)r); *n = i; return -1; }
        if (port == 0) in_free[i] = 1;
    }
    return 0;
}

/* After OMX_EventPortSettingsChanged: new output buffers for the real stream size (feed thread). */
static int reconfigure_output(void)
{
    int i, rc = 0;
    LOG("reconfiguring output port");
    pthread_mutex_lock(&use_mu);
    pthread_mutex_lock(&mu); reconf = 1; held = NULL; port_changed = 0; cmd_done_port = -1; pthread_mutex_unlock(&mu);
    OMX_SendCommand(comp, OMX_CommandPortDisable, 1, NULL);
    for (i = 0; i < nout; i++) OMX_FreeBuffer(comp, 1, outb[i]);
    nout = 0;
    if (!wait_for(&cmd_done_port, 1, 3000)) LOG("port disable timeout");
    pthread_mutex_lock(&mu); cmd_done_port = -1; pthread_mutex_unlock(&mu);
    OMX_SendCommand(comp, OMX_CommandPortEnable, 1, NULL);
    if (alloc_port(1, outb, &nout)) rc = -1;
    if (!wait_for(&cmd_done_port, 1, 3000)) LOG("port enable timeout");
    pthread_mutex_lock(&mu); reconf = 0; pthread_mutex_unlock(&mu);
    for (i = 0; i < nout; i++) OMX_FillThisBuffer(comp, outb[i]);
    pthread_mutex_unlock(&use_mu);
    return rc;
}

int omxdec_open(unsigned width, unsigned height)
{
    static const char *dirs[] = { "/mnt/app/armle/lib/", "/eso/lib/", "/mnt/app/armle/usr/lib/", "/usr/lib/", "" };
    static const char *deps[] = { "liblibstd.so", "libOmxBase.so", "libOmxCore.so" };
    static void *core;
    static init_fn oinit;
    static gethandle_fn oget;
    static OMX_CALLBACKTYPE cb = { on_event, on_empty, on_fill };
    OMX_PARAM_PORTDEFINITIONTYPE d;
    OMX_ERRORTYPE r;
    int i, k, di;

    if (!core) {
        for (k = 0; k < 3; k++) {                   /* dependencies first, by full path */
            void *lib = NULL;
            for (di = 0; di < 5 && !lib; di++) {
                char p[256];
                snprintf(p, sizeof(p), "%s%s", dirs[di], deps[k]);
                lib = dlopen(p, RTLD_NOW | RTLD_GLOBAL);
            }
            if (!lib) { LOG("dlopen %s failed: %s", deps[k], dlerror()); if (k == 2) return -1; }
            if (k == 2) core = lib;
        }
        oinit = (init_fn)dlsym(core, "OMX_Init");
        oget = (gethandle_fn)dlsym(core, "OMX_GetHandle");
        ofree = (freehandle_fn)dlsym(core, "OMX_FreeHandle");
        if (!oinit || !oget || !ofree) { LOG("missing OMX symbols"); core = NULL; return -1; }
        if ((r = oinit()) != OMX_ErrorNone) { LOG("OMX_Init=0x%08x", (unsigned)r); core = NULL; return -1; }
    }
    pthread_mutex_lock(&mu);
    state_reached = -1; cmd_done_port = -1; port_changed = 0; errors = 0; reconf = 0; closing = 0;
    frames = 0; fed = 0; held = NULL; nin = nout = 0;
    pthread_mutex_unlock(&mu);
    comp = NULL;
    r = oget(&comp, (OMX_STRING)"OMX.qcom.video.decoder.avc", NULL, &cb);
    if (r != OMX_ErrorNone || !comp) { LOG("OMX_GetHandle=0x%08x", (unsigned)r); comp = NULL; return -1; }
    for (i = 0; i < 2; i++) {
        INIT(d); d.nPortIndex = (OMX_U32)i;
        if (OMX_GetParameter(comp, OMX_IndexParamPortDefinition, &d) != OMX_ErrorNone) continue;
        d.format.video.nFrameWidth = width; d.format.video.nFrameHeight = height;
        if (i == 0) d.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
        else { d.format.video.nStride = (OMX_S32)width; d.format.video.nSliceHeight = height; }
        OMX_SetParameter(comp, OMX_IndexParamPortDefinition, &d);
    }
    OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (alloc_port(0, inb, &nin) || alloc_port(1, outb, &nout)) goto fail;
    if (!wait_for(&state_reached, OMX_StateIdle, 5000)) { LOG("Idle timeout"); goto fail; }
    OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (!wait_for(&state_reached, OMX_StateExecuting, 5000)) { LOG("Executing timeout"); goto fail; }
    for (i = 0; i < nout; i++) OMX_FillThisBuffer(comp, outb[i]);
    opened = 1;
    LOG("decoder running %ux%u in=%dx%u out=%d", width, height, nin, (unsigned)inb[0]->nAllocLen, nout);
    return 0;
fail:
    opened = 1;
    omxdec_close();
    return -1;
}

int omxdec_feed(const unsigned char *data, unsigned size)
{
    uint64_t end = now_ms() + 1000u;
    int i, k;
    if (!comp) return -1;
    for (;;) {
        if (port_changed && reconfigure_output()) { errors++; return -1; }
        pthread_mutex_lock(&mu);
        for (i = 0, k = -1; i < nin; i++) if (in_free[i]) { in_free[i] = 0; k = i; break; }
        pthread_mutex_unlock(&mu);
        if (k >= 0) break;
        if (errors) return -1;
        if (now_ms() > end) { LOG("input buffer not returned in 1 s (fed=%u frames=%u)", fed, frames); return -1; }
        usleep(1000);
    }
    if (size > inb[k]->nAllocLen) {                 /* cannot happen at 1080p (3 MB buffers); keep the decoder */
        LOG("packet %u bytes > input buffer %u, dropped", size, (unsigned)inb[k]->nAllocLen);
        pthread_mutex_lock(&mu); in_free[k] = 1; pthread_mutex_unlock(&mu);
        return 0;
    }
    memcpy(inb[k]->pBuffer, data, size);
    inb[k]->nOffset = 0; inb[k]->nFilledLen = size;
    inb[k]->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
    inb[k]->nTimeStamp = (OMX_TICKS)fed * 33333;
    if (OMX_EmptyThisBuffer(comp, inb[k]) != OMX_ErrorNone) {
        LOG("EmptyThisBuffer failed at packet %u", fed);
        pthread_mutex_lock(&mu); in_free[k] = 1; pthread_mutex_unlock(&mu);
        errors++;
        return -1;
    }
    fed++;
    return errors ? -1 : 0;
}

/* one de-tile job, split by rows across the cores (run 137: the fetch was ~14 ms on one core) */
struct dt_job { const unsigned char *src; size_t len; int x0, x1; struct AVFrame *f; };
static void dt_rows(void *arg, int ya, int yb)
{
    const struct dt_job *j = (const struct dt_job *)arg;
    dt_detile(j->src, j->len, out_w, out_h, ya, yb, j->x0, j->x1, j->f->data[0], j->f->linesize[0],
              j->f->data[1], j->f->linesize[1], j->f->data[2], j->f->linesize[2]);
}

int omxdec_take(struct AVFrame *f, int y0, int y1, int x0, int x1, omx_split_fn split)
{
    OMX_BUFFERHEADERTYPE *b;
    int ok = 0;
    pthread_mutex_lock(&use_mu);
    pthread_mutex_lock(&mu); b = held; held = NULL; pthread_mutex_unlock(&mu);
    if (!b) { pthread_mutex_unlock(&use_mu); return 0; }
    if (f->format != AV_PIX_FMT_YUV420P || f->width != out_w || f->height != out_h || !f->data[0]) {
        av_frame_unref(f);
        f->format = AV_PIX_FMT_YUV420P; f->width = out_w; f->height = out_h;
        if (av_frame_get_buffer(f, 32) < 0) { av_frame_unref(f); f->format = -1; }
    }
    if (f->data[0]) {
        const unsigned char *src = b->pBuffer + b->nOffset;
        size_t len = b->nAllocLen - b->nOffset;
        /* the phone stream is BT.709 limited range (decoded.first space=1 range=1) */
        f->color_range = AVCOL_RANGE_MPEG; f->colorspace = AVCOL_SPC_BT709;
        if (out_color == QCOM_TILED) {
            struct dt_job j;
            j.src = src; j.len = len; j.x0 = x0; j.x1 = x1; j.f = f;
            if (y0 < 0) y0 = 0;
            if (y1 > out_h) y1 = out_h;
            if (split) split(dt_rows, &j, y0 & ~1, y1, DT_TH);   /* halves meet on a tile-row boundary */
            else dt_rows(&j, y0 & ~1, y1);
        } else {                                    /* linear NV12 (not seen on this unit, kept for safety) */
            int y, x;
            const unsigned char *uv = src + (size_t)out_stride * (size_t)out_slice;
            for (y = 0; y < out_h; y++) memcpy(f->data[0] + y * f->linesize[0], src + (size_t)y * out_stride, (size_t)out_w);
            for (y = 0; y < out_h / 2; y++)
                for (x = 0; x < out_w / 2; x++) {
                    f->data[1][y * f->linesize[1] + x] = uv[(size_t)y * out_stride + 2 * x];
                    f->data[2][y * f->linesize[2] + x] = uv[(size_t)y * out_stride + 2 * x + 1];
                }
        }
        ok = 1;
    }
    pthread_mutex_lock(&mu);
    if (!reconf && !closing) { pthread_mutex_unlock(&mu); OMX_FillThisBuffer(comp, b); }
    else pthread_mutex_unlock(&mu);
    pthread_mutex_unlock(&use_mu);
    return ok;
}

int omxdec_frames(void) { return (int)frames; }
int omxdec_failed(void) { return errors != 0; }

void omxdec_close(void)
{
    int i;
    if (!opened || !comp) { comp = NULL; return; }
    pthread_mutex_lock(&use_mu);
    pthread_mutex_lock(&mu); closing = 1; held = NULL; state_reached = -1; pthread_mutex_unlock(&mu);
    OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateIdle, NULL);
    wait_for(&state_reached, OMX_StateIdle, 3000);
    pthread_mutex_lock(&mu); state_reached = -1; pthread_mutex_unlock(&mu);
    OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    for (i = 0; i < nin; i++) OMX_FreeBuffer(comp, 0, inb[i]);
    for (i = 0; i < nout; i++) OMX_FreeBuffer(comp, 1, outb[i]);
    nin = nout = 0;
    wait_for(&state_reached, OMX_StateLoaded, 3000);
    LOG("closed frames=%u fed=%u errors=%d free=0x%08x", frames, fed, errors, (unsigned)ofree(comp));
    comp = NULL; opened = 0;
    pthread_mutex_unlock(&use_mu);
}
