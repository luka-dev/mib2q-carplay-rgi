/* Disposable QNX4-RAM-disk exercise for the wireless-only installer. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct file_entry {
    const char *image_name;
    const char *relative_path;
    mode_t mode;
};
#include "stage_map.h"

struct replacement {
    const char *payload;
    const char *path;
    const char *reference;
};
static const struct replacement replacements[] = {
    {"bin/btstack", "/mnt/app/eso/bin/apps/btstack", "reference/btstack.stock"},
    {"lib/libairplay.so", "/mnt/app/eso/lib/libairplay.so", "reference/libairplay.stock.so"},
    {"pf/pf.conf", "/mnt/system/etc/pf.conf", "reference/pf.conf.wired"},
    {"pf/pf.ecm0.conf", "/mnt/system/etc/pf.ecm0.conf", "reference/pf.ecm0.conf.wired"},
    {"pf/pf.mlan0.conf", "/mnt/system/etc/pf.mlan0.conf", "reference/pf.mlan0.conf.wired"},
    {"pf/pf.ppp0.conf", "/mnt/system/etc/pf.ppp0.conf", "reference/pf.ppp0.conf.wired"},
    {"integration/carplay_startup.sh", "/mnt/app/root/hooks/carplay_startup.sh",
     "reference/carplay_startup.wired.sh"},
    {"integration/rc.local.wireless.candidate.sh", "/mnt/app/root/rc.local",
     "reference/rc.local.before.sh"},
};
static const char *const owned[] = {
    "/mnt/app/root/hooks/mhi2q-wireless-daemon",
    "/mnt/app/root/hooks/dio_owner_exec",
    "/mnt/app/root/hooks/wireless_dio_supervisor_bin",
    "/mnt/app/root/hooks/libdio_receiver_bridge.so",
    "/mnt/app/root/hooks/wireless_dio_supervisor.sh",
    "/mnt/app/root/hooks/wireless_carplay.env",
};

static int mkdir_parent(const char *path)
{
    char copy[512];
    if (strlen(path) >= sizeof(copy)) return -1;
    strcpy(copy, path);
    for (char *at = copy + 1; *at; ++at) {
        if (*at != '/') continue;
        *at = '\0';
        if (mkdir(copy, 0755) && errno != EEXIST) return -1;
        *at = '/';
    }
    return 0;
}

static int copy_file(const char *source, const char *destination, mode_t mode,
                     int decode)
{
    if (mkdir_parent(destination)) return -1;
    int input = open(source, O_RDONLY);
    if (input < 0) return -1;
    int output = open(destination, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (output < 0) { close(input); return -1; }
    char buffer[8192];
    ssize_t count;
    while ((count = read(input, buffer, sizeof(buffer))) > 0) {
        if (decode)
            for (ssize_t i = 0; i < count; ++i) buffer[i] ^= (char)0xa5;
        ssize_t done = 0;
        while (done < count) {
            ssize_t written = write(output, buffer + done, (size_t)(count - done));
            if (written <= 0) { close(input); close(output); return -1; }
            done += written;
        }
    }
    close(input);
    close(output);
    return count < 0 ? -1 : chmod(destination, mode);
}

static int same_file(const char *left, const char *right)
{
    int a = open(left, O_RDONLY), b = open(right, O_RDONLY);
    if (a < 0 || b < 0) { if (a >= 0) close(a); if (b >= 0) close(b); return 0; }
    char x[4096], y[4096];
    ssize_t count;
    int same = 1;
    while ((count = read(a, x, sizeof(x))) >= 0) {
        ssize_t other = read(b, y, sizeof(y));
        if (other != count || memcmp(x, y, (size_t)count)) { same = 0; break; }
        if (!count) break;
    }
    if (count < 0) same = 0;
    close(a); close(b);
    return same;
}

static int run_mode(const char *mode, const char *fault)
{
    pid_t child = fork();
    if (child == 0) {
        if (fault) setenv("MHI2Q_TEST_FAIL_PATH", fault, 1);
        else unsetenv("MHI2Q_TEST_FAIL_PATH");
        char *const args[] = {
            "/mnt/owner/bin/sh",
            "/mnt/owner/stage/integration/install_wireless_candidate.sh",
            (char *)mode, 0,
        };
        execv(args[0], args);
        _exit(127);
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) != child) return 127;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static int check_replacements(int installed)
{
    for (size_t i = 0; i < sizeof(replacements) / sizeof(replacements[0]); ++i) {
        const struct replacement *item = &replacements[i];
        char target[512], expected[512], backup[540];
        snprintf(target, sizeof(target), "/mnt/owner/target%s", item->path);
        snprintf(expected, sizeof(expected), "/mnt/owner/stage/%s",
                 installed ? item->payload : item->reference);
        snprintf(backup, sizeof(backup), "%s.wireless-prev", target);
        if (!same_file(target, expected) ||
            (access(backup, F_OK) == 0) != installed) return 0;
    }
    for (size_t i = 0; i < sizeof(owned) / sizeof(owned[0]); ++i) {
        char target[512];
        snprintf(target, sizeof(target), "/mnt/owner/target%s", owned[i]);
        if ((access(target, F_OK) == 0) != installed) return 0;
    }
    return 1;
}

int main(void)
{
    struct timespec delay = {2, 0};
    nanosleep(&delay, 0);
    for (size_t i = 0; i < sizeof(stage_files) / sizeof(stage_files[0]); ++i) {
        char source[100], destination[512];
        snprintf(source, sizeof(source), "/proc/boot/%s", stage_files[i].image_name);
        snprintf(destination, sizeof(destination), "/mnt/owner/stage/%s",
                 stage_files[i].relative_path);
        if (copy_file(source, destination, stage_files[i].mode, 1)) {
            printf("[install-qemu] stage failed %s errno=%d\n",
                   stage_files[i].relative_path, errno);
            return 1;
        }
    }
    for (size_t i = 0; i < sizeof(cli_files) / sizeof(cli_files[0]); ++i) {
        char source[100], destination[160];
        snprintf(source, sizeof(source), "/proc/boot/%s", cli_files[i].image_name);
        snprintf(destination, sizeof(destination), "/mnt/owner/bin/%s",
                 cli_files[i].relative_path);
        if (copy_file(source, destination, 0755, 1)) {
            printf("[install-qemu] CLI stage failed %s errno=%d\n",
                   cli_files[i].relative_path, errno);
            return 2;
        }
    }
    for (size_t i = 0; i < sizeof(replacements) / sizeof(replacements[0]); ++i) {
        char source[512], destination[512];
        snprintf(source, sizeof(source), "/mnt/owner/stage/%s", replacements[i].reference);
        snprintf(destination, sizeof(destination), "/mnt/owner/target%s", replacements[i].path);
        if (copy_file(source, destination, 0755, 0)) return 3;
    }
    setenv("MHI2Q_STAGE", "/mnt/owner/stage", 1);
    setenv("MHI2Q_TARGET_ROOT", "/mnt/owner/target", 1);
    setenv("PATH", "/mnt/owner/bin:/proc/boot", 1);
    setenv("LD_LIBRARY_PATH", "/proc/boot", 1);
    int preflight = run_mode("preflight", 0);
    printf("[install-qemu] preflight=%d\n", preflight);
    fflush(stdout);
    if (preflight || !check_replacements(0)) return 4;
    int missing_locks = run_mode("install", 0);
    printf("[install-qemu] missing persist locks=%d\n", missing_locks);
    fflush(stdout);
    if (!missing_locks || !check_replacements(0)) return 8;
    if (mkdir_parent("/mnt/owner/target/mnt/persist/.probe") != 0) return 9;
    int failed = run_mode("install", "/mnt/system/etc/pf.ecm0.conf");
    printf("[install-qemu] forced failure=%d\n", failed);
    fflush(stdout);
    if (!failed || !check_replacements(0)) return 5;
    int installed = run_mode("install", 0);
    printf("[install-qemu] install=%d\n", installed);
    fflush(stdout);
    if (installed || !check_replacements(1)) return 6;
    int rollback = run_mode("rollback", 0);
    printf("[install-qemu] rollback=%d\n", rollback);
    fflush(stdout);
    if (rollback || !check_replacements(0)) return 7;
    printf("[install-qemu] preflight, failure rollback, install and explicit rollback passed\n");
    fflush(stdout);
    return 0;
}
