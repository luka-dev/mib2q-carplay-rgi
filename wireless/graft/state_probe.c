/* QEMU-only, read-only probe of the graft's dedicated RW PT_LOAD. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc > 1) {
        unsigned long delay = strtoul(argv[1], NULL, 10);
        if (delay > 60) return 2;
        (void)sleep((unsigned)delay);
        printf("=== late graft state probe ===\n");
    }
    char text[24] = {0};
    char path[48];
    uint8_t code[16] = {0};
    uint32_t state[12] = {0};
    int stage_fd = open("/dev/shmem/btstack-wrapper-stage", O_RDONLY);
    if (stage_fd >= 0) {
        char stage = '?';
        if (read(stage_fd, &stage, 1) == 1)
            printf("btstack-wrapper-stage: %c\n", stage);
        (void)close(stage_fd);
    } else {
        printf("btstack-wrapper-stage: absent errno=%d\n", errno);
    }
    int fd = open("/dev/shmem/btstack-loader-pid", O_RDONLY);
    if (fd < 0) {
        printf("graft-state: pid file errno=%d\n", errno);
        return 1;
    }
    ssize_t count = read(fd, text, sizeof(text) - 1);
    (void)close(fd);
    if (count <= 0) return 1;
    unsigned long pid = strtoul(text, NULL, 10);
    snprintf(path, sizeof(path), "/proc/%lu/as", pid);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("graft-state: process %lu unavailable errno=%d\n", pid, errno);
        return 1;
    }
    count = pread(fd, code, sizeof(code), (off_t)0x508000);
    if (count == (ssize_t)sizeof(code)) {
        printf("graft-code:");
        for (unsigned i = 0; i < sizeof(code); ++i) printf("%02x", code[i]);
        printf("\n");
    } else {
        printf("graft-code: read count=%ld errno=%d\n", (long)count, errno);
    }
    count = pread(fd, state, sizeof(state), (off_t)0x510000);
    if (count != (ssize_t)sizeof(state)) {
        printf("graft-state: read pid=%lu count=%ld errno=%d\n",
               pid, (long)count, errno);
        (void)close(fd);
        return 1;
    }
    (void)close(fd);
    printf("graft-state: pid=%lu once=%u status=0x%08x generation=%u connected=%u queue=%u\n",
           pid, state[0], state[1], state[3], state[4], state[7]);
    return 0;
}
