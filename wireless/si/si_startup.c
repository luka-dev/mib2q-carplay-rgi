/* Exec-only SI launcher. Diagnostic files must never be boot dependencies.
 * No child, waits, recovery, USB access or fallback to another SI image.
 * Build-time paths deliberately cannot be redirected by inherited env vars. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef SI_STARTUP_DIR
#error SI_STARTUP_DIR must name the installed candidate directory
#endif
#ifndef SI_STARTUP_LOG
#define SI_STARTUP_LOG "/tmp/si-startup.log"
#endif
#ifndef SI_STARTUP_PID
#define SI_STARTUP_PID "/tmp/si-stage1.pid"
#endif
#ifndef SI_STARTUP_STAGE2
#define SI_STARTUP_STAGE2 0
#endif

extern char **environ;

static int hook_variable(const char *entry) {
    const char *name = "MHI2Q_SI_HOOK";
    size_t size = strlen(name);
    return strncmp(entry, name, size) == 0 &&
           (entry[size] == '=' || entry[size] == '_');
}

static int configure(void) {
    /* Remove inherited test overrides, including CHILD_ENVS=wireless.
     * Keep every unrelated factory variable, including LD_LIBRARY_PATH. */
    for (size_t i = 0; environ[i];) {
        if (!hook_variable(environ[i])) { ++i; continue; }
        const char *eq = strchr(environ[i], '=');
        if (!eq) { errno = EINVAL; return -1; }
        size_t size = (size_t)(eq - environ[i]);
        char *name = malloc(size + 1);
        if (!name) return -1;
        memcpy(name, environ[i], size);
        name[size] = 0;
        int result = unsetenv(name);
        free(name);
        if (result) return -1;
    }
    return setenv("LD_PRELOAD", SI_STARTUP_DIR "/libsi_adapter.so", 1) ||
           setenv("MHI2Q_SI_HOOK", "1", 1) ||
           setenv("MHI2Q_SI_HOOK_VERBOSE", "1", 1) ||
           setenv("MHI2Q_SI_HOOK_TRACK_JOBS", "1", 1) ||
           setenv("MHI2Q_SI_HOOK_WIRELESS", SI_STARTUP_STAGE2 ? "1" : "0", 1);
}

int main(int argc, char **argv) {
    (void)argc;
    /* Open optional diagnostics directly, so ENOENT/EROFS/ENOSPC cannot
     * cause shell redirection to abort before SI's exec. */
    int fd = open(SI_STARTUP_LOG, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd >= 0) {
        if (fd != STDOUT_FILENO && dup2(fd, STDOUT_FILENO) < 0)
            perror("si-startup: stdout");
        if (fd != STDERR_FILENO && dup2(fd, STDERR_FILENO) < 0)
            perror("si-startup: stderr");
        if (fd > STDERR_FILENO) close(fd);
    } else {
        fprintf(stderr, "si-startup: optional log unavailable: %s\n", strerror(errno));
    }
    FILE *pid_file = fopen(SI_STARTUP_PID, "w");
    if (pid_file) {
        fprintf(pid_file, "%ld\n", (long)getpid());
        fclose(pid_file);
    }
    if (configure()) { perror("si-startup: environment"); return 126; }
    fprintf(stderr, "si-startup: pid=%ld abi=%d exec=%s\n", (long)getpid(),
            SI_STARTUP_STAGE2 ? 2 : 1, SI_STARTUP_DIR "/smartphone_integrator");
    fflush(NULL);
    argv[0] = (char *)SI_STARTUP_DIR "/smartphone_integrator";
    execve(argv[0], argv, environ);
    perror("si-startup: exec candidate");
    return 127;
}
