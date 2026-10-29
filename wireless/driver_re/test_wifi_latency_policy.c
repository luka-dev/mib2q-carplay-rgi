#include "wifi_latency_policy.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

static void rx_cases(void) {
    struct wl_rx_age s = {0}, old;
    struct wl_rx_age_result r;
    uint32_t slots[64] = {0};
    assert(wl_rx_age_create(&s, 1) == WL_OK);
    slots[15] = 100;
    assert(wl_rx_age_observe(&s, 1, 0, 0, slots, 16, 100, 15, 0, &r) == WL_OK);
    assert(r.valid && r.due == 6 && !r.expired && r.oldest_packet == 100);
    slots[15] = 0; slots[14] = 100;
    assert(wl_rx_age_observe(&s, 1, 4, 1, slots, 16, 200, 0, 4, &r) == WL_OK);
    assert(r.due == 6 && !r.expired);
    assert(wl_rx_age_observe(&s, 1, 6, 1, slots, 16, 300, 15, 6, &r) == WL_OK);
    assert(r.due == 6 && r.expired); /* rejected duplicate must not refresh it */
    old = s;
    assert(wl_rx_age_observe(&s, 2, 7, 1, slots, 16, 0, 0, 0, &r) == WL_STALE);
    assert(!r.valid && !memcmp(&s, &old, sizeof(s)));
    assert(wl_rx_age_retire(&s, 1) == WL_OK);
    assert(wl_rx_age_create(&s, 1) == WL_STALE);
    assert(wl_rx_age_create(&s, 2) == WL_OK);

    memset(slots, 0, sizeof(slots)); slots[0] = 100;
    assert(wl_rx_age_observe(&s, 2, 10, 64, slots, 16, 100, 64, 9, &r) == WL_OK);
    assert(r.due == 15); /* same pointer, new sequence and explicit arrival */
    memset(slots, 0, sizeof(slots));
    assert(wl_rx_age_observe(&s, 2, 11, 65, slots, 16, 0, 0, 0, &r) == WL_OK);
    assert(!r.valid && !r.pending && !s.present[0] && !s.present[1]);
    assert(wl_rx_age_observe(&s, 2, 10, 65, slots, 16, 0, 0, 0, &r) == WL_CLOCK_FAULT);
    assert(wl_rx_age_observe(&s, 2, 12, 65, slots, 16, 0, 0, 0, &r) == WL_CLOCK_FAULT);
    assert(wl_rx_age_retire(&s, 2) == WL_OK);
    assert(wl_rx_age_create(&s, 3) == WL_OK);

    slots[1] = 400;
    assert(wl_rx_age_observe(&s, 3, UINT32_MAX - 3, 4095, slots, 16,
                             400, 0, UINT32_MAX - 3, &r) == WL_OK);
    assert(r.due == 2);
    assert(wl_rx_age_observe(&s, 3, 1, 4095, slots, 16, 0, 0, 0, &r) == WL_OK);
    assert(!r.expired && r.due == 2);
    assert(wl_rx_age_observe(&s, 3, 2, 4095, slots, 16, 0, 0, 0, &r) == WL_OK);
    assert(r.expired);
    assert(wl_rx_age_retire(&s, 3) == WL_OK);
    assert(wl_rx_age_create(&s, 4) == WL_OK);
    for (unsigned i = 0; i < 64; ++i) slots[i] = 4000 + i;
    assert(wl_rx_age_observe(&s, 4, 20, 4080, slots, 64, 0, 0, 0, &r) == WL_OK);
    assert(r.pending == 64 && r.due == 26);
    old = s;
    assert(wl_rx_age_observe(&s, 4, 21, 4080, slots, 65, 0, 0, 0, &r) == WL_INVALID);
    assert(!memcmp(&s, &old, sizeof(s)));
}

