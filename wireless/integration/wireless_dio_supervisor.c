/* Keep wireless dio and the iAP2 daemon in one process generation.
 * The SI wired child takes the dio owner lock from dio_owner_exec; when that
 * wireless child exits, this supervisor stops the daemon and removes our IE.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "bridge_quiesce.h"

static volatile sig_atomic_t stopping;

static void on_stop(int signo)
{
    (void)signo;
    stopping = 1;
}

static void pause_ms(int milliseconds)
{
    struct timespec delay = { milliseconds / 1000,
                              (milliseconds % 1000) * 1000000L };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR && !stopping)
        ;
}

static int setting(const char *name, int fallback, int minimum, int maximum)
{
    const char *value = getenv(name);
    if (!value || !*value)
        return fallback;
    char *end;
    long number = strtol(value, &end, 10);
    if (*end || number < minimum || number > maximum)
        return fallback;
    return (int)number;
}

static pid_t spawn(const char *path, char *const args[])
{
    pid_t child = fork();
    if (child == 0) {
        execv(path, args);
        perror("wireless-dio exec");
        _exit(127);
    }
    return child;
}

static pid_t spawn_daemon(const char *path, char *const args[])
{
    pid_t child = fork();
    if (child == 0) {
        const char *preload = getenv("MHI2Q_CP_DAEMON_PRELOAD");
        if (preload && *preload && setenv("LD_PRELOAD", preload, 1) != 0)
            _exit(127);
        execv(path, args);
        perror("wireless-dio daemon exec");
        _exit(127);
    }
    return child;
}

static int reap_if_done(pid_t child, int *status)
{
    pid_t result = waitpid(child, status, WNOHANG);
    return result == child || (result < 0 && errno == ECHILD);
}

static void stop_child(pid_t child)
{
    if (child <= 0)
        return;
    int status;
    if (reap_if_done(child, &status))
        return;
    kill(child, SIGTERM);
    for (int elapsed = 0; elapsed < 2000; elapsed += 100) {
        if (reap_if_done(child, &status))
            return;
        pause_ms(100);
    }
    kill(child, SIGKILL);
    waitpid(child, &status, 0);
}

static int stop_owner(pid_t owner)
{
    int status;
    if (owner <= 0 || reap_if_done(owner, &status))
        return 0;
    if (mhi2q_bridge_quiesce_required()) {
        int attempt;
        for (attempt = 0; attempt < 5; ++attempt) {
            if (mhi2q_bridge_quiesce() == 0)
                break;
            pause_ms(100);
        }
        if (attempt == 5) {
            fprintf(stderr, "[wireless-dio] bridge quiesce not confirmed; owner remains running\n");
            return -1;
        }
    }
    stop_child(owner);
    return 0;
}

static int bridge_ready(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int flags;
    int ready = 0;
    if (fd < 0)
        return 0;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return 0;
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
        ready = 1;
    } else if (errno == EINPROGRESS || errno == EAGAIN ||
               errno == EWOULDBLOCK) {
        fd_set writable;
        struct timeval timeout = {0, 100000};
        int error = 0;
        socklen_t length = sizeof(error);
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        if (select(fd + 1, NULL, &writable, NULL, &timeout) > 0 &&
            FD_ISSET(fd, &writable) &&
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 &&
            error == 0)
            ready = 1;
    }
    close(fd);
    return ready;
}

/* A leftover listener from the previous process is not readiness for a new
 * generation. Require the new helper's inherited owner lock as well. */
static int owner_lock_held_by(pid_t owner)
{
    const char *path = getenv("MHI2Q_DIO_OWNER_LOCK");
    struct flock lock;
    int fd;
    int held;
    if (!path || !*path)
        path = "/mnt/persist/.mhi2q_dio_owner.lock";
    fd = open(path, O_RDWR);
    if (fd < 0)
        return 0;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    held = fcntl(fd, F_GETLK, &lock) == 0 &&
        lock.l_type == F_WRLCK && lock.l_pid == owner;
    close(fd);
    return held;
}

static void clear_ie(const char *daemon)
{
    char *const args[] = { (char *)daemon, "--clear-ie", 0 };
    pid_t child = spawn(daemon, args);
    if (child <= 0) {
        perror("wireless-dio clear-ie fork");
        return;
    }
    int status;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0)
        fprintf(stderr, "[wireless-dio] custom IE cleanup failed\n");
}

