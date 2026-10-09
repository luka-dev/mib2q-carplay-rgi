#include <assert.h>
#define main renderer_program_main
#include "../altscreen_render/main.c"
#undef main
int av_sink_open(void) { return 0; }
int av_sink_frame(const uint8_t *b, uint32_t n, uint64_t t) {
    (void)b;
    (void)n;
    (void)t;
    return 0;
}
int av_sink_frame_ex(const uint8_t *b, uint32_t n, uint64_t t, const alt_frame_meta_t *m) {
    (void)m;
    return av_sink_frame(b, n, t);
}
void av_sink_session_end(void) {}
int av_sink_video_ready(void) { return 1; }
void av_sink_close(void) {}
void av_sink_set_layout(int a, int b, int c) {
    (void)a;
    (void)b;
    (void)c;
}
void altctl_start(int (*ready)(void)) { (void)ready; }
void altctl_stop(void) {}
static void consume_pending(void) {
    pthread_mutex_lock(&g_decode_lock);
    decode_queue_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
}
static void startup_origin_transition(void) {
    int fd[2];
    uint8_t data = 0x65;
    alt_frame_meta_t meta = {0};
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    uint32_t gen = decode_queue_begin(fd[0]);
    assert(!g_decode_latest_input_ms && !g_decode_steady);
    consume_pending();
    g_decode_active_generation = gen;
    meta.version = ALT_WIRE_VERSION;
    meta.received_us = (decode_now_ms() - 1900u) * 1000u;
    for (unsigned i = 0; i < 3; ++i)
        assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    assert(decode_video_ready());
    assert(!g_decode_steady && !g_decode_failed && g_decode_items == 3);
    consume_pending();
    assert(decode_video_ready()); /* Empty local queue still has an old last input. */
    assert(!g_decode_steady && !g_decode_failed);

    /* A fresh head and tail do not hide an older middle startup AU. */
    meta.received_us = alt_timing_now_us();
    assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    meta.received_us = (decode_now_ms() - 1900u) * 1000u;
    assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    meta.received_us = alt_timing_now_us();
    assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    assert(decode_video_ready() && !g_decode_steady && !g_decode_failed);
    consume_pending();
    /* An in-flight startup input must complete before steady promotion. */
    g_decode_worker_busy = 1;
    g_decode_busy_origin_ms = decode_now_ms() - 1900u;
    assert(decode_video_ready() && !g_decode_steady);
    g_decode_busy_origin_ms = decode_now_ms();
    assert(decode_video_ready() && g_decode_steady); /* Healthy busy work can promote. */
    g_decode_worker_busy = 0;
    g_decode_busy_origin_ms = 0;
    assert(decode_video_ready() && g_decode_steady);

    /* Once steady, actual new accumulating backlog still triggers recovery. */
    meta.received_us = (decode_now_ms() - 1900u) * 1000u;
    assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    assert(decode_video_ready()); /* A lone old/final picture remains protected. */
    assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    assert(!decode_video_ready() && g_decode_failed && !g_decode_head);
    close(fd[0]);
    close(fd[1]);

    /* Future/missing v2 origin and legacy metadata use local enqueue time. */
    for (unsigned mode = 0; mode < 3; ++mode) {
        assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
        gen = decode_queue_begin(fd[0]);
        consume_pending();
        assert(!g_decode_latest_input_ms && !g_decode_steady);
        g_decode_active_generation = gen;
        meta.version = mode ? ALT_WIRE_VERSION : 0;
        meta.received_us = mode == 2 ? alt_timing_now_us() + 1000000u : 0;
        assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
        assert(decode_video_ready() && g_decode_steady && !g_decode_failed);
        consume_pending();
        close(fd[0]);
        close(fd[1]);
    }
    pthread_mutex_lock(&g_decode_lock);
    decode_pool_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
}
int main(void) {
    int fd[2];
    uint32_t gen;
    uint8_t data = 0x65;
    uint8_t *owned, *original;
    uint32_t cap = 65536;
    alt_frame_meta_t meta = {0};
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    gen = decode_queue_begin(fd[0]);
    assert(gen);
    pthread_mutex_lock(&g_decode_lock);
    decode_queue_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
    assert(!decode_queue_frame(gen, &data, 1));
    assert(!decode_queue_frame(gen, &data, 1));
    g_decode_head->received_ms = decode_now_ms() - 5000u;
    assert(!decode_queue_frame(gen, &data, 1)); /* startup buffering is not age-filtered */
    pthread_mutex_lock(&g_decode_lock);
    decode_queue_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
    g_decode_active_generation = gen;
    assert(!decode_queue_frame(gen, &data, 1));
    consume_pending(); /* Model a freshly consumed input before steady readiness. */
    assert(decode_video_ready());
    assert(g_decode_steady);
    assert(!decode_queue_frame(gen, &data, 1));
    g_decode_head->received_ms = decode_now_ms() - 5000u;
    assert(!decode_queue_frame(gen, &data, 1)); /* never reject a lone/final old picture */
    assert(decode_queue_frame(gen, &data, 1) == -1 && g_decode_failed && !g_decode_head);
    {
        uint8_t b;
        assert(read(fd[1], &b, 1) == 0);
    } /* continuity recovery wakes the reader */
    close(fd[0]);
    close(fd[1]);
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    gen = decode_queue_begin(fd[0]);
    pthread_mutex_lock(&g_decode_lock);
    decode_queue_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
    owned = original = (uint8_t *)malloc(cap);
    assert(owned);
    owned[0] = data;
    meta.version = 2;
    meta.generation = 7;
    meta.sequence = 88;
    meta.sender_pts = 99;
    assert(!decode_queue_frame_ex(gen, owned, 1, &meta, &owned, &cap));
    assert(g_decode_tail->data == original && g_decode_tail->meta.sequence == 88);
    assert(g_decode_tail->meta.sender_pts == 99);
    free(owned);
    pthread_mutex_lock(&g_decode_lock);
    decode_queue_clear_locked();
    assert(g_decode_pool_items <= DECODE_POOL_ITEMS && g_decode_pool_bytes <= DECODE_POOL_BYTES);
    decode_pool_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
    close(fd[0]);
    close(fd[1]);
    /* Origin time includes the hook queue, not just renderer residence. */
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    gen = decode_queue_begin(fd[0]);
    pthread_mutex_lock(&g_decode_lock);
    decode_queue_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
    g_decode_active_generation = gen;
    assert(!decode_queue_frame(gen, &data, 1));
    consume_pending();
    assert(decode_video_ready());
    meta.received_us = (decode_now_ms() - 1900u) * 1000u;
    assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    assert(!decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL));
    g_decode_head->received_ms = decode_now_ms() - 900u;
    assert(decode_queue_frame_ex(gen, &data, 1, &meta, NULL, NULL) == -1 && g_decode_failed);
    pthread_mutex_lock(&g_decode_lock);
    decode_pool_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
    close(fd[0]);
    close(fd[1]);
    startup_origin_transition();
    puts("altscreen_queue_policy_test: origin-aware startup transition/static-frame survival, "
         "steady backlog recovery, "
         "owned buffer transfer and pool bounds PASS");
    return 0;
}
