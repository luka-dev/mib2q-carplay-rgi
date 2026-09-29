/* QNX 6.5 process output capture through its native spawn/fd-map ABI. */
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

int mhi2q_qnx_capture(const char *path, char *const argv[],
                      char *output, unsigned capacity) {
    if (!path || !argv || !output || capacity < 2) return -EINVAL;
    int fd[2];
    if (pipe(fd) != 0) {
        fprintf(stderr, "[qnx-capture] pipe errno=%d\n", errno);
        return -errno;
    }
    /* QNX spawn uses an explicit fd map; capture both output streams. */
    const int fd_map[3] = {SPAWN_FDCLOSED, fd[1], fd[1]};
    pid_t pid = spawn(path, 3, fd_map, 0, argv, environ);
    int spawn_error = errno;
    close(fd[1]);
    if (pid < 0) {
        fprintf(stderr, "[qnx-capture] spawn %s errno=%d\n", path, spawn_error);
        close(fd[0]);
        return -spawn_error;
    }
    unsigned used = 0;
    int overflow = 0;
    for (;;) {
        char discard[256];
        char *dest = used < capacity - 1 ? output + used : discard;
        unsigned space = used < capacity - 1 ? capacity - 1 - used : sizeof(discard);
        ssize_t got = read(fd[0], dest, space);
        if (got > 0) {
            if (dest == discard) overflow = 1;
            else used += (unsigned)got;
        } else if (got == 0) {
            break;
        } else if (errno != EINTR) {
            int read_error = errno;
            close(fd[0]);
            (void)waitpid(pid, 0, 0);
            return -read_error;
        }
    }
    close(fd[0]);
    output[used] = 0;
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -errno;
    }
    if (overflow) return -EOVERFLOW;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -EIO;
    return (int)used;
}
