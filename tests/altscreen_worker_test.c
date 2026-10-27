/* Real renderer queue: a failed OMX submission must end the TCP generation
 * and discard queued dependants, while stale failures cannot kill a replug. */
#include <assert.h>
#include <poll.h>
#define main renderer_program_main
#include "../altscreen_render/main.c"
static int last_fatal_code;
static const char *last_fatal_reason;
#undef main

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static int sink_block = 1, sink_release, sink_error = -1;
static unsigned sink_calls, last_byte;

int av_sink_open(void) { return 0; }
int av_sink_frame(const uint8_t *bytes, uint32_t len, uint64_t received_ms) {
    int result;
    assert(len == 1);
    assert(received_ms > 0 && received_ms <= decode_now_ms());
    pthread_mutex_lock(&test_lock);
    ++sink_calls;
    last_byte = bytes[0];
    pthread_cond_broadcast(&test_cond);
    while (sink_block && !sink_release) pthread_cond_wait(&test_cond, &test_lock);
    result = sink_error;
    sink_error = 0;
    pthread_mutex_unlock(&test_lock);
    return result;
}
int av_sink_frame_ex(const uint8_t *b, uint32_t n, uint64_t stamp, const alt_frame_meta_t *meta) {
    (void)meta;
    return av_sink_frame(b, n, stamp);
}
void av_sink_session_end(void) { }
int av_sink_video_ready(void) { return 0; }
const char *av_sink_omx_stage(void) { return 0; }
void av_sink_fatal_exit(int code, const char *reason) {
    /* test seam: remember the last watchdog death instead of exiting */
    last_fatal_code = code;
    last_fatal_reason = reason;
}
void av_sink_close(void) { }
void av_sink_set_layout(int m, int s, int raw) { (void)m; (void)s; (void)raw; }
void altctl_start(int (*video_ready)(void)) { (void)video_ready; }
void altctl_stop(void) { }

static void wait_for_calls(unsigned count) {
    pthread_mutex_lock(&test_lock);
    while (sink_calls < count) pthread_cond_wait(&test_cond, &test_lock);
    pthread_mutex_unlock(&test_lock);
}

int main(void) {
    int pair[2], replacement[2];
    uint32_t generation, next;
    uint8_t first = 1, dependent = 2, fresh = 3, byte;
    struct pollfd p;
    alarm(15);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    assert(decode_worker_start() == 0);
    generation = decode_queue_begin(pair[0]);
    assert(generation);
    assert(decode_queue_frame(generation, &first, 1) == 0);
    wait_for_calls(1);
    assert(decode_queue_frame(generation, &dependent, 1) == 0);
    assert(decode_queue_frame(generation, &dependent, 1) == 0);
    pthread_mutex_lock(&test_lock);
    sink_release = 1;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
    p.fd = pair[1]; p.events = POLLIN; p.revents = 0;
    assert(poll(&p, 1, 2000) == 1);
    assert(read(pair[1], &byte, 1) == 0); /* peer sees EOF, can recover with config + IDR */
    assert(decode_queue_frame(generation, &dependent, 1) == -1);
    decode_queue_end(generation);
    close(pair[0]); close(pair[1]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, replacement) == 0);
    next = decode_queue_begin(replacement[0]);
    assert(next && next != generation);
    decode_queue_fail(generation); /* delayed old-generation failure */
    assert(write(replacement[0], &fresh, 1) == 1);
    assert(read(replacement[1], &byte, 1) == 1 && byte == fresh);
    assert(decode_queue_frame(next, &fresh, 1) == 0);
    wait_for_calls(2);
    decode_queue_end(next);
    close(replacement[0]); close(replacement[1]);
    decode_worker_stop();
    assert(sink_calls == 2 && last_byte == fresh); /* old dependent pictures never reached the sink */
    alarm(0);
    puts("altscreen_worker_test: sink failure -> queue discard + EOF, stale failure and new generation PASS");
    return 0;
}
