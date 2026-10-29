/* Real socket I/O: the shipping reader must drain data and remain cancellable. */
#include "../hook/altscreen/receiver_io.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void io_pause_ms(long ms)
{
    struct timespec pause = {ms / 1000, ms % 1000 * 1000000};
    while (nanosleep(&pause, &pause) && errno == EINTR) {}
}

typedef struct { int fd; volatile int *stop; } io_writer_t;
static void *io_fragment_writer(void *arg)
{
    io_writer_t *writer = arg;
    unsigned char bytes[4096];
    size_t at = 0;
    unsigned i;
    for (i = 0; i < sizeof(bytes); ++i) bytes[i] = (unsigned char)i;
    io_pause_ms(10);
    while (at < sizeof(bytes)) {
        size_t size = sizeof(bytes) - at;
        ssize_t sent;
        if (size > 73) size = 73;
        sent = write(writer->fd, bytes + at, size);
        if (sent < 0 && errno == EINTR) continue;
        assert(sent > 0);
        at += (size_t)sent;
        if (at == 73) io_pause_ms(10);
    }
    assert(!shutdown(writer->fd, SHUT_WR));
    return NULL;
}

static void *io_cancel_writer(void *arg)
{
    io_writer_t *writer = arg;
    io_pause_ms(10);
    __atomic_store_n(writer->stop, 1, __ATOMIC_RELEASE);
    return NULL;
}

static int alt_receiver_io_check(void)
{
    int pair[2];
    unsigned char bytes[4096], expected[4096];
    alt_receiver_io_t stats = {0};
    volatile int stop = 0;
    pthread_t writer;
    io_writer_t arg;
    unsigned i;
    struct timespec before, after;
    long elapsed_ms;
    for (i = 0; i < sizeof(expected); ++i) expected[i] = (unsigned char)i;

    /* Both header and body are already buffered, followed by a close. The
     * reader must retain the entire final record and avoid readiness syscalls. */
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    assert(write(pair[1], expected, sizeof(expected)) == sizeof(expected));
    assert(!shutdown(pair[1], SHUT_WR));
    assert(!alt_receiver_read_exact(pair[0], bytes, 128, &stop, &stats));
    assert(!alt_receiver_read_exact(pair[0], bytes + 128, sizeof(bytes) - 128, &stop, &stats));
    assert(!memcmp(bytes, expected, sizeof(bytes)));
    assert(stats.poll_calls == 0 && stats.recv_calls == 2 && stats.bytes == sizeof(bytes));
    assert(alt_receiver_read_exact(pair[0], bytes, 1, &stop, &stats) == -1);
    close(pair[0]); close(pair[1]);

    memset(&stats, 0, sizeof(stats));
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    arg.fd = pair[1]; arg.stop = &stop;
    assert(!pthread_create(&writer, NULL, io_fragment_writer, &arg));
    assert(!alt_receiver_read_exact(pair[0], bytes, sizeof(bytes), &stop, &stats));
    assert(!pthread_join(writer, NULL));
    assert(!memcmp(bytes, expected, sizeof(bytes)));
    assert(stats.bytes == sizeof(bytes) && stats.poll_calls > 0 && stats.would_block > 0);
    printf("receiver fragmented bytes=%llu recv=%llu poll=%llu\n",
           (unsigned long long)stats.bytes, (unsigned long long)stats.recv_calls,
           (unsigned long long)stats.poll_calls);
    close(pair[0]); close(pair[1]);

    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    arg.fd = pair[1]; arg.stop = &stop;
    assert(!pthread_create(&writer, NULL, io_cancel_writer, &arg));
    assert(!clock_gettime(CLOCK_MONOTONIC, &before));
    assert(alt_receiver_read_exact(pair[0], bytes, 1, &stop, NULL) == -1 && errno == ECANCELED);
    assert(!clock_gettime(CLOCK_MONOTONIC, &after));
    assert(!pthread_join(writer, NULL));
    elapsed_ms = (after.tv_sec - before.tv_sec) * 1000 + (after.tv_nsec - before.tv_nsec) / 1000000;
    assert(elapsed_ms < 1000);
    close(pair[0]); close(pair[1]);
    assert(!alt_receiver_read_exact(-1, bytes, 0, NULL, NULL));
    assert(alt_receiver_read_exact(-1, bytes, 1, NULL, NULL) == -1 && errno == EBADF);
    puts("ALT_RECEIVER_IO_PASS (buffered drain, fragmented record, EOF, bounded cancellation)");
    return 0;
}

#ifndef ALT_RECEIVER_IO_TEST_NO_MAIN
int main(void) { alarm(8); return alt_receiver_io_check(); }
#endif
