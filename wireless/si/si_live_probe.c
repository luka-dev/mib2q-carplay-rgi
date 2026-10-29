/* Read-only HU probe. Does not stop the target or change its memory. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "si_runtime_guard.h"
#include "si_candidate_guard.h"

static int read_memory(void *ctx, uint32_t va, void *out, size_t size) {
    int fd = *(int *)ctx;
    size_t done = 0;
    while (done < size) {
        ssize_t n = pread(fd, (char *)out + done, size - done, (off_t)va + done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        done += (size_t)n;
    }
    return 1;
}

static uint32_t word(int fd, uint32_t va, const char *name) {
    uint32_t v = 0;
    int ok = read_memory(&fd, va, &v, sizeof(v));
    printf("LIVE: %s va=%#x read=%d value=%#x\n", name, va, ok, v);
    return ok ? v : 0;
}

int main(int argc, char **argv) {
    char path[64], *end;
    long pid;
    if (argc < 2 || argc > 3) return 2;
    pid = strtol(argv[1], &end, 10);
    if (*end || pid <= 1) return 2;
    snprintf(path, sizeof(path), "/proc/%ld/as", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror(path); return 1; }
    struct si_ready_snapshot snapshot;
    printf("LIVE: pid=%ld ready=%d\n", pid, si_guard_ready(read_memory, &fd, &snapshot));
    uint32_t c = word(fd, SI_CONTROLLER_PTR, "controller");
    if (c >= 0x10000 && c < 0xffff0000) {
        uint32_t impl = word(fd, c + SI_CONTROLLER_IMPL_OFF, "impl");
        if (impl >= 0x10000 && impl < 0xffff0000)
            word(fd, impl + SI_IMPL_STATE_OFF, "state");
    }
    uint32_t l = word(fd, SI_REGISTRATION_ARRAY + 4, "listener");
    if (l >= 0x10000 && l < 0xffff0000) {
        uint32_t vt = word(fd, l + 4, "listener_vtable");
        if (vt >= 0x10000 && vt < 0xffff0000) word(fd, vt + 24, "dispatch");
    }
    word(fd, 0x1c5e00, "root");
    unsigned char b[128];
    int matches = 1;
    for (size_t i = 0; i < sizeof(si_guard_ranges) / sizeof(si_guard_ranges[0]); ++i) {
        const struct si_guard_range *r = &si_guard_ranges[i];
        for (size_t n = 0; n < r->size;) {
            size_t size = r->size - n;
            if (size > sizeof(b)) size = sizeof(b);
            if (!read_memory(&fd, r->va + n, b, size) || memcmp(b, r->bytes + n, size)) {
                printf("LIVE: candidate mismatch va=%#x\n", (unsigned)(r->va + n));
                matches = 0;
                break;
            }
            n += size;
        }
    }
    printf("LIVE: immutable_candidate=%d\n", matches);
    if (argc == 3) {
        unsigned long va = strtoul(argv[2], &end, 0);
        if (*end || va > UINT32_MAX - 256) { close(fd); return 2; }
        for (unsigned i = 0; i < 64; ++i) word(fd, (uint32_t)va + i * 4, "dump");
    }
    close(fd);
    return 0;
}
