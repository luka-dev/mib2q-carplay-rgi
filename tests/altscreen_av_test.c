/* Real forwarder, localhost sockets, injected send faults. No QNX decoder. */
#define ENABLE_LOGGING 0
#include <sys/socket.h>
#include <poll.h>
#include <assert.h>
#include <errno.h>
static int send_fault, poll_timeout, send_calls, poll_calls, short_writes;
static ssize_t test_send(int fd, const void *p, size_t n, int flags)
{
    ++send_calls;
    if (send_fault) { errno = send_fault; send_fault = 0; return -1; }
    if (short_writes && n > 3) n = 3;
    return send(fd, p, n, flags);
}
static int test_poll(struct pollfd *p, nfds_t n, int timeout)
{
    ++poll_calls;
    if (poll_timeout) return 0;
    return poll(p, n, timeout);
}
#define send test_send
#define poll test_poll
#include "../hook/altscreen/altscreen_av.c"
#undef send
#undef poll
#include "../hook/altscreen/avcc_config.h"

static void config_parser_tests(void)
{
    const uint8_t avcc[] = {1,100,0,31,255,225,0,2,0x67,0x11,1,0,2,0x68,0x22};
    const uint8_t expected[] = {0,0,0,1,0x67,0x11,0,0,0,1,0x68,0x22};
    uint8_t out[32], broken[sizeof(avcc)];
    uint32_t len, i;
    assert(alt_avcc_config_to_annexb(avcc, sizeof(avcc), out, sizeof(out), &len) == 4);
    assert(len == sizeof(expected) && !memcmp(out, expected, len));
    for (i = 0; i < sizeof(avcc); ++i) {
        assert(!alt_avcc_config_to_annexb(avcc, i, out, sizeof(out), &len));
        assert(!len); /* every truncated config is rejected, including after a complete SPS */
    }
    assert(!alt_avcc_config_to_annexb(avcc, sizeof(avcc), out, 11, &len) && !len);
    memcpy(broken, avcc, sizeof(avcc)); broken[13] = 0;
    broken[12] = 0;
    assert(!alt_avcc_config_to_annexb(broken, sizeof(broken), out, sizeof(out), &len));

    /* A header-only screen-state packet may precede or follow real avcC.
     * State packets must leave the previously parsed bytes untouched. */
    assert(alt_video_config_to_annexb(NULL, 0, out, sizeof(out), &len) == 0 && !len);
    assert(alt_video_config_to_annexb(avcc, sizeof(avcc), out, sizeof(out), &len) == 4);
    assert(alt_video_config_to_annexb(NULL, 0, out, sizeof(out), &len) == 0);
    assert(!len && !memcmp(out, expected, sizeof(expected)));
    assert(alt_video_config_to_annexb(broken, sizeof(broken), out, sizeof(out), &len) == -1);
    assert(!len);
    assert(alt_video_config_to_annexb(avcc, sizeof(avcc), out, sizeof(out), &len) == 4);
}

static void read_all(int fd, uint8_t *p, size_t len)
{
    while (len) {
        ssize_t n = read(fd, p, len);
        assert(n > 0); p += n; len -= (size_t)n;
    }
}
static void record(int fd, const uint8_t *expected, size_t size)
{
    uint32_t len;
    uint8_t bytes[2048];
    read_all(fd, (uint8_t *)&len, 4);
    assert(ntohl(len) == size && size <= sizeof(bytes));
    read_all(fd, bytes, size); assert(!memcmp(bytes, expected, size));
}
static void no_record(int fd)
{
    struct pollfd p = { fd, POLLIN, 0 };
    assert(poll(&p, 1, 0) == 0);
}

