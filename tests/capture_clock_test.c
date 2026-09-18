#define CAPTURE_CLOCK_TEST 1
#include <assert.h>
#include "../native/capture_clock/capture_clock.c"

#define TEST_TIMER ((timer_t)(uintptr_t)7)
static unsigned writes;
static int rounding, reject, get_fail, start_result;
static struct itimerspec current, seen[4];
static struct itimerspec stock(long ns) {
    struct itimerspec s = {{0, ns}, {0, ns}};
    return s;
}
static int fake_set(timer_t id, int flags, const struct itimerspec *v, struct itimerspec *old) {
    assert(id == TEST_TIMER); (void)flags;
    seen[writes++ % 4] = *v;
    if (reject && v->it_interval.tv_nsec == VC_PERIOD_NS) { errno = EINVAL; return -1; }
    if (old) *old = current;
    current = *v;
    if (rounding && current.it_interval.tv_nsec == VC_PERIOD_NS) current.it_interval.tv_nsec = 34000000L;
    return 0;
}
static int fake_get(timer_t id, struct itimerspec *v) {
    assert(id == TEST_TIMER); if (get_fail) { errno = EIO; return -1; } *v = current; return 0;
}
static int fake_start(void *group, unsigned id) {
    assert(group == (void *)3); assert(id == 9);
    struct itimerspec request = stock(33000000L), old;
    assert(vc_settime(TEST_TIMER, 0, &request, &old) == 0);
    assert(vc_period_is(&old, 100000000L));
    errno = EAGAIN; return start_result;
}
static void fake_resolve(void) {
    real_start = fake_start; real_set = fake_set; real_get = fake_get;
    scope_key_ok = pthread_key_create(&scope_key, NULL) == 0;
    assert(scope_key_ok);
}
static void reset(void) {
    writes = 0; rounding = reject = get_fail = 0; current = stock(100000000L);
}
static void *other_thread(void *arg) {
    struct itimerspec request = stock(33000000L);
    (void)arg;
    assert(pthread_getspecific(scope_key) == NULL);
    assert(vc_settime(TEST_TIMER, 0, &request, NULL) == 0);
    return NULL;
}
int main(void) {
    pthread_once(&resolve_once, fake_resolve);
    uint32_t damaged[3]; memcpy(damaged, vc_start_code, sizeof(damaged)); damaged[2] ^= 1;
    assert(!vc_callsite_matches(VC_START_RETURN, damaged));
    assert(!vc_callsite_matches(VC_START_RETURN+4, vc_start_code));
    assert(!vc_callsite_matches(VC_START_RETURN, NULL));
    for (unsigned mode = 0; mode < 8; ++mode) {
        reset(); rounding = mode == 1; reject = mode == 2; get_fail = mode == 3;
        uintptr_t pc = mode == 4 ? 0 : VC_START_RETURN;
        const void *code = mode == 5 ? damaged : vc_start_code;
        start_result = (int)mode;
        assert(vc_start((void *)3, 9, pc, code, mode == 6 ? 1 : mode == 7 ? ~0u : 0) == start_result && errno == EAGAIN);
        assert(current.it_interval.tv_nsec == (mode == 0 ? VC_PERIOD_NS : 33000000L));
        assert(writes == ((mode >= 1 && mode <= 3) ? 2 : 1));
        assert(pthread_getspecific(scope_key) == NULL);
    }
    capture_scope armed = {1};
    reset(); assert(!pthread_setspecific(scope_key, &armed));
    for (long period = 0; period <= 100000000L; period += 10000000L) {
        struct itimerspec request = stock(period);
        vc_settime(TEST_TIMER, 0, &request, NULL);
        assert(current.it_interval.tv_nsec == period && armed.armed);
    }
    struct itimerspec request = stock(33000000L);
    vc_settime(TEST_TIMER, 1, &request, NULL); /* absolute timer untouched */
    assert(current.it_interval.tv_nsec == 33000000L && armed.armed);
    request.it_interval.tv_nsec = 0; vc_settime(TEST_TIMER, 0, &request, NULL);
    assert(current.it_interval.tv_nsec == 0 && armed.armed); /* one-shot untouched */
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, other_thread, NULL));
    pthread_join(thread, NULL); assert(current.it_interval.tv_nsec == 33000000L);
    /* Nested unrelated start cannot inherit outer capture permission. */
    reset(); start_result = 0; vc_start((void *)3, 9, 0, NULL, 0);
    assert(current.it_interval.tv_nsec == 33000000L && armed.armed);
    assert(pthread_getspecific(scope_key) == &armed);
    request = stock(33000000L); vc_settime(TEST_TIMER, 0, &request, NULL);
    assert(!armed.armed && current.it_interval.tv_nsec == VC_PERIOD_NS);
    vc_settime(TEST_TIMER, 0, &request, NULL); assert(current.it_interval.tv_nsec == 33000000L);
    pthread_setspecific(scope_key, NULL); pthread_key_delete(scope_key);
    puts("capture_clock_test: exact callsite, nested/thread scope, periods, old value, return/errno, rejection/rounding fallback PASS");
}
