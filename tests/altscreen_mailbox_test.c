#include "altscreen_sink_fixture.h"
static unsigned refill_budget, second_present_budget, burst_start;
static void refill(OMX_BUFFERHEADERTYPE *b) {
    if (refill_budget) {
        --refill_budget;
        b->nFilledLen = 1024;
        _OnFillDone(&component, &g_ss, b);
    }
}
static void observed_present(unsigned slot) {
    (void)slot;
    if (presents == burst_start + 2)
        second_present_budget = refill_budget;
}
static void burst(int mailbox, int empty_last, unsigned refill_count) {
    OMX_BUFFERHEADERTYPE b[12] = {{0}}, *ptrs[12];
    unsigned base = recycled, before = presents;
    for (unsigned i = 0; i < 12; ++i) {
        b[i].nFilledLen = 1024;
        b[i].nTimeStamp = 1;
        ptrs[i] = b + i;
    }
    if (empty_last)
        b[11].nFilledLen = 0;
    g_ss.outBufs = ptrs;
    g_ss.nOutBufs = 12;
    g_ss.running = g_ss.winReady = g_ss.started = 1;
    g_ss.callbacksClosing = 0;
    g_ss.omxError = 0;
    g_ss.mailbox = mailbox;
    gpu_block = 1;
    gpu_entered = 0;
    refill_budget = refill_count;
    second_present_budget = 0;
    recycle_observer = refill_count ? refill : NULL;
    present_observer = observed_present;
    burst_start = before;
    assert(!_StartPresentThread(&g_ss));
    _OnFillDone(&component, &g_ss, b);
    pthread_mutex_lock(&gpu_lock);
    while (!gpu_entered)
        pthread_cond_wait(&gpu_cond, &gpu_lock);
    pthread_mutex_unlock(&gpu_lock);
    for (unsigned i = 1; i < 12; ++i)
        _OnFillDone(&component, &g_ss, b + i);
    pthread_mutex_lock(&gpu_lock);
    gpu_block = 0;
    pthread_cond_broadcast(&gpu_cond);
    pthread_mutex_unlock(&gpu_lock);
    wait_recycled(base + 12 + refill_count);
    _DrainCallbacksAndPresenter(&g_ss);
    assert(recycled == base + 12 + refill_count && !g_ss.omxError);
    if (!refill_count) {
        assert(presents - before == (mailbox ? 2u : empty_last ? 11u : 12u));
        assert(last_present_slot == (empty_last ? 10u : 11u));
    } else
        assert(second_present_budget > 0); /* bounded snapshot cannot starve behind refills */
    recycle_observer = NULL;
    present_observer = NULL;
}
static void fail_next_recycle(OMX_BUFFERHEADERTYPE *b) {
    (void)b;
    recycle_error = 1;
}
static void stop_and_fault_tests(void) {
    OMX_BUFFERHEADERTYPE b[3] = {{0}}, *p[3];
    unsigned before = presents, base = recycled;
    for (unsigned i = 0; i < 3; ++i) {
        b[i].nFilledLen = 1024;
        p[i] = b + i;
    }
    g_ss.outBufs = p;
    g_ss.nOutBufs = 3;
    g_ss.mailbox = 1;
    g_ss.running = g_ss.winReady = g_ss.started = 1;
    g_ss.callbacksClosing = g_ss.omxError = 0;
    gpu_block = 1;
    gpu_entered = 0;
    assert(!_StartPresentThread(&g_ss));
    _OnFillDone(&component, &g_ss, b);
    pthread_mutex_lock(&gpu_lock);
    while (!gpu_entered)
        pthread_cond_wait(&gpu_cond, &gpu_lock);
    pthread_mutex_unlock(&gpu_lock);
    _OnFillDone(&component, &g_ss, b + 1);
    _OnFillDone(&component, &g_ss, b + 2);
    pthread_mutex_lock(&g_ss.lock);
    g_ss.presentStop = 1;
    pthread_mutex_unlock(&g_ss.lock);
    pthread_mutex_lock(&gpu_lock);
    gpu_block = 0;
    pthread_cond_broadcast(&gpu_cond);
    pthread_mutex_unlock(&gpu_lock);
    _DrainCallbacksAndPresenter(&g_ss);
    assert(presents == before + 1 && recycled == base);
    /* Fail the first superseded-slot return, after the in-flight picture returns. */
    g_ss.running = g_ss.winReady = g_ss.started = 1;
    g_ss.callbacksClosing = g_ss.omxError = 0;
    gpu_block = 1;
    gpu_entered = 0;
    recycle_error = 0;
    recycle_observer = fail_next_recycle;
    assert(!_StartPresentThread(&g_ss));
    _OnFillDone(&component, &g_ss, b);
    pthread_mutex_lock(&gpu_lock);
    while (!gpu_entered)
        pthread_cond_wait(&gpu_cond, &gpu_lock);
    pthread_mutex_unlock(&gpu_lock);
    _OnFillDone(&component, &g_ss, b + 1);
    _OnFillDone(&component, &g_ss, b + 2);
    pthread_mutex_lock(&gpu_lock);
    gpu_block = 0;
    pthread_cond_broadcast(&gpu_cond);
    pthread_mutex_unlock(&gpu_lock);
    pthread_mutex_lock(&g_ss.lock);
    while (!g_ss.omxError)
        pthread_cond_wait(&g_ss.cond, &g_ss.lock);
    pthread_mutex_unlock(&g_ss.lock);
    _DrainCallbacksAndPresenter(&g_ss);
    assert(!g_ss.presentThreadStarted && g_ss.omxError);
    recycle_observer = NULL;
    recycle_error = 0;
    g_ss.omxError = 0;
}
static void flight_updates(int newer_picture) {
    OMX_BUFFERHEADERTYPE b[3]={{0}},*ptrs[3]={b,b+1,b+2};
    unsigned before=presents,base=recycled;
    b[0].nFilledLen=b[2].nFilledLen=1024;
    g_ss.outBufs=ptrs;g_ss.nOutBufs=3;
    g_ss.running=g_ss.winReady=g_ss.started=1;
    g_ss.callbacksClosing=g_ss.omxError=0;g_ss.mailbox=1;
    transition_until_present=before+5;
    gpu_block=1;gpu_entered=0;
    assert(!_StartPresentThread(&g_ss));
    _OnFillDone(&component,&g_ss,b);
    pthread_mutex_lock(&gpu_lock);
    while(!gpu_entered)pthread_cond_wait(&gpu_cond,&gpu_lock);
    pthread_mutex_unlock(&gpu_lock);
    _OnFillDone(&component,&g_ss,b+1); /* empty cannot replace held picture */
    if(newer_picture)_OnFillDone(&component,&g_ss,b+2);
    pthread_mutex_lock(&gpu_lock);gpu_block=0;pthread_cond_broadcast(&gpu_cond);pthread_mutex_unlock(&gpu_lock);
    wait_recycled(base+2+(unsigned)newer_picture);
    _DrainCallbacksAndPresenter(&g_ss);
    assert(presents==before+5 && last_present_slot==(newer_picture?2u:0u));
    assert(recycled==base+2+(unsigned)newer_picture);
    transition_until_present=0;
}

