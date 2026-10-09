/* Advance the fake monotonic clock on successful partial writes. The budget
 * must not restart between config/header/body fragments. No wall-clock sleeps. */
#define ENABLE_LOGGING 0
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>
static uint64_t fake_us;
static int test_clock(clockid_t c, struct timespec *t) {
    if (c == CLOCK_MONOTONIC && fake_us) {
        t->tv_sec = (time_t)(fake_us / 1000000);
        t->tv_nsec = (long)(fake_us % 1000000) * 1000;
        return 0;
    }
    return clock_gettime(c, t);
}
static ssize_t advancing_writev(int fd, const struct iovec *v, int parts) {
    struct iovec limited[2];
    size_t budget = 3;
    int count = 0;
    for (int i = 0; i < parts && budget; ++i) {
        limited[count] = v[i];
        if (limited[count].iov_len > budget)
            limited[count].iov_len = budget;
        budget -= limited[count].iov_len;
        ++count;
    }
    ssize_t sent = writev(fd, limited, count);
    if (fake_us && sent > 0)
        fake_us += 450000;
    return sent;
}
#define clock_gettime test_clock
#define writev advancing_writev
#include "../hook/altscreen/altscreen_av.c"
#undef writev
#undef clock_gettime
int main(void) {
    int pair[2];
    uint8_t config[] = {0, 0, 0, 1, 0x67}, idr[] = {0, 0, 0, 1, 0x65};
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    assert(altav_claim(1));
    av_set_config(1, config, sizeof(config));
    g_transport.fd = pair[0];
    g_transport.need_config = 1;
    fake_us = 1000000;
    assert(av_send_locked(idr, sizeof(idr), NULL) == -1);
    assert(g_transport.fd == -1 && fake_us == 2350000); /* config consumed budget; frame never starts */
    close(pair[1]);
    fake_us = 0;
    altav_shutdown();
    puts("altscreen_deadline_test: one aggregate deadline across config + frame PASS");
    return 0;
}
