/* Own mDNS lifetime around the existing DIO generation supervisor.
 * All waits are bounded. A responder discovered through IPC is never killed.
 * No PID file is treated as authority; only unreaped direct children are signalled.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;
static void stop_signal(int n) { (void)n; stopping = 1; }
static uint64_t now_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
static const char *value(const char *key, const char *fallback)
{
    const char *s = getenv(key);
    return s && *s ? s : fallback;
}
static int number(const char *key, int fallback, int minimum, int maximum)
{
    const char *s = getenv(key); char *end;
    if (!s || !*s) return fallback;
    long n = strtol(s, &end, 10);
    return *end || n < minimum || n > maximum ? fallback : (int)n;
}
static int reaped(pid_t *pid, int *status)
{
    if (*pid <= 0) return 1;
    pid_t got = waitpid(*pid, status, WNOHANG);
    if (got == *pid || (got < 0 && errno == ECHILD)) { *pid = -1; return 1; }
    return 0;
}
static int wait_child(pid_t *pid, int milliseconds, int *status)
{
    uint64_t end = now_ms() + (unsigned)milliseconds;
    do {
        if (reaped(pid, status)) return 0;
        (void)poll(NULL, 0, 20);
    } while (now_ms() && now_ms() < end);
    return -1;
}
static int stop_child(pid_t *pid, int grace, int force)
{
    int status = 0;
    if (reaped(pid, &status)) return 0;
    (void)kill(*pid, SIGTERM);
    if (!wait_child(pid, grace, &status)) return 0;
    if (!force) return -1;
    (void)kill(*pid, SIGKILL);
    return wait_child(pid, 500, &status);
}
static pid_t launch(const char *path, int responder)
{
    pid_t pid = fork();
    if (!pid) {
        struct sigaction action;
        memset(&action, 0, sizeof(action)); action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        sigaction(SIGTERM, &action, NULL); sigaction(SIGINT, &action, NULL);
        sigaction(SIGHUP, &action, NULL);
        if (responder) {
            const char *preload = getenv("MHI2Q_CP_MDNS_PRELOAD");
            if (preload && *preload) setenv("LD_PRELOAD", preload, 1);
            else unsetenv("LD_PRELOAD");
            execl(path, path, "-f", (char *)NULL);
        } else {
            unsetenv("LD_PRELOAD");
            execl(path, path, (char *)NULL);
        }
        _exit(127);
    }
    return pid;
}
static int probe(const char *path, int timeout)
{
    pid_t pid = launch(path, 0);
    if (pid < 0) return 1;
    int status = 0;
    if (wait_child(&pid, timeout, &status)) {
        (void)stop_child(&pid, 100, 1);
        return 124;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
static int runtime_lock(const char *path)
{
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    struct stat st; struct flock lock;
    if (fd < 0) return -1;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        fchmod(fd, 0600) || fcntl(fd, F_SETFD, FD_CLOEXEC)) { close(fd); return -1; }
    memset(&lock, 0, sizeof(lock)); lock.l_type = F_WRLCK; lock.l_whence = SEEK_SET;
    if (fcntl(fd, F_SETLK, &lock)) { close(fd); return -1; }
    return fd;
}
/* Kernel locks distinguish factory wired teardown from a responder crash in
 * active wireless media. Both locks must identify the same wired owner. */
