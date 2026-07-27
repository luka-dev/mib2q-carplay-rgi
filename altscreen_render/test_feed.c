/*
 * altscreen_render — framing self-test (host build).
 * Exercises av_read_frame over a socketpair: happy path (incl. empty keep-alive),
 * EOF, and the oversize-length protocol error.  Build+run: `make test && ./test`.
 */
#include "frame_io.h"
#include "protocol.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static void write_u32be(int fd, uint32_t v) {
    uint8_t h[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    assert(write(fd, h, 4) == 4);
}
static void write_frame(int fd, const uint8_t *p, uint32_t len) {
    write_u32be(fd, len);
    if (len) assert(write(fd, p, len) == (ssize_t)len);
}

int main(void) {
    const uint8_t a[] = { 0,0,0,1, 0x67, 0x42, 0x00, 0x1e };   /* fake SPS */
    const uint8_t b[] = { 0,0,0,1, 0x65, 0x88, 0x84, 0x21 };   /* fake IDR */

    /* --- happy path: a, empty keep-alive, b, then EOF --- */
    {
        int sp[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
        write_frame(sp[1], a, sizeof a);
        write_frame(sp[1], NULL, 0);           /* keep-alive */
        write_frame(sp[1], b, sizeof b);
        close(sp[1]);                          /* signal EOF after the frames */

        uint8_t *buf = NULL; uint32_t cap = 0, len = 0;
        assert(av_read_frame(sp[0], &buf, &cap, &len) == 0);
        assert(len == sizeof a && memcmp(buf, a, len) == 0);

        assert(av_read_frame(sp[0], &buf, &cap, &len) == 0);
        assert(len == 0);                      /* empty keep-alive */

        assert(av_read_frame(sp[0], &buf, &cap, &len) == 0);
        assert(len == sizeof b && memcmp(buf, b, len) == 0);

        assert(av_read_frame(sp[0], &buf, &cap, &len) == -1);  /* EOF */
        assert(cap >= sizeof a);               /* buffer was reused, not reallocated per call */
        free(buf);
        close(sp[0]);
    }

    /* --- oversize length → protocol error (-2), header only --- */
    {
        int sp[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
        write_u32be(sp[1], ALTAV_MAX_MSG + 1);
        close(sp[1]);

        uint8_t *buf = NULL; uint32_t cap = 0, len = 0;
        assert(av_read_frame(sp[0], &buf, &cap, &len) == -2);
        free(buf);
        close(sp[0]);
    }

    printf("altscreen_render framing self-test: PASS\n");
    return 0;
}