int main(void)
{
    int listener, peer, peer2, peer3, calls;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    uint8_t config[800]; /* valid cache sizes beyond the old 512-byte cutoff */
    uint8_t changed_config[800];
    const uint8_t idr[] = {0,0,0,2,6,0x33,0,0,0,2,0x65,0x44};
    const uint8_t idr_annexb[] = {0,0,0,1,6,0x33,0,0,0,1,0x65,0x44};
    const uint8_t pframe[] = {0,0,0,2,0x41,0x55};
    const uint8_t p_annexb[] = {0,0,0,1,0x41,0x55};
    const uint8_t broken[] = {0,0,0,2,0x41,0x55,0,0};
    signal(SIGPIPE, SIG_IGN);
    config_parser_tests();
    memset(config, 0x55, sizeof(config));
    config[0]=config[1]=config[2]=0; config[3]=1; config[4]=0x67;
    listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
    memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(listener, (void *)&addr, sizeof(addr)) && !listen(listener, 4));
    assert(!getsockname(listener, (void *)&addr, &addr_len));
    altav_init("127.0.0.1", ntohs(addr.sin_port)); assert(altav_claim(1));
    assert(altav_forward_avcc_frame(1, idr, sizeof(idr), 4) == -1); /* config missing */
    altav_set_config(1, config, sizeof(config));
    assert(altav_forward_avcc_frame(1, pframe, sizeof(pframe), 4) == -1); no_record(listener);
    short_writes = 1;
    assert(altav_forward_avcc_frame(1, idr, sizeof(idr), 4) == 1);
    assert((fcntl(g_fd, F_GETFL, 0) & O_NONBLOCK) != 0);
    peer = accept(listener, NULL, NULL); assert(peer >= 0);
    record(peer, config, sizeof(config)); record(peer, idr_annexb, sizeof(idr_annexb));
    short_writes = 0;
    calls = send_calls;
    assert(altav_forward_avcc_frame(1, broken, sizeof(broken), 4) == -1);
    assert(send_calls == calls); no_record(peer); /* reject the entire malformed access unit */
    send_fault = EAGAIN; calls = poll_calls;
    assert(altav_forward_avcc_frame(1, pframe, sizeof(pframe), 4) == 0);
    assert(poll_calls == calls + 1); record(peer, p_annexb, sizeof(p_annexb));
    calls = send_calls;
    altav_set_config(1, config, sizeof(config));
    assert(g_fd >= 0 && send_calls == calls); no_record(peer);
    send_fault = EPIPE;
    assert(altav_forward_avcc_frame(1, pframe, sizeof(pframe), 4) == -1);
    assert(g_fd == -1);
    assert(altav_forward_avcc_frame(1, pframe, sizeof(pframe), 4) == -1); no_record(listener);
    assert(altav_forward_avcc_frame(1, idr, sizeof(idr), 4) == 1);
    peer2 = accept(listener, NULL, NULL); assert(peer2 >= 0); close(peer);
    record(peer2, config, sizeof(config)); record(peer2, idr_annexb, sizeof(idr_annexb));
    memcpy(changed_config, config, sizeof(config)); changed_config[5] ^= 0x11;
    altav_set_config(1, changed_config, sizeof(changed_config)); assert(g_fd == -1);
    assert(altav_forward_avcc_frame(1, pframe, sizeof(pframe), 4) == -1); no_record(listener);
    assert(altav_forward_avcc_frame(1, idr, sizeof(idr), 4) == 1);
    peer3 = accept(listener, NULL, NULL); assert(peer3 >= 0); close(peer2);
    record(peer3, changed_config, sizeof(changed_config));
    record(peer3, idr_annexb, sizeof(idr_annexb));
    send_fault = EAGAIN; poll_timeout = 1; calls = send_calls;
    assert(altav_forward_avcc_frame(1, pframe, sizeof(pframe), 4) == -1);
    assert(send_calls == calls + 1 && g_fd == -1); poll_timeout = 0;
    close(peer3);
    altav_set_enabled(0); assert(!altav_is_enabled());
    assert(altav_forward_avcc_frame(1, idr, sizeof(idr), 4) == -1);
    altav_set_enabled(1); assert(altav_is_enabled());
    assert(altav_forward_avcc_frame(1, pframe, sizeof(pframe), 4) == -1);
    assert(altav_claim(2)); altav_set_config(1, config, sizeof(config));
    assert(altav_forward_avcc_frame(2, idr, sizeof(idr), 4) == -1);
    altav_shutdown(); close(listener);
    puts("altscreen_av_test: config+IDR, reconnect, partial writes, EAGAIN wait, timeout, stale owner and malformed frames PASS");
    return 0;
}
