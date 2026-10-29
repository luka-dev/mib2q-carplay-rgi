/* Stage-1 spawn-injection proof (guest). The adapter is preloaded into THIS
 * process by the IFS script; this parent calls the QNX-native spawn() twice
 * (a direct PLT call, so the interposer sees it):
 *   1) path contains "carplay_startup.sh" -> adapter must inject
 *      MHI2Q_DIO_OWNER_MODE=wireless into the child's envv;
 *   2) path without the marker -> no injection (negative control).
 * The child prints CHILDENV: lines; the host runner asserts them. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>

static int run_child(const char *path) {
    char *argv[] = {(char *)path, NULL};
    char *envp[] = {
        (char *)"LD_LIBRARY_PATH=/proc/boot",
        (char *)"FROM_PARENT=1",
        NULL,
    };
    int fd_map[3] = {0, 1, 2};
    int status = -1;
    pid_t pid = spawn(path, 3, fd_map, NULL, argv, envp);
    if (pid == -1) {
        printf("CHILDENV:spawn-failed=%s\n", path);
        return -1;
    }
    waitpid(pid, &status, 0);
    printf("SPAWNTEST: %s exit=%d\n", path, (status >> 8) & 0xFF);
    return status;
}

int main(void) {
    printf("=== SPAWNTEST BEGIN (MHI2Q_SI_HOOK=%s) ===\n",
           getenv("MHI2Q_SI_HOOK") ? getenv("MHI2Q_SI_HOOK") : "unset");
    (void)run_child("/proc/boot/carplay_startup.sh_probe"); /* matched */
    (void)run_child("/proc/boot/si_spawn_env_child");        /* negative control */
    printf("=== SPAWNTEST DONE ===\n");
    return 0;
}
