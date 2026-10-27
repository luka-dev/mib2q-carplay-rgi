/* Real socket reader, decode worker and control loop. Only the hardware sink
 * is replaced: it completes after the reader blocks on a static scene. */
#include <assert.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "../altscreen_render/protocol.h"
#undef ALTCTL_PORT
static int test_control_port;
#define ALTCTL_PORT test_control_port
#define main renderer_program_main
#define av_read_frame_mode observed_read_frame
#include "../altscreen_render/main.c"
static int last_fatal_code;
static const char *last_fatal_reason;
#undef av_read_frame_mode
#undef main
#include "../altscreen_render/altctl.c"
int av_read_frame_mode(int, uint8_t **, uint32_t *, uint32_t *, alt_frame_meta_t *, unsigned);

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static unsigned reads, sink_calls;
static int allow_picture, ready, hold_reset, reset_entered;

int observed_read_frame(int fd, uint8_t **buf, uint32_t *cap, uint32_t *len, alt_frame_meta_t *meta,
                        unsigned mode) {
    pthread_mutex_lock(&test_lock);
    ++reads;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
    return av_read_frame_mode(fd, buf, cap, len, meta, mode);
}
int av_sink_open(void) { return 0; }
int av_sink_frame(const uint8_t *b, uint32_t n, uint64_t stamp) {
    (void)b; (void)n; (void)stamp;
    pthread_mutex_lock(&test_lock);
    ++sink_calls;
    pthread_cond_broadcast(&test_cond);
    while (!allow_picture) pthread_cond_wait(&test_cond, &test_lock);
    ready = 1;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
    return 0;
}
int av_sink_frame_ex(const uint8_t *b, uint32_t n, uint64_t stamp, const alt_frame_meta_t *meta) {
    (void)meta;
    return av_sink_frame(b, n, stamp);
}
void av_sink_session_end(void) {
    pthread_mutex_lock(&test_lock);
    reset_entered = 1;
    pthread_cond_broadcast(&test_cond);
    while (hold_reset) pthread_cond_wait(&test_cond, &test_lock);
    ready = 0;
    pthread_mutex_unlock(&test_lock);
}
int av_sink_video_ready(void) {
    int r;
    pthread_mutex_lock(&test_lock); r = ready; pthread_mutex_unlock(&test_lock);
    return r;
}
const char *av_sink_omx_stage(void) { return 0; }
void av_sink_fatal_exit(int code, const char *reason) {
    /* test seam: remember the last watchdog death instead of exiting */
    last_fatal_code = code;
    last_fatal_reason = reason;
}
void av_sink_close(void) { }
void av_sink_set_layout(int a, int b, int c) { (void)a; (void)b; (void)c; }

struct reader_args { int fd; uint32_t generation; };
static void *read_thread(void *context) {
    struct reader_args *a = context;
    serve_client(a->fd, a->generation);
    decode_queue_end(a->generation);
    return NULL;
}
static void expect_event(int fd, unsigned char event) {
    unsigned char pkt[ALTCTL_PKT_SIZE];
    size_t used = 0;
    while (used < sizeof(pkt)) {
        struct pollfd p = {fd, POLLIN, 0};
        assert(poll(&p, 1, 1500) == 1);
        ssize_t n = read(fd, pkt + used, sizeof(pkt) - used);
        assert(n > 0);
        used += (size_t)n;
    }
    assert(pkt[0] == event);
}
static void expect_silence(int fd) {
    struct pollfd p = {fd, POLLIN, 0};
    assert(poll(&p, 1, 250) == 0);
}
static int accept_control(int listener) {
    struct pollfd p = {listener, POLLIN, 0};
    assert(poll(&p, 1, 1500) == 1);
    int fd = accept(listener, NULL, NULL);
    assert(fd >= 0);
    return fd;
}
static void set_ready(int value) {
    pthread_mutex_lock(&test_lock); ready = value; pthread_mutex_unlock(&test_lock);
}
static void release_picture(void) {
    pthread_mutex_lock(&test_lock);
    allow_picture = 1;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
}
static void wait_for_static_reader(unsigned count) {
    pthread_mutex_lock(&test_lock);
    while (reads < count) pthread_cond_wait(&test_cond, &test_lock);
    pthread_mutex_unlock(&test_lock);
}

