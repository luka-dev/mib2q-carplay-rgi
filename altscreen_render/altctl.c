/*
 * altctl — control channel to Java/lsd.  See altctl.h.  QNX 6.5 ARMv7 gcc 4.4: no __thread.
 */
#include "altctl.h"
#include "protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static volatile int g_video_on = 0;   /* set by the main thread (single writer) */
static volatile int g_run = 0;
static pthread_t     g_thr;

void altctl_set_video(int on) { g_video_on = on ? 1 : 0; }

static int ctl_connect(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_port        = htons((uint16_t)ALTCTL_PORT);
    a.sin_addr.s_addr = inet_addr(ALTCTL_HOST);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { close(fd); return -1; }
    return fd;
}

static void *ctl_loop(void *arg) {
    (void)arg;
    int fd = -1;
    int last_sent = -1;
    while (g_run) {
        if (fd < 0) {
            fd = ctl_connect();
            last_sent = -1;                       /* force resend of the current state on (re)connect */
            if (fd < 0) { sleep(1); continue; }   /* Java control server not up yet — retry */
            fprintf(stderr, "altscreen_render: control link to lsd up (:%d)\n", ALTCTL_PORT);
        }

        int state = g_video_on;
        if (state != last_sent) {
            unsigned char pkt[ALTCTL_PKT_SIZE];
            memset(pkt, 0, sizeof(pkt));
            pkt[0] = (unsigned char)(state ? ALTEVT_VIDEO_ON : ALTEVT_VIDEO_OFF);
            ssize_t k = send(fd, pkt, sizeof(pkt), 0);
            if (k != (ssize_t)sizeof(pkt)) { close(fd); fd = -1; continue; }  /* reconnect */
            last_sent = state;
            fprintf(stderr, "altscreen_render: -> VIDEO_%s\n", state ? "ON" : "OFF");
        }

        /* ~10 Hz idle: catch state changes promptly + notice a peer close. */
        struct timespec ts = { 0, 100L * 1000000L };
        nanosleep(&ts, NULL);
        char b;
        ssize_t r = recv(fd, &b, 1, MSG_DONTWAIT);
        if (r == 0) { close(fd); fd = -1; }        /* lsd closed the control link */
        else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            close(fd); fd = -1;                    /* RST / hard error → drop dead fd, reconnect */
        }
    }
    if (fd >= 0) close(fd);
    return NULL;
}

void altctl_start(void) {
    if (g_run) return;
    g_run = 1;
    if (pthread_create(&g_thr, NULL, ctl_loop, NULL) != 0) {
        g_run = 0;
        fprintf(stderr, "altscreen_render: control thread spawn failed\n");
    }
}

void altctl_stop(void) {
    if (!g_run) return;
    g_run = 0;
    pthread_join(g_thr, NULL);
}
