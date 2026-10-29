/* Bounded child/output capture. A timeout does not cancel a driver ioctl. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef MHI2Q_CAPTURE_TIMEOUT_MS
#define MHI2Q_CAPTURE_TIMEOUT_MS 3000
#endif
extern char **environ;

static int monotonic_ms(uint64_t *value) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -errno;
    *value = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    return 0;
}

static pid_t capture_spawn(const char *path, char *const argv[], int fd[2]) {
#ifdef __QNXNTO__
    /* QNX's native descriptor-map ABI is not POSIX posix_spawn's ABI. */
    const int fd_map[3] = {SPAWN_FDCLOSED, fd[1], fd[1]};
    return spawn(path, 3, fd_map, 0, argv, environ);
#else
    /* Exercise the same capture/reap logic with real children on the host. */
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (error != 0) { errno = error; return -1; }
    error = posix_spawn_file_actions_adddup2(&actions, fd[1], STDOUT_FILENO);
    if (!error) error = posix_spawn_file_actions_adddup2(&actions, fd[1], STDERR_FILENO);
    if (!error) error = posix_spawn_file_actions_addclose(&actions, STDIN_FILENO);
    if (!error && fd[0] > STDERR_FILENO)
        error = posix_spawn_file_actions_addclose(&actions, fd[0]);
    if (!error && fd[1] > STDERR_FILENO)
        error = posix_spawn_file_actions_addclose(&actions, fd[1]);
    pid_t pid = -1;
    if (!error) error = posix_spawn(&pid, path, &actions, 0, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error) { errno = error; return -1; }
    return pid;
#endif
}

/* Never signal a PID after waitpid has reaped it; it could have been reused.
 * Both grace and reap are bounded even if a kernel operation cannot stop. */
static void stop_child(pid_t pid, int reaped) {
    if (reaped) return;
    (void)kill(pid, SIGTERM);
    for (unsigned attempt = 0; attempt < 30; ++attempt) {
        if (attempt == 10) (void)kill(pid, SIGKILL);
        pid_t got = waitpid(pid, 0, WNOHANG);
        if (got == pid || (got < 0 && errno == ECHILD)) return;
        (void)poll(0, 0, 10);
    }
}

int mhi2q_qnx_capture(const char *path, char *const argv[],
                      char *output, unsigned capacity, int *incomplete) {
    if (incomplete) *incomplete = 0;
    if (!path || !argv || !output || !incomplete || capacity < 2 || capacity > INT_MAX)
        return -EINVAL;
    output[0] = 0;
    uint64_t now;
    int result = monotonic_ms(&now);
    if (result != 0) return result;
    const uint64_t deadline = now + MHI2Q_CAPTURE_TIMEOUT_MS;
    int fd[2];
    if (pipe(fd) != 0) return -errno;
    int flags = fcntl(fd[0], F_GETFL);
    if (flags < 0 || fcntl(fd[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        result = -errno;
        close(fd[0]); close(fd[1]);
        return result;
    }
    pid_t pid = capture_spawn(path, argv, fd);
    int spawn_error = errno;
    close(fd[1]);
    if (pid < 0) { close(fd[0]); return -spawn_error; }

    unsigned used = 0;
    int overflow = 0, reaped = 0, eof = 0, status = 0;
    for (;;) {
        result = monotonic_ms(&now);
        if (result != 0) break;
        if (now >= deadline) { *incomplete = 1; result = -ETIMEDOUT; break; }
        if (!reaped) {
            pid_t got = waitpid(pid, &status, WNOHANG);
            if (got == pid) reaped = 1;
            else if (got < 0 && errno != EINTR) {
                result = -errno;
                if (errno == ECHILD) reaped = 1;
                break;
            }
        }
        if (eof && reaped) {
            result = overflow ? -EOVERFLOW :
                (!WIFEXITED(status) || WEXITSTATUS(status) != 0) ? -EIO : (int)used;
            break;
        }
        struct pollfd readable = {eof ? -1 : fd[0], POLLIN, 0};
        uint64_t remaining = deadline - now;
        int slice = remaining < 20 ? (int)remaining : 20;
        int ready = poll(&readable, 1, slice);
        if (ready < 0) { if (errno == EINTR) continue; result = -errno; break; }
        if (!ready) continue;
        if (readable.revents & POLLNVAL) { result = -EBADF; break; }
        char discard[4096];
        char *dest = used < capacity - 1 ? output + used : discard;
        unsigned space = used < capacity - 1 ? capacity - 1 - used : sizeof(discard);
        ssize_t got = read(fd[0], dest, space);
        if (got > 0) {
            if (dest == discard) overflow = 1;
            else used += (unsigned)got;
        } else if (got == 0) eof = 1;
        else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            result = -errno;
            break;
        }
    }
    close(fd[0]);
    output[used] = 0;
    if (!reaped) *incomplete = 1;
    stop_child(pid, reaped);
    return result;
}
