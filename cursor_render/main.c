/* Independent HU cursor/PRISM service. Own managed EGL surface; no DIO or OMX. */
#include "session.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static volatile sig_atomic_t running = 1;
static pthread_mutex_t progress_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t progress;
static int watchdog_done;
static uint64_t now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void mark_progress(void) {
    pthread_mutex_lock(&progress_lock); progress = now_ms(); pthread_mutex_unlock(&progress_lock);
}
static void *watchdog(void *unused) {
    (void)unused;
    for (;;) {
        int done; uint64_t last;
        struct timespec pause = {0, 100000000}; nanosleep(&pause, NULL);
        pthread_mutex_lock(&progress_lock); done = watchdog_done; last = progress;
        pthread_mutex_unlock(&progress_lock);
        if (done) return NULL;
        if (now_ms() - last > 1500) {
            const char msg[] = "cursor_render: Screen progress watchdog; exiting isolated process\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            _exit(70); // QNX reclaims this process's windows; supervisor can restart it.
        }
    }
}
static void stop_signal(int signo) { (void)signo; running = 0; }
static int nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
static int listener(unsigned port) {
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    if (fd < 0) return -1;
    memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); addr.sin_port = htons(port);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (nonblock(fd) || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) || listen(fd, 2)) {
        close(fd); return -1;
    }
    return fd;
}
static void drop(int *fd, cursor_session *s) {
    if (*fd >= 0) close(*fd);
    *fd = -1; cursor_session_reset(s);
}
int main(int argc, char **argv) {
    cursor_session s;
    pthread_t guard;
    int lfd, peer = -1, rx_used = 0, tx_used = 0, tx_sent = 0;
    uint8_t rx[CURSOR_FRAME_SIZE], tx[CURSOR_ACK_SIZE];
    uint64_t partial_since = 0;
    unsigned port = CURSOR_PORT;
    if (argc == 3 && !strcmp(argv[1], "--port")) {
        char *end; unsigned long value = strtoul(argv[2], &end, 10);
        if (*end || value < 1024 || value > 65535) return 2;
        port = value;
    } else if (argc != 1) { fprintf(stderr, "usage: cursor_render [--port N]\n"); return 2; }
    signal(SIGPIPE, SIG_IGN); signal(SIGINT, stop_signal); signal(SIGTERM, stop_signal);
    signal(SIGHUP, stop_signal);
    mark_progress();
    if (pthread_create(&guard, NULL, watchdog, NULL) != 0) return 1;
    cursor_session_init(&s, (uint32_t)now_ms() ^ (uint32_t)getpid());
    lfd = listener(port);
    if (lfd < 0) { perror("cursor_render: listen"); return 1; }
    fprintf(stderr, "cursor_render: listening on 127.0.0.1:%u, managed displayable 103\n", port);
    while (running) {
        fd_set reads, writes; struct timeval timeout = {0, 25000};
        int highest = lfd, ready;
        uint64_t tick_now = now_ms();
        if (s.engine.initialized && s.next_draw > tick_now && s.next_draw - tick_now < 25)
            timeout.tv_usec = (s.next_draw - tick_now) * 1000;
        mark_progress();
        FD_ZERO(&reads); FD_ZERO(&writes); FD_SET(lfd, &reads);
        if (peer >= 0) {
            if (tx_used) FD_SET(peer, &writes); else FD_SET(peer, &reads);
            if (peer > highest) highest = peer;
        }
        ready = select(highest + 1, &reads, &writes, NULL, &timeout);
        if (ready < 0) { if (errno == EINTR) continue; break; }
        if (FD_ISSET(lfd, &reads)) {
            int next = accept(lfd, NULL, NULL), one = 1;
            if (next >= 0) {
                if (next >= FD_SETSIZE || nonblock(next) != 0) close(next);
                else {
                    setsockopt(next, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                    drop(&peer, &s); peer = next;
                    rx_used = tx_used = tx_sent = 0; partial_since = 0;
                    continue; // Never apply readiness bits from the replaced descriptor.
                }
            }
        }
        if (peer >= 0 && FD_ISSET(peer, &writes) && tx_used) {
            int n = send(peer, tx + tx_sent, tx_used - tx_sent, 0);
            if (n > 0) { tx_sent += n; if (tx_sent == tx_used) tx_used = tx_sent = 0; }
            else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) drop(&peer, &s);
        }
        if (peer >= 0 && FD_ISSET(peer, &reads)) {
            int n = recv(peer, rx + rx_used, sizeof(rx) - rx_used, 0);
            if (n > 0) {
                if (!rx_used) partial_since = now_ms();
                rx_used += n;
                if (rx_used == CURSOR_FRAME_SIZE) {
                    int result = cursor_session_receive(&s, rx, sizeof(rx), now_ms(), tx);
                    rx_used = 0; partial_since = 0;
                    if (result < 0) drop(&peer, &s);
                    else if (result > 0) { tx_used = sizeof(tx); tx_sent = 0; }
                }
            } else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) drop(&peer, &s);
        }
        if (peer >= 0 && partial_since && now_ms() - partial_since >= CURSOR_LEASE_MS) drop(&peer, &s);
        if (peer >= 0 && cursor_session_tick(&s, now_ms(), tx)) { tx_used = sizeof(tx); tx_sent = 0; }
    }
    close(lfd); drop(&peer, &s);
    pthread_mutex_lock(&progress_lock); watchdog_done = 1; pthread_mutex_unlock(&progress_lock);
    pthread_join(guard, NULL);
    return 0;
}
