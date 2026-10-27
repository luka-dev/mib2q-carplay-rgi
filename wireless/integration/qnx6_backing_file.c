/* QEMU-only: create a disposable block backing file on the QNX4 RAM disk. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#ifdef MHI2Q_FACTORY_QNX6_MODULE
static int stage_factory_module(const char *source, const char *destination,
                                mode_t mode)
{
    int input = open(source, O_RDONLY);
    int output = open(destination, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (input < 0 || output < 0) return -1;
    char buffer[4096];
    ssize_t count;
    while ((count = read(input, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < count; ++i) buffer[i] ^= (char)0xa5;
        ssize_t done = 0;
        while (done < count) {
            ssize_t written = write(output, buffer + done, (size_t)(count - done));
            if (written <= 0) return -1;
            done += written;
        }
    }
    close(input);
    close(output);
    return count < 0 ? -1 : 0;
}
#endif

int main(void)
{
    int fd = open("/mnt/backing/qnx6.img", O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0 || ftruncate(fd, 16 * 1024 * 1024) != 0) {
        printf("[qnx6-backing] create failed errno=%d\n", errno);
        return 1;
    }
    close(fd);
#ifdef MHI2Q_FACTORY_QNX6_MODULE
    if (stage_factory_module("/proc/boot/factory_qnx6_xor",
                             "/mnt/backing/fs-qnx6.so", 0644) != 0 ||
        stage_factory_module("/proc/boot/factory_ioblk_xor",
                             "/mnt/backing/io-blk.so", 0644) != 0 ||
        stage_factory_module("/proc/boot/factory_camdisk_xor",
                             "/mnt/backing/cam-disk.so", 0644) != 0 ||
        stage_factory_module("/proc/boot/factory_libcam_xor",
                             "/mnt/backing/libcam.so.2", 0644) != 0 ||
        stage_factory_module("/proc/boot/factory_loopback_xor",
                             "/mnt/backing/devb-loopback", 0755) != 0) {
        printf("[qnx6-backing] factory module stage failed errno=%d\n", errno);
        return 2;
    }
    printf("[qnx6-backing] factory loopback/storage modules staged from opaque images\n");
#endif
    printf("[qnx6-backing] created 16 MiB image\n");
    return 0;
}
