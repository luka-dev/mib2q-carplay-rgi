/* Disposable QNX target for the exact wired owner helper's final exec. */
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    puts("[wired-confirm-qemu] wired child claimed owner");
    fflush(stdout);
#ifdef MHI2Q_QEMU_WIRED_HOLD
    sleep(3);
    puts("[wired-confirm-qemu] USB released");
    fflush(stdout);
    {
        FILE *marker = fopen("/dev/shmem/wired_released", "w");
        if (!marker)
            return 2;
        fclose(marker);
    }
#endif
    return 0;
}
