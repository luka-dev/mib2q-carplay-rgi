/*
 * altscreen_render — receives the CarPlay cluster (111) H.264 (Annex-B) stream
 * from libairplay's alt forward (screenstream.c) over TCP :19831 and feeds it to a
 * video sink (file capture on Mac, OMX decode + cluster present on QNX).
 *
 * Single-threaded blocking server: libairplay is the only client and (re)connects
 * once per CarPlay session.  On disconnect we notify the sink and accept again.
 * POSIX sockets — builds on macOS and QNX 6.5.
 */
#include "protocol.h"
#include "frame_io.h"
#include "sink.h"
#include "altctl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static volatile sig_atomic_t g_running = 1;
static void on_signal(int sig) { (void)sig; g_running = 0; }

static int listen_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ALTAV_PORT);
    addr.sin_addr.s_addr = inet_addr(ALTAV_HOST);   /* loopback only */

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, 1) < 0) { perror("listen"); close(fd); return -1; }
    return fd;
}

/* Serve one connected client until it disconnects or a protocol error. */
static void serve_client(int cfd) {
    int one = 1;
    setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    /* A stalled or half-open peer must never hang the sole server thread forever
     * (it would freeze the service and never fire altctl_set_video(0)).
     * KEEPALIVE reaps a genuinely dead TCP peer (crash / half-open).  RCVTIMEO is a
     * generous 60s backstop for a peer that holds the connection but stops sending
     * mid-frame → av_read_frame then returns an error and we drop + accept() again.
     * 60s is longer than any realistic active-stream gap (H.264 refreshes periodically),
     * so it won't false-drop a healthy stream. */
    setsockopt(cfd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    struct timeval tv; tv.tv_sec = 60; tv.tv_usec = 0;
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t *buf = NULL;
    uint32_t cap = 0, len = 0;
    for (;;) {
        int r = av_read_frame(cfd, &buf, &cap, &len);
        if (r == 0) {
            if (len > 0) av_sink_frame(buf, len);
            continue;
        }
        if (r == -2) fprintf(stderr, "altscreen_render: protocol error, dropping link\n");
        break;                                   /* -1 disconnect / -2 proto error */
    }
    free(buf);
}

int main(void) {
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    if (av_sink_open() != 0) return 1;

    int lfd = listen_socket();
    if (lfd < 0) { av_sink_close(); return 1; }
    fprintf(stderr, "altscreen_render: listening on %s:%d\n", ALTAV_HOST, ALTAV_PORT);

    /* Control channel to Java/lsd — reports video on/off so Java flips setVideoAvailable(). */
    altctl_start();

    while (g_running) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;        /* signal → re-check g_running */
            perror("accept");
            break;
        }
        fprintf(stderr, "altscreen_render: hook connected\n");
        altctl_set_video(1);                     /* 111 video client connected → tell Java */
        serve_client(cfd);
        close(cfd);
        av_sink_session_end();
        altctl_set_video(0);                     /* stream ended → tell Java (Java clears ctx 82/83) */
        fprintf(stderr, "altscreen_render: hook disconnected\n");
    }

    altctl_stop();
    close(lfd);
    av_sink_close();
    return 0;
}
