/* Actual greeting negotiation with fragmented reads and deterministic faults.
 * A fake monotonic clock advances timeout waits, never the host clock. */
#define ENABLE_LOGGING 0
#include <assert.h>
#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>
#include <time.h>
static uint64_t fake_us;
static uint32_t read_step_us, poll_ready_step_us;
static unsigned fragment_limit, poll_interrupts, recv_interrupts, recv_again;
static int negotiated_clock(clockid_t kind, struct timespec *out) {
    if (kind == CLOCK_MONOTONIC && fake_us) {
        out->tv_sec = (time_t)(fake_us / 1000000u);
        out->tv_nsec = (long)(fake_us % 1000000u) * 1000L;
        return 0;
    }
    return clock_gettime(kind, out);
}
#include <errno.h>
static int negotiated_poll(struct pollfd *fds, nfds_t count, int timeout) {
    if (poll_interrupts) {
        --poll_interrupts;
        ++fake_us;
        errno = EINTR;
        return -1;
    }
    int ready = poll(fds, count, fake_us ? 0 : timeout);
    if (ready > 0 && fake_us)
        fake_us += poll_ready_step_us;
    if (!ready && fake_us && timeout > 0)
        fake_us += (uint64_t)timeout * 1000u;
    return ready;
}
static ssize_t negotiated_recv(int fd, void *data, size_t len, int flags) {
    if (recv_interrupts) {
        --recv_interrupts;
        ++fake_us;
        errno = EINTR;
        return -1;
    }
    if (recv_again) {
        --recv_again;
        ++fake_us;
        errno = EAGAIN;
        return -1;
    }
    if (fragment_limit && len > fragment_limit)
        len = fragment_limit;
    ssize_t got = recv(fd, data, len, flags);
    if (got > 0)
        fake_us += read_step_us;
    return got;
}
#define clock_gettime negotiated_clock
#define poll negotiated_poll
#define recv negotiated_recv
#include "../hook/altscreen/altscreen_av.c"
#undef recv
#undef poll
#undef clock_gettime

static int install_peer(uint32_t gen, uint32_t capability, unsigned bytes, int *peer) {
    int fd[2];
    uint8_t hello[ALT_WIRE_HELLO_SIZE];
    assert(altav_claim(gen));
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    int flags = fcntl(fd[0], F_GETFL, 0);
    assert(flags >= 0 && !fcntl(fd[0], F_SETFL, flags | O_NONBLOCK));
    alt_wire_put32(hello, ALT_WIRE_HELLO);
    alt_wire_put32(hello + 4, capability);
    if (bytes)
        assert(write(fd[1], hello, bytes) == (ssize_t)bytes);
    *peer = fd[1];
    fake_us = 1000000;
    fragment_limit = 1;
    read_step_us = 1000;
    poll_ready_step_us = 0;
    return fd[0];
}
int main(void) {
    int fd, peer;
    fd = install_peer(1, ALT_HELLO_STATUS_CAPABILITY, 8, &peer);
    poll_interrupts = 2;
    recv_interrupts = 1;
    recv_again = 1;
    assert(av_negotiate_locked(fd, fake_us + 1000000u) == 1);
    assert(g_transport.wire_v2 && g_transport.status_supported && g_transport.ack_pending && !g_transport.control.used);
    assert(fake_us < 1250000u);
    close(fd);
    close(peer);
    fd = install_peer(2, ALT_WIRE_VERSION, 8, &peer);
    assert(av_negotiate_locked(fd, fake_us + 1000000u) == 1);
    assert(g_transport.wire_v2 && !g_transport.status_supported && !g_transport.ack_pending);
    close(fd);
    close(peer);
    fd = install_peer(3, ALT_HELLO_STATUS_CAPABILITY, 0, &peer);
    assert(!av_negotiate_locked(fd, fake_us + 1000000u));
    assert(fake_us == 1250000u && !g_transport.wire_v2 && !g_transport.control.used);
    close(fd);
    close(peer);

    fd = install_peer(4, ALT_HELLO_STATUS_CAPABILITY, 1, &peer);
    assert(!av_negotiate_locked(fd, fake_us + 1000000u));
    assert(!g_transport.wire_v2 && g_transport.control.used == 1 && fake_us <= 1251000u);
    uint8_t hello[8];
    alt_wire_put32(hello, ALT_WIRE_HELLO);
    alt_wire_put32(hello + 4, ALT_HELLO_STATUS_CAPABILITY);
    assert(write(peer, hello + 1, 7) == 7);
    g_transport.fd = fd;
    g_transport.io_epoch = g_forward.epoch;
    read_step_us = 0;
    av_idle_tick(4, g_forward.epoch);
    assert(g_transport.fd >= 0 && !g_transport.control.used && !g_transport.wire_v2 && !g_transport.status_supported);
    /* A STATUS after the late greeting remains properly aligned. */
    uint8_t status[12];
    alt_wire_put32(status, ALT_WIRE_STATUS);
    alt_wire_put32(status + 4, 4);
    alt_wire_put32(status + 8, 1);
    assert(write(peer, status, sizeof(status)) == sizeof(status));
    av_idle_tick(4, g_forward.epoch);
    assert(g_transport.fd < 0 && !g_forward.recovery.health_seen); /* No negotiated channel may assert health. */
    close(peer);
    av_transport_reset(4);

    fd = install_peer(5, ALT_HELLO_STATUS_CAPABILITY, 8, &peer);
    read_step_us = 3000;
    assert(!av_negotiate_locked(fd, fake_us + 5000u));
    assert(g_transport.control.used == 2 && fake_us == 1006000u);
    close(fd);
    close(peer);
    fd = install_peer(6, ALT_HELLO_STATUS_CAPABILITY, 1, &peer);
    close(peer);
    assert(av_negotiate_locked(fd, fake_us + 1000000u) < 0);
    close(fd);
    fd = install_peer(7, 99, 8, &peer);
    assert(av_negotiate_locked(fd, fake_us + 1000000u) < 0);
    close(fd);
    close(peer);
    fd = install_peer(8, ALT_HELLO_STATUS_CAPABILITY, 8, &peer);
    poll_ready_step_us = 300000; /* a sender descheduled 300 ms before its first byte misses the 250 ms greeting window */
    fragment_limit = 8;
    assert(!av_negotiate_locked(fd, fake_us + 1000000u));
    assert(!g_transport.wire_v2 && !g_transport.status_supported && !g_transport.ack_pending && !g_transport.control.used);
    close(fd);
    close(peer);
    fd = install_peer(9, ALT_HELLO_STATUS_CAPABILITY, 8, &peer);
    fragment_limit = 8;
    read_step_us = 300000; /* Descheduled during the final read, not only during poll. */
    assert(!av_negotiate_locked(fd, fake_us + 1000000u));
    assert(!g_transport.wire_v2 && !g_transport.status_supported && !g_transport.ack_pending && !g_transport.control.used);
    g_transport.fd = fd;
    g_transport.io_epoch = g_forward.epoch;
    read_step_us = 0;
    av_idle_tick(9, g_forward.epoch);
    assert(g_transport.fd >= 0); /* Fully consumed late HELLO must not look like a zero-byte EOF. */
    close(peer);
    av_transport_reset(9);
    fake_us = 0;
    altav_shutdown();
    puts("altscreen_negotiation_test: byte-fragmented HELLO, EINTR/EAGAIN, v2 without status, "
         "silent legacy, bounded partial fallback, late alignment, aggregate budget and EOF PASS");
    return 0;
}
