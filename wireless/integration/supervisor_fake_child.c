/* Host-only test peer for the owner, supervisor, and daemon lifecycle. */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;

static void on_term(int signo)
{
    (void)signo;
    stopping = 1;
}

static void mark(const char *message)
{
    const char *path = getenv("EVENTS");
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0)
        _exit(30);
    char line[128];
    int length = snprintf(line, sizeof(line), "%s %ld\n", message, (long)getpid());
    if (length <= 0 || length >= (int)sizeof(line) ||
        write(fd, line, (size_t)length) != length)
        _exit(31);
    close(fd);
}

int main(int argc, char **argv)
{
    int bridge_fd = -1;
    int bridge_busy_sent = 0;
    const char *name = strrchr(argv[0], '/');
    name = name ? name + 1 : argv[0];
    if (strstr(name, "mhi2q-wireless-daemon")) {
        if (argc == 2 && !strcmp(argv[1], "--clear-ie")) {
            mark("clear");
            return 0;
        }
        mark("daemon start");
    } else if (strstr(name, "carplay_monitor")) {
        mark("monitor start");
    } else {
        const char *mode = getenv("MHI2Q_DIO_OWNER_MODE");
        if (!mode)
            return 31;
        if (!strcmp(mode, "wireless")) {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            int one = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            struct sockaddr_in address = {0};
            address.sin_family = AF_INET;
            inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
            address.sin_port = htons((unsigned short)atoi(getenv("MHI2Q_OWNER_BRIDGE_PORT")));
            if (bind(fd, (struct sockaddr *)&address, sizeof(address)) || listen(fd, 8))
                return 32;
            bridge_fd = fd;
            mark("wireless start");
        } else {
            mark("wired start");
        }
    }
    signal(SIGTERM, on_term);
    while (!stopping) {
        if (bridge_fd >= 0 &&
            (getenv("MHI2Q_REQUIRE_BRIDGE_QUIESCE") ||
             getenv("MHI2Q_FAKE_BRIDGE_QUIESCE"))) {
            fd_set readable;
            struct timeval timeout = {0, 100000};
            FD_ZERO(&readable);
            FD_SET(bridge_fd, &readable);
            if (select(bridge_fd + 1, &readable, 0, 0, &timeout) > 0) {
                int client = accept(bridge_fd, 0, 0);
                if (client >= 0) {
                    unsigned char request[16] = {0};
                    unsigned char response[8] = {'C','P','R','A',2,0,0,0};
                    size_t used = 0;
                    while (used < sizeof(request)) {
                        ssize_t step = read(client, request + used,
                                            sizeof(request) - used);
                        if (step <= 0) break;
                        used += (size_t)step;
                    }
                    if (used == sizeof(request) &&
                        !memcmp(request, "CPRX\002\003", 6)) {
                        if (getenv("MHI2Q_FAKE_BRIDGE_BUSY_ONCE") &&
                            !bridge_busy_sent) {
                            response[5] = 3;
                            bridge_busy_sent = 1;
                            mark("bridge busy");
                        } else {
                            mark("bridge quiesce");
                        }
                        (void)write(client, response, sizeof(response));
                    } else if (used == sizeof(request) &&
                               !memcmp(request, "CPRX\002\004", 6)) {
                        mark("bridge drain");
                        (void)write(client, response, sizeof(response));
                    }
                    close(client);
                }
            }
        } else {
            struct timespec delay = {0, 100000000};
            nanosleep(&delay, 0);
        }
    }
    mark(strstr(name, "mhi2q-wireless-daemon") ? "daemon stop" :
         strstr(name, "carplay_monitor") ? "monitor stop" :
         !strcmp(getenv("MHI2Q_DIO_OWNER_MODE"), "wireless") ?
         "wireless stop" : "wired stop");
    return 0;
}