static void tx_cases(void) {
    struct wl_tx_wake s;
    struct wl_tx_ticket a, b, next;
    wl_tx_init(&s);
    assert(wl_tx_bind(&s, 1) == WL_OK);
    assert(wl_tx_request(&s, 1, 0, 1, &a) == WL_TX_NONE);
    assert(wl_tx_request(&s, 1, 1, 0, &a) == WL_TX_NONE);
    assert(wl_tx_request(&s, 1, 1, 1, &a) == WL_TX_SUBMIT);
    for (int i = 0; i < 32; ++i) assert(wl_tx_request(&s, 1, 1, 1, &b) == WL_TX_NONE);
    assert(wl_tx_begin(&s, a) == WL_TX_RUN);
    assert(wl_tx_begin(&s, a) == WL_TX_NONE);
    for (int i = 0; i < 32; ++i) assert(wl_tx_request(&s, 1, 1, 1, &b) == WL_TX_NONE);
    assert(wl_tx_finish(&s, a, &b) == WL_TX_SUBMIT);
    assert(b.serial == a.serial + 1);
    assert(wl_tx_submit_failed(&s, a) == WL_STALE);
    assert(wl_tx_begin(&s, a) == WL_TX_NONE);
    assert(wl_tx_begin(&s, b) == WL_TX_RUN);
    assert(wl_tx_finish(&s, b, &next) == WL_TX_NONE);
    assert(wl_tx_retire(&s, 1) == WL_OK && wl_tx_reclaimable(&s));
    assert(wl_tx_bind(&s, 2) == WL_OK);
    assert(wl_tx_request(&s, 1, 1, 1, &next) == WL_TX_NONE);
    assert(wl_tx_request(&s, 2, 1, 1, &a) == WL_TX_SUBMIT);
    assert(wl_tx_retire(&s, 2) == WL_BUSY && !wl_tx_reclaimable(&s));
    assert(wl_tx_bind(&s, 3) == WL_BUSY);
    assert(wl_tx_begin(&s, a) == WL_TX_DISCARD && wl_tx_reclaimable(&s));
    assert(wl_tx_bind(&s, 3) == WL_OK);
    assert(wl_tx_begin(&s, a) == WL_TX_NONE);
    assert(wl_tx_request(&s, 3, 1, 1, &a) == WL_TX_SUBMIT);
    assert(wl_tx_submit_failed(&s, a) == WL_OK);
    assert(wl_tx_request(&s, 3, 1, 1, &b) == WL_TX_SUBMIT);
    assert(wl_tx_submit_failed(&s, a) == WL_STALE);
    assert(wl_tx_begin(&s, b) == WL_TX_RUN);
    assert(wl_tx_request(&s, 3, 1, 1, &a) == WL_TX_NONE);
    assert(wl_tx_retire(&s, 3) == WL_BUSY);
    assert(wl_tx_finish(&s, b, &next) == WL_TX_NONE && wl_tx_reclaimable(&s));
    /* Serial exhaustion fails open to the retained stock fallback, not ABA. */
    atomic_store(&s.control, (UINT64_C(4) << 32) | (UINT64_C(1) << 30) | WL_TX_SERIAL_MAX);
    assert(wl_tx_serial_exhausted(&s));
    assert(wl_tx_request(&s, 4, 1, 1, &a) == WL_TX_NONE);
    a.generation=4; a.serial=WL_TX_SERIAL_MAX;
    assert(wl_tx_begin(&s, a) == WL_TX_NONE); /* never reuse the last serial */
    atomic_store(&s.control, (UINT64_C(4) << 32) | (UINT64_C(1) << 30) | (UINT64_C(1) << 28) | WL_TX_SERIAL_MAX);
    assert(!wl_tx_serial_exhausted(&s));
    assert(wl_tx_begin(&s, a) == WL_TX_RUN);
    assert(!wl_tx_serial_exhausted(&s));
    assert(wl_tx_finish(&s, a, &next) == WL_TX_NONE);
    assert(wl_tx_serial_exhausted(&s));
    assert(wl_tx_retire(&s, 4) == WL_OK);
    assert(wl_tx_bind(&s, 5) == WL_OK);
    assert(wl_tx_retire(&s, 5) == WL_OK);
    assert(wl_tx_bind(&s, 4) == WL_STALE);
    assert(wl_tx_bind(&s, UINT32_MAX) == WL_OK);
    assert(wl_tx_retire(&s, UINT32_MAX) == WL_OK);
    assert(wl_tx_bind(&s, 0) == WL_INVALID);
    assert(wl_tx_bind(&s, 1) == WL_STALE);
}

static struct wl_tx_wake shared;
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_ready = PTHREAD_COND_INITIALIZER;
static struct wl_tx_ticket queued;
static int has_ticket, done;
static _Atomic unsigned submitted, failed, consumed;

static void submit(struct wl_tx_ticket t) {
    atomic_fetch_add(&submitted, 1);
    if (t.serial % 7 == 0) {
        assert(wl_tx_submit_failed(&shared, t) == WL_OK);
        atomic_fetch_add(&failed, 1);
        return;
    }
    pthread_mutex_lock(&queue_lock);
    assert(!has_ticket);
    queued = t; has_ticket = 1;
    pthread_cond_signal(&queue_ready);
    pthread_mutex_unlock(&queue_lock);
}
static void *producer(void *unused) {
    (void)unused;
    for (unsigned i = 0; i < 20000; ++i) {
        struct wl_tx_ticket t;
        if (wl_tx_request(&shared, 1, 1, 1, &t) == WL_TX_SUBMIT) submit(t);
        if (i % 31 == 0) sched_yield();
    }
    return NULL;
}
static void *consumer(void *unused) {
    (void)unused;
    for (;;) {
        pthread_mutex_lock(&queue_lock);
        while (!has_ticket && !done) pthread_cond_wait(&queue_ready, &queue_lock);
        if (!has_ticket && done) { pthread_mutex_unlock(&queue_lock); return NULL; }
        struct wl_tx_ticket t = queued;
        has_ticket = 0;
        pthread_mutex_unlock(&queue_lock);
        assert(wl_tx_begin(&shared, t) == WL_TX_RUN);
        sched_yield();
        struct wl_tx_ticket next;
        int action = wl_tx_finish(&shared, t, &next);
        atomic_fetch_add(&consumed, 1);
        if (action == WL_TX_SUBMIT) submit(next);
    }
}
int main(void) {
    rx_cases(); tx_cases();
    wl_tx_init(&shared);
    assert(wl_tx_bind(&shared, 1) == WL_OK);
    pthread_t workers[4], pump;
    assert(!pthread_create(&pump, NULL, consumer, NULL));
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_create(&workers[i], NULL, producer, NULL));
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_join(workers[i], NULL));
    pthread_mutex_lock(&queue_lock); done = 1; pthread_cond_signal(&queue_ready);
    pthread_mutex_unlock(&queue_lock);
    assert(!pthread_join(pump, NULL));
    assert(atomic_load(&submitted) == atomic_load(&consumed) + atomic_load(&failed));
    assert(wl_tx_retire(&shared, 1) == WL_OK && wl_tx_reclaimable(&shared));
    printf("PASS: RX age/lifecycle/wrap; TX lifecycle/stale/failure/exhaustion; 80000 concurrent requests; %u callbacks, %u enqueue failures\n",
           atomic_load(&consumed), atomic_load(&failed));
    return 0;
}
