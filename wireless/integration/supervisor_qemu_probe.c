/* QNX-only, disposable process probe for wireless supervisor handoff. */
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

static void pause_short(void)
{
    struct timespec delay = { 0, 100000000 };
    nanosleep(&delay, 0);
}

static int copy_program(const char *source, const char *name)
{
    char destination[120];
    snprintf(destination, sizeof(destination), "/mnt/owner/%s", name);
    int from = open(source, O_RDONLY);
    int to = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (from < 0 || to < 0)
        return -1;
    char buffer[4096];
    ssize_t count;
    while ((count = read(from, buffer, sizeof(buffer))) > 0) {
        ssize_t done = 0;
        while (done < count) {
            ssize_t n = write(to, buffer + done, (size_t)(count - done));
            if (n <= 0)
                return -1;
            done += n;
        }
    }
    close(from);
    close(to);
    return count < 0 ? -1 : chmod(destination, 0755);
}

static int count_event(const char *prefix)
{
    FILE *file = fopen("/mnt/owner/events", "r");
    if (!file)
        return 0;
    char line[160];
    int count = 0;
    while (fgets(line, sizeof(line), file))
        if (strncmp(line, prefix, strlen(prefix)) == 0)
            ++count;
    fclose(file);
    return count;
}

static int await_event(const char *prefix, int target, int ticks)
{
    while (ticks-- > 0) {
        if (count_event(prefix) >= target)
            return 1;
        pause_short();
    }
    return 0;
}

#ifdef MHI2Q_QEMU_REQUIRE_QUIESCE
static int quiesce_precedes_owner_stop(int expected)
{
    FILE *file = fopen("/mnt/owner/events", "r");
    char line[160];
    int armed = 0;
    int completed = 0;
    if (!file) return 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "bridge quiesce ", 15) == 0)
            armed = 1;
        else if (strncmp(line, "wireless stop ", 14) == 0) {
            if (!armed) { fclose(file); return 0; }
            armed = 0;
            ++completed;
        }
    }
    fclose(file);
    return completed == expected;
}
#endif

static pid_t launch(const char *path, const char *mode, const char *wrapper)
{
    pid_t child = fork();
    if (child == 0) {
        if (mode) {
            char *const args[] = { (char *)path, (char *)mode, (char *)wrapper, 0 };
            execv(path, args);
        } else {
            char *const args[] = { (char *)path, 0 };
            execv(path, args);
        }
        _exit(127);
    }
    return child;
}

int main(void)
{
    if (copy_program("/proc/boot/dio_owner_exec", "dio_owner_exec") ||
        copy_program("/proc/boot/wireless_dio_supervisor_bin", "wireless_dio_supervisor_bin") ||
        copy_program("/proc/boot/supervisor_fake_child", "carplay_startup.sh") ||
        copy_program("/proc/boot/supervisor_fake_child", "carplay_monitor.sh") ||
        copy_program("/proc/boot/supervisor_fake_child", "mhi2q-wireless-daemon")) {
        printf("[supervisor-qemu] staging failed errno=%d\n", errno);
        return 1;
    }
    setenv("H", "/mnt/owner", 1);
    setenv("EVENTS", "/mnt/owner/events", 1);
    setenv("MHI2Q_DIO_OWNER_LOCK", "/mnt/owner/owner.lock", 1);
    setenv("MHI2Q_DIO_WIRED_PRIORITY_LOCK", "/mnt/owner/priority.lock", 1);
    setenv("OWNER_FILE", "/mnt/owner/generation", 1);
    setenv("MHI2Q_OWNER_BRIDGE_PORT", "57343", 1);
    setenv("MHI2Q_OWNER_READY_MS", "5000", 1);
    setenv("MHI2Q_OWNER_RETRY_MS", "200", 1);
#ifdef MHI2Q_QEMU_REQUIRE_QUIESCE
    /* The fake peer must serve requests, but the owner/supervisor binaries
     * must require QUIESCE even without the runtime configuration variable. */
    setenv("MHI2Q_FAKE_BRIDGE_QUIESCE", "1", 1);
    setenv("MHI2Q_FAKE_BRIDGE_BUSY_ONCE", "1", 1);
#endif
    pid_t supervisor = launch("/mnt/owner/wireless_dio_supervisor_bin", 0, 0);
    if (supervisor <= 0 || !await_event("daemon start ", 1, 100) ||
        !await_event("monitor start ", 1, 30)) {
        printf("[supervisor-qemu] first daemon failed\n");
        return 2;
    }
    printf("[supervisor-qemu] first wireless and daemon started\n");
    fflush(stdout);
    pid_t wired = launch("/mnt/owner/dio_owner_exec", "wired",
                         "/mnt/owner/carplay_startup.sh");
    if (wired <= 0 || !await_event("wired start ", 1, 100) ||
        !await_event("clear ", 1, 100) || count_event("wireless stop ") != 1 ||
        count_event("daemon stop ") != 1 || count_event("monitor stop ") != 1) {
        printf("[supervisor-qemu] wired handoff or cleanup failed\n");
        return 3;
    }
#ifdef MHI2Q_QEMU_REQUIRE_QUIESCE
    if (count_event("bridge busy ") != 1 ||
        count_event("bridge quiesce ") != 1 ||
        !quiesce_precedes_owner_stop(1)) {
        printf("[supervisor-qemu] wired takeover skipped bridge quiesce\n");
        return 6;
    }
#endif
    printf("[supervisor-qemu] wired handoff stopped daemon and cleared IE\n");
    fflush(stdout);
    kill(wired, SIGTERM);
    waitpid(wired, 0, 0);
    if (!await_event("daemon start ", 2, 100) ||
        !await_event("monitor start ", 2, 30)) {
        printf("[supervisor-qemu] wireless restart failed\n");
        return 4;
    }
    kill(supervisor, SIGTERM);
    waitpid(supervisor, 0, 0);
    if (!await_event("clear ", 2, 30) ||
        count_event("monitor stop ") != 2) {
        printf("[supervisor-qemu] final cleanup failed\n");
        return 5;
    }
#ifdef MHI2Q_QEMU_REQUIRE_QUIESCE
    if (count_event("bridge busy ") != 2 ||
        count_event("bridge quiesce ") != 2 ||
        !quiesce_precedes_owner_stop(2)) {
        printf("[supervisor-qemu] supervisor shutdown skipped bridge quiesce\n");
        return 7;
    }
#endif
    printf("[supervisor-qemu] lifecycle passed\n");
    fflush(stdout);
    return 0;
}