static pid_t lock_owner(const char *path)
{
    int fd = open(path, O_RDWR); struct flock held;
    if (fd < 0) return errno == ENOENT ? 0 : -1;
    memset(&held, 0, sizeof(held)); held.l_type = F_WRLCK; held.l_whence = SEEK_SET;
    int status = fcntl(fd, F_GETLK, &held); close(fd);
    return status ? -1 : held.l_type == F_UNLCK ? 0 : held.l_pid;
}
static pid_t wired_owner(void)
{
    pid_t owner = lock_owner(value("MHI2Q_DIO_OWNER_LOCK", "/mnt/persist/.mhi2q_dio_owner.lock"));
    pid_t priority = lock_owner(value("MHI2Q_DIO_WIRED_PRIORITY_LOCK", "/mnt/persist/.mhi2q_dio_wired_priority.lock"));
    return owner > 1 && owner == priority ? owner : 0;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !now_ms()) return 78;
    const char *si_endpoint = getenv("MHI2Q_CP_SI_CONTROL");
    int si_mode = si_endpoint != NULL;
    if (si_mode && (si_endpoint[0] != '/' || strlen(si_endpoint)>100 ||
                    strstr(argv[1], "wireless_dio_supervisor"))) return 78;
    struct sigaction action;
    memset(&action, 0, sizeof(action)); action.sa_handler = stop_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL); sigaction(SIGINT, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
    const char *home = value("H", "/mnt/app/root/hooks");
    char default_probe[512];
    if (snprintf(default_probe, sizeof(default_probe), "%s/mhi2q_mdns_probe", home) >= (int)sizeof(default_probe)) return 78;
    const char *check = value("MHI2Q_CP_MDNS_PROBE", default_probe);
    const char *binary = value("MHI2Q_CP_MDNS_BINARY", "/mnt/app/armle/usr/sbin/mdnsd");
    int deadline = number("MHI2Q_CP_MDNS_READY_MS", 8000, 500, 30000);
    int probe_ms = number("MHI2Q_CP_MDNS_PROBE_MS", 1000, 100, 3000);
    int health_ms = number("MHI2Q_CP_MDNS_HEALTH_MS", 5000, 200, 30000);
    int stop_ms = number("MHI2Q_CP_RUNTIME_STOP_MS", 25000, 1000, 60000);
    int lock = runtime_lock(value("MHI2Q_CP_MDNS_LOCK", "/mnt/persist/.mhi2q_mdns_runtime.lock"));
    if (lock < 0) { fprintf(stderr, "[wireless-runtime] runtime already owned or lock unavailable\n"); return 75; }
    pid_t mdns = -1, child = -1;
    int result = 1, status = 0;
    pid_t handoff = 0;
restart:
    result = 1; handoff = 0;
    if (access(check, X_OK) || access(argv[1], X_OK)) goto done;
    handoff = si_mode ? 0 : wired_owner();
    if (handoff > 1) goto done;
    int ready = probe(check, probe_ms);
    if (stopping) goto done;
    if (!ready) fprintf(stderr, mdns > 0 ? "[wireless-runtime] owned mDNS remains ready\n" :
                       "[wireless-runtime] mDNS adopted; external responder preserved\n");
    else if (ready == 2) {
        if (mdns > 0 && !reaped(&mdns, &status)) goto done;
        const char *dir = value("MHI2Q_CP_MDNS_RUN_DIR", "/var/run/mdnsd");
        struct stat st;
        if (stat(dir, &st) && (errno != ENOENT || mkdir(dir, 0755) || stat(dir, &st))) {
            fprintf(stderr, "[wireless-runtime] mDNS run directory unavailable\n"); goto done;
        }
        if (!S_ISDIR(st.st_mode) || access(binary, X_OK)) {
            fprintf(stderr, "[wireless-runtime] mDNS directory/binary invalid\n"); goto done;
        }
        mdns = launch(binary, 1);
        if (mdns < 0) goto done;
        fprintf(stderr, "[wireless-runtime] mDNS child=%ld starting\n", (long)mdns);
        uint64_t end = now_ms() + (unsigned)deadline;
        while (!stopping && now_ms() && now_ms() < end) {
            if (reaped(&mdns, &status)) goto done;
            ready = probe(check, probe_ms);
            if (!ready) break;
            if (ready != 2) goto done;
            (void)poll(NULL, 0, 100);
        }
        if (stopping || ready || reaped(&mdns, &status)) goto done;
        fprintf(stderr, "[wireless-runtime] mDNS ready via IPC\n");
    } else {
        fprintf(stderr, "[wireless-runtime] mDNS IPC unknown/unresponsive status=%d; no responder started\n", ready);
        goto done;
    }
    if (setenv("MHI2Q_CP_MDNS_MANAGED", "1", 1) ||
        setenv("MHI2Q_CP_MDNS_BINARY", binary, 1)) goto done;
    if (child <= 0) {
        child = launch(argv[1], 0);
        if (child < 0) goto done;
        fprintf(stderr, "[wireless-runtime] %s child=%ld\n", si_mode ? "bootstrap" : "supervisor", (long)child);
    }
    uint64_t next = now_ms() + (unsigned)health_ms;
    result = 0;
    while (!stopping) {
        if (reaped(&child, &status)) {
            result = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            break;
        }
        handoff = si_mode ? 0 : wired_owner();
        if (handoff > 1) { result = 0; break; }
        if (mdns > 0 && reaped(&mdns, &status)) {
            if (si_mode) {
                fprintf(stderr, "[wireless-runtime] restoring mDNS; SI bootstrap retained\n");
                goto restart;
            }
            handoff = wired_owner();
            fprintf(stderr, "[wireless-runtime] owned mDNS exited\n"); result = 1; break;
        }
        if (!now_ms()) { result = 1; break; }
        if (now_ms() >= next) {
            int health = probe(check, probe_ms);
            if (health) {
                if (si_mode && health == 2) goto restart;
                handoff = wired_owner();
                fprintf(stderr, "[wireless-runtime] mDNS IPC lost status=%d\n", health); result = 1; break;
            }
            next = now_ms() + (unsigned)health_ms;
        }
        (void)poll(NULL, 0, 100);
    }
done:
    /* The DIO supervisor may refuse to stop while a session is still active.
     * In that case keep mDNS alive and do not force-kill the supervisor. */
    if (child > 0 && stop_child(&child, stop_ms, 0)) {
        fprintf(stderr, "[wireless-runtime] supervisor did not finish; mDNS preserved for recovery\n");
        result = 1;
    } else {
        if (handoff > 1) {
            fprintf(stderr, "[wireless-runtime] wired owner active; wireless supervisor suspended\n");
            /* A connected wired owner may remain indefinitely. This is an
             * interruptible supervision state, with no child wait or IPC. */
            pid_t priority;
            do {
                if (mdns > 0) (void)reaped(&mdns, &status);
                priority = lock_owner(value("MHI2Q_DIO_WIRED_PRIORITY_LOCK", "/mnt/persist/.mhi2q_dio_wired_priority.lock"));
                if (!stopping && priority > 0) (void)poll(NULL, 0, 100);
            } while (!stopping && priority > 0);
            if (mdns > 0) (void)reaped(&mdns, &status);
            if (stopping) { result = mdns > 0 ? 1 : 0; goto finish; }
            if (priority < 0 || lock_owner(value("MHI2Q_DIO_OWNER_LOCK", "/mnt/persist/.mhi2q_dio_owner.lock")) != 0)
                goto finish;
            fprintf(stderr, "[wireless-runtime] wired release; restoring mDNS and supervisor\n");
            goto restart;
        }
        /* A supervisor can leave an owner after refusing QUIESCE.
         * Preserve discovery if an owner lock is still held. */
        const char *owner_path = value("MHI2Q_DIO_OWNER_LOCK", "/mnt/persist/.mhi2q_dio_owner.lock");
        int fd = open(owner_path, O_RDWR); struct flock held;
        memset(&held, 0, sizeof(held)); held.l_type = F_WRLCK; held.l_whence = SEEK_SET;
        int busy = fd < 0 ? errno != ENOENT :
            (fcntl(fd, F_GETLK, &held) || held.l_type != F_UNLCK);
        if (fd >= 0) close(fd);
        if (busy) {
            fprintf(stderr, "[wireless-runtime] owner remains; mDNS preserved for recovery\n");
            result = 1;
        }
        else if (stop_child(&mdns, 3000, 1)) result = 1;
    }
finish:
    close(lock);
    fprintf(stderr, "[wireless-runtime] exit=%d\n", result);
    return result;
}
