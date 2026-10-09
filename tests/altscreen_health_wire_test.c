/* Actual idle control parser: partial records, bounded drain and bad owners. */
#define ENABLE_LOGGING 0
#include "../hook/altscreen/altscreen_av.c"
#include <assert.h>

static int install_peer(uint32_t gen) {
    int fds[2], flags;
    assert(altav_claim(gen));
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    flags = fcntl(fds[0], F_GETFL, 0);
    assert(flags >= 0 && !fcntl(fds[0], F_SETFL, flags | O_NONBLOCK));
    g_transport.fd = fds[0];
    g_transport.io_epoch = g_forward.epoch;
    g_transport.status_supported = 1;
    return fds[1];
}
static void tick(void) { av_idle_tick(g_forward.generation, g_forward.epoch); }
int main(void) {
    uint8_t status[ALT_WIRE_STATUS_SIZE], hello[ALT_WIRE_HELLO_SIZE];
    int peer = install_peer(1);
    alt_wire_put32(hello, ALT_WIRE_HELLO);
    alt_wire_put32(hello + 4, ALT_HELLO_STATUS_CAPABILITY);
    for (unsigned i = 0; i < sizeof(hello); ++i) {
        assert(write(peer, hello + i, 1) == 1);
        tick();
        assert(g_transport.fd >= 0);
        assert(g_transport.control.used == (i + 1 == sizeof(hello) ? 0 : i + 1));
    }
    alt_wire_put32(status, ALT_WIRE_STATUS);
    alt_wire_put32(status + 4, 1);
    alt_wire_put32(status + 8, 1);
    for (unsigned i = 0; i < sizeof(status); ++i) {
        assert(write(peer, status + i, 1) == 1);
        tick();
        assert(g_transport.fd >= 0);
        if (i + 1 < sizeof(status))
            assert(!g_forward.recovery.health_seen);
    }
    assert(g_forward.recovery.health_seen && g_forward.recovery.health_ready);
    for (unsigned i = 0; i < 9; ++i)
        assert(write(peer, status, sizeof(status)) == sizeof(status));
    tick(); /* At most eight records per observation. */
    assert(g_transport.fd >= 0);
    {
        uint8_t byte;
        assert(recv(g_transport.fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT) == 1);
    }
    tick();
    {
        uint8_t byte;
        assert(recv(g_transport.fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT) < 0 && errno == EAGAIN);
    }
    tick(); /* Static scene and silent peer are healthy. */
    assert(g_transport.fd >= 0 && !altav_take_failure(1));
    alt_wire_put32(status + 4, 2); /* Status from a different generation. */
    assert(write(peer, status, sizeof(status)) == sizeof(status));
    tick();
    assert(g_transport.fd < 0 && altav_take_failure(1) && g_forward.need_idr);
    close(peer);

    peer = install_peer(2);
    assert(write(peer, status, 5) == 5);
    tick();
    assert(g_transport.fd >= 0 && g_transport.control.used == 5);
    close(peer); /* EOF inside a fragmented status is also transport loss. */
    tick();
    assert(g_transport.fd < 0 && altav_take_failure(2));

    peer = install_peer(3);
    alt_wire_put32(status, 0x12345678);
    assert(write(peer, status, 4) == 4);
    tick();
    assert(g_transport.fd < 0 && altav_take_failure(3));
    close(peer);
    altav_shutdown();
    puts("altscreen_health_wire_test: fragmented HELLO/status, bounded drain, silent scene, stale "
         "owner, malformed record and partial EOF PASS");
    return 0;
}
