/* QEMU-only fake for the MU1316 custom-IE CLI. Never deployed. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct state {
    unsigned queries, sets, clears, present, unavailable;
    char hex[514];
};

int main(int argc, char **argv) {
    int fd = open("/dev/shmem/wifi_ie_probe_state", O_RDWR | O_CREAT, 0600);
    if (fd < 0) return 2;
    struct state s = {0};
    (void)read(fd, &s, sizeof(s));
    if (argc == 2 && strcmp(argv[1], "--stats") == 0) {
        printf("sets=%u clears=%u queries=%u present=%u unavailable=%u\n",
               s.sets, s.clears, s.queries, s.present, s.unavailable);
        return 0;
    }
    if (argc < 4 || strcmp(argv[1], "-i") != 0 ||
        strcmp(argv[2], "uap0") != 0 || strcmp(argv[3], "sys_cfg_custom_ie") != 0)
        return 3;
    if (argc == 4) {
        const char *delay_text = getenv("MHI2Q_QEMU_AP_DELAY_QUERIES");
        unsigned delay = delay_text ? (unsigned)atoi(delay_text) : 0;
        if (delay > 5) return 6;
        ++s.queries;
        if (s.queries <= delay) {
            ++s.unavailable;
            puts("AP unavailable");
        } else {
            if (s.queries == delay + 3 && !getenv("MHI2Q_QEMU_NO_AP_RESET"))
                s.present = 0; /* QEMU AP restart */
            puts("Querying custom IE successful");
            if (s.present) {
                size_t length = strlen(s.hex) / 2;
                puts("Index [2]");
                puts("Management Subtype Mask = 0x12a");
                printf("IE Buffer: len=%u\n", (unsigned)length);
                for (size_t i = 0; i < length; ++i) {
                    printf("%c%c%c", s.hex[i * 2], s.hex[i * 2 + 1],
                           i % 16 == 15 ? '\n' : ' ');
                }
                putchar('\n');
            }
        }
    } else if (argc == 7 && strcmp(argv[4], "2") == 0 &&
               strcmp(argv[5], "0x12a") == 0 &&
               strncmp(argv[6], "0x", 2) == 0 &&
               strlen(argv[6]) > 2 && strlen(argv[6]) < sizeof(s.hex) + 2) {
        strcpy(s.hex, argv[6] + 2);
        ++s.sets;
        s.present = 1;
        puts("custom IE setting successful");
    } else if (argc == 6 && strcmp(argv[4], "2") == 0 &&
               strcmp(argv[5], "0") == 0) {
        ++s.clears;
        s.present = 0;
        s.hex[0] = '\0';
        puts("custom IE setting successful");
    } else {
        return 4;
    }
    lseek(fd, 0, SEEK_SET);
    if (write(fd, &s, sizeof(s)) != (ssize_t)sizeof(s)) return 5;
    close(fd);
    return 0;
}
