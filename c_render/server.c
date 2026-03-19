/*
 * Non-blocking TCP server -- single client, reconnectable.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"

static int g_listen_fd = -1;
static int g_client_fd = -1;

#define RECV_BUF_SIZE 512
static uint8_t g_recv_buf[RECV_BUF_SIZE];
static int g_recv_len = 0;

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int cr_server_init(int port) {
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        fprintf(stderr, "server: socket failed: %s\n", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(g_listen_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "server: bind port %d failed: %s\n", port, strerror(errno));
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    if (listen(g_listen_fd, 1) < 0) {
        fprintf(stderr, "server: listen failed: %s\n", strerror(errno));
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    fprintf(stderr, "server: listening on 0.0.0.0:%d\n", port);
    return 0;
}

void cr_server_poll(void) {
    if (g_listen_fd < 0) return;

    /* Accept new client if none connected */
    if (g_client_fd < 0) {
        g_client_fd = accept(g_listen_fd, NULL, NULL);
        if (g_client_fd >= 0) {
            set_nonblocking(g_client_fd);
            g_recv_len = 0;
            fprintf(stderr, "server: client connected\n");
        }
    }

    /* Read available data from client */
    if (g_client_fd >= 0) {
        int space = RECV_BUF_SIZE - g_recv_len;
        if (space > 0) {
            int n = (int)recv(g_client_fd, g_recv_buf + g_recv_len, space, 0);
            if (n > 0) {
                g_recv_len += n;
            } else if (n == 0) {
                /* Client disconnected */
                fprintf(stderr, "server: client disconnected\n");
                close(g_client_fd);
                g_client_fd = -1;
                g_recv_len = 0;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "server: recv error: %s\n", strerror(errno));
                close(g_client_fd);
                g_client_fd = -1;
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
    if (g_client_fd >= 0) { close(g_client_fd); g_client_fd = -1; }
    if (g_listen_fd >= 0) { close(g_listen_fd); g_listen_fd = -1; }
    g_recv_len = 0;
    fprintf(stderr, "server: shutdown\n");
}