int main(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_stop;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, 0);
    sigaction(SIGINT, &action, 0);
    sigaction(SIGHUP, &action, 0);

    const char *home = getenv("H");
    if (!home || !*home)
        home = "/mnt/app/root/hooks";
    char helper[512], wrapper[512], daemon[512], monitor_path[512];
    if (snprintf(helper, sizeof(helper), "%s/dio_owner_exec", home) >= (int)sizeof(helper) ||
        snprintf(wrapper, sizeof(wrapper), "%s/carplay_startup.sh", home) >= (int)sizeof(wrapper) ||
        snprintf(daemon, sizeof(daemon), "%s/mhi2q-wireless-daemon", home) >= (int)sizeof(daemon) ||
        snprintf(monitor_path, sizeof(monitor_path), "%s/carplay_monitor.sh", home) >= (int)sizeof(monitor_path)) {
        fprintf(stderr, "[wireless-dio] hook path too long\n");
        return 127;
    }
    int port = setting("MHI2Q_OWNER_BRIDGE_PORT", 57343, 1, 65535);
    int ready_ms = setting("MHI2Q_OWNER_READY_MS", 15000, 100, 120000);
    int retry_ms = setting("MHI2Q_OWNER_RETRY_MS", 3000, 100, 60000);
#ifdef MHI2Q_QEMU_SUPERVISOR_TRACE
    fprintf(stderr, "[wireless-dio-qemu] ready_ms=%d retry_ms=%d\n", ready_ms, retry_ms);
#endif
    if (access(helper, X_OK) || access(wrapper, X_OK) || access(daemon, X_OK)) {
        fprintf(stderr, "[wireless-dio] helper, wrapper, or daemon unavailable\n");
        return 127;
    }
    if (setenv("MHI2Q_DIO_PUBLISH_OWNER", "1", 1) != 0)
        return 127;

    while (!stopping) {
        char *const owner_args[] = { helper, "wireless", wrapper, 0 };
        pid_t owner = spawn(helper, owner_args);
#ifdef MHI2Q_QEMU_SUPERVISOR_TRACE
        fprintf(stderr, "[wireless-dio-qemu] spawned owner=%ld\n", (long)owner);
#endif
        if (owner <= 0) {
            perror("wireless-dio owner fork");
            return 127;
        }
        int owner_status = 0;
        int owner_done = 0;
        int ready = 0;
        for (int elapsed = 0; elapsed < ready_ms && !stopping; elapsed += 100) {
            if (reap_if_done(owner, &owner_status)) {
                owner_done = 1;
                break;
            }
            if (bridge_ready(port) && owner_lock_held_by(owner)) {
                if (reap_if_done(owner, &owner_status)) {
                    owner_done = 1;
                    break;
                }
                ready = 1;
                break;
            }
            pause_ms(100);
        }
        if (!ready || stopping || !owner_lock_held_by(owner) ||
            (owner_done = reap_if_done(owner, &owner_status))) {
#ifdef MHI2Q_QEMU_SUPERVISOR_TRACE
            fprintf(stderr, "[wireless-dio-qemu] owner not ready pid=%ld ready=%d done=%d status=%d stopping=%d\n",
                    (long)owner, ready, owner_done, owner_status, stopping);
#endif
            if (!owner_done)
                if (stop_owner(owner) != 0)
                    return 127;
            if (!stopping)
                pause_ms(retry_ms);
            continue;
        }

        pid_t monitor = -1;
        if (access(monitor_path, X_OK) == 0) {
            char pid_text[32];
            snprintf(pid_text, sizeof(pid_text), "%ld", (long)owner);
            monitor = fork();
            if (monitor == 0) {
                setenv("LD_PRELOAD", "", 1);
                char *const args[] = { monitor_path, pid_text, 0 };
                execv(monitor_path, args);
                _exit(127);
            }
            if (monitor < 0) {
                if (stop_owner(owner) != 0)
                    return 127;
                if (!stopping)
                    pause_ms(retry_ms);
                continue;
            }
        }

        char *const daemon_args[] = { daemon, 0 };
        pid_t worker = spawn_daemon(daemon, daemon_args);
        if (worker <= 0) {
            if (stop_owner(owner) != 0)
                return 127;
            stop_child(monitor);
            perror("wireless-dio daemon fork");
            return 127;
        }
        fprintf(stderr, "[wireless-dio] generation owner=%ld daemon=%ld\n",
                (long)owner, (long)worker);
        int worker_status = 0;
        while (!stopping) {
            owner_done = reap_if_done(owner, &owner_status);
            int worker_done = reap_if_done(worker, &worker_status);
            if (owner_done || worker_done)
                break;
            pause_ms(100);
        }
        if (stop_owner(owner) != 0)
            return 127;
        stop_child(monitor);
        stop_child(worker);
        clear_ie(daemon);
        fprintf(stderr, "[wireless-dio] generation stopped\n");
        if (!stopping)
            pause_ms(retry_ms);
    }
    return 0;
}
