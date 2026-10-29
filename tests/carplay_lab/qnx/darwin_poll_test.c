/* Native FD semantics for the isolated Darwin QEMU timer experiment. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "darwin_poll.h"

static uint64_t stamp(void)
{
    struct timespec now;
    assert(!clock_gettime(CLOCK_MONOTONIC, &now));
    return (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}

static void *delayed_write(void *argument)
{
    int fd = *(int *)argument;
    uint64_t end = stamp() + 200000;
    while (stamp() < end) {}
    assert(write(fd, "x", 1) == 1);
    return NULL;
}

static int descriptors(void)
{
    int count = 0, fd;
    for (fd = 0; fd < 1024; fd++) {
        if (fcntl(fd, F_GETFD) != -1) count++;
    }
    return count;
}

int main(void)
{
    int sockets[2], before, i;
    char byte;
    GPollFD fds[65];
    pthread_t writer;
    uint64_t total = 0;
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    fds[0] = (GPollFD){sockets[0], POLLIN, 0};
    before = descriptors();
    for (i = 0; i < 100; i++) {
        uint64_t start = stamp();
        assert(fps_darwin_poll_ns(fds, 1, 125000) == 0);
        total += stamp() - start;
    }
    assert(descriptors() == before);
    assert(!pthread_create(&writer, NULL, delayed_write, &sockets[1]));
    for (i = 0; i < 100 && !fps_darwin_poll_ns(fds, 1, 1000000); i++) {}
    assert(i < 100 && (fds[0].revents & POLLIN));
    assert(read(sockets[0], &byte, 1) == 1 && byte == 'x');
    assert(!pthread_join(writer, NULL));
    fds[0].events = POLLOUT;
    assert(fps_darwin_poll_ns(fds, 1, 125000) == 1 && (fds[0].revents & POLLOUT));
    fds[0].events = POLLIN;
    assert(!fps_darwin_poll_ns(fds, 1, 0));
    assert(write(sockets[1], "x", 1) == 1);
    assert(fps_darwin_poll_ns(fds, 1, -1) == 1 && (fds[0].revents & POLLIN));
    assert(read(sockets[0], &byte, 1) == 1);
    for (i = 1; i < 65; i++) fds[i] = (GPollFD){-1, POLLIN, 0};
    assert(!fps_darwin_poll_ns(fds, 65, 100000));
    close(sockets[1]);
    assert(fps_darwin_poll_ns(fds, 1, 125000) == 1);
    assert(fds[0].revents & (POLLIN | POLLHUP));
    close(sockets[0]);
    assert(fps_darwin_poll_ns(fds, 1, 125000) == 1 && (fds[0].revents & POLLNVAL));
    assert(!fps_darwin_poll_ns(NULL, 0, 125000));
    printf("Darwin wait FD tests passed; mean 125-us empty wait: %.3f ms\n", total / 100.0 / 1000000.0);
    return 0;
}
