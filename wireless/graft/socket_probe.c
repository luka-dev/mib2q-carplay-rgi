/* QEMU-only loopback peer for the btstack graft worker. Never deployed. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static void report(const char *label, unsigned value)
{
    char line[80];
    int length = snprintf(line, sizeof(line), "[graft-socket-probe] %s=%u\n",
                          label, value);
    if (length > 0 && (size_t)length < sizeof(line))
        (void)write(2, line, (size_t)length);
}

#ifdef MHI2Q_SOCKET_SYNTHETIC
static int read_exact(int fd, unsigned char *buffer, size_t length)
{
    size_t used = 0;
    while (used < length) {
        ssize_t got = recv(fd, buffer + used, length - used, 0);
        if (got <= 0)
            return -1;
        used += (size_t)got;
    }
    return 0;
}

static int check_synthetic_frames(int peer)
{
    static const unsigned char expected[] = "IAP2-QEMU-RX";
    unsigned char header[8], data[sizeof(expected) - 1];
    unsigned connected = 0;
    struct timeval timeout = {20, 0};
    (void)setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    for (unsigned frame = 0; frame < 3; ++frame) {
        if (read_exact(peer, header, sizeof(header)) != 0)
            return -1;
        unsigned length = (unsigned)header[2] | ((unsigned)header[3] << 8);
        unsigned generation = (unsigned)header[4] | ((unsigned)header[5] << 8) |
                              ((unsigned)header[6] << 16) | ((unsigned)header[7] << 24);
        if (header[1] != 0 || generation != 1)
            return -1;
        if (header[0] == 1 && length == 0 && connected < 2) {
            ++connected;
            continue;
        }
        if (header[0] == 2 && length == sizeof(data) && connected >= 1 &&
            read_exact(peer, data, length) == 0 &&
            memcmp(data, expected, length) == 0)
            return 0;
        return -1;
    }
    return -1;
}
#endif

int main(void)
{
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        return 1;
    int reuse = 1;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(57342);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 4) != 0) {
        report("listen_errno", (unsigned)errno);
        close(listener);
        return 1;
    }
    static const char listening[] = "[graft-socket-probe] listening\n";
    (void)write(2, listening, sizeof(listening) - 1);
#ifdef MHI2Q_SOCKET_SYNTHETIC
    int peer = accept(listener, NULL, NULL);
    if (peer < 0 || check_synthetic_frames(peer) != 0) {
        report("synthetic_data", 0);
        if (peer >= 0) close(peer);
        close(listener);
        return 1;
    }
    report("synthetic_data", 1);
    close(peer);
#else
    for (unsigned count = 1; count <= 4; ++count) {
        int peer = accept(listener, NULL, NULL);
        if (peer < 0) {
            report("accept_errno", (unsigned)errno);
            close(listener);
            return 1;
        }
        report("accepted", count);
        (void)shutdown(peer, SHUT_RDWR);
        close(peer);
    }
#endif
    close(listener);
    return 0;
}
