/* QNX-only process probe for dio_owner_exec; no stock binary is changed. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;

static void on_term(int signo)
{
    (void)signo;
    stopping = 1;
}

static void pause_short(void)
{
    struct timespec t = { 0, 100000000 };
    nanosleep(&t, 0);
}

static int mark(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return -1;
    return close(fd);
}

static int exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static int await(const char *path, int ticks)
{
    while (ticks-- > 0) {
        if (exists(path))
            return 1;
        pause_short();
    }
    return 0;
}

static pid_t launch(const char *mode, int sequence)
{
    char helper[64];
    char child_program[64];
    snprintf(helper, sizeof(helper), "/proc/boot/owner_helper_%d", sequence);
    snprintf(child_program, sizeof(child_program), "/proc/boot/owner_child_%d", sequence);
    pid_t child = fork();
    if (child == 0) {
        char *const args[] = { helper, (char *)mode,
            child_program, "child", (char *)mode, 0 };
        execv(args[0], args);
        _exit(127);
    }
    return child;
}

static int ended(pid_t child, int expected)
{
    int status;
    if (waitpid(child, &status, 0) != child)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == expected;
}

int main(int argc, char **argv)
{
    const char *start_w = "/tmp/mhi2q_owner_qemu_wireless_start";
    const char *stop_w = "/tmp/mhi2q_owner_qemu_wireless_stop";
    const char *start_d = "/tmp/mhi2q_owner_qemu_wired_start";
    if (argc == 3 && strcmp(argv[1], "child") == 0) {
        const char *mode = getenv("MHI2Q_DIO_OWNER_MODE");
        if (!mode || strcmp(mode, argv[2]) != 0)
            return 15;
        signal(SIGTERM, on_term);
        if (strcmp(mode, "wireless") == 0) {
            if (mark(start_w) != 0)
                return 16;
            printf("[owner-qemu] wireless started pid=%ld\n", (long)getpid());
        } else {
            if (mark(start_d) != 0)
                return 17;
            printf("[owner-qemu] wired started after wireless stop=%d pid=%ld\n",
                   exists(stop_w), (long)getpid());
        }
        fflush(stdout);
        while (!stopping)
            pause_short();
        if (strcmp(mode, "wireless") == 0)
            mark(stop_w);
        printf("[owner-qemu] %s stopped\n", mode);
        fflush(stdout);
        return 0;
    }

    unlink(start_w);
    unlink(stop_w);
    unlink(start_d);
    pid_t wireless = launch("wireless", 1);
    if (wireless <= 0 || !await(start_w, 30)) {
        printf("[owner-qemu] wireless launch failed errno=%d\n", errno);
        return 1;
    }
    pid_t wired = launch("wired", 2);
    if (wired <= 0 || !await(start_d, 80) || !exists(stop_w) || !ended(wireless, 0)) {
        printf("[owner-qemu] wired takeover failed errno=%d\n", errno);
        return 2;
    }
    pid_t deferred = launch("wireless", 3);
    if (deferred <= 0 || !ended(deferred, 75)) {
        printf("[owner-qemu] wired priority failed errno=%d\n", errno);
        return 3;
    }
    kill(wired, SIGTERM);
    if (!ended(wired, 0)) {
        printf("[owner-qemu] wired release failed\n");
        return 4;
    }
    printf("[owner-qemu] handoff and wired priority passed\n");
    fflush(stdout);
    return 0;
}
