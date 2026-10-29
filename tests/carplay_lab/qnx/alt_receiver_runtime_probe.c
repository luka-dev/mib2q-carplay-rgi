/* Exercise the hook helper with stock-210 SetCurrentThreadPriority in QNX. */
#include "receiver_runtime.h"
#include "sender_telemetry.h"
#include "sender_cases.h"
#define ALT_RECEIVER_IO_TEST_NO_MAIN
#include "receiver_io_test.c"
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

static alt_receiver_set_priority_t stock_set_priority;
static int wanted;
static int failed;

static int check_sender(void *library)
{
    alt_sender_parser_t parser, absent;
    alt_sender_sample_t sample;
    char output[768];
    struct { char text[17]; unsigned char guard; } bounded;
    unsigned i;
    void *libc = dlopen("/proc/boot/libc.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!libc || alt_sender_parser_init(&absent, libc) != -1) return 1;
    dlclose(libc);
    if (alt_sender_parser_init(&parser, library)) return 2;
    for (i = 0; i < 200; ++i) {
        if (alt_sender_parse(&parser, sender_complete, sizeof(sender_complete), &sample) ||
            sample.valid != 0x3fff || sample.value[0] != 2 || sample.value[7] != 30 ||
            sample.value[9] != 28 || sample.value[3] != 3.25) return 3;
    }
    alt_sender_format(&sample, output, sizeof(output));
    printf("sender complete %s\n", output);
    if (!strstr(output, "submitSurfaceFPS=30.000") || !strstr(output, "sinkOverflowDropFPS=28.000")) return 4;
    if (alt_sender_parse(&parser, sender_partial, sizeof(sender_partial), &sample) ||
        sample.valid != ((1u << 0) | (1u << 8) | (1u << 9) | (1u << 12)) ||
        sample.value[0] != 0 || sample.value[8] != 30.5 || sample.value[9] != 0) return 5;
    alt_sender_format(&sample, output, sizeof(output));
    printf("sender partial %s\n", output);
    if (!strstr(output, "queuedFramesAvg=NA") || !strstr(output, "sinkOverflowDropFPS=0.000")) return 6;
    bounded.guard = 0xA5;
    alt_sender_format(&sample, bounded.text, sizeof(bounded.text));
    if (bounded.guard != 0xA5 || bounded.text[16] != 0) return 7;
    bounded.text[0] = 'Q';
    alt_sender_format(&sample, bounded.text, 0);
    if (bounded.text[0] != 'Q') return 8;
    if (alt_sender_parse(&parser, sender_empty, sizeof(sender_empty), &sample) || sample.valid) return 9;
    if (alt_sender_parse(&parser, sender_array, sizeof(sender_array), &sample) != -1 || sample.valid) return 10;
    if (alt_sender_parse(&parser, sender_complete, 20, &sample) != -1 || sample.valid) return 11;
    if (alt_sender_parse(&parser, sender_complete, ALT_SENDER_PLIST_MAX + 1, &sample) != -1) return 12;
    if (alt_sender_parse(&parser, sender_complete, sizeof(sender_complete) - 1, &sample) != -1) return 13;
    alt_sender_parser_close(&parser);
    alt_sender_parser_close(&parser);
    if (alt_sender_parse(&parser, sender_complete, sizeof(sender_complete), &sample) != -1) return 14;
    puts("ALT_SENDER_TELEMETRY_PASS");
    return 0;
}

static void *worker(void *unused)
{
    alt_receiver_schedule_t state = alt_receiver_schedule(stock_set_priority);
    alt_receiver_schedule_t missing;
    (void)unused;
    printf("receiver requested=%d rc=%d before=%d/%d get=%d after=%d/%d get=%d\n",
           state.requested, state.result, state.before_policy, state.before_priority,
           state.before_error, state.after_policy, state.after_priority, state.after_error);
    if (state.requested != wanted || state.result || state.before_error ||
        state.after_error || state.after_policy != SCHED_FIFO ||
        state.after_priority != wanted) failed = 1;
    missing = alt_receiver_schedule(NULL);
    if (missing.result != ENOSYS || missing.after_error ||
        missing.after_priority != wanted || missing.after_policy != SCHED_FIFO)
        failed = 1;
    return NULL;
}

int main(void)
{
    void *library;
    pthread_t thread;
    int before_policy = -1, after_policy = -1;
    struct sched_param before = {0}, after = {0};
    unsigned i;
    alarm(12);
    library = dlopen("/proc/boot/libairplay.so", RTLD_NOW | RTLD_LOCAL);
    if (!library) { printf("stock dlopen failed: %s\n", dlerror()); return 2; }
    if (alt_receiver_io_check()) return 9;
    {
        int rc = check_sender(library);
        if (rc) { printf("ALT_SENDER_TELEMETRY_FAIL case=%d\n", rc); return 10; }
    }
    stock_set_priority = (alt_receiver_set_priority_t)dlsym(library, "SetCurrentThreadPriority");
    if (!stock_set_priority) return 3;
    if (pthread_getschedparam(pthread_self(), &before_policy, &before)) return 4;
    for (i = 0; i < 2; i++) {
        wanted = i ? 16 : 17;
        if (i) setenv("SCREEN_RECEIVER_THREAD_PRIO", "16", 1);
        else unsetenv("SCREEN_RECEIVER_THREAD_PRIO");
        if (pthread_create(&thread, NULL, worker, NULL) || pthread_join(thread, NULL)) return 5;
        if (pthread_getschedparam(pthread_self(), &after_policy, &after)) return 6;
        if (before_policy != after_policy || before.sched_priority != after.sched_priority)
            failed = 1;
    }
    unsetenv("SCREEN_RECEIVER_THREAD_PRIO");
    printf("parent before=%d/%d after=%d/%d\n", before_policy, before.sched_priority,
           after_policy, after.sched_priority);
    printf("ALT_RX_RUNTIME_%s\n", failed ? "FAIL" : "PASS");
    fflush(stdout);
    return failed;
}
