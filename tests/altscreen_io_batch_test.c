#define ENABLE_LOGGING 0
#include <assert.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
static unsigned writes, reads;
static ssize_t counted_writev(int f, const struct iovec *v, int n) {
    ++writes;
    return writev(f, v, n);
}
static ssize_t counted_read(int f, void *p, size_t n) {
    ++reads;
    return read(f, p, n);
}
#define writev counted_writev
#include "../hook/altscreen/altscreen_av.c"
#undef writev
#define read counted_read
#include "../altscreen_render/frame_io.c"
#undef read
int main(void) {
    int fd[2];
    uint8_t payload[1000], *buf = NULL;
    uint32_t cap = 0, len;
    alt_frame_meta_t meta = {0}, out;
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    memset(payload, 0x55, sizeof(payload));
    meta.generation = 1;
    meta.sequence = 1;
    meta.version = 2;
    meta.flags = ALT_WIRE_IDR;
    g_transport.fd = fd[0];
    g_transport.wire_v2 = 1;
    assert(!av_send_record_locked(payload, sizeof(payload), &meta, alt_timing_now_us() + 1000000));
    assert(!av_read_frame_mode(fd[1], &buf, &cap, &len, &out, 2));
    assert(writes == 1 && reads == 2 && len == sizeof(payload) && !memcmp(buf, payload, len));
    assert(out.sequence == 1);
    free(buf);
    close(fd[0]);
    close(fd[1]);
    g_transport.fd = -1;
    puts("altscreen_io_batch_test: one vector write + two known-v2 reads, exact byte stream PASS");
    return 0;
}
