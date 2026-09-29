/* Disposable QEMU stand-in for the final lines of carplay_startup.sh. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    const char *preload = getenv("MHI2Q_OWNER_DIO_PRELOAD");
    if (!preload || setenv("LD_PRELOAD", preload, 1) != 0)
        return 1;
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
    char *const args[] = { "/mnt/pairing/bin/dio_manager", 0 };
#else
    char *const args[] = { "/proc/boot/dio_manager", 0 };
#endif
    execv(args[0], args);
    perror("[owner-stock-dio] exec stock dio");
    return 2;
}
