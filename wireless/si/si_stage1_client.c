/* Stage-1 guest client for the si_adapter status socket (QNX).
 * Waits for /tmp/mhi2q_si_adapter.*.sock, runs STATUS -> ENQ NOOP -> STATUS,
 * prints machine-readable markers the host runner asserts on. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>

static int find_socket(char *out, size_t out_size) {
    DIR *dir = opendir("/tmp");
    if (!dir) return -1;
    struct dirent *e;
    int found = -1;
    while ((e = readdir(dir)) != NULL) {
        if (strcmp(e->d_name, "mhi2q_si_control.sock") == 0 ||
            (strncmp(e->d_name, "mhi2q_si_adapter.", 17) == 0 &&
             strstr(e->d_name, ".sock"))) {
            snprintf(out, out_size, "/tmp/%s", e->d_name);
            found = 0;
            break;
        }
    }
    closedir(dir);
    return found;
}

static int ask(const char *path, const char *req, char *reply, size_t reply_size) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct timeval timeout = {2, 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) {
        close(fd);
        return -1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    send(fd, req, strlen(req), 0);
    ssize_t n = recv(fd, reply, reply_size - 1, 0);
    close(fd);
    if (n <= 0) return -1;
    reply[n] = '\0';
    return 0;
}

static long counter_value(const char *reply, const char *key) {
    const char *p = strstr(reply, key);
    if (!p) return -1;
    return strtol(p + strlen(key), NULL, 10);
}

int main(int argc, char **argv) {
    char path[128] = "";
    char reply[512] = "";
    int ok = 1;
    int auto_path = argc == 2 && strcmp(argv[1], "--require-delete") == 0;
    int require_delete = auto_path || (argc == 3 && strcmp(argv[2], "--require-delete") == 0);

    if (argc > 3 || (argc == 3 && !require_delete) ||
        (argc >= 2 && strlen(argv[1]) >= sizeof(path))) return 2;
    if (argc >= 2 && !auto_path) snprintf(path, sizeof(path), "%s", argv[1]);
    else {
        for (int attempt = 0; attempt < 60 && find_socket(path, sizeof(path)) != 0; ++attempt)
            sleep(1);
    }
    if (path[0] == '\0') {
        printf("=== CLIENT FAIL: no adapter socket after 60s ===\n");
        return 1;
    }

    if (ask(path, "STATUS", reply, sizeof(reply)) != 0 ||
        strstr(reply, "published=1") == NULL) {
        printf("=== CLIENT FAIL: not published (%s) ===\n", reply);
        return 1;
    }
    printf("CLIENT: STATUS published=1 [%s]\n", reply);
    long initial_noops = counter_value(reply, "job_noop=");
    long initial_deleted = counter_value(reply, "job_deleted=");
    if (initial_noops < 0 || (require_delete && initial_deleted < 0)) return 1;

    if (ask(path, "ENQ 1 1000", reply, sizeof(reply)) != 0 ||
        strstr(reply, "ENQOK") == NULL) {
        printf("=== CLIENT FAIL: ENQ rejected (%s) ===\n", reply);
        return 1;
    }
    printf("CLIENT: ENQ accepted (device=1 op=0x1000 NOOP)\n");

    long noops = initial_noops;
    long deleted = initial_deleted;
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (ask(path, "STATUS", reply, sizeof(reply)) != 0) {
            printf("=== CLIENT FAIL: second STATUS ===\n");
            return 1;
        }
        noops = counter_value(reply, "job_noop=");
        deleted = counter_value(reply, "job_deleted=");
        if (noops > initial_noops && (!require_delete || deleted > initial_deleted)) break;
        usleep(20000);
    }
    if (noops <= initial_noops || (require_delete && deleted <= initial_deleted)) {
        printf("=== CLIENT FAIL: job lifecycle counter did not advance (%s) ===\n", reply);
        ok = 0;
    } else {
        printf("CLIENT: ENQ CONTOUR OK job_noop=%ld\n", noops);
        if (require_delete) printf("CLIENT: NATIVE DELETE OK job_deleted=%ld\n", deleted);
    }
    printf("CLIENT: FINAL STATUS [%s]\n", reply);

    printf(ok ? "=== CLIENT DONE OK ===\n" : "=== CLIENT DONE WITH ERRORS ===\n");
    return ok ? 0 : 1;
}
