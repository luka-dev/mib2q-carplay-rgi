/* Isolated QNX/virtual USB throughput measurement. No CarPlay or HU state. */
#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + ts.tv_nsec;
}

static int read_all(int fd, void *target, size_t size) {
    unsigned char *p = target;
    while (size) {
        ssize_t n = recv(fd, p, size, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        size -= (size_t)n;
    }
    return 0;
}

static void *receive_payload(void *argument) {
    int fd = *(int *)argument, actual = 0, policy = 0;
    uint32_t request[2];
    uint64_t response[4], received = 0, started;
    unsigned char *buffer = malloc(65536);
    struct sched_param scheduling;
    socklen_t length = sizeof(actual);
    struct timeval timeout;
    free(argument);
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 10;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (!buffer || read_all(fd, request, sizeof(request)) ||
        ntohl(request[0]) != UINT32_C(0x46505330) ||
        !ntohl(request[1]) || ntohl(request[1]) > 4u*1024u*1024u) goto done;
    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &actual, &length);
    memset(&scheduling, 0, sizeof(scheduling));
    pthread_getschedparam(pthread_self(), &policy, &scheduling);
    started = now_ns();
    while (received < ntohl(request[1])) {
        size_t wanted = ntohl(request[1]) - received;
        ssize_t n;
        if (wanted > 65536) wanted = 65536;
        n = recv(fd, buffer, wanted, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) goto done;
        received += (size_t)n;
    }
    /* QNX target is little endian ARM32; host decodes these explicit fields. */
    response[0] = received;
    response[1] = now_ns() - started;
    response[2] = (uint32_t)actual;
    response[3] = (uint32_t)scheduling.sched_priority;
    {
        size_t at = 0;
        while (at < sizeof(response)) {
            ssize_t n = send(fd, (unsigned char *)response + at, sizeof(response)-at, 0);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) goto done;
            at += (size_t)n;
        }
    }
    printf("fps-usb bytes=%llu elapsed_ns=%llu rcvbuf=%d priority=%d\n",
           (unsigned long long)response[0], (unsigned long long)response[1],
           actual, scheduling.sched_priority);
    fflush(stdout);
done:
    free(buffer);
    close(fd);
    return NULL;
}

int main(void) {
    struct sockaddr_in address;
    int listener = socket(AF_INET, SOCK_STREAM, 0), one = 1, buffer_size = 262144;
    unsigned accepted;
    if (listener < 0) return 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    {
        int result = setsockopt(listener, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
        int error = result ? errno : 0;
        printf("fps-usb receive-buffer requested=%d result=%d errno=%d\n", buffer_size, result, error);
        fflush(stdout);
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(33550);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) || listen(listener, 4)) return 2;
    puts("fps-usb ready port=33550");
    fflush(stdout);
    /* Bounded diagnostic lifetime; the host also owns and bounds its VM. */
    for (accepted = 0; accepted < 8; ++accepted) {
        int *fd = malloc(sizeof(*fd));
        pthread_t thread;
        if (!fd) return 3;
        *fd = accept(listener, NULL, NULL);
        if (*fd < 0) { free(fd); return 4; }
        setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        if (pthread_create(&thread, NULL, receive_payload, fd)) {
            close(*fd); free(fd); return 5;
        }
        pthread_detach(thread);
    }
    close(listener);
    sleep(30);
    return 0;
}
