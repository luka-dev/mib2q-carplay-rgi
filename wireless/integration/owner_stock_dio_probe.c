/* Disposable QEMU probe: verify that stock dio_manager inherited owner lock. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    const char *path = "/mnt/owner/owner.lock";
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("[owner-stock-dio] open failed errno=%d\n", errno);
        return 1;
    }
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(fd, F_GETLK, &lock) != 0 || lock.l_type != F_WRLCK ||
        lock.l_pid <= 0) {
        printf("[owner-stock-dio] lock not held errno=%d type=%d pid=%ld\n",
               errno, lock.l_type, (long)lock.l_pid);
        close(fd);
        return 2;
    }
    printf("[owner-stock-dio] stock dio lock held by pid=%ld\n",
           (long)lock.l_pid);
    close(fd);
    return 0;
}
