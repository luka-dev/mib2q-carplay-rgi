/* Offline candidate: serialize wireless and SI-owned dio_manager executions.
 * The locks remain held by this PID across exec of the normal startup script
 * and its final exec of dio_manager. No head-unit operation is performed by
 * building or testing this file.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "bridge_quiesce.h"

/* /tmp is /dev/shmem and returns ENOSYS for F_SETLK. The app partition is
 * mounted read-only at boot; /mnt/persist is the writable QNX6 partition. */
#define DEFAULT_OWNER_LOCK "/mnt/persist/.mhi2q_dio_owner.lock"
#define DEFAULT_PRIORITY_LOCK "/mnt/persist/.mhi2q_dio_wired_priority.lock"
#define EXIT_DEFERRED 75
#define EXIT_OWNER_FAILURE 127
#define WIRED_DEADLINE_MS 7000
#define TERM_GRACE_MS 2000

static long long now_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
        return -1;
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void pause_short(void)
{
    struct timespec t = { 0, 100000000 };
    while (nanosleep(&t, &t) != 0 && errno == EINTR)
        ;
}

static int open_lock(const char *name)
{
    int fd = open(name, O_RDWR | O_CREAT, 0600);
    if (fd < 0)
        return -1;
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0 || fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int try_lock(int fd)
{
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(fd, F_SETLK, &lock) == 0)
        return 1;
    if (errno == EACCES || errno == EAGAIN)
        return 0;
    return -1;
}

static int unlock(int fd)
{
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    return fcntl(fd, F_SETLK, &lock);
}

static pid_t lock_holder(int fd)
{
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(fd, F_GETLK, &lock) != 0)
        return -1;
    return lock.l_type == F_UNLCK ? 0 : lock.l_pid;
}

/* Install-time capability check on the same filesystem as the real locks.
 * A private O_EXCL temp file avoids touching a running owner's lock file. */
