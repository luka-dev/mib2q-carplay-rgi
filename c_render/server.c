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

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int try_connect(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

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

int cr_server_init(int port) {
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
    if (g_server_fd >= 0) { close(g_server_fd); g_server_fd = -1; }
    g_recv_len = 0;
    fprintf(stderr, "server: shutdown\n");
}

/* Send EVT_HEARTBEAT — single 48-byte packet, all-zero payload, cmd=0x80.
 * Java side has SO_TIMEOUT=5s on the renderer socket; the heartbeat is
 * what keeps that timeout from firing during quiet periods. */
void cr_server_send_heartbeat(void) {
    if (g_server_fd < 0) return;

    uint8_t pkt[CR_PKT_SIZE];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = EVT_HEARTBEAT;

    int n = (int)send(g_server_fd, pkt, CR_PKT_SIZE, 0);
    if (n != CR_PKT_SIZE) {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            /* Java reader stuck or slow — just skip this beat. */
            return;
        }
        fprintf(stderr, "server: heartbeat send failed (n=%d, %s)\n",
                n, strerror(errno));
        close(g_server_fd);
        g_server_fd = -1;
        g_recv_len = 0;
    }
}
