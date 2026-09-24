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

static int (*g_video_ready)(void);   /* installed before thread creation */
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
        if (poll(&pfd, 1, 250) > 0 && (pfd.revents & POLLOUT) &&
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
    unsigned char rx[ALTCTL_PKT_SIZE];
    unsigned int rx_used = 0;
    while (g_run) {
        if (fd < 0) {
            fd = ctl_connect();
            last_sent = -1;                       /* force resend of the current state on (re)connect */
            rx_used = 0;
            if (fd < 0) {                          /* Java control server not up yet — retry */
                struct timespec retry = { 0, 200L * 1000000L };
                nanosleep(&retry, NULL);
                continue;
            }
            fprintf(stderr, "%s altscreen_render: control link to lsd up (:%d)\n", log_stamp(), ALTCTL_PORT);
        }

        /* Decode/presentation can complete after the last socket record. Never
         * require another compressed frame to notice ON/OFF or replay readiness. */
        int state = g_video_ready && g_video_ready();
        if (state != last_sent) {
            unsigned char pkt[ALTCTL_PKT_SIZE];
            memset(pkt, 0, sizeof(pkt));
            pkt[0] = (unsigned char)(state ? ALTEVT_VIDEO_ON : ALTEVT_VIDEO_OFF);
            ssize_t k = send(fd, pkt, sizeof(pkt), 0);
            if (k != (ssize_t)sizeof(pkt)) { close(fd); fd = -1; continue; }  /* reconnect */
            last_sent = state;
            fprintf(stderr, "%s altscreen_render: -> VIDEO_%s\n", log_stamp(), state ? "ON" : "OFF");
        }

        /* ~10 Hz idle: catch state changes promptly + notice a peer close. */
        struct timespec ts = { 0, 100L * 1000000L };
        nanosleep(&ts, NULL);
        ssize_t r = recv(fd, rx + rx_used, ALTCTL_PKT_SIZE - rx_used, MSG_DONTWAIT);
        if (r == 0) { close(fd); fd = -1; }        /* lsd closed the control link */
        else if (r > 0) {
            rx_used += (unsigned int)r;
            if (rx_used == ALTCTL_PKT_SIZE) {
                if (rx[0] == ALTCMD_VIEW_AREA) {
                    av_sink_set_layout(rx[1] == 1 ? 1 : 0,
                                       (int)rx[2], (int)rx[3]);
                } else {
                    fprintf(stderr, "altscreen_render: unknown control command 0x%02x\n",
                            (unsigned int)rx[0]);
                }
                rx_used = 0;
            }
        }
        else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            close(fd); fd = -1;                    /* RST / hard error → drop dead fd, reconnect */
        }
    }
    if (fd >= 0) close(fd);
    __sync_synchronize();
    g_exited = 1;
    __sync_synchronize();
    return NULL;
}

void altctl_start(int (*video_ready)(void)) {
    if (g_run) return;
    g_video_ready = video_ready;
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
