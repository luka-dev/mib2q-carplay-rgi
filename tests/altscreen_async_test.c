/* Actual forwarder + socket reader. Hold the writer inside send while the
 * producer fills/overflows the queue. This is a fault injection, not a benchmark. */
#define ENABLE_LOGGING 0
#include <sys/socket.h>
#include <sys/uio.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
static pthread_mutex_t fault_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fault_cond = PTHREAD_COND_INITIALIZER;
static int block_send = 1, send_entered, release_send, second_done;
static ssize_t held_writev(int fd, const struct iovec *v, int count) {
    pthread_mutex_lock(&fault_lock);
    if (block_send) {
        send_entered = 1;
        pthread_cond_broadcast(&fault_cond);
        while (!release_send)
            pthread_cond_wait(&fault_cond, &fault_lock);
        block_send = 0;
    }
    pthread_mutex_unlock(&fault_lock);
    return writev(fd, v, count);
}
#define writev held_writev
#include "../hook/altscreen/altscreen_av.c"
#include "altscreen_forward_compat.h"
#undef writev
#include "../altscreen_render/frame_io.h"
static int listener;
static const uint8_t config[] = {0, 0, 0, 1, 0x67, 0x55};
static const uint8_t idr[] = {0, 0, 0, 2, 0x65, 0x44};
static const uint8_t delta[] = {0, 0, 0, 2, 0x41, 0x55};
static void *server(void *unused) {
    (void)unused;
    for (unsigned connection = 0; connection < 3; ++connection) {
        int fd = accept(listener, NULL, NULL);
        uint8_t hello[8], *buf = NULL;
        uint32_t cap = 0, n;
        alt_frame_meta_t meta;
        assert(fd >= 0);
        alt_wire_put32(hello, ALT_WIRE_HELLO);
        alt_wire_put32(hello + 4, 2);
        if (connection < 2)
            assert(send(fd, hello, sizeof(hello), 0) == sizeof(hello));
        assert(!av_read_frame_ex(fd, &buf, &cap, &n, &meta));
        assert(n == sizeof(config));
        if (connection < 2)
            assert(meta.flags == ALT_WIRE_CONFIG && meta.generation == 1);
        else
            assert(!meta.version);
        assert(!memcmp(buf, config, n));
        assert(!av_read_frame_ex(fd, &buf, &cap, &n, &meta));
        if (connection < 2) {
            assert(meta.flags == ALT_WIRE_IDR && meta.sequence == (connection ? 100u : 1u));
            assert(meta.sender_pts == meta.sequence * 123u && meta.received_us == 456u);
        } else
            assert(!meta.version && buf[4] == 0x65);
        if (connection == 1) {
            assert(!av_read_frame_ex(fd, &buf, &cap, &n, &meta));
            assert(!meta.flags && meta.sequence == 101 && buf[4] == 0x41);
        } else if (connection == 0)
            assert(av_read_frame_ex(fd, &buf, &cap, &n, &meta) == -1);
        free(buf);
        close(fd);
        if (connection == 1) {
            pthread_mutex_lock(&fault_lock);
            second_done = 1;
            pthread_cond_broadcast(&fault_cond);
            pthread_mutex_unlock(&fault_lock);
        }
    }
    return NULL;
}
static uint8_t *owned_body;
static uint32_t owned_capacity;
static void enqueue(uint64_t sequence, int keyframe, int expected) {
    alt_frame_meta_t m = {0};
    alt_avcc_frame_t prepared;
    if (owned_capacity < sizeof(idr)) {
        owned_body = realloc(owned_body, sizeof(idr));
        assert(owned_body);
        owned_capacity = sizeof(idr);
    }
    memcpy(owned_body, keyframe ? idr : delta, sizeof(idr));
    assert(!alt_avcc_prepare(owned_body, sizeof(idr), 4, 1, &prepared));
    m.sequence = sequence;
    m.sender_pts = sequence * 123u;
    m.received_us = 456u;
    assert(altav_enqueue_owned(1, &owned_body, &owned_capacity, &prepared, &m) == expected);
}
int main(void) {
    pthread_t thread;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    signal(SIGPIPE, SIG_IGN);
    alarm(15);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(listener, (void *)&addr, sizeof(addr)) && !listen(listener, 4));
    assert(!getsockname(listener, (void *)&addr, &len));
    assert(!pthread_create(&thread, NULL, server, NULL));
    altav_init("127.0.0.1", ntohs(addr.sin_port));
    assert(altav_claim(1));
    assert(!altav_async_start(NULL, NULL));
    assert(!altav_enqueue_config(1, config, sizeof(config)));
    enqueue(1, 1, 0);
    pthread_mutex_lock(&fault_lock);
    while (!send_entered)
        pthread_cond_wait(&fault_cond, &fault_lock);
    pthread_mutex_unlock(&fault_lock);
    assert(pthread_mutex_trylock(&g_transport.lock) == EBUSY);
    assert(altav_is_owner(1)); /* ownership reads cannot wait behind socket I/O */
    for (unsigned i = 0; i < AV_QUEUE_ITEMS; ++i)
        enqueue(i + 2, 0, 0);
    enqueue(90, 0, -1);
    assert(altav_take_failure(1));
    enqueue(91, 0, -1);
    enqueue(100, 1, 0);
    enqueue(101, 0, 0);
    pthread_mutex_lock(&fault_lock);
    release_send = 1;
    pthread_cond_broadcast(&fault_cond);
    pthread_mutex_unlock(&fault_lock);
    pthread_mutex_lock(&fault_lock);
    while (!second_done)
        pthread_cond_wait(&fault_cond, &fault_lock);
    pthread_mutex_unlock(&fault_lock);
    pthread_mutex_lock(&g_forward.lock);
    while (g_forward.busy || g_forward.head)
        pthread_cond_wait(g_forward.cond, &g_forward.lock);
    assert(g_forward.pool_count > 0 && g_forward.pool_count <= AV_QUEUE_ITEMS &&
           g_forward.pool_bytes <= AV_POOL_BYTES);
    pthread_mutex_unlock(&g_forward.lock);
    assert(altav_claim(2));
    assert(!altav_claim(1));
    assert(altav_enqueue_config(1, config, sizeof(config)) == -1);
    enqueue(102, 1, -1);
    altav_shutdown_if_owner(1);
    assert(altav_is_owner(2));
    assert(!altav_enqueue_config(2, config, sizeof(config)));
    {
        alt_frame_meta_t m = {0};
        m.sequence = 1;
        m.received_us = alt_timing_now_us();
        assert(!altav_enqueue_frame(2, idr, sizeof(idr), 4, &m));
    }
    pthread_join(thread, NULL);
    altav_shutdown();
    free(owned_body);
    owned_body = NULL;
    assert(!g_forward.pool && !g_transport.annexb);
    close(listener);
    alarm(0);
    puts("altscreen_async_test: nonblocking producer, bounded overflow, config+IDR, v2 metadata, "
         "pool reuse and stale generation PASS");
    return 0;
}
