/* SPDX-License-Identifier: GPL-3.0-or-later
 * Hardware H.264 decode probe (owner 2026-09-30: "no wonder we were laggy, let's fix this").
 * The cluster player decodes the phone's cockpit stream with FFmpeg on the CPU; the unit has Qualcomm's
 * OpenMAX IL decoder (/mnt/app/armle/lib/libOmxCore.so, component OMX.qcom.video.decoder.avc - listed in the
 * core's registry on MU0918). This standalone probe decodes a recorded cockpit stream with it and reports
 * whether it works, how fast, and in which output format (stride / slice height / colour format), and saves
 * the first decoded frame raw. Nothing else on the unit is touched; run from live.sh when SD:/sq5_omx_probe.
 *   omx_probe <stream.h264> <outdir> [width height]
 * Headers: Khronos OpenMAX IL 1.1.2 (vendor/openmax, MIT-style licence).
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
#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_Video.h"

#define MAXBUF 32
#define LOG(...) do { printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

typedef OMX_ERRORTYPE (*init_fn)(void);
typedef OMX_ERRORTYPE (*gethandle_fn)(OMX_HANDLETYPE *, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE *);
typedef OMX_ERRORTYPE (*freehandle_fn)(OMX_HANDLETYPE);

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static OMX_HANDLETYPE comp;
static OMX_BUFFERHEADERTYPE *inb[MAXBUF], *outb[MAXBUF];
static int nin, nout, in_free[MAXBUF];
static int state_reached = -1, cmd_done_port = -1, port_changed, eos_seen, errors;
static unsigned frames, saved;
static uint64_t first_frame_ms, last_frame_ms;
static const char *outdir;
static unsigned save_at = 300;

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

#define INIT(x) do { memset(&(x), 0, sizeof(x)); (x).nSize = sizeof(x); (x).nVersion.s.nVersionMajor = 1; \
                     (x).nVersion.s.nVersionMinor = 1; (x).nVersion.s.nRevision = 2; } while (0)

static OMX_ERRORTYPE on_event(OMX_HANDLETYPE h, OMX_PTR app, OMX_EVENTTYPE e, OMX_U32 d1, OMX_U32 d2, OMX_PTR data)
{
    (void)h; (void)app; (void)data;
    pthread_mutex_lock(&mu);
    if (e == OMX_EventCmdComplete && d1 == OMX_CommandStateSet) state_reached = (int)d2;
    else if (e == OMX_EventCmdComplete && (d1 == OMX_CommandPortDisable || d1 == OMX_CommandPortEnable)) cmd_done_port = (int)d2;
    else if (e == OMX_EventPortSettingsChanged) port_changed = 1;
    else if (e == OMX_EventBufferFlag) eos_seen = 1;
    else if (e == OMX_EventError) errors++;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mu);
    LOG("event type=%d data1=0x%08x data2=0x%08x", (int)e, (unsigned)d1, (unsigned)d2);
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
    (void)app;
    pthread_mutex_lock(&mu);
    if (b->nFilledLen > 0) {
        uint64_t t = now_ms();
        if (!frames) first_frame_ms = t;
        last_frame_ms = t;
        frames++;
        if (!saved && outdir && frames == save_at) {   /* run 112: frame 1 is the black pre-map frame -> save a later one */
            char p[512];
            FILE *f;
            snprintf(p, sizeof(p), "%s/omx_frame.raw", outdir);
            if ((f = fopen(p, "wb")) != NULL) {
                fwrite(b->pBuffer + b->nOffset, 1, b->nFilledLen, f);
                fclose(f);
            }
            saved = 1;
            LOG("first frame bytes=%u offset=%u flags=0x%08x saved=%s", (unsigned)b->nFilledLen, (unsigned)b->nOffset,
                (unsigned)b->nFlags, p);
        }
    }
    if (b->nFlags & OMX_BUFFERFLAG_EOS) eos_seen = 1;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mu);
    if (!port_changed && !eos_seen) OMX_FillThisBuffer(h, b);   /* keep the output port busy */
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

