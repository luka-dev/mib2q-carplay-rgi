/* Disposable QNX4 fixture only: exercises actual persistent-store syscalls. */
#include "../../../hook/wireless/pairing.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "[pairing-store-qemu] failed line=%d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    const char *dir = "/mnt/pairing/store-test";
    const char *file = "/mnt/pairing/store-test/identity.v1";
    cp_pair_store *store = NULL;
    char before[CP_PAIR_ID_MAX + 1], after[CP_PAIR_ID_MAX + 1];
    uint8_t pk[32], again[32];
    struct stat st;
    pid_t child;
    int status, fd;
    CHECK(cp_pair_store_open(dir, &store) == 0);
    CHECK(cp_pair_identity(store, before, pk) == 0);
    CHECK(strlen(before) == 36);
    CHECK(stat(file, &st) == 0 && st.st_size == 326 && (st.st_mode & 0777) == 0600);
    child = fork();
    CHECK(child >= 0);
    if (!child) {
        cp_pair_store *other = NULL;
        int err = cp_pair_store_open(dir, &other);
        if (other) cp_pair_store_close(other);
        _exit(err ? 0 : 1);
    }
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    cp_pair_store_close(store); store = NULL;
    CHECK(cp_pair_store_open(dir, &store) == 0);
    CHECK(cp_pair_identity(store, after, again) == 0);
    CHECK(!strcmp(before, after) && !memcmp(pk, again, 32));
    cp_pair_store_close(store); store = NULL;
    CHECK(chmod(file, 0644) == 0);
    CHECK(cp_pair_store_open(dir, &store) != 0 && store == NULL);
    CHECK(chmod(file, 0600) == 0);
    fd = open(file, O_WRONLY);
    CHECK(fd >= 0 && write(fd, "!", 1) == 1 && fsync(fd) == 0);
    close(fd);
    CHECK(cp_pair_store_open(dir, &store) != 0 && store == NULL);
    fd = open(file, O_RDONLY);
    CHECK(fd >= 0 && read(fd, after, 1) == 1 && after[0] == '!');
    close(fd);
    CHECK(symlink(dir, "/mnt/pairing/store-link") == 0);
    CHECK(cp_pair_store_open("/mnt/pairing/store-link", &store) != 0 && store == NULL);
    {
        static const char marker[] = "[pairing-store-qemu] reopen, process lock, permissions, corruption, symlink passed\n";
        (void)write(2, marker, sizeof(marker) - 1);
    }
    return 0;
}
