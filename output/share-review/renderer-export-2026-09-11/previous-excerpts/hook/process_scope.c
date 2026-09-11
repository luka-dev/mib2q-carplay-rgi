/* Copyright (c) 2026, original project author. */
/* Standalone adaptation of the already-public process-scope check. */
#include "process_scope.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int hook_process_name_matches(const char *executable, const char *expected_name)
{
    const char *base;
    if (!executable || !*executable || !expected_name || !*expected_name ||
        strchr(expected_name, '/') != NULL)
        return HOOK_PROCESS_UNKNOWN;
    base = strrchr(executable, '/');
    base = base ? base + 1 : executable;
    if (!*base) return HOOK_PROCESS_UNKNOWN;
    return strcmp(base, expected_name) == 0 ? HOOK_PROCESS_MATCH : HOOK_PROCESS_OTHER;
}

int hook_process_matches(const char *expected_name)
{
    char buffer[512];
    size_t used = 0;
    int result = HOOK_PROCESS_UNKNOWN;
    int saved_errno = errno;
    int fd;
    if (!expected_name || !*expected_name || strchr(expected_name, '/') != NULL)
        return result;
    fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) { errno = saved_errno; return result; }
    while (used < sizeof(buffer)) {
        ssize_t count = read(fd, buffer + used, sizeof(buffer) - used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        used += (size_t)count;
        if (memchr(buffer, '\0', used) != NULL) {
            result = hook_process_name_matches(buffer, expected_name);
            break;
        }
    }
    close(fd);
    errno = saved_errno;
    return result;
}
