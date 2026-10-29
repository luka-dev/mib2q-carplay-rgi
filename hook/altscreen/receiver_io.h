/* Drain available stream bytes before waiting; never block on a readiness race. */
#ifndef ALT_RECEIVER_IO_H
#define ALT_RECEIVER_IO_H

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>

typedef struct {
    uint64_t recv_calls, poll_calls, would_block, bytes;
} alt_receiver_io_t;

static inline int alt_receiver_read_exact(int fd, uint8_t *buf, uint32_t size,
                                          volatile int *stop, alt_receiver_io_t *stats)
{
    uint32_t got = 0;
    int terminal = 0;
    while (got < size) {
        ssize_t count;
        struct pollfd pfd;
        int ready;
        if (stop && __atomic_load_n(stop, __ATOMIC_ACQUIRE)) { errno = ECANCELED; return -1; }
        if (stats) ++stats->recv_calls;
        count = recv(fd, buf + got, size - got, MSG_DONTWAIT);
        if (count > 0) {
            got += (uint32_t)count;
            if (stats) stats->bytes += (uint32_t)count;
            continue;
        }
        if (!count) { errno = ECONNRESET; return -1; }
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        if (stats) ++stats->would_block;
        if (terminal) { errno = ECONNRESET; return -1; }
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (stats) ++stats->poll_calls;
        ready = poll(&pfd, 1, 60);
        if (ready < 0 && errno != EINTR) return -1;
        if (ready <= 0) continue;
        if (pfd.revents & POLLNVAL) { errno = EBADF; return -1; }
        /* A close may arrive with the final buffered bytes. Read them before
         * reporting EOF rather than dropping the final complete access unit. */
        terminal = (pfd.revents & (POLLHUP | POLLERR)) != 0;
    }
    return 0;
}

#endif
