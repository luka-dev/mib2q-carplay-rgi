/*
 * altctl — control channel to Java/lsd.  See altctl.h.  QNX 6.5 ARMv7 gcc 4.4: no __thread.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "log_stamp.h"
#include "altctl.h"
#include "protocol.h"
#include "sink.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static int (*g_presentation_state)(void);   /* installed before thread creation */
static volatile int g_run = 0;
static volatile int g_exited = 1;
static pthread_t     g_thr;

static int ctl_connect(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int flags;
    int connected = 0;
    if (fd < 0) return -1;
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_port        = htons((uint16_t)ALTCTL_PORT);
    a.sin_addr.s_addr = inet_addr(ALTCTL_HOST);
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0) {
        connected = 1;
    } else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
        struct pollfd pfd;
        int socket_error = 0;
        socklen_t error_len = sizeof(socket_error);
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        if (poll(&pfd, 1, 50) > 0 && (pfd.revents & POLLOUT) &&
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) == 0 &&
            socket_error == 0)
            connected = 1;
    }
    if (!connected || fcntl(fd, F_SETFL, flags) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void *ctl_loop(void *arg) {
    (void)arg;
    int fd = -1;
    int last_sent = -1;
    int ever_connected = 0;
    unsigned int epoch = 0;
    unsigned char rx[ALTCTL_PKT_SIZE];
    unsigned int rx_used = 0;
    while (g_run) {
        /* Sample/publish hook health even while the Java control peer is absent. */
        int state = g_presentation_state ? g_presentation_state() : ALT_PRESENT_OFF;
        if (fd < 0) {
            fd = ctl_connect();
            last_sent = -1;                       /* force resend of the current state on (re)connect */
            rx_used = 0;
            epoch = 0;
            if (fd < 0) {                          /* Java control server not up yet — retry */
                struct timespec retry = { 0, (ever_connected ? 200L : 50L) * 1000000L };
                nanosleep(&retry, NULL);
                continue;
            }
            ever_connected = 1;
            fprintf(stderr, "%s altscreen_render: control link to lsd up (:%d)\n", log_stamp(), ALTCTL_PORT);
        }

        /* Decode/presentation can complete after the last socket record. Never
         * require another compressed frame to notice ON/OFF or replay readiness. */
        if (state != last_sent) {
            unsigned char pkt[ALTCTL_PKT_SIZE];
            memset(pkt, 0, sizeof(pkt));
            pkt[0] = (unsigned char)(state == ALT_PRESENT_VIDEO ? ALTEVT_VIDEO_ON
                : (state == ALT_PRESENT_LOADER ? ALTEVT_LOADER_READY : ALTEVT_VIDEO_OFF));
            pkt[1] = (unsigned char)(epoch >> 16);
            pkt[2] = (unsigned char)(epoch >> 8);
            pkt[3] = (unsigned char)epoch;
            ssize_t k = send(fd, pkt, sizeof(pkt), 0);
            if (k != (ssize_t)sizeof(pkt)) { close(fd); fd = -1; continue; }  /* reconnect */
            last_sent = state;
            fprintf(stderr, "%s altscreen_render: -> %s\n", log_stamp(),
                    state == ALT_PRESENT_VIDEO ? "VIDEO_ON" :
                    (state == ALT_PRESENT_LOADER ? "LOADER_READY" : "VIDEO_OFF"));
        }

        /* 20 ms poll (~50 Hz state sampling) with instant wakeup on incoming
         * Java commands (ALTCMD_VIEW_AREA) or peer close. */
        struct pollfd pfd = { fd, POLLIN, 0 };
        int pr = poll(&pfd, 1, 20);
        if (pr < 0 && errno != EINTR) {
            close(fd); fd = -1;
            continue;
        }
        if (pr > 0) {
            ssize_t r = recv(fd, rx + rx_used, ALTCTL_PKT_SIZE - rx_used, MSG_DONTWAIT);
            if (r == 0) { close(fd); fd = -1; }    /* lsd closed the control link */
            else if (r > 0) {
                rx_used += (unsigned int)r;
                if (rx_used == ALTCTL_PKT_SIZE) {
                    if (rx[0] == ALTCMD_VIEW_AREA) {
                        av_sink_set_layout(rx[1] == 1 ? 1 : 0,
                                           (int)rx[2], (int)rx[3]);
                    } else if (rx[0] == ALTCMD_READINESS) {
                        epoch = ((unsigned int)rx[1] << 16) | ((unsigned int)rx[2] << 8) | rx[3];
                        /* Re-sample on the NEXT loop, after receiving the epoch.
                         * Never relabel a cached pre-request readiness snapshot. */
                        last_sent = -1;
                    } else {
                        fprintf(stderr, "altscreen_render: unknown control command 0x%02x\n",
                                (unsigned int)rx[0]);
                    }
                    rx_used = 0;
                }
            }
            else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                close(fd); fd = -1;                /* RST / hard error → drop dead fd, reconnect */
            }
        }
    }
    if (fd >= 0) close(fd);
    __sync_synchronize();
    g_exited = 1;
    __sync_synchronize();
    return NULL;
}

void altctl_start(int (*presentation_state)(void)) {
    if (g_run) return;
    g_presentation_state = presentation_state;
    g_run = 1;
    g_exited = 0;
    if (pthread_create(&g_thr, NULL, ctl_loop, NULL) != 0) {
        g_run = 0;
        g_exited = 1;
        fprintf(stderr, "altscreen_render: control thread spawn failed\n");
    }
}

void altctl_stop(void) {
    unsigned int waited = 0;
    if (!g_run) return;
    g_run = 0;
    while (!g_exited && waited < 1250) {
        struct timespec pause = { 0, 10L * 1000000L };
        nanosleep(&pause, NULL);
        waited += 10;
    }
    if (!g_exited) {
        static const char msg[] =
            "altscreen_render: control worker stop TIMEOUT; exiting isolated renderer\n";
        (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(91);
    }
    pthread_join(g_thr, NULL);
}
