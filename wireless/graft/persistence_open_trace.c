/* Disposable QEMU probe of factory persistence file opens. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef int (*open64_fn)(const char *, int, ...);

int open64(const char *path, int flags, ...)
{
    static open64_fn original;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
    }
    if (!original)
        original = (open64_fn)dlsym(RTLD_NEXT, "open64");
    if (!original) return -1;
    int fd = original(path, flags, mode);
    if (path && (strstr(path, "persist") || strstr(path, "config") ||
                 strstr(path, "sqlite"))) {
        char line[320];
        int count = snprintf(line, sizeof(line), "PSTRACE open64 path=%s flags=0x%x fd=%d\n",
                             path, flags, fd);
        int output = open("/dev/shmem/persistence-open-trace.log",
                          O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (output >= 0) {
            if (count > 0) (void)write(output, line,
                                       (size_t)count < sizeof(line) ? (size_t)count : sizeof(line) - 1);
            (void)close(output);
        }
    }
    return fd;
}
