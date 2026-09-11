/* Real bus cache/wire paths with deterministic allocation and send failures. */
#define ENABLE_LOGGING 0
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
static void *test_malloc(size_t);
static ssize_t test_send(int, const void *, size_t, int);
#define malloc test_malloc
#define send test_send
#include "../hook/framework/bus.c"
#undef malloc
#undef send

static int fail_alloc_at, alloc_calls;
static int fail_send_at_byte = -1, sent_bytes, failed_send, sends_after_failure;
static int send_calls;

static void *test_malloc(size_t len) {
    if (fail_alloc_at && ++alloc_calls == fail_alloc_at) return NULL;
    return malloc(len);
}

static ssize_t test_send(int fd, const void *data, size_t len, int flags) {
    ++send_calls;
    if (failed_send) ++sends_after_failure;
    if (fail_send_at_byte >= 0) {
        if (sent_bytes >= fail_send_at_byte) {
            failed_send = 1;
            errno = EPIPE;
            return -1;
        }
        if (len > (size_t)(fail_send_at_byte - sent_bytes))
            len = (size_t)(fail_send_at_byte - sent_bytes);
    }
    if (len > 5) len = 5; /* every header and payload uses short writes */
    ssize_t result = send(fd, data, len, flags);
    if (result > 0) sent_bytes += (int)result;
    return result;
}

static void reset_send_fault(void) {
    fail_send_at_byte = -1;
    sent_bytes = failed_send = sends_after_failure = send_calls = 0;
}

static void drain_queue(void) {
    frame_t f;
    while (q_dequeue_nolock(&f)) frame_dispose(&f);
}

static void assert_wire_frame(int fd, uint16_t type, const void *payload, size_t len) {
    uint8_t header[BUS_HEADER_SIZE], actual[128];
    assert(len <= sizeof(actual));
    assert(read_all(fd, header, sizeof(header)) == 0);
    assert(read_be32(header) == BUS_MAGIC);
    assert(read_be16(header + 8) == type);
    assert(read_be32(header + 12) == len);
    assert(read_all(fd, actual, len) == 0);
    if (len) assert(!memcmp(actual, payload, len));
}

static void cache_allocation_failure(void) {
    const uint8_t old[] = "old cover", newer[] = "new cover";
    int pair[2];
    assert(bus_send(EVT_COVERART, BUS_FLAG_STICKY, old, sizeof(old)) == HOOK_OK);
    drain_queue();
    alloc_calls = 0;
    fail_alloc_at = 2; /* source copy succeeds, replacement sticky copy fails */
    assert(bus_send(EVT_COVERART, BUS_FLAG_STICKY, newer, sizeof(newer)) == HOOK_ERR_MEMORY);
    fail_alloc_at = 0;
    assert(q_is_empty());
    assert(g_types[EVT_COVERART].has_last);
    assert(g_types[EVT_COVERART].last.len == sizeof(old));
    assert(g_types[EVT_COVERART].last.payload);
    assert(!memcmp(g_types[EVT_COVERART].last.payload, old, sizeof(old)));
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    assert(send_sync_snapshot(pair[0], 0, false) == 0);
    assert_wire_frame(pair[1], EVT_SYNC_BEGIN, NULL, 0);
    assert_wire_frame(pair[1], EVT_COVERART, old, sizeof(old));
    assert_wire_frame(pair[1], EVT_SYNC_END, NULL, 0);
    close(pair[0]); close(pair[1]);
}

static pthread_mutex_t reader_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reader_cond = PTHREAD_COND_INITIALIZER;
static int reader_ready, reader_result;
static void *wait_for_read(void *ctx) {
    int fd = *(int *)ctx;
    uint8_t byte;
    pthread_mutex_lock(&reader_lock);
    reader_ready = 1;
    pthread_cond_signal(&reader_cond);
    pthread_mutex_unlock(&reader_lock);
    reader_result = read_all(fd, &byte, 1);
    return NULL;
}

static void snapshot_failure(void) {
    int pair[2];
    pthread_t reader;
    uint8_t wire[128];
    size_t received = 0;
    ssize_t n;
    reset_send_fault();
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    g_client_fd = pair[0];
    ++g_client_gen;
    assert(pthread_create(&reader, NULL, wait_for_read, &pair[0]) == 0);
    pthread_mutex_lock(&reader_lock);
    while (!reader_ready) pthread_cond_wait(&reader_cond, &reader_lock);
    pthread_mutex_unlock(&reader_lock);
    fail_send_at_byte = 2 * BUS_HEADER_SIZE + 2; /* replay header + partial payload */
    assert(send_sync_snapshot(pair[0], g_client_gen, true) == -1);
    assert(g_client_fd == -1 && failed_send && !sends_after_failure);
    assert(fcntl(pair[0], F_GETFD) >= 0); /* no fd reuse while recv may be active */
    assert(pthread_join(reader, NULL) == 0);
    assert(reader_result == -1); /* retirement wakes the connector's recv */
    release_connection(pair[0], g_client_gen);
    while ((n = recv(pair[1], wire + received, sizeof(wire) - received, 0)) > 0)
        received += (size_t)n;
    assert(n == 0 && received == (size_t)fail_send_at_byte);
    close(pair[1]);
    reset_send_fault();

    /* The private initial handshake must also stop at its first failure. */
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    fail_send_at_byte = 3;
    assert(send_sync_snapshot(pair[0], 0, false) == -1);
    assert(failed_send && !sends_after_failure);
    close(pair[0]); close(pair[1]);
    reset_send_fault();
}

static void reused_descriptor(void) {
    int old_pair[2], pair[2];
    uint8_t payload[] = {1, 2, 3};
    frame_t f = {EVT_RGD_UPDATE, 0, 1, sizeof(payload), payload};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, old_pair) == 0);
    int old_fd = old_pair[0];
    uint32_t old_gen = ++g_client_gen;
    g_client_fd = old_fd;
    release_connection(old_fd, old_gen);
    close(old_pair[1]);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    /* Force the same descriptor even on hosts with a different allocator. */
    if (pair[1] == old_fd) { int tmp = pair[0]; pair[0] = pair[1]; pair[1] = tmp; }
    if (pair[0] != old_fd) {
        assert(dup2(pair[0], old_fd) == old_fd);
        close(pair[0]); pair[0] = old_fd;
    }
    g_client_fd = pair[0];
    ++g_client_gen;
    reset_send_fault();
    assert(send_frame(old_fd, old_gen, true, &f) == -1);
    assert(!send_calls && g_client_fd == pair[0]);
    disconnect_current_fd_if_matches(old_fd, old_gen, "stale completion");
    assert(g_client_fd == pair[0]);
    assert(send_frame(pair[0], g_client_gen, true, &f) == 0);
    assert_wire_frame(pair[1], f.type, payload, sizeof(payload));
    release_connection(pair[0], g_client_gen);
    close(pair[1]);
}

int main(void) {
    alarm(15); /* fail a stuck reconnect test instead of hanging the suite */
    signal(SIGPIPE, SIG_IGN);
    cache_allocation_failure();
    snapshot_failure();
    reused_descriptor();
    frame_dispose(&g_types[EVT_COVERART].last);
    g_types[EVT_COVERART].has_last = false;
    alarm(0);
    puts("bus_transport_test: cache OOM, short writes, failed replay, recv wakeup and reused fd PASS");
    return 0;
}
