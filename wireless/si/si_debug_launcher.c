/* Disposable-QNX diagnostic only. Debug our own SPAWN_HOLD child; no hooks,
 * no changes to SI instructions, registers or service return values. */
#include <sys/procfs.h>
#include <sys/wait.h>
#include <devctl.h>
#include <spawn.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;
static volatile sig_atomic_t child;
static volatile sig_atomic_t expired;

static void timeout(int sig) {
    (void)sig;
    static const char message[] = "SI-DEBUG: diagnostic window expired; stopping for snapshot\n";
    (void)write(1, message, sizeof(message) - 1);
    expired = 1;
    if (child > 1) kill(child, SIGSTOP);
}

static void object_at(int fd, uint32_t va, const char *label) {
    union { procfs_debuginfo info; char data[sizeof(procfs_debuginfo) + 512]; } d;
    memset(&d, 0, sizeof(d));
    d.info.vaddr = va;
    int e = devctl(fd, DCMD_PROC_MAPDEBUG, &d, sizeof(d), NULL);
    printf("SI-DEBUG: %s=%#x object_va=%#llx err=%d path=%s\n", label,
           va, (unsigned long long)d.info.vaddr, e, e ? "?" : d.info.path);
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGALRM, timeout);
    alarm(25);
    struct inheritance inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.flags = SPAWN_HOLD | SPAWN_DEBUG;
    int fds[] = {0, 1, 2};
    const char *preload = getenv("SI_DEBUG_PRELOAD");
    if (preload && setenv("LD_PRELOAD", preload, 1)) return 1;
    child = spawn(argv[1], 3, fds, &inherit, argv + 1, environ);
    if (child < 1) { printf("SI-DEBUG: spawn errno=%d\n", errno); return 1; }
    char path[48];
    snprintf(path, sizeof(path), "/proc/%d/as", (int)child);
    int fd = open(path, O_RDWR);
    if (fd < 0) { printf("SI-DEBUG: open errno=%d\n", errno); kill(child, SIGKILL); return 1; }
    procfs_status status;
    memset(&status, 0, sizeof(status));
    int e = devctl(fd, DCMD_PROC_STOP, &status, sizeof(status), NULL);
    printf("SI-DEBUG: attach pid=%d err=%d flags=%#x ip=%#llx\n",
           (int)child, e, status.flags, (unsigned long long)status.ip);
    procfs_run run;
    memset(&run, 0, sizeof(run));
    run.flags = _DEBUG_RUN_TRACE | _DEBUG_RUN_CLRSIG | _DEBUG_RUN_CLRFLT;
    sigemptyset(&run.trace);
    sigaddset(&run.trace, SIGSEGV);
    sigaddset(&run.trace, SIGBUS);
    sigaddset(&run.trace, SIGILL);
    sigaddset(&run.trace, SIGABRT);
    sigaddset(&run.trace, SIGSTOP);
    for (int attempt = 0; attempt < 8; ++attempt) {
        e = devctl(fd, DCMD_PROC_RUN, &run, sizeof(run), NULL);
        printf("SI-DEBUG: run err=%d attempt=%d\n", e, attempt);
        if (e) break;
        kill(child, SIGCONT);
        usleep(100000);
        e = devctl(fd, DCMD_PROC_WAITSTOP, &status, sizeof(status), NULL);
        if (expired) {
            memset(&status, 0, sizeof(status));
            e = devctl(fd, DCMD_PROC_STOP, &status, sizeof(status), NULL);
        }
        if (e || expired || status.why != 0) break;
    }
    printf("SI-DEBUG: stopped err=%d why=%u what=%u tid=%u ip=%#llx sp=%#llx sig=%d addr=%p\n",
           e, status.why, status.what, (unsigned)status.tid,
           (unsigned long long)status.ip, (unsigned long long)status.sp,
           status.info.si_signo, status.info.si_addr);
    procfs_greg regs;
    memset(&regs, 0, sizeof(regs));
    e = devctl(fd, DCMD_PROC_GETGREG, &regs, sizeof(regs), NULL);
    printf("SI-DEBUG: registers err=%d\n", e);
    if (!e) {
        for (int i = 0; i < 16; ++i) printf("SI-DEBUG: r%d=%#x\n", i, regs.arm.gpr[i]);
        object_at(fd, regs.arm.gpr[15], "PC");
        object_at(fd, regs.arm.gpr[14], "LR");
    }
    uint32_t stack[64];
    ssize_t n = e ? -1 : pread(fd, stack, sizeof(stack), (off_t)regs.arm.gpr[13]);
    if (n > 0) for (unsigned i = 0; i < (unsigned)n / 4; i += 4)
        printf("SI-DEBUG: stack+%03x %08x %08x %08x %08x\n", i*4,
               stack[i], stack[i+1], stack[i+2], stack[i+3]);
    int count = 0;
    if (!devctl(fd, DCMD_PROC_MAPINFO, NULL, 0, &count) && count > 0 && count < 2048) {
        procfs_mapinfo *maps = calloc((size_t)count, sizeof(*maps));
        int capacity = count;
        if (maps && !devctl(fd, DCMD_PROC_MAPINFO, maps, (size_t)count * sizeof(*maps), &count))
            for (int i = 0; i < count && i < capacity; ++i)
                object_at(fd, (uint32_t)maps[i].vaddr, "MAP");
        free(maps);
    }
    kill(child, SIGKILL);
    close(fd);
    waitpid(child, NULL, 0);
    alarm(0);
    puts("=== SI-DEBUG DONE ===");
    return 0;
}
