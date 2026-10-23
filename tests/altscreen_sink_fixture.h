/* Run the actual QNX sink intake against an OMX component with deterministic
 * EmptyThisBuffer results. No decoder/GPU emulation or hardware is required. */
#include <assert.h>
#include <sys/wait.h>
#include "../altscreen_render/sink_omx_qnx.c"

static OMX_COMPONENTTYPE component;
static OMX_BUFFERHEADERTYPE input;
static OMX_BUFFERHEADERTYPE *inputs[] = {&input};
static OMX_U32 free_input[1];
static uint8_t storage[256], submitted[256];
static unsigned submissions, submitted_len, submitted_flags, init_attempts;
static OMX_ERRORTYPE submit_error;
static unsigned presents, recycled;
static OMX_TICKS submitted_time;
static pthread_mutex_t gpu_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gpu_cond = PTHREAD_COND_INITIALIZER;
static int gpu_block, gpu_entered, gpu_error, recycle_error;
static unsigned slot_order[8], slot_order_n;
static pthread_t callback_thread;
static void (*recycle_observer)(OMX_BUFFERHEADERTYPE *);
static void (*present_observer)(unsigned);
static unsigned last_present_slot;

OMX_ERRORTYPE OMX_Init(void) {
    ++init_attempts;
    return OMX_ErrorInsufficientResources;
}
OMX_ERRORTYPE OMX_Deinit(void) { return OMX_ErrorNone; }
OMX_ERRORTYPE OMX_GetHandle(OMX_HANDLETYPE *h, OMX_STRING n, OMX_PTR p, OMX_CALLBACKTYPE *c) {
    (void)h;
    (void)n;
    (void)p;
    (void)c;
    assert(0);
    return OMX_ErrorUndefined;
}
OMX_ERRORTYPE OMX_FreeHandle(OMX_HANDLETYPE h) {
    (void)h;
    assert(0);
    return OMX_ErrorUndefined;
}

