/*
 * altscreen_render — receives the CarPlay cluster (111) H.264 stream from the
 * hook over TCP :19830 and feeds it to a video sink (file capture on Mac, OMX
 * decode + cluster present on QNX).
 *
 * Single-threaded blocking server: the hook is the only client and (re)connects
 * once per CarPlay session.  On disconnect we notify the sink and accept again.
 * POSIX sockets — builds on macOS and QNX 6.5.
 */
#include "protocol.h"
#include "frame_io.h"
#include "sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
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

    while (g_running) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;        /* signal → re-check g_running */
            perror("accept");
            break;
        }
        fprintf(stderr, "altscreen_render: hook connected\n");
        serve_client(cfd);
        close(cfd);
        av_sink_session_end();
        fprintf(stderr, "altscreen_render: hook disconnected\n");
    }

    close(lfd);
    av_sink_close();
    return 0;
}
