/* Real-child regression for the production capture/reap engine. */
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern int mhi2q_qnx_capture(const char *, char *const [], char *, unsigned, int *);
static void child(const char *mode) {
    if (!strcmp(mode, "done")) {
        puts("stdout"); fputs("stderr\n", stderr);
        char value;
        assert(read(0, &value, 1) < 0 && errno == EBADF);
        exit(0);
    }
    if (!strcmp(mode, "fail")) exit(7);
    if (!strcmp(mode, "overflow")) {
        for (int n = 0; n < 32768; ++n) putchar('x');
        exit(0);
    }
    signal(SIGTERM, SIG_IGN);
    printf("PID=%ld\n", (long)getpid()); fflush(stdout);
    if (!strcmp(mode, "closed")) { close(1); close(2); }
    if (!strcmp(mode, "flood")) {
        char bytes[4096]; memset(bytes, 'y', sizeof(bytes));
        for (;;) if (write(1, bytes, sizeof(bytes)) < 0) exit(0);
    }
    for (;;) pause();
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--child")) child(argv[2]);
    const char *self = getenv("MHI2Q_CAPTURE_TEST_PATH");
    if (!self) self = argv[0];
    char output[128]; int incomplete;
    char *args[] = {(char *)self, "--child", "done", 0};
    for (int n = 0; n < 10; ++n) {
        int count = mhi2q_qnx_capture(self, args, output, sizeof(output), &incomplete);
        if (count <= 0 || incomplete)
            fprintf(stderr, "capture done iteration=%d result=%d incomplete=%d output=%s\n", n, count, incomplete, output);
        assert(count > 0 && !incomplete);
        assert(strstr(output, "stdout") && strstr(output, "stderr"));
    }
    args[2] = "fail";
    assert(mhi2q_qnx_capture(self, args, output, sizeof(output), &incomplete) == -EIO);
    assert(!incomplete);
    args[2] = "overflow";
    assert(mhi2q_qnx_capture(self, args, output, sizeof(output), &incomplete) == -EOVERFLOW);
    assert(!incomplete && strlen(output) == sizeof(output) - 1);
    for (int n = 0; n < 3; ++n) {
        args[2] = n == 0 ? "hang" : n == 1 ? "closed" : "flood";
        assert(mhi2q_qnx_capture(self, args, output, sizeof(output), &incomplete) == -ETIMEDOUT);
        assert(incomplete);
        long pid = 0; assert(sscanf(output, "PID=%ld", &pid) == 1 && pid > 0);
        assert(waitpid((pid_t)pid, 0, WNOHANG) == -1 && errno == ECHILD);
    }
    assert(mhi2q_qnx_capture("/no/such/program", args, output, sizeof(output), &incomplete) == -ENOENT);
    assert(!incomplete);
    assert(mhi2q_qnx_capture(self, args, output, 1, &incomplete) == -EINVAL);
    puts("capture: output, EOF, failure, overflow, TERM-resistant timeout, flood deadline and reap passed");
    return 0;
}