static int probe_lock_path(const char *lock_path)
{
    char probe[512];
    int length = snprintf(probe, sizeof(probe), "%s.probe.XXXXXX", lock_path);
    if (length <= 0 || length >= (int)sizeof(probe))
        return -1;
    int fd = mkstemp(probe);
    if (fd < 0)
        return -1;
    int okay = 0;
    if (fchmod(fd, 0600) != 0 || try_lock(fd) != 1)
        goto done;
    pid_t child = fork();
    if (child == 0) {
        close(fd);
        int other = open(probe, O_RDWR);
        int denied = other >= 0 && try_lock(other) == 0;
        if (other >= 0)
            close(other);
        _exit(denied ? 0 : 1);
    }
    if (child < 0)
        goto done;
    int status;
    if (waitpid(child, &status, 0) != child ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        goto done;
    if (unlock(fd) != 0 || try_lock(fd) != 1 || unlock(fd) != 0)
        goto done;
    okay = 1;
done:
    close(fd);
    unlink(probe);
    return okay ? 0 : -1;
}

static int claim_wired(int priority_fd, int owner_fd)
{
    long long start = now_ms();
    if (start < 0)
        return -1;
    long long deadline = start + WIRED_DEADLINE_MS;
    int held = 0;
    while (now_ms() < deadline) {
        held = try_lock(priority_fd);
        if (held != 0)
            break;
        pause_short();
    }
    if (held != 1)
        return -1;

    pid_t signaled = 0;
    pid_t drain_attempted = 0;
    long long term_at = 0;
    int sent_kill = 0;
    while (now_ms() < deadline) {
        held = try_lock(owner_fd);
        if (held == 1)
            return 0;
        if (held < 0)
            return -1;
        pid_t holder = lock_holder(owner_fd);
        if (holder <= 0 || holder == getpid()) {
            pause_short();
            continue;
        }
        if (holder != signaled) {
            if (mhi2q_bridge_quiesce_required()) {
                int quiesce = mhi2q_bridge_quiesce();
                if (quiesce != 0) {
                    /* Only an explicit BUSY permits the wired child to ask
                     * the receiver to end active media. Other failures keep
                     * the wireless owner untouched. */
                    if (quiesce == MHI2Q_RBR_BUSY &&
                        drain_attempted != holder) {
                        drain_attempted = holder;
                        (void)mhi2q_bridge_wired_drain();
                    }
                    pause_short();
                    continue;
                }
            }
            if (lock_holder(owner_fd) != holder)
                continue;
            if (kill(holder, SIGTERM) != 0 && errno != ESRCH)
                return -1;
            signaled = holder;
            term_at = now_ms();
            sent_kill = 0;
        } else if (!sent_kill && now_ms() - term_at >= TERM_GRACE_MS) {
            /* F_GETLK still identifies the same owner; never signal a PID
             * taken only from an unverified pid file. */
            if (lock_holder(owner_fd) == holder) {
                if (kill(holder, SIGKILL) != 0 && errno != ESRCH)
                    return -1;
            }
            sent_kill = 1;
        }
        pause_short();
    }
    if (mhi2q_bridge_quiesce_required())
        fprintf(stderr, "dio_owner_exec: bridge quiesce not confirmed before deadline\n");
    return -1;
}

static int claim_wireless(int priority_fd, int owner_fd)
{
    int claimed = try_lock(owner_fd);
    if (claimed != 1)
        return claimed == 0 ? EXIT_DEFERRED : EXIT_OWNER_FAILURE;
    claimed = try_lock(priority_fd);
    if (claimed != 1)
        return claimed == 0 ? EXIT_DEFERRED : EXIT_OWNER_FAILURE;
    /* SI can acquire this priority lock immediately afterward. It will then
     * identify our owner lock with F_GETLK, stop us, and wait for release. */
    if (unlock(priority_fd) != 0)
        return EXIT_OWNER_FAILURE;
    return 0;
}

static int publish_generation(void)
{
    const char *path = getenv("OWNER_FILE");
    if (!path || !*path)
        path = "/tmp/carplay_supervisor.owner";
    char staged[512];
    int length = snprintf(staged, sizeof(staged), "%s.%ld", path, (long)getpid());
    if (length <= 0 || length >= (int)sizeof(staged))
        return -1;
    int fd = open(staged, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return -1;
    char value[32];
    length = snprintf(value, sizeof(value), "%ld\n", (long)getpid());
    int okay = length > 0 && length < (int)sizeof(value) &&
               write(fd, value, (size_t)length) == length;
    if (close(fd) != 0)
        okay = 0;
    if (!okay || rename(staged, path) != 0) {
        unlink(staged);
        return -1;
    }
    return setenv("MHI2Q_DIO_GENERATION_PUBLISHED", "1", 1);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--check-locks") == 0) {
        const char *owner = getenv("MHI2Q_DIO_OWNER_LOCK");
        const char *priority = getenv("MHI2Q_DIO_WIRED_PRIORITY_LOCK");
        if (!owner || !*owner)
            owner = DEFAULT_OWNER_LOCK;
        if (!priority || !*priority)
            priority = DEFAULT_PRIORITY_LOCK;
        if (probe_lock_path(owner) != 0 || probe_lock_path(priority) != 0) {
            fprintf(stderr, "dio_owner_exec: lock filesystem check failed\n");
            return EXIT_OWNER_FAILURE;
        }
        return 0;
    }
    if (argc < 3 || (strcmp(argv[1], "wired") && strcmp(argv[1], "wireless"))) {
        fprintf(stderr, "usage: dio_owner_exec wired|wireless startup_script [args...]\n");
        return EXIT_OWNER_FAILURE;
    }
    const char *priority_name = getenv("MHI2Q_DIO_WIRED_PRIORITY_LOCK");
    const char *owner_name = getenv("MHI2Q_DIO_OWNER_LOCK");
    if (!priority_name || !*priority_name)
        priority_name = DEFAULT_PRIORITY_LOCK;
    if (!owner_name || !*owner_name)
        owner_name = DEFAULT_OWNER_LOCK;

    int priority_fd = open_lock(priority_name);
    int owner_fd = open_lock(owner_name);
    if (priority_fd < 0 || owner_fd < 0) {
        perror("dio_owner_exec open lock");
        return EXIT_OWNER_FAILURE;
    }

    int wireless = strcmp(argv[1], "wireless") == 0;
    int status = wireless ? claim_wireless(priority_fd, owner_fd)
                          : claim_wired(priority_fd, owner_fd);
    if (status != 0) {
        if (status != EXIT_DEFERRED)
            fprintf(stderr, "dio_owner_exec: claim failed mode=%s errno=%d\n", argv[1], errno);
        return status == EXIT_DEFERRED ? EXIT_DEFERRED : EXIT_OWNER_FAILURE;
    }
    if (setenv("MHI2Q_DIO_OWNER_CLAIMED", "1", 1) != 0 ||
        setenv("MHI2Q_DIO_OWNER_MODE", argv[1], 1) != 0) {
        perror("dio_owner_exec setenv");
        return EXIT_OWNER_FAILURE;
    }
    const char *publish = getenv("MHI2Q_DIO_PUBLISH_OWNER");
    if (publish && strcmp(publish, "1") == 0 && publish_generation() != 0) {
        perror("dio_owner_exec publish generation");
        return EXIT_OWNER_FAILURE;
    }
    execv(argv[2], argv + 2);
    perror("dio_owner_exec execv");
    return EXIT_OWNER_FAILURE;
}
