/* MU1316 capture cadence candidate. Load ONLY in videoencoderservice, never
 * system-wide or in dio_manager. No new threads, clocks or timer ownership.
 * The OEM TimerGroup and OSAL still create, dispatch, stop and delete the timer.
 * See docs/VC_CAPTURE_CLOCK.md for binary guards and activation limitations. */
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <dlfcn.h>
#if defined(CAPTURE_CLOCK_TEST) && defined(__APPLE__)
/* Darwin has no POSIX interval timers. The test supplies deterministic stubs. */
typedef int timer_t;
struct itimerspec { struct timespec it_interval, it_value; };
#endif

#define VC_START_SYMBOL "_ZN4util10TimerGroup10startTimerEj"
#define VC_START_RETURN ((uintptr_t)0x10a85c)
#define VC_PERIOD_NS 33333333L /* nearest representable ns to 1/30 s */
static const uint32_t vc_start_code[] = {0xe1a00009, 0xe59d100c, 0xebffe4ff};

static int vc_callsite_matches(uintptr_t pc, const void *instructions) {
    return pc == VC_START_RETURN && instructions &&
        memcmp(instructions, vc_start_code, sizeof(vc_start_code)) == 0;
}
static int vc_period_is(const struct itimerspec *v, long ns) {
    return v && v->it_value.tv_sec == 0 && v->it_value.tv_nsec == ns &&
        v->it_interval.tv_sec == 0 && v->it_interval.tv_nsec == ns;
}

typedef int (*start_fn)(void *, unsigned);
typedef int (*set_fn)(timer_t, int, const struct itimerspec *, struct itimerspec *);
typedef int (*get_fn)(timer_t, struct itimerspec *);
static start_fn real_start;
static set_fn real_set;
static get_fn real_get;
static pthread_once_t resolve_once = PTHREAD_ONCE_INIT;
static pthread_key_t scope_key;
static int scope_key_ok;
/* Stack scoped, QNX 6.5 compatible pthread TLS (no __thread / emutls). */
typedef struct { int armed; } capture_scope;

static void vc_resolve(void) {
    real_start = (start_fn)dlsym(RTLD_NEXT, VC_START_SYMBOL);
    real_set = (set_fn)dlsym(RTLD_NEXT, "timer_settime");
    real_get = (get_fn)dlsym(RTLD_NEXT, "timer_gettime");
    scope_key_ok = pthread_key_create(&scope_key, NULL) == 0;
}

static int vc_start(void *group, unsigned id, uintptr_t caller, const void *code, unsigned remote_display) {
    capture_scope scope = {0};
    void *previous;
    int rc, saved_errno;
    pthread_once(&resolve_once, vc_resolve);
    if (!real_start) { errno = ENOSYS; return 1; } /* util/OSAL: zero is success */
    if (!scope_key_ok) return real_start(group, id);
    previous = pthread_getspecific(scope_key);
    scope.armed = remote_display == 0 && real_set && real_get && vc_callsite_matches(caller, code);
    /* Every nested TimerGroup::startTimer gets its own scope, even an unrelated
     * one; it cannot borrow the outer video start's authorization. */
    if (pthread_setspecific(scope_key, &scope) != 0) return real_start(group, id);
    rc = real_start(group, id);
    saved_errno = errno;
    if (pthread_setspecific(scope_key, previous) != 0) {
        /* A stale stack pointer is worse than stopping this isolated service. */
        abort();
    }
    errno = saved_errno;
    return rc;
}

#ifndef CAPTURE_CLOCK_TEST
/* entry_arm.S captures r6 before a C prologue can repurpose it. At this exact
 * OEM callsite r6 is remoteDisplayId: 0 = VC (display 4), 1 = other display.
 * All ordinary arguments / return registers retain their original ABI. */
__attribute__((visibility("hidden"), noinline))
int vc_start_guarded(void *group, unsigned id, uintptr_t pc, unsigned remote_display) {
    return vc_start(group, id, pc,
        pc == VC_START_RETURN ? (const void *)(pc - sizeof(vc_start_code)) : NULL,
        remote_display);
}
__attribute__((visibility("default")))
int timer_settime(timer_t id, int flags, const struct itimerspec *v, struct itimerspec *old)
#else
static int vc_settime(timer_t id, int flags, const struct itimerspec *v, struct itimerspec *old)
#endif
{
    capture_scope *scope;
    struct itimerspec precise, prior, actual;
    int rc, saved_errno;
    pthread_once(&resolve_once, vc_resolve);
    if (!real_set) { errno = ENOSYS; return -1; }
    scope = scope_key_ok ? (capture_scope *)pthread_getspecific(scope_key) : NULL;
    if (!scope || !scope->armed || !real_get || flags != 0 || !vc_period_is(v, 33000000L))
        return real_set(id, flags, v, old);
    scope->armed = 0; /* at most one native timer per verified video start */
    precise = *v;
    precise.it_value.tv_nsec = precise.it_interval.tv_nsec = VC_PERIOD_NS;
    rc = real_set(id, flags, &precise, &prior);
    if (rc != 0) return real_set(id, flags, v, old); /* driver rejected candidate */
    if (real_get(id, &actual) == 0 && actual.it_interval.tv_sec == 0 &&
            actual.it_interval.tv_nsec == VC_PERIOD_NS) {
        if (old) *old = prior;
        fprintf(stderr, "vc_capture_clock: 30 Hz requested; native interval readback=%ld ns (delivery cadence unverified)\n", VC_PERIOD_NS);
        return 0;
    }
    /* QNX may quantize the interval to its timer resolution. A 34/40 ms repeat
     * would be slower than stock: restore the original request in that case.
     * Preserve the caller's original old-value semantics, not the trial timer. */
    rc = real_set(id, flags, v, NULL);
    saved_errno = errno;
    if (rc == 0 && old) *old = prior;
    fprintf(stderr, "vc_capture_clock: precise interval unsupported; restored stock 33 ms result=%d\n", rc);
    errno = saved_errno;
    return rc;
}
