/* Disposable daemon peer for a QNX supervisor + real media owner fixture. */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;

static void on_stop(int signo)
{
    (void)signo;
    stopping = 1;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--clear-ie") == 0) {
        puts("[supervisor-idle-qemu] IE cleanup");
        fflush(stdout);
        return 0;
    }
    signal(SIGTERM, on_stop);
    puts("[supervisor-idle-qemu] daemon started");
    fflush(stdout);
    while (!stopping)
        sleep(1);
    puts("[supervisor-idle-qemu] daemon stopped");
    fflush(stdout);
    return 0;
}