static void log_port(int port, OMX_PARAM_PORTDEFINITIONTYPE *d)
{
    LOG("port=%d dir=%d count=%u/%u size=%u enabled=%d dims=%ux%u stride=%d slice=%u color=0x%08x compression=%d",
        port, (int)d->eDir, (unsigned)d->nBufferCountActual, (unsigned)d->nBufferCountMin, (unsigned)d->nBufferSize,
        (int)d->bEnabled, (unsigned)d->format.video.nFrameWidth, (unsigned)d->format.video.nFrameHeight,
        (int)d->format.video.nStride, (unsigned)d->format.video.nSliceHeight, (unsigned)d->format.video.eColorFormat,
        (int)d->format.video.eCompressionFormat);
}

static int alloc_port(int port, OMX_BUFFERHEADERTYPE **arr, int *n)
{
    OMX_PARAM_PORTDEFINITIONTYPE d;
    int i;
    INIT(d); d.nPortIndex = (OMX_U32)port;
    if (OMX_GetParameter(comp, OMX_IndexParamPortDefinition, &d) != OMX_ErrorNone) return -1;
    log_port(port, &d);
    *n = (int)d.nBufferCountActual > MAXBUF ? MAXBUF : (int)d.nBufferCountActual;
    for (i = 0; i < *n; i++) {
        OMX_ERRORTYPE r = OMX_AllocateBuffer(comp, &arr[i], (OMX_U32)port, NULL, d.nBufferSize);
        if (r != OMX_ErrorNone) { LOG("AllocateBuffer port=%d index=%d result=0x%08x", port, i, (unsigned)r); return -1; }
        if (port == 0) in_free[i] = 1;
    }
    LOG("allocated port=%d count=%d size=%u", port, *n, (unsigned)d.nBufferSize);
    return 0;
}

/* Output port reconfiguration after OMX_EventPortSettingsChanged (the real stream size/stride). */
static int reconfigure_output(void)
{
    int i;
    LOG("reconfiguring output port");
    cmd_done_port = -1;
    OMX_SendCommand(comp, OMX_CommandPortDisable, 1, NULL);
    for (i = 0; i < nout; i++) OMX_FreeBuffer(comp, 1, outb[i]);
    if (!wait_for(&cmd_done_port, 1, 3000)) LOG("port disable timeout");
    pthread_mutex_lock(&mu); port_changed = 0; pthread_mutex_unlock(&mu);
    cmd_done_port = -1;
    OMX_SendCommand(comp, OMX_CommandPortEnable, 1, NULL);
    if (alloc_port(1, outb, &nout)) return -1;
    if (!wait_for(&cmd_done_port, 1, 3000)) LOG("port enable timeout");
    for (i = 0; i < nout; i++) OMX_FillThisBuffer(comp, outb[i]);
    return 0;
}

/* Next NAL unit (start code included) in an Annex-B buffer: returns its length, sets *type. */
static size_t next_nal(const unsigned char *p, size_t n, size_t at, int *type)
{
    size_t i = at + 3, sc = at;
    if (at + 4 > n) return 0;
    if (!(p[at] == 0 && p[at + 1] == 0 && (p[at + 2] == 1 || (p[at + 2] == 0 && p[at + 3] == 1)))) return 0;
    *type = p[at + 2] == 1 ? p[at + 3] & 31 : p[at + 4] & 31;
    for (; i + 3 <= n; i++)
        if (p[i] == 0 && p[i + 1] == 0 && (p[i + 2] == 1 || (i + 3 < n && p[i + 2] == 0 && p[i + 3] == 1))) break;
    if (i + 3 > n) i = n;
    return i - sc;
}

static int free_input(int ms)
{
    int i, k;
    uint64_t end = now_ms() + (uint64_t)ms;
    for (;;) {
        pthread_mutex_lock(&mu);
        for (i = 0, k = -1; i < nin; i++) if (in_free[i]) { in_free[i] = 0; k = i; break; }
        pthread_mutex_unlock(&mu);
        if (k >= 0) return k;
        if (port_changed) reconfigure_output();
        if (now_ms() > end) return -1;
        usleep(2000);
    }
}

