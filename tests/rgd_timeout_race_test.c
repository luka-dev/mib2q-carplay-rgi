/* Real RGI handler/timer with an active iAP2 update racing the timer's bus
 * publication. Only framework/socket leaves are replaced. */
#include "routeguidance/rgd_hook.c"
#include <assert.h>
#include <errno.h>

static hook_context_t ctx;
hook_context_t* hook_framework_get_context(void) { return &ctx; }
bool hook_is_ready(void) { return false; }
hook_result_t hook_inject_message(uint16_t m, const uint8_t* p, size_t n) {
    (void)m; (void)p; (void)n; return HOOK_OK;
}
uint8_t* hook_identify_grow(hook_context_t* c, size_t l, size_t n, size_t* cap) {
    (void)c; (void)l; (void)n; (void)cap; return NULL;
}

static pthread_mutex_t capture_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t capture_cond = PTHREAD_COND_INITIALIZER;
static pthread_t active_thread;
static bool race_next_zero, active_started, active_done;
static int frames, last_state;

static void route_state(uint8_t state) {
    uint8_t raw[11] = {0x40, 0x40, 0, 11, 0x52, 0x01, 0, 5, 0, 1, 0};
    write_be16(raw + 8, RGD_TLV_ROUTE_GUIDANCE_STATE);
    raw[10] = state;
    iap2_frame_t frame;
    memset(&frame, 0, sizeof(frame)); frame.msgid = 0x5201; frame.frame_len = sizeof(raw);
    ctx.direction = MSG_DIR_INCOMING; ctx.raw_buf = raw; ctx.raw_len = sizeof(raw);
    rgd_message_handler(&ctx, &frame);
}

static void* activate(void* unused) {
    (void)unused;
    pthread_mutex_lock(&capture_lock);
    active_started = true; pthread_cond_broadcast(&capture_cond);
    pthread_mutex_unlock(&capture_lock);
    route_state(1);
    pthread_mutex_lock(&capture_lock);
    active_done = true; pthread_cond_broadcast(&capture_cond);
    pthread_mutex_unlock(&capture_lock);
    return NULL;
}

hook_result_t bus_send_text(uint16_t type, uint8_t flags, bus_text_builder_t* b) {
    char text[8192];
    (void)type; (void)flags;
    assert(!b->overflow && b->len < sizeof(text));
    memcpy(text, b->buf, b->len); text[b->len] = '\0';
    bool zero = strstr(text, "route_state:n:0\n") != NULL;
    if (zero && race_next_zero) {
        race_next_zero = false;
        assert(pthread_create(&active_thread, NULL, activate, NULL) == 0);
        pthread_mutex_lock(&capture_lock);
        while (!active_started) pthread_cond_wait(&capture_cond, &capture_lock);
        /* On the old implementation activation overtakes this paused stop.
         * A correctly serialized timer makes activation wait until the stop
         * is published. Bound the wait so that both orders can finish. */
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec++;
        while (!active_done) {
            int rc = pthread_cond_timedwait(&capture_cond, &capture_lock, &deadline);
            if (rc == ETIMEDOUT) break;
            assert(rc == 0);
        }
        pthread_mutex_unlock(&capture_lock);
    }
    pthread_mutex_lock(&capture_lock);
    if (zero) last_state = 0;
    else if (strstr(text, "route_state:n:1\n")) last_state = 1;
    frames++;
    pthread_mutex_unlock(&capture_lock);
    bus_text_free(b);
    return HOOK_OK;
}

static void expire_pending(void) {
    pthread_mutex_lock(&g_rgd_debounce_lock);
    assert(g_rgd.state_zero_started_ms != 0);
    g_rgd.state_zero_deadline_ms = 1; // deterministic clock advance, no grace-period sleep
    pthread_mutex_unlock(&g_rgd_debounce_lock);
}

int main(void) {
    route_state(1); route_state(0); expire_pending();
    race_next_zero = true;
    rgd_periodic_tick();
    pthread_join(active_thread, NULL);
    assert(g_rgd.emitted_route_state == 1 && last_state == 1);

    // Activation winning before the timeout cancels it without publishing zero.
    route_state(0); expire_pending(); route_state(1);
    int before = frames; rgd_periodic_tick(); assert(frames == before && last_state == 1);

    // Genuine silence ends the route exactly once.
    route_state(0); expire_pending(); rgd_periodic_tick(); assert(last_state == 0);
    before = frames; rgd_periodic_tick(); assert(frames == before);

    // A reset while rerouting retains its no-timeout policy.
    route_state(5); route_state(0); expire_pending();
    before = frames; rgd_periodic_tick(); assert(frames == before);
    puts("RGI timeout: concurrent activation wins final publication, cancellation, genuine end and reroute hold PASS");
    return 0;
}