static void check_log_cap(void) {
    char dir[] = "/tmp/altscreen-log-test.XXXXXX", path[160], previous[160];
    struct stat st;
    int status;
    assert(mkdtemp(dir));
    snprintf(path, sizeof(path), "%s/current", dir);
    snprintf(previous, sizeof(previous), "%s/previous", dir);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        char chunk[1024];
        memset(chunk, 'x', sizeof(chunk));
        open_runtime_log(path, previous);
        for (unsigned i = 0; i < 600; ++i)
            assert(fwrite(chunk, 1, sizeof(chunk), stderr) == sizeof(chunk));
        pid_t capper = fork();
        assert(capper >= 0);
        if (capper == 0) {
            execl("/bin/sh", "sh", "-c", ". \"$1\"; cp_cap_log \"$2\"", "cap",
                  "./deploy/smartphone_integrator/carplay_processes.sh", path, (char *)NULL);
            _exit(127);
        }
        assert(waitpid(capper, &status, 0) == capper && WIFEXITED(status) && !WEXITSTATUS(status));
        fputs("next renderer line\n", stderr);
        assert(stat(path, &st) == 0 && st.st_size > 0 && st.st_size < 1024);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    unlink(path); unlink(previous); rmdir(dir);
}

int main(void) {
    int pair[2], next_pair[2], listener, control;
    pthread_t reader, next_reader;
    struct reader_args a, next;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    uint8_t record[] = {0, 0, 0, 1, 0x65};
    alarm(20);
    signal(SIGPIPE, SIG_IGN);
    check_log_cap();
    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(getsockname(listener, (struct sockaddr *)&addr, &addrlen) == 0);
    test_control_port = ntohs(addr.sin_port);
    assert(listen(listener, 2) == 0);
    assert(decode_worker_start() == 0);
    altctl_start(decode_video_ready);
    control = accept_control(listener);
    expect_event(control, ALTEVT_VIDEO_OFF);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    a.fd = pair[0]; a.generation = decode_queue_begin(a.fd);
    assert(pthread_create(&reader, NULL, read_thread, &a) == 0);
    assert(write(pair[1], record, sizeof(record)) == sizeof(record));
    wait_for_static_reader(2);
    expect_silence(control); /* queued input alone is not a presented frame */
    release_picture();
    expect_event(control, ALTEVT_VIDEO_ON); /* no next input packet */
    expect_silence(control); /* healthy static video stays on */
    set_ready(0); expect_event(control, ALTEVT_VIDEO_OFF);
    set_ready(1); expect_event(control, ALTEVT_VIDEO_ON);

    shutdown(control, SHUT_RDWR); close(control);
    control = accept_control(listener);
    expect_event(control, ALTEVT_VIDEO_ON); /* control reconnect re-samples */
    shutdown(pair[1], SHUT_RDWR); pthread_join(reader, NULL);
    expect_event(control, ALTEVT_VIDEO_OFF); /* socket end wins over a late frame */
    close(pair[0]); close(pair[1]);

    pthread_mutex_lock(&test_lock);
    hold_reset = 1; reset_entered = 0; allow_picture = 0;
    pthread_mutex_unlock(&test_lock);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, next_pair) == 0);
    next.fd = next_pair[0]; next.generation = decode_queue_begin(next.fd);
    pthread_mutex_lock(&test_lock);
    while (!reset_entered) pthread_cond_wait(&test_cond, &test_lock);
    ready = 1; /* stale old presentation during new-generation teardown */
    pthread_mutex_unlock(&test_lock);
    expect_silence(control);
    pthread_mutex_lock(&test_lock);
    hold_reset = 0; pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
    assert(pthread_create(&next_reader, NULL, read_thread, &next) == 0);
    assert(write(next_pair[1], record, sizeof(record)) == sizeof(record));
    wait_for_static_reader(4);
    expect_silence(control);
    release_picture(); expect_event(control, ALTEVT_VIDEO_ON);
    decode_queue_end(a.generation); decode_queue_fail(a.generation);
    expect_silence(control); /* old generation cannot turn off the new one */
    shutdown(next_pair[1], SHUT_RDWR); pthread_join(next_reader, NULL);
    expect_event(control, ALTEVT_VIDEO_OFF);
    close(next_pair[0]); close(next_pair[1]);
    altctl_stop(); close(control); close(listener); decode_worker_stop();
    alarm(0);
    puts("altscreen_readiness_test: static first frame, health edges, control reconnect, generation guards and real log truncation PASS");
    return 0;
}
