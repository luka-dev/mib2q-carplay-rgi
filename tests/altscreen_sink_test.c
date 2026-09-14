/* Run the actual QNX sink intake against an OMX component with deterministic
 * EmptyThisBuffer results. No decoder/GPU emulation or hardware is required. */
#include <assert.h>
#include "../altscreen_render/sink_omx_qnx.c"

static OMX_COMPONENTTYPE component;
static OMX_BUFFERHEADERTYPE input;
static OMX_BUFFERHEADERTYPE *inputs[] = { &input };
static OMX_U32 free_input[1];
static uint8_t storage[256], submitted[256];
static unsigned submissions, submitted_len, submitted_flags, init_attempts;
static OMX_ERRORTYPE submit_error;
static unsigned presents, recycled;
static OMX_TICKS submitted_time;

OMX_ERRORTYPE OMX_Init(void) { ++init_attempts; return OMX_ErrorInsufficientResources; }
OMX_ERRORTYPE OMX_Deinit(void) { return OMX_ErrorNone; }
OMX_ERRORTYPE OMX_GetHandle(OMX_HANDLETYPE *h, OMX_STRING n, OMX_PTR p, OMX_CALLBACKTYPE *c)
{ (void)h; (void)n; (void)p; (void)c; assert(0); return OMX_ErrorUndefined; }
OMX_ERRORTYPE OMX_FreeHandle(OMX_HANDLETYPE h)
{ (void)h; assert(0); return OMX_ErrorUndefined; }

screen_egl_present_t *screen_egl_present_create_output(int w, int h, volatile int *m, volatile int *s)
{ (void)w; (void)h; (void)m; (void)s; assert(0); return NULL; }
int screen_egl_present_attach_sources(screen_egl_present_t *p, unsigned int n)
{ (void)p; (void)n; assert(0); return -1; }
int screen_egl_present_start_loader(screen_egl_present_t *p)
{ (void)p; assert(0); return -1; }
void screen_egl_present_stop_loader(screen_egl_present_t *p) { (void)p; assert(0); }
void *screen_egl_present_source_pointer(screen_egl_present_t *p, unsigned int n)
{ (void)p; (void)n; assert(0); return NULL; }
unsigned int screen_egl_present_source_capacity(screen_egl_present_t *p, unsigned int n)
{ (void)p; (void)n; assert(0); return 0; }
int screen_egl_present_frame(screen_egl_present_t *p, unsigned int n, unsigned int len)
{ (void)p; assert(n == 0 && len > 0); ++presents; return 0; }
int screen_egl_present_recover_output(screen_egl_present_t *p)
{ (void)p; assert(0); return -1; }
void screen_egl_present_destroy(screen_egl_present_t *p) { (void)p; assert(0); }

static OMX_ERRORTYPE submit(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE *b) {
    if (submit_error) return submit_error;
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
    (void)handle; (void)b; ++recycled; return OMX_ErrorNone;
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

static void presentation_catchup(void) {
    OMX_BUFFERHEADERTYPE output = {0};
    OMX_BUFFERHEADERTYPE *outputs[] = {&output};
    uint64_t now = _NowMs();
    unsigned old_submissions = submissions;
    component.FillThisBuffer = recycle;
    g_ss.outBufs = outputs; g_ss.nOutBufs = 1; g_ss.winReady = 1;
    output.nFilledLen = 1024;
    g_ss.newestInputMs = now;
    output.nTimeStamp = (OMX_TICKS)((now - 2000) * 1000);
    _OnFillDone(&component, &g_ss, &output);
    assert(presents == 0 && recycled == 1 && g_ss.outputSkipped == 1);
    assert(!g_ss.omxError && !g_ss.presentRecoveryPending && !g_ss.presentationHealthy);
    /* Final/only picture survives even if startup delayed it by two seconds. */
    g_ss.newestInputMs = now - 2000;
    _OnFillDone(&component, &g_ss, &output);
    assert(presents == 1 && recycled == 2 && g_ss.presentationHealthy);
    g_ss.newestInputMs = now;
    _OnFillDone(&component, &g_ss, &output);
    assert(presents == 1 && recycled == 3 && g_ss.presentationHealthy);
    assert(!g_ss.presentRecoveryPending); /* skipping never means VIDEO_OFF */
    output.nTimeStamp = 0; /* component without timestamp propagation */
    _OnFillDone(&component, &g_ss, &output);
    assert(presents == 2 && recycled == 4);
    output.nTimeStamp = (OMX_TICKS)((now + 1000) * 1000);
    _OnFillDone(&component, &g_ss, &output);
    assert(presents == 3 && recycled == 5); /* invalid/future timestamp */
    assert(submissions == old_submissions); /* catch-up never restarts/feeds OMX */
    g_ss.outBufs = NULL; g_ss.nOutBufs = 0; g_ss.winReady = 0;
}

int main(void) {
    const uint8_t config[] = {0,0,0,1,0x67,0x11,0,0,1,0x68,0x22};
    const uint8_t next_config[] = {0,0,0,1,0x67,0x33,0,0,1,0x68,0x44};
    const uint8_t idr[] = {0,0,0,1,0x65,0x55};
    const uint8_t pframe[] = {0,0,0,1,0x41,0x66};
    uint8_t mixed[sizeof(next_config) + sizeof(idr)];
    unsigned before;
    pthread_mutex_init(&g_ss.lock, NULL);
    pthread_cond_init(&g_ss.cond, NULL);
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

    /* Mid-stream config must reach OMX even after fedConfig was already set. */
    before = submissions;
    assert(av_sink_frame(next_config, sizeof(next_config), 1000) == 0);
    assert(submissions == before + 1 && submitted_len == sizeof(next_config));
    assert(!memcmp(submitted, next_config, sizeof(next_config)));
    assert(submitted_flags & OMX_BUFFERFLAG_CODECCONFIG);
    assert(g_ss.needsIDR);

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
    presentation_catchup();

    /* State notifications without a returned buffer must not extend the input
     * deadline indefinitely. GPU ownership still has its separate watchdog. */
    g_ss.freeInHead = g_ss.freeInTail;
    uint64_t wait_begin = _NowMs();
    assert(!pthread_create(&returning_input, NULL, spurious_wakes, NULL));
    assert(av_sink_frame(pframe, sizeof(pframe), 1300) < 0 && g_ss.omxError);
    uint64_t waited = _NowMs() - wait_begin;
    assert(waited >= 900 && waited < 1800);
    pthread_join(returning_input, NULL);

    /* Failed starts report loss of continuity and preserve the bounded retry
     * budget across transport sessions that never produced a working decoder. */
    g_ss.started = g_ss.running = 0;
    g_ss.decoder = NULL;
    g_ss.inBufs = NULL; g_ss.freeIn = NULL; g_ss.nInBufs = 0;
    for (unsigned i = 0; i < 3; ++i) {
        assert(av_sink_frame(config, sizeof(config), 1000) < 0);
        av_sink_session_end();
    }
    assert(init_attempts == 3 && g_ss.startFails == 3);
    assert(av_sink_frame(config, sizeof(config), 1000) < 0 && init_attempts == 3);
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
    puts("altscreen_sink_test: config/IDR, rejection, cooldown, delayed EmptyDone, timestamp catch-up and final/static frame PASS");
    return 0;
}
