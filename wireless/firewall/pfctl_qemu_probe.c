/* Disposable QNX guest: parse pinned PF profiles with the factory pfctl. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int copy_pfctl(void)
{
    int source = open("/proc/boot/pfctl", O_RDONLY);
    int target = open("/dev/shmem/pfctl", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    char bytes[8192];
    ssize_t count;
    if (source < 0 || target < 0)
        return -1;
    while ((count = read(source, bytes, sizeof(bytes))) > 0) {
        ssize_t offset = 0;
        while (offset < count) {
            ssize_t written = write(target, bytes + offset, (size_t)(count - offset));
            if (written <= 0)
                return -1;
            offset += written;
        }
    }
    close(source);
    close(target);
    return count == 0 ? 0 : -1;
}

int main(void)
{
    const char *names[] = {"pf.conf", "pf.ecm0.conf", "pf.mlan0.conf",
                           "pf.ppp0.conf", "wired.pf.conf",
                           "wired.pf.ecm0.conf", "wired.pf.mlan0.conf",
                           "wired.pf.ppp0.conf"};
    int failures = 0;
    if (copy_pfctl() != 0) {
        printf("[pfctl-qemu] copy pfctl errno=%d\n", errno);
        return 2;
    }
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[128];
        int status = -1;
        if (snprintf(path, sizeof(path), "/proc/boot/%s", names[i]) >=
            (int)sizeof(path))
            return 2;
        printf("[pfctl-qemu] begin %s\n", names[i]);
        fflush(stdout);
        pid_t pid = fork();
        if (pid < 0) {
            printf("[pfctl-qemu] %s fork errno=%d\n", names[i], errno);
            ++failures;
            continue;
        }
        if (pid == 0) {
            char *const args[] = {"/dev/shmem/pfctl", "-n",
                "-D", "int_if=lo0", "-D", "wlan_if=lo0",
                "-D", "dbg_if=lo0", "-D", "ppp_if=lo0",
                "-D", "ecm_if=lo0", "-D", "ext_if=lo0",
                "-D", "ex2_if=lo0", "-f", path, 0};
            execv(args[0], args);
            printf("[pfctl-qemu] %s exec errno=%d\n", names[i], errno);
            _exit(127);
        }
        if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
            WEXITSTATUS(status) != 0)
            ++failures;
        printf("[pfctl-qemu] %s status=%d\n", names[i],
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        fflush(stdout);
    }
    printf("[pfctl-qemu] complete failures=%d\n", failures);
    fflush(stdout);
    return failures ? 1 : 0;
}