/* ---- wedged-core teardown harness ---------------------------------------- */
static volatile int block_send_command;
static unsigned teardown_freebufs;
static OMX_ERRORTYPE stub_send(OMX_HANDLETYPE h, OMX_COMMANDTYPE c, OMX_U32 p, OMX_PTR ed) {
    (void)h; (void)c; (void)p; (void)ed;
    while (block_send_command) {
        struct timespec pause = { 0, 50L * 1000000L };
        nanosleep(&pause, NULL);
    }
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE stub_freebuf(OMX_HANDLETYPE h, OMX_U32 port, OMX_BUFFERHEADERTYPE *b) {
    (void)h; (void)port; (void)b;
    ++teardown_freebufs;
    return OMX_ErrorNone;
}

screen_egl_present_t *screen_egl_present_create_output(int w, int h, volatile int *m,
                                                       volatile int *s) {
    (void)w;
    (void)h;
    (void)m;
    (void)s;
    assert(0);
    return NULL;
}
int screen_egl_present_attach_sources(screen_egl_present_t *p, unsigned int n) {
    (void)p;
    (void)n;
    assert(0);
    return -1;
}
int screen_egl_present_start_loader(screen_egl_present_t *p) {
    (void)p;
    assert(0);
    return -1;
}
void screen_egl_present_stop_loader(screen_egl_present_t *p) {
    (void)p;
    assert(0);
}
void screen_egl_present_set_stream_pending(screen_egl_present_t *p, int pending) {
    (void)p;
    (void)pending;
}
void *screen_egl_present_source_pointer(screen_egl_present_t *p, unsigned int n) {
    (void)p;
    (void)n;
    assert(0);
    return NULL;
}
unsigned int screen_egl_present_source_capacity(screen_egl_present_t *p, unsigned int n) {
    (void)p;
    (void)n;
    assert(0);
    return 0;
}
int screen_egl_present_frame(screen_egl_present_t *p, unsigned int n, unsigned int len) {
    last_present_slot = n;
    (void)p;
    assert(len > 0);
    assert(!pthread_equal(pthread_self(), callback_thread));
    pthread_mutex_lock(&gpu_lock);
    gpu_entered = 1;
    pthread_cond_broadcast(&gpu_cond);
    while (gpu_block)
        pthread_cond_wait(&gpu_cond, &gpu_lock);
    ++presents;
    if (slot_order_n < 8)
        slot_order[slot_order_n++] = n;
    int err = gpu_error;
    pthread_mutex_unlock(&gpu_lock);
    if (present_observer)
        present_observer(n);
    return err;
}
int screen_egl_present_recover_output(screen_egl_present_t *p) {
    (void)p;
    assert(0);
    return -1;
}
void screen_egl_present_destroy(screen_egl_present_t *p) {
    (void)p;
    assert(0);
}
void screen_egl_present_reset_timing(screen_egl_present_t *p) { (void)p; }

static OMX_ERRORTYPE submit(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE *b) {
    if (submit_error)
        return submit_error;
    assert(b->nFilledLen <= sizeof(submitted));
    memcpy(submitted, b->pBuffer, b->nFilledLen);
    submitted_len = b->nFilledLen;
    submitted_flags = b->nFlags;
    submitted_time = b->nTimeStamp;
    ++submissions;
    _OnEmptyDone(handle, &g_ss, b);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE recycle(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE *b) {
    (void)handle;
    (void)b;
    pthread_mutex_lock(&gpu_lock);
    assert(!gpu_block); /* buffer cannot return while its GPU read is pending */
    int saved_error = recycle_error;
    ++recycled;
    pthread_cond_broadcast(&gpu_cond);
    pthread_mutex_unlock(&gpu_lock);
    if (!saved_error && recycle_observer)
        recycle_observer(b);
    return saved_error ? OMX_ErrorHardware : OMX_ErrorNone;
}

static void *delayed_empty(void *unused) {
    struct timespec pause = {0, 600000000L};
    (void)unused;
    nanosleep(&pause, NULL);
    _OnEmptyDone(&component, &g_ss, &input);
    return NULL;
}

static void *spurious_wakes(void *unused) {
    (void)unused;
    for (unsigned i = 0; i < 120; ++i) {
        struct timespec pause = {0, 10000000L};
        nanosleep(&pause, NULL);
        pthread_mutex_lock(&g_ss.lock);
        pthread_cond_broadcast(&g_ss.cond);
        pthread_mutex_unlock(&g_ss.lock);
    }
    return NULL;
}

static void wait_recycled(unsigned count) {
    /* gpu_cond is a default (CLOCK_REALTIME) condition, while _AbsTimeoutMs uses the sink's
     * clock (CLOCK_MONOTONIC off macOS): build this deadline on the condition's own clock. */
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);
    end.tv_sec += 1;
    pthread_mutex_lock(&gpu_lock);
    while (recycled < count)
        assert(pthread_cond_timedwait(&gpu_cond, &gpu_lock, &end) != ETIMEDOUT);
    pthread_mutex_unlock(&gpu_lock);
}

static void present_and_wait(OMX_BUFFERHEADERTYPE *b) {
    unsigned target = recycled + 1;
    _OnFillDone(&component, &g_ss, b);
    wait_recycled(target);
}

static void presentation_latency(void) {
    OMX_BUFFERHEADERTYPE output = {0};
    OMX_BUFFERHEADERTYPE *outputs[] = {&output};
    uint64_t now = _NowMs();
    unsigned old_submissions = submissions;
    component.FillThisBuffer = recycle;
    g_ss.outBufs = outputs;
    g_ss.nOutBufs = 1;
    g_ss.winReady = 1;
    assert(_StartPresentThread(&g_ss) == 0);
    output.nFilledLen = 1024;
    g_ss.newestInputMs = now;
    output.nTimeStamp = (OMX_TICKS)((now - 2000) * 1000);
    present_and_wait(&output);
    assert(presents == 1 && recycled == 1);
    assert(!g_ss.omxError && !g_ss.presentRecoveryPending && g_ss.presentationHealthy);
    /* Final/only picture survives even if startup delayed it by two seconds. */
    g_ss.newestInputMs = now - 2000;
    present_and_wait(&output);
    assert(presents == 2 && recycled == 2 && g_ss.presentationHealthy);
    assert(av_sink_video_ready());
    /* The control poll must report OFF as soon as teardown starts, before
     * the last successful presentation is reset or any further input arrives. */
    g_ss.callbacksClosing = 1;
    assert(!av_sink_video_ready());
    g_ss.callbacksClosing = 0;
    assert(av_sink_video_ready());
    g_ss.newestInputMs = now;
    present_and_wait(&output);
    assert(presents == 3 && recycled == 3 && g_ss.presentationHealthy);
    assert(!g_ss.presentRecoveryPending);
    output.nTimeStamp = 0; /* component without timestamp propagation */
    present_and_wait(&output);
    assert(presents == 4 && recycled == 4);
    output.nTimeStamp = (OMX_TICKS)((now + 1000) * 1000);
    present_and_wait(&output);
    assert(presents == 5 && recycled == 5); /* invalid/future timestamp */
    /* Replay the HU's steady four-picture decoder delay at 20/25/30 fps.
     * Input continues, so newestInputMs always leads this decoded picture.
     * Latency crossing 200 ms must not discard most of a healthy stream. */
    for (unsigned rate = 20; rate <= 30; rate += 5) {
        unsigned baseline = presents;
        for (unsigned frame = 0; frame < 600; ++frame) {
            uint64_t sample_now = _NowMs();
            uint64_t decoder_delay = 4000 / rate;
            output.nTimeStamp = (OMX_TICKS)((sample_now - decoder_delay - 12 - frame % 5) * 1000);
            g_ss.newestInputMs = sample_now - 12;
            present_and_wait(&output);
        }
        assert(presents - baseline == 600);
    }
    assert(presents == recycled);
    assert(submissions == old_submissions); /* presentation never re-feeds OMX */
    _DrainCallbacksAndPresenter(&g_ss);
    g_ss.callbacksClosing = 0;
    g_ss.running = 1;
    g_ss.outBufs = NULL;
    g_ss.nOutBufs = 0;
    g_ss.winReady = 0;
}

static unsigned order_calls, order_fail_at, order_bad_readback;
static DecoderOrderParam vendor_order;
static OMX_PARAM_PORTDEFINITIONTYPE ports[2];
static int bad_fps_port = -1;
static OMX_ERRORTYPE extension(OMX_HANDLETYPE h, OMX_STRING name, OMX_INDEXTYPE *idx) {
    (void)h;
    assert(!strcmp(name, kQOMX_ExtIndex_DisplayPicBuf));
    if (++order_calls == order_fail_at)
        return OMX_ErrorUnsupportedIndex;
    *idx = (OMX_INDEXTYPE)0x7f100008;
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE get_param(OMX_HANDLETYPE h, OMX_INDEXTYPE idx, OMX_PTR data) {
    (void)h;
    if (idx == OMX_IndexParamPortDefinition) {
        OMX_PARAM_PORTDEFINITIONTYPE *pd = data;
        assert(pd->nPortIndex < 2);
        *pd = ports[pd->nPortIndex];
        if ((int)pd->nPortIndex == bad_fps_port)
            pd->format.video.xFramerate = 20u << 16;
        return OMX_ErrorNone;
    }
    DecoderOrderParam *p = data;
    assert((unsigned)idx == 0x7f100008 && p->nSize == 20 && p->nVersion.nVersion == 0x101);
    if (++order_calls == order_fail_at)
        return OMX_ErrorHardware;
    p->vendorWord = 77;
    p->enabled = vendor_order.enabled;
    p->mode = vendor_order.mode;
    if (order_bad_readback && order_calls == 4)
        p->enabled = 0;
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE set_param(OMX_HANDLETYPE h, OMX_INDEXTYPE idx, OMX_PTR data) {
    (void)h;
    if (idx == OMX_IndexParamPortDefinition) {
        OMX_PARAM_PORTDEFINITIONTYPE *pd = data;
        assert(pd->nPortIndex < 2 && pd->format.video.xFramerate == (30u << 16));
        ports[pd->nPortIndex] = *pd;
        return OMX_ErrorNone;
    }
    DecoderOrderParam *p = data;
    assert((unsigned)idx == 0x7f100008 && p->nSize == 20 && p->vendorWord == 77);
    assert(p->enabled == 1 && p->mode == 1);
    if (++order_calls == order_fail_at)
        return OMX_ErrorHardware;
    vendor_order = *p;
    return OMX_ErrorNone;
}
static void omx_contract(void) {
    component.GetExtensionIndex = extension;
    component.GetParameter = get_param;
    component.SetParameter = set_param; /* SetConfig deliberately remains NULL */
    for (unsigned failure = 0; failure <= 4; ++failure) {
        order_calls = 0;
        order_fail_at = failure;
        memset(&vendor_order, 0, sizeof(vendor_order));
        assert((_EnableDecoderOrderMode(&g_ss) == 0) == (failure == 0));
        assert(order_calls == (failure ? failure : 4));
    }
    order_calls = order_fail_at = 0;
    order_bad_readback = 1;
    assert(_EnableDecoderOrderMode(&g_ss) < 0);
    order_bad_readback = 0;
    ports[0].nPortIndex = 0;
    ports[1].nPortIndex = 1;
    assert(_ConfigurePorts(&g_ss) == 0);
    for (bad_fps_port = 0; bad_fps_port < 2; ++bad_fps_port)
        assert(_ConfigurePorts(&g_ss) < 0);
    bad_fps_port = -1;
}

static void presentation_queue(void) {
    OMX_BUFFERHEADERTYPE b[3] = {{0}};
    OMX_BUFFERHEADERTYPE *out[] = {b, b + 1, b + 2};
    unsigned base = recycled;
    g_ss.outBufs = out;
    g_ss.nOutBufs = 3;
    g_ss.winReady = g_ss.running = 1;
    g_ss.callbacksClosing = 0;
    gpu_block = 1;
    gpu_entered = 0;
    slot_order_n = 0;
    assert(_StartPresentThread(&g_ss) == 0);
    for (unsigned i = 0; i < 3; ++i) {
        b[i].nFilledLen = 1024;
        _OnFillDone(&component, &g_ss, b + i);
    }
    pthread_mutex_lock(&gpu_lock);
    while (!gpu_entered)
        pthread_cond_wait(&gpu_cond, &gpu_lock);
    assert(recycled == base); /* callbacks returned while GPU is blocked */
    gpu_block = 0;
    pthread_cond_broadcast(&gpu_cond);
    pthread_mutex_unlock(&gpu_lock);
    wait_recycled(base + 3);
    assert(slot_order_n == 3 && slot_order[0] == 0 && slot_order[1] == 1 && slot_order[2] == 2);
    /* Empty output is recycled without drawing. */
    unsigned drawn = presents;
    b[0].nFilledLen = 0;
    present_and_wait(b);
    assert(presents == drawn);
    b[0].nFilledLen = 1024;
    gpu_error = 1;
    for (unsigned i = 0; i < 3; ++i)
        present_and_wait(b);
    assert(g_ss.presentRecoveryPending && !g_ss.presentationHealthy);
    gpu_error = 0;
    present_and_wait(b);
    assert(!g_ss.presentRecoveryPending && g_ss.presentationHealthy);

    /* Stop while the first GPU read is in flight and two outputs are queued.
     * The stop worker must wait for GPU ownership; queued frames must not draw. */
    gpu_block = 1;
    gpu_entered = 0;
    slot_order_n = 0;
    for (unsigned i = 0; i < 3; ++i)
        _OnFillDone(&component, &g_ss, b + i);
    pthread_mutex_lock(&gpu_lock);
    while (!gpu_entered)
        pthread_cond_wait(&gpu_cond, &gpu_lock);
    pthread_mutex_unlock(&gpu_lock);
    pthread_mutex_lock(&g_ss.lock);
    g_ss.presentStop = 1; /* same transition as teardown, while GPU still owns b0 */
    pthread_mutex_unlock(&g_ss.lock);
    pthread_mutex_lock(&gpu_lock);
    base = recycled;
    gpu_block = 0;
    pthread_cond_broadcast(&gpu_cond);
    pthread_mutex_unlock(&gpu_lock);
    _DrainCallbacksAndPresenter(&g_ss);
    assert(slot_order_n == 1 && recycled == base && !g_ss.presentThreadStarted);
    _OnFillDone(&component, &g_ss, b); /* late callback after gate closed */
    assert(recycled == base);
    g_ss.outBufs = NULL;
    g_ss.nOutBufs = 0;
    g_ss.callbacksClosing = 0;
    g_ss.running = 1;
}

static void presentation_faults(void) {
    OMX_BUFFERHEADERTYPE b = {0}, alien = {0};
    OMX_BUFFERHEADERTYPE *out[] = {&b};
    g_ss.outBufs = out;
    g_ss.nOutBufs = 1;
    g_ss.winReady = g_ss.running = 1;
    g_ss.callbacksClosing = g_ss.omxError = 0;
    gpu_block = 1;
    gpu_entered = 0;
    b.nFilledLen = 1024;
    assert(_StartPresentThread(&g_ss) == 0);
    _OnFillDone(&component, &g_ss, &b);
    pthread_mutex_lock(&gpu_lock);
    while (!gpu_entered)
        pthread_cond_wait(&gpu_cond, &gpu_lock);
    pthread_mutex_unlock(&gpu_lock);
    _OnFillDone(&component, &g_ss, &b); /* duplicate cannot reuse live GPU slot */
    assert(g_ss.omxError);
    _OnFillDone(&component, &g_ss, &alien); /* unknown pointer cannot index the pool */
    pthread_mutex_lock(&gpu_lock);
    unsigned target = recycled + 1;
    gpu_block = 0;
    pthread_cond_broadcast(&gpu_cond);
    pthread_mutex_unlock(&gpu_lock);
    wait_recycled(target);
    pthread_mutex_lock(&g_ss.lock);
    g_ss.omxError = 0;
    pthread_mutex_unlock(&g_ss.lock);
    recycle_error = 1;
    present_and_wait(&b);
    pthread_mutex_lock(&g_ss.lock);
    while (!g_ss.omxError)
        pthread_cond_wait(&g_ss.cond, &g_ss.lock);
    pthread_mutex_unlock(&g_ss.lock);
    _DrainCallbacksAndPresenter(&g_ss);
    recycle_error = 0;
    g_ss.omxError = 0;
    /* A genuinely wedged GPU exits the isolated process without freeing live
     * storage. Test the actual timed drain in a disposable child process. */
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        gpu_block = 1;
        gpu_entered = 0;
        g_ss.winReady = g_ss.running = 1;
        g_ss.callbacksClosing = 0;
        assert(_StartPresentThread(&g_ss) == 0);
        _OnFillDone(&component, &g_ss, &b);
        pthread_mutex_lock(&gpu_lock);
        while (!gpu_entered)
            pthread_cond_wait(&gpu_cond, &gpu_lock);
        pthread_mutex_unlock(&gpu_lock);
        _DrainCallbacksAndPresenter(&g_ss);
        _exit(99); /* returning would free a buffer that GPU still owns */
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 86);
    g_ss.outBufs = NULL;
    g_ss.nOutBufs = 0;
    g_ss.callbacksClosing = 0;
    g_ss.running = 1;
}
