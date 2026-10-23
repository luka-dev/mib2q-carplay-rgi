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
    stop_and_fault_tests();
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
    puts("altscreen_mailbox_test: FIFO fallback, newest picture, empty output, bounded synchronous "
         "refill and sole old frame PASS");
    return 0;
}
