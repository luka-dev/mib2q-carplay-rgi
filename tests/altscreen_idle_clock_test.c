/* A backward wall-clock adjustment is modeled by giving the old realtime
 * deadline builder a future clock while kernel waits retain the real clock.
 * No system clock is changed; idle EOF must still be observed promptly. */
#define ENABLE_LOGGING 0
#include <assert.h>
#include <time.h>
static unsigned realtime_reads;
static int shifted_clock(clockid_t kind, struct timespec *out) {
    int result = clock_gettime(kind, out);
    if (!result && kind == CLOCK_REALTIME) {
        __sync_add_and_fetch(&realtime_reads, 1);
        out->tv_sec += 3600;
    }
    return result;
}
#define clock_gettime shifted_clock
#include "../hook/altscreen/altscreen_av.c"
#undef clock_gettime
static unsigned callbacks;
static void failed(uint32_t gen, void *unused) {
    (void)unused;
    assert(gen == 1);
    __sync_add_and_fetch(&callbacks, 1);
}
int main(void) {
    int fd[2];
    assert(altav_claim(1));
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    int flags = fcntl(fd[0], F_GETFL, 0);
    assert(flags >= 0 && !fcntl(fd[0], F_SETFL, flags | O_NONBLOCK));
    g_transport.fd = fd[0];
    g_transport.io_epoch = g_forward.epoch;
    close(fd[1]);
    assert(!altav_async_start(failed, NULL));
    uint64_t start = alt_timing_now_us();
    while (!__sync_add_and_fetch(&callbacks, 0) && alt_timing_now_us() - start < 600000u) {
        struct timespec pause = {0, 10000000L};
        nanosleep(&pause, NULL);
    }
    unsigned observed = __sync_add_and_fetch(&callbacks, 0);
    altav_shutdown();
    assert(observed == 1 && !__sync_add_and_fetch(&realtime_reads, 0));
    puts("altscreen_idle_clock_test: modeled backward wall-clock step cannot delay idle EOF; "
         "bounded recovery and stop PASS");
    return 0;
}
