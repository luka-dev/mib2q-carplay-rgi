/*
 * Non-blocking TCP CLIENT -- connects to Java BAPBridge server
 * (127.0.0.1:19800), receives CMD_MANEUVER / CMD_BARGRAPH / etc. packets.
 *
 * Topology: renderer is the short-lived process spawned per-route.  Java
 * (long-lived) opens the listen socket once at boot and accepts whenever
 * we connect.  No timing/sleep hacks needed -- as soon as renderer is
 * up enough to call cr_server_init(), it just connects.
 *
 * Symbol names kept (cr_server_*) to avoid touching every call site in
 * main.c.  The "server" naming is now a misnomer but harmless.
 *
 * Accumulates partial reads into complete CR_PKT_SIZE packets.
 * POSIX sockets -- works on macOS and QNX 6.5.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "server.h"

static int g_server_fd = -1;        /* connected fd to Java BAPBridge */
static int g_target_port = 0;
static struct timeval g_last_retry = {0, 0};

#define RECV_BUF_SIZE 512
static uint8_t g_recv_buf[RECV_BUF_SIZE];
static int g_recv_len = 0;

#define RETRY_INTERVAL_SEC 1   /* re-attempt connect every 1 s if Java not up yet */

/* Heartbeat thread — sends EVT_HEARTBEAT once per second.  Lives on a
 * dedicated pthread so the render main loop never pays for any TCP
 * syscalls; this keeps frame pacing perfectly clean at 30 FPS even on
 * QNX 6.5 where send() over loopback can occasionally take a few ms. */
static pthread_t      g_hb_thread;
static volatile int   g_hb_running = 0;
static int            g_hb_started = 0;

/* Force fd into O_NONBLOCK and verify it stuck.  fcntl can silently
 * no-op on broken QNX 6.5 socket subsystems if the GETFL → SETFL race
 * loses to another thread, so we re-read the flags.  Returns 0 on
 * success, -1 if the flag did not apply.  Caller treats failure as
 * non-fatal but logs it loudly — without O_NONBLOCK the send/recv
 * paths in this file degrade silently into blocking I/O. */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "server: fcntl F_GETFL failed: %s\n", strerror(errno));
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        fprintf(stderr, "server: fcntl F_SETFL O_NONBLOCK failed: %s\n", strerror(errno));
        return -1;
    }
    int actual = fcntl(fd, F_GETFL, 0);
    if (actual < 0 || !(actual & O_NONBLOCK)) {
        fprintf(stderr, "server: O_NONBLOCK did not stick (flags=0x%x)\n", actual);
        return -1;
    }
    return 0;
}

static int try_connect(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    /* Belt-and-suspenders against set_nonblocking() failing silently:
     * if O_NONBLOCK doesn't stick, send/recv would otherwise block the
     * caller indefinitely.  1 s timeout caps any blocking syscall and
     * surfaces as EAGAIN, which all callers already handle. */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)g_target_port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    set_nonblocking(fd);
    return fd;
}

/* Heartbeat thread main.  Sleeps 1 s, sends one EVT_HEARTBEAT packet,
 * repeats.  Reads g_server_fd lock-free as a simple int snapshot — the
 * race window (main thread close+reopen between snapshot and send) is
 * benign on loopback: worst case we send to a freshly-reused fd, which
 * is still a valid heartbeat to the new connection.
 *
 * send() is bounded by SO_SNDTIMEO=1s set in try_connect().  Even if
 * O_NONBLOCK silently failed to apply, this thread can't wedge for
 * more than ~1 s — keeps Java's SO_TIMEOUT=5s liveness check happy. */
static void *heartbeat_thread_main(void *arg) {
    (void)arg;
    while (g_hb_running) {
        struct timespec ts = { 1, 0 };
        nanosleep(&ts, NULL);
        if (!g_hb_running) break;

        int fd = g_server_fd;
        if (fd < 0) continue;

        uint8_t pkt[CR_PKT_SIZE];
        memset(pkt, 0, sizeof(pkt));
        pkt[0] = EVT_HEARTBEAT;
        (void)send(fd, pkt, CR_PKT_SIZE, 0);
    }
    return NULL;
}

int cr_server_init(int port) {
    if (g_hb_started) {
        fprintf(stderr, "server: cr_server_init called twice, ignoring\n");
        return 0;
    }

    g_target_port = port;

    /* First connect attempt -- Java should already be listening if it's
     * up.  If not, cr_server_poll() will keep retrying. */
    g_server_fd = try_connect();
    if (g_server_fd >= 0) {
        fprintf(stderr, "server: connected to 127.0.0.1:%d\n", port);
    } else {
        fprintf(stderr, "server: 127.0.0.1:%d not yet listening, will retry\n", port);
    }
    g_recv_len = 0;

    /* Heartbeat thread does almost nothing — 64 KB stack is ample. */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64 * 1024);

    g_hb_running = 1;
    if (pthread_create(&g_hb_thread, &attr, heartbeat_thread_main, NULL) == 0) {
        g_hb_started = 1;
    } else {
        g_hb_running = 0;
        fprintf(stderr, "server: heartbeat thread failed to start\n");
    }
    pthread_attr_destroy(&attr);

    return 0;   /* Always succeeds -- reconnect handled by poll. */
}

void cr_server_poll(void) {
    /* Reconnect if disconnected, throttled to RETRY_INTERVAL_SEC. */
    if (g_server_fd < 0 && g_target_port > 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec - g_last_retry.tv_sec >= RETRY_INTERVAL_SEC) {
            g_last_retry = now;
            g_server_fd = try_connect();
            if (g_server_fd >= 0) {
                fprintf(stderr, "server: connected to 127.0.0.1:%d\n", g_target_port);
                g_recv_len = 0;
            }
        }
    }

    /* Read available data from server */
    if (g_server_fd >= 0) {
        int space = RECV_BUF_SIZE - g_recv_len;
        if (space > 0) {
            int n = (int)recv(g_server_fd, g_recv_buf + g_recv_len, space, 0);
            if (n > 0) {
                g_recv_len += n;
            } else if (n == 0) {
                /* Server (Java) disconnected — likely BAPBridge teardown.
                 * Renderer process should exit shortly via slay. */
                fprintf(stderr, "server: disconnected\n");
                close(g_server_fd);
                g_server_fd = -1;
                g_recv_len = 0;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "server: recv error: %s\n", strerror(errno));
                close(g_server_fd);
                g_server_fd = -1;
                g_recv_len = 0;
            }
        }
    }
}

int cr_server_read_cmd(cr_cmd_t *out) {
    if (g_recv_len < CR_PKT_SIZE) return 0;

    memcpy(out, g_recv_buf, CR_PKT_SIZE);
    g_recv_len -= CR_PKT_SIZE;
    if (g_recv_len > 0)
        memmove(g_recv_buf, g_recv_buf + CR_PKT_SIZE, g_recv_len);
    return 1;
}

void cr_server_shutdown(void) {
    if (g_hb_started) {
        g_hb_running = 0;
        pthread_join(g_hb_thread, NULL);
        g_hb_started = 0;
    }
    if (g_server_fd >= 0) { close(g_server_fd); g_server_fd = -1; }
    g_recv_len = 0;
    fprintf(stderr, "server: shutdown\n");
}
