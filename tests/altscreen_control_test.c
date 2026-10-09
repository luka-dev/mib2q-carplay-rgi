/* Real hook writer, renderer parser/queue and opt-in status channel. Only the
 * hardware sink is replaced. EOF must be noticed without another phone AU. */
#define ENABLE_LOGGING 0
#include "../hook/altscreen/altscreen_av.c"
#define main renderer_program_main
#include "../altscreen_render/main.c"
#undef main
#include <assert.h>
static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static int picture_ready, publisher_stop;
static unsigned recovery_callbacks;
static int listener, accepted = -1;
int av_sink_open(void) { return 0; }
int av_sink_frame(const uint8_t *b, uint32_t n, uint64_t t) {
    (void)b;
    (void)n;
    (void)t;
    return 0;
}
int av_sink_frame_ex(const uint8_t *b, uint32_t n, uint64_t t, const alt_frame_meta_t *m) {
    (void)b;
    (void)n;
    (void)t;
    if (m->flags & ALT_WIRE_IDR) {
        pthread_mutex_lock(&test_lock);
        picture_ready = 1;
        pthread_mutex_unlock(&test_lock);
    }
    return 0;
}
void av_sink_session_end(void) {
    pthread_mutex_lock(&test_lock);
    picture_ready = 0;
    pthread_mutex_unlock(&test_lock);
}
int av_sink_video_ready(void) {
    int r;
    pthread_mutex_lock(&test_lock);
    r = picture_ready;
    pthread_mutex_unlock(&test_lock);
    return r;
}
void av_sink_close(void) {}
void av_sink_set_layout(int a, int b, int c) {
    (void)a;
    (void)b;
    (void)c;
}
void altctl_start(int (*ready)(void)) { (void)ready; }
void altctl_stop(void) {}
static void failed(uint32_t gen, void *context) {
    (void)context;
    assert(gen == 1);
    __sync_add_and_fetch(&recovery_callbacks, 1);
}
static void *server(void *unused) {
    int fd;
    uint32_t gen;
    (void)unused;
    fd = accept(listener, NULL, NULL);
    assert(fd >= 0);
    pthread_mutex_lock(&test_lock);
    accepted = fd;
    pthread_mutex_unlock(&test_lock);
    gen = decode_queue_begin(fd);
    serve_client(fd, gen);
    decode_queue_end(gen);
    close(fd);
    return NULL;
}
static void *publish(void *unused) {
    (void)unused;
    for (;;) {
        int stop;
        struct timespec pause = {0, 50000000L};
        pthread_mutex_lock(&test_lock);
        stop = publisher_stop;
        pthread_mutex_unlock(&test_lock);
        if (stop)
            return NULL;
        (void)decode_video_ready();
        nanosleep(&pause, NULL);
    }
}
int main(void) {
    struct sockaddr_in a;
    socklen_t size = sizeof(a);
    pthread_t peer, status;
    uint8_t cfg[] = {0, 0, 0, 1, 0x67, 0x55}, *body;
    uint32_t cap = 6;
    alt_frame_meta_t meta = {0};
    alt_avcc_frame_t info;
    uint64_t begin;
    int healthy = 0;
    alarm(15);
    signal(SIGPIPE, SIG_IGN);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener >= 0);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(listener, (void *)&a, sizeof(a)) && !listen(listener, 2));
    assert(!getsockname(listener, (void *)&a, &size));
    assert(!decode_worker_start());
    assert(!pthread_create(&peer, NULL, server, NULL));
    assert(!pthread_create(&status, NULL, publish, NULL));
    altav_init("127.0.0.1", ntohs(a.sin_port));
    assert(altav_claim(1));
    assert(!altav_async_start(failed, NULL));
    assert(altav_keyframe_permit(1, ALT_KEYFRAME_START));
    assert(!altav_enqueue_config(1, cfg, sizeof(cfg)));
    body = (uint8_t *)malloc(cap);
    assert(body);
    memcpy(body, (uint8_t[]){0, 0, 0, 2, 0x65, 0x55}, 6);
    assert(!alt_avcc_prepare(body, 6, 4, 1, &info));
    meta.sequence = 1;
    meta.received_us = alt_timing_now_us();
    assert(!altav_enqueue_owned(1, &body, &cap, &info, &meta));
    free(body);
    begin = alt_timing_now_us();
    while (alt_timing_now_us() - begin < 3000000u) {
        struct timespec pause = {0, 10000000L};
        pthread_mutex_lock(&g_forward.lock);
        healthy = g_forward.recovery.health_seen && g_forward.recovery.health_ready && !g_forward.recovery.active;
        pthread_mutex_unlock(&g_forward.lock);
        if (healthy)
            break;
        nanosleep(&pause, NULL);
    }
    assert(healthy && !__sync_add_and_fetch(&recovery_callbacks, 0));
    pthread_mutex_lock(&test_lock);
    assert(accepted >= 0);
    assert(!shutdown(accepted, SHUT_RDWR));
    pthread_mutex_unlock(&test_lock);
    pthread_join(peer, NULL);
    begin = alt_timing_now_us();
    while (!__sync_add_and_fetch(&recovery_callbacks, 0) &&
           alt_timing_now_us() - begin < 1000000u) {
        struct timespec pause = {0, 10000000L};
        nanosleep(&pause, NULL);
    }
    assert(__sync_add_and_fetch(&recovery_callbacks, 0) == 1);
    for (unsigned i = 0; i < 20; ++i)
        assert(!altav_keyframe_permit(1, ALT_KEYFRAME_RECOVERY));
    assert(altav_claim(2));
    assert(!altav_keyframe_permit(1, ALT_KEYFRAME_RECOVERY));
    pthread_mutex_lock(&test_lock);
    publisher_stop = 1;
    pthread_mutex_unlock(&test_lock);
    pthread_join(status, NULL);
    altav_shutdown();
    decode_worker_stop();
    close(listener);
    alarm(0);
    puts("altscreen_control_test: negotiated status, real first-picture ACK, stable health, idle "
         "EOF, retry coalescing and stale generation PASS");
    return 0;
}