int main(int argc, char **argv)
{
    static const char *dirs[] = { "/mnt/app/armle/lib/", "/eso/lib/", "/mnt/app/armle/usr/lib/", "/usr/lib/", "" };
    static const char *deps[] = { "liblibstd.so", "libOmxBase.so", "libOmxCore.so" };
    OMX_CALLBACKTYPE cb = { on_event, on_empty, on_fill };
    OMX_PARAM_PORTDEFINITIONTYPE d;
    init_fn oinit; gethandle_fn oget; freehandle_fn ofree;
    void *core = NULL;
    unsigned char *s;
    size_t n, at, au_start;
    long sz;
    unsigned w = 1920, h = 1080, aus = 0, nals = 0;
    uint64_t t0;
    FILE *f;
    int i, k, di;
    OMX_ERRORTYPE r;

    if (argc < 3) { fprintf(stderr, "usage: %s stream.h264 outdir [w h]\n", argv[0]); return 2; }
    outdir = argv[2];
    if (argc >= 5) { w = (unsigned)atoi(argv[3]); h = (unsigned)atoi(argv[4]); }
    if (argc >= 6) save_at = (unsigned)atoi(argv[5]);
    if (!(f = fopen(argv[1], "rb"))) { LOG("open %s: %s", argv[1], strerror(errno)); return 3; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    s = malloc((size_t)sz); n = s ? fread(s, 1, (size_t)sz, f) : 0; fclose(f);
    LOG("stream %s bytes=%u expected=%ux%u", argv[1], (unsigned)n, w, h);
    if (!n) return 3;

    for (k = 0; k < 3; k++) {                       /* dependencies first, by full path (LD_LIBRARY_PATH unknown) */
        void *lib = NULL;
        for (di = 0; di < 5 && !lib; di++) {
            char p[256];
            snprintf(p, sizeof(p), "%s%s", dirs[di], deps[k]);
            lib = dlopen(p, RTLD_NOW | RTLD_GLOBAL);
            if (lib) LOG("loaded %s", p);
        }
        if (!lib) LOG("dlopen %s failed: %s", deps[k], dlerror());
        if (k == 2) core = lib;
    }
    if (!core) return 4;
    oinit = (init_fn)dlsym(core, "OMX_Init");
    oget = (gethandle_fn)dlsym(core, "OMX_GetHandle");
    ofree = (freehandle_fn)dlsym(core, "OMX_FreeHandle");
    if (!oinit || !oget || !ofree) { LOG("missing OMX symbols"); return 4; }
    r = oinit(); LOG("OMX_Init=0x%08x", (unsigned)r);
    r = oget(&comp, (OMX_STRING)"OMX.qcom.video.decoder.avc", NULL, &cb);
    LOG("OMX_GetHandle=0x%08x component=%p", (unsigned)r, comp);
    if (r != OMX_ErrorNone || !comp) return 5;

    for (i = 0; i < 2; i++) {
        INIT(d); d.nPortIndex = (OMX_U32)i;
        r = OMX_GetParameter(comp, OMX_IndexParamPortDefinition, &d);
        LOG("GetParameter port=%d result=0x%08x", i, (unsigned)r);
        log_port(i, &d);
        d.format.video.nFrameWidth = w; d.format.video.nFrameHeight = h;
        if (i == 0) d.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
        else { d.format.video.nStride = (OMX_S32)w; d.format.video.nSliceHeight = h; }
        r = OMX_SetParameter(comp, OMX_IndexParamPortDefinition, &d);
        LOG("SetParameter port=%d result=0x%08x", i, (unsigned)r);
    }
    for (i = 0; i < 16; i++) {                      /* supported output formats */
        OMX_VIDEO_PARAM_PORTFORMATTYPE pf;
        INIT(pf); pf.nPortIndex = 1; pf.nIndex = (OMX_U32)i;
        if (OMX_GetParameter(comp, OMX_IndexParamVideoPortFormat, &pf) != OMX_ErrorNone) break;
        LOG("output format index=%d color=0x%08x compression=%d", i, (unsigned)pf.eColorFormat, (int)pf.eCompressionFormat);
    }

    r = OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateIdle, NULL);
    LOG("SendCommand Idle=0x%08x", (unsigned)r);
    if (alloc_port(0, inb, &nin) || alloc_port(1, outb, &nout)) goto out;
    if (!wait_for(&state_reached, OMX_StateIdle, 5000)) { LOG("Idle timeout"); goto out; }
    r = OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    LOG("SendCommand Executing=0x%08x", (unsigned)r);
    if (!wait_for(&state_reached, OMX_StateExecuting, 5000)) { LOG("Executing timeout"); goto out; }
    for (i = 0; i < nout; i++) OMX_FillThisBuffer(comp, outb[i]);

    /* Feed whole access units: an AU starts at AUD/SPS or at a slice with first_mb_in_slice == 0. SPS+PPS
     * go first as codec config. */
    t0 = now_ms();
    at = 0; au_start = 0;
    while (at < n) {
        int type = 0, starts_au;
        size_t len = next_nal(s, n, at, &type);
        if (!len) { at++; continue; }
        nals++;
        {
            size_t hdr = s[at + 2] == 1 ? 4 : 5;
            starts_au = type == 9 || type == 7 || ((type == 1 || type == 5) && at + hdr < n && (s[at + hdr] & 0x80));
        }
        if (starts_au && at > au_start) {
            /* submit [au_start, at) */
            size_t al = at - au_start;
            if ((k = free_input(3000)) < 0) { LOG("input return timeout at AU %u", aus); break; }
            if (al > inb[k]->nAllocLen) { LOG("AU %u too large %u > %u", aus, (unsigned)al, (unsigned)inb[k]->nAllocLen); al = inb[k]->nAllocLen; }
            memcpy(inb[k]->pBuffer, s + au_start, al);
            inb[k]->nOffset = 0; inb[k]->nFilledLen = (OMX_U32)al;
            inb[k]->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;   /* the first AU carries SPS+PPS+IDR together */
            inb[k]->nTimeStamp = (OMX_TICKS)aus * 33333;
            r = OMX_EmptyThisBuffer(comp, inb[k]);
            if (r != OMX_ErrorNone) { LOG("EmptyThisBuffer AU=%u result=0x%08x", aus, (unsigned)r); break; }
            aus++;
            au_start = at;
            if (now_ms() - t0 > 25000) { LOG("25 s budget used"); break; }
        }
        at += len;
    }
    /* end of stream */
    if ((k = free_input(3000)) >= 0) {
        inb[k]->nFilledLen = 0; inb[k]->nFlags = OMX_BUFFERFLAG_EOS;
        OMX_EmptyThisBuffer(comp, inb[k]);
    }
    for (i = 0; i < 300 && !eos_seen; i++) { if (port_changed) reconfigure_output(); usleep(10000); }
    LOG("decode summary access_units=%u nals=%u frames=%u errors=%d eos=%d feed_ms=%u",
        aus, nals, frames, errors, eos_seen, (unsigned)(now_ms() - t0));
    if (frames > 1)
        LOG("decode rate %.1f fps over %u ms (frame 1 after %u ms)", (frames - 1) * 1000.0 / (double)(last_frame_ms - first_frame_ms + 1),
            (unsigned)(last_frame_ms - first_frame_ms), (unsigned)(first_frame_ms - t0));
    INIT(d); d.nPortIndex = 1;
    if (OMX_GetParameter(comp, OMX_IndexParamPortDefinition, &d) == OMX_ErrorNone) log_port(1, &d);
out:
    state_reached = -1;
    OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateIdle, NULL);
    wait_for(&state_reached, OMX_StateIdle, 3000);
    state_reached = -1;
    OMX_SendCommand(comp, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    for (i = 0; i < nin; i++) OMX_FreeBuffer(comp, 0, inb[i]);
    for (i = 0; i < nout; i++) OMX_FreeBuffer(comp, 1, outb[i]);
    wait_for(&state_reached, OMX_StateLoaded, 3000);
    LOG("OMX_FreeHandle=0x%08x", (unsigned)ofree(comp));
    LOG("external hardware decode success=%d", frames > 0);
    return frames > 0 ? 0 : 1;
}
