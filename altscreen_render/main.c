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
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_listen_fd = -1;
static volatile sig_atomic_t g_client_fd = -1;

/* iOS offers type 111 just before stock 210 starts its type-110 decoder. Starting
 * a second Qualcomm OMX/EGL pipeline at that exact point can contend for the
 * APQ8064 video/GPU services. Keep the TCP contract live immediately, but let
 * the stock main-screen decoder claim its resources first. 750 ms covers the
 * measured 110 INITIALIZING->READY interval (~260 ms) with margin while staying
 * well below the hook's 8-second bounded localhost back-pressure window. */
static void main_screen_priority_grace(void) {
    const char *s = getenv("ALTSCREEN_MAIN_GRACE_MS");
    long ms = s ? strtol(s, NULL, 10) : 750;
    struct timespec req, rem;
    if (ms <= 0) return;
    if (ms > 3000) ms = 3000;
    fprintf(stderr,
            "altscreen_render: main-screen priority grace %ld ms before alt OMX/EGL\n",
            ms);
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    while (g_running && nanosleep(&req, &rem) != 0 && errno == EINTR)
        req = rem;
}

/* close() is async-signal-safe. Closing both sockets guarantees SIGTERM breaks
 * an idle accept() and an active/partial frame read immediately, so the normal
 * OMX/Screen teardown runs before the supervisor's SIGKILL backstop. */
static void on_signal(int sig) {
    int fd;
    (void)sig;
    g_running = 0;
    fd = (int)g_client_fd;
    g_client_fd = -1;
    if (fd >= 0) close(fd);
    fd = (int)g_listen_fd;
    g_listen_fd = -1;
    if (fd >= 0) close(fd);
}

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

    /* Qualcomm's QNX EGL loader dereferences its graphics-module root during
     * eglGetDisplay.  maneuver_render has always supplied this explicitly;
     * altscreen_render did not need EGL before the native-buffer GPU path.
     * Keep the renderer self-contained so a supervisor/environment change
     * cannot turn a missing variable into an egl14.so SIGSEGV. */
    if (!getenv("GRAPHICS_ROOT"))
        putenv("GRAPHICS_ROOT=/proc/boot");
    if (!getenv("IPL_CONFIG_DIR"))
        putenv("IPL_CONFIG_DIR=/etc/eso/production");

    /* Keep RAM-backed diagnostics bounded. The hook log already has size-based
     * rotation; retain one previous renderer generation and truncate the current
     * file on process start. rename() also preserves the supervisor-opened inode,
     * while freopen gives this process a clean current log. */
    unlink("/tmp/altscreen_render.log.1");
    rename("/tmp/altscreen_render.log", "/tmp/altscreen_render.log.1");
    freopen("/tmp/altscreen_render.log", "w", stderr);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (av_sink_open() != 0) return 1;

    int lfd = listen_socket();
    if (lfd < 0) { av_sink_close(); return 1; }
    g_listen_fd = lfd;
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
        g_client_fd = cfd;
        fprintf(stderr, "altscreen_render: hook connected\n");
        altctl_set_video(1);                     /* 111 video client connected → tell Java */
        main_screen_priority_grace();
        serve_client(cfd);
        if ((int)g_client_fd == cfd) {
            g_client_fd = -1;
            close(cfd);
        }
        av_sink_session_end();
        altctl_set_video(0);                     /* stream ended → tell Java (Java clears ctx 82/83) */
        fprintf(stderr, "altscreen_render: hook disconnected\n");
    }

    altctl_stop();
    if ((int)g_listen_fd == lfd) {
        g_listen_fd = -1;
        close(lfd);
    }
    av_sink_close();
    return 0;
}
