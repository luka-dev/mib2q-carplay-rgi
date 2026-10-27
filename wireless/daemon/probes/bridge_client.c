/* QEMU-only fake RFCOMM side of the daemon's loopback bridge. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

struct IeState { unsigned queries, sets, clears, present, unavailable; };

static int read_ie_state(struct IeState *state) {
    int fd = open("/dev/shmem/wifi_ie_probe_state", O_RDONLY);
    if (fd < 0) {
        memset(state, 0, sizeof(*state));
        return errno == ENOENT ? 0 : -1;
    }
    int size = read(fd, state, sizeof(*state));
    close(fd);
    return size == (int)sizeof(*state) ? 0 : -1;
}

static int check_ie_after_disconnect(void) {
    struct IeState state;
    sleep(7);
    if (read_ie_state(&state) != 0) return 5;
    printf("[bridge-client] IE after disconnect sets=%u clears=%u present=%u\n",
           state.sets, state.clears, state.present);
    return state.sets >= 1 && state.clears >= 1 && state.present == 0 ? 0 : 5;
}

static int read_exact(int fd, unsigned char *out, unsigned length) {
    unsigned used = 0;
    while (used < length) {
        int got = recv(fd, out + used, length - used, 0);
        if (got <= 0) return -1;
        used += (unsigned)got;
    }
    return 0;
}

int main(int argc, char **argv) {
    int ie_lifecycle = argc == 2 && strcmp(argv[1], "--ie-lifecycle") == 0;
    if (argc != 1 && !ie_lifecycle) return 5;
    if (ie_lifecycle) {
        struct IeState state;
        if (read_ie_state(&state) != 0) return 5;
        printf("[bridge-client] IE before connect sets=%u clears=%u present=%u\n",
               state.sets, state.clears, state.present);
        if (state.sets || state.clears || state.present) return 5;
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_port = htons(57342);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int fd = -1;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0 && connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0)
            break;
        if (fd >= 0) close(fd);
        fd = -1;
        usleep(100000);
    }
    if (fd < 0) {
        fprintf(stderr, "[bridge-client] connect failed errno=%d\n", errno);
        return 1;
    }
    const struct timeval timeout = {5, 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    static const unsigned char connected[8] = {1, 0, 0, 0, 1, 0, 0, 0};
    static const unsigned char marker[14] = {
        2, 0, 6, 0, 1, 0, 0, 0, 0xff, 0x55, 0x02, 0x00, 0xee, 0x10
    };
    if (send(fd, connected, sizeof(connected), 0) != sizeof(connected) ||
        send(fd, marker, sizeof(marker), 0) != sizeof(marker)) {
        fprintf(stderr, "[bridge-client] send failed\n");
        close(fd);
        return 2;
    }
    unsigned char stream[256];
    unsigned used = 0;
    for (unsigned frame = 0; frame < 13; ++frame) {
        unsigned char header[8];
        if (read_exact(fd, header, 8) != 0) break;
        unsigned length = (unsigned)header[2] | ((unsigned)header[3] << 8);
        if (header[0] != 2 || header[1] != 0 || length == 0 || length > 20 ||
            header[4] != 1 || header[5] != 0 || header[6] != 0 || header[7] != 0 ||
            used + length > sizeof(stream) ||
            read_exact(fd, stream + used, length) != 0) {
            fprintf(stderr, "[bridge-client] invalid frame\n");
            close(fd);
            return 3;
        }
        used += length;
        static const unsigned char expected_marker[6] = {0xff,0x55,0x02,0,0xee,0x10};
        if (used >= 8 && memcmp(stream, expected_marker, 6) == 0) {
            for (unsigned i = 6; i + 1 < used; ++i) {
                if (stream[i] == 0xff && stream[i + 1] == 0x5a) {
                    puts("[bridge-client] daemon_bridge=1");
                    close(fd);
                    return ie_lifecycle ? check_ie_after_disconnect() : 0;
                }
            }
        }
    }
    fprintf(stderr, "[bridge-client] no iAP2 SYN after marker, bytes=%u\n", used);
    close(fd);
    return 4;
}