int main(void) {
    OMX_BUFFERHEADERTYPE only = {0}, *ptr = &only;
    unsigned before;
    alarm(15);
    callback_thread = pthread_self();
    pthread_mutex_init(&g_ss.lock, NULL);
    _CondInit(&g_ss.cond); /* same clock as the sink's deadlines (CLOCK_MONOTONIC off macOS) */
    component.FillThisBuffer = recycle;
    g_ss.decoder = &component;
    burst(0, 0, 0);
    burst(1, 0, 0);
    burst(1, 1, 0);
    burst(1, 0, 32);
    g_ss.outBufs = &ptr;
    g_ss.nOutBufs = 1;
    g_ss.running = g_ss.winReady = g_ss.started = 1;
    g_ss.callbacksClosing = 0;
    only.nFilledLen = 1024;
    only.nTimeStamp = 1;
    before = presents;
    assert(!_StartPresentThread(&g_ss));
    present_and_wait(&only);
    assert(presents == before + 1 && av_sink_video_ready());
    _DrainCallbacksAndPresenter(&g_ss);
    /* One decoded picture must finish the transition without input/refills.
     * It remains client-owned until the final GPU read, then returns once. */
    g_ss.running=g_ss.winReady=g_ss.started=1;
    g_ss.callbacksClosing=0;
    before=presents;
    unsigned recycle_before=recycled;
    transition_until_present=before+5;
    assert(!_StartPresentThread(&g_ss));
    present_and_wait(&only);
    assert(presents==before+5 && recycled==recycle_before+1);
    _DrainCallbacksAndPresenter(&g_ss);
    transition_until_present=0;
    flight_updates(0);
    flight_updates(1);
    stop_and_fault_tests();
    /* Disconnect freezes the RGBA surface: no Apple flash before stock takeover. */
    g_ss.decoder=NULL;g_ss.omxInitialized=1;
    g_ss.present=(screen_egl_present_t *)1;
    g_ss.inBufs=NULL;g_ss.outBufs=NULL;g_ss.freeIn=NULL;
    unsigned waiting_before=waiting_posts;
    g_ss.loaderReady=1;
    unsigned stops_before=waiting_stops;
    _Teardown(&g_ss,0);
    assert(waiting_posts==waiting_before && waiting_before_deinit==waiting_before);
    assert(waiting_stops==stops_before+1 && !av_sink_loader_ready());
    /* Even an early session with no OMX component stops its waiting thread. */
    _Teardown(&g_ss,0);
    assert(waiting_posts==waiting_before && waiting_stops==stops_before+2);
    g_ss.callbacksClosing=0;
    av_sink_set_layout(1,0,0);
    assert(waiting_posts==waiting_before); /* View changes cannot revive idle PRISM. */
    waiting_error=-1;
    assert(_StartLoader(&g_ss)==-1 && !av_sink_loader_ready());
    waiting_error=0;
    assert(!_StartLoader(&g_ss) && av_sink_loader_ready());
    g_ss.present=NULL;
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
    puts("altscreen_mailbox_test: FIFO fallback, newest picture, empty output, bounded synchronous "
         "refill and sole old frame PASS");
    return 0;
}
