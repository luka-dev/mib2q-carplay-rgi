/* Exact-MU1316 copy-property handoff for the offline receiver bridge.
 *
 * The stock provider posts a worker event and waits on a shared signal.
 * A previously signalled flag can make the wait return before this request's
 * result is ready. Once a current result is returned, manager D1 also owns
 * the same +0x1f0 pointer and releases it again at teardown. This interposer
 * serializes requests, clears the signal before posting, and transfers the
 * matching result out of the manager slot. It never moves value creation to
 * the AirPlay thread. All fixed addresses are guarded against the pinned
 * MU1316 dio_manager image before the delegate is changed.
 */
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

extern char *__progname;
extern void *mhi2q_probe_manager(void);

typedef void *(*copy_property_fn)(void *, void *, const void *, int *, void *);
typedef void (*session_created_fn)(void *, void *, void *);
typedef void (*session_failed_fn)(void *, int, void *);
typedef void (*set_delegate_fn)(void *, const void *);

static pthread_mutex_t property_gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t callback_gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t callback_drained = PTHREAD_COND_INITIALIZER;
static unsigned int callbacks_inflight;
static copy_property_fn original_copy_property;
static session_created_fn original_session_created;
static session_failed_fn original_session_failed;
static volatile unsigned int property_installed;
static volatile unsigned int property_failed;
static volatile unsigned int property_quiesced;
#ifdef MHI2Q_PROPERTY_HANDOFF_RACE_QEMU
static volatile unsigned int property_race_inflight;
static volatile unsigned int property_race_armed;
void mhi2q_property_handoff_qemu_arm(unsigned int armed)
{
    __sync_lock_test_and_set(&property_race_armed, armed);
}
unsigned int mhi2q_property_handoff_qemu_inflight(void)
{
    return __sync_val_compare_and_swap(&property_race_inflight, 0, 0);
}
#endif

#ifndef MHI2Q_PROPERTY_HANDOFF_FORCE_GUARD_FAIL_QEMU
static int code_matches(uintptr_t address, const uint8_t *bytes, size_t length)
{
    return memcmp((const void *)address, bytes, length) == 0;
}
#endif

static int exact_property_image(void)
{
#ifdef MHI2Q_PROPERTY_HANDOFF_FORCE_GUARD_FAIL_QEMU
    return 0;
#else
    static const uint8_t delegate_call[] = {
        0x4c, 0x30, 0x8d, 0xe5, 0xc5, 0xd8, 0xfe, 0xeb,
        0x00, 0xc0, 0xa0, 0xe3
    };
    static const uint8_t provider_post_wait[] = {
        0xca, 0x22, 0xff, 0xeb, 0x04, 0x00, 0xa0, 0xe1,
        0x00, 0x10, 0xe0, 0xe3, 0x55, 0xfb, 0xff, 0xeb
    };
    static const uint8_t wait_wrapper[] = {
        0x19, 0x0e, 0x80, 0xe2, 0x00, 0x20, 0xa0, 0xe3,
        0x09, 0x7c, 0x00, 0xea
    };
    static const uint8_t signal_entry[] = {
        0xf0, 0x40, 0x2d, 0xe9, 0x10, 0x60, 0x80, 0xe2,
        0x0c, 0xd0, 0x4d, 0xe2
    };
    static const uint8_t wait_entry[] = {
        0xf0, 0x4f, 0x2d, 0xe9, 0x01, 0x70, 0xa0, 0xe1,
        0xc8, 0x12, 0x9f, 0xe5
    };
    static const uint8_t worker_result[] = {
        0x05, 0x20, 0xa0, 0xe1, 0x04, 0x00, 0xa0, 0xe1,
        0x01, 0x30, 0xa0, 0xe1, 0x7d, 0xcf, 0x84, 0xe2,
        0x00, 0xc0, 0x8d, 0xe5, 0xe3, 0xfc, 0xff, 0xeb,
        0xf0, 0x01, 0x84, 0xe5
    };
    static const uint8_t destructor_release[] = {
        0xf0, 0x01, 0x94, 0xe5, 0x00, 0x00, 0x50, 0xe3,
        0x00, 0x00, 0x00, 0x0a, 0x7e, 0xdc, 0xfe, 0xeb,
        0xe0, 0x02, 0x94, 0xe5
    };
    return code_matches(0x0015c6c0u, delegate_call, sizeof(delegate_call)) &&
           code_matches(0x001586ecu, provider_post_wait, sizeof(provider_post_wait)) &&
           code_matches(0x00157454u, wait_wrapper, sizeof(wait_wrapper)) &&
           code_matches(0x0017638cu, signal_entry, sizeof(signal_entry)) &&
           code_matches(0x00176488u, wait_entry, sizeof(wait_entry)) &&
           code_matches(0x00160a74u, worker_result, sizeof(worker_result)) &&
           code_matches(0x0015b498u, destructor_release, sizeof(destructor_release));
#endif
}

static int callback_enter(void)
{
    int admitted;
    pthread_mutex_lock(&callback_gate);
    admitted = !property_quiesced;
    if (admitted)
        ++callbacks_inflight;
    pthread_mutex_unlock(&callback_gate);
    return admitted;
}

static void callback_leave(void)
{
    pthread_mutex_lock(&callback_gate);
    if (--callbacks_inflight == 0)
        pthread_cond_broadcast(&callback_drained);
    pthread_mutex_unlock(&callback_gate);
}

static void handoff_session_created(void *server, void *session, void *context)
{
    if (!context || context != mhi2q_probe_manager() || !callback_enter())
        return;
    original_session_created(server, session, context);
    callback_leave();
}

static void handoff_session_failed(void *server, int reason, void *context)
{
    if (!context || context != mhi2q_probe_manager() || !callback_enter())
        return;
    original_session_failed(server, reason, context);
    callback_leave();
}

/* Both pinned AirPlay 280 builds copy seven 32-bit delegate fields from the
 * caller at SetDelegate+0x1c. Check the loaded library before passing a
 * stack-backed replacement delegate to that API. */
static int delegate_copy_matches(set_delegate_fn set_delegate)
{
#ifdef MHI2Q_BRIDGE_STOCK210
    /* 210 copies 15 words, beginning at the function entry. */
    static const uint8_t copy_bytes[] = {
        0x04,0x40,0x2d,0xe5,0x01,0x40,0xa0,0xe1,0x10,0xc0,0x80,0xe2,
        0x0f,0x00,0xb4,0xe8,0x0f,0x00,0xac,0xe8,0x0f,0x00,0xb4,0xe8,
        0x0f,0x00,0xac,0xe8,0x0f,0x00,0xb4,0xe8,0x0f,0x00,0xac,0xe8,
        0x07,0x00,0x94,0xe8,0x07,0x00,0x8c,0xe8
    };
    const size_t copy_offset=0;
#else
    static const uint8_t seven_word_copy[] = {
        0x10, 0xc0, 0x83, 0xe2, 0x02, 0xe0, 0xa0, 0xe1,
        0x0f, 0x00, 0xbe, 0xe8, 0x0f, 0x00, 0xac, 0xe8,
        0x07, 0x00, 0x9e, 0xe8, 0x07, 0x00, 0x8c, 0xe8
    };
    const size_t copy_offset=0x1c;
#define copy_bytes seven_word_copy
#endif
    if (memcmp((const uint8_t *)(uintptr_t)set_delegate + copy_offset,
               copy_bytes, sizeof(copy_bytes)) == 0)
        return 1;
#ifdef MHI2Q_PROPERTY_HANDOFF_ALLOW_DIOPROBE_QEMU
    /* The QEMU IFS places libdioprobe between this preload and libairplay.
     * It forwards SetDelegate to the receiver. This exception is never built
     * into the on-unit candidate. */
    {
        Dl_info owner = {0};
        void *handle;
        set_delegate_fn receiver;
        int match;
        if (!dladdr((void *)set_delegate, &owner) || !owner.dli_fname ||
            !strstr(owner.dli_fname, "libdioprobe.so"))
            return 0;
        handle = dlopen("libairplay.so", RTLD_NOW | RTLD_NOLOAD);
        if (!handle)
            handle = dlopen("/mnt/app/eso/lib/libairplay.so",
                            RTLD_NOW | RTLD_NOLOAD);
        if (!handle)
            handle = dlopen("/proc/boot/libairplay.so",
                            RTLD_NOW | RTLD_NOLOAD);
        if (!handle)
            return 0;
        receiver = (set_delegate_fn)dlsym(handle,
                                          "AirPlayReceiverServerSetDelegate");
        match = receiver &&
                memcmp((const uint8_t *)(uintptr_t)receiver + copy_offset,
                       copy_bytes, sizeof(copy_bytes)) == 0;
        dlclose(handle);
        return match;
    }
#else
    return 0;
#endif
}

static void *handoff_copy_property(void *server, void *key,
                                   const void *qualifier, int *out_error,
                                   void *context)
{
    pthread_mutex_t *sync_mutex;
    int local_error = 0;
    int *error = out_error ? out_error : &local_error;
    void *value;
#ifdef MHI2Q_PROPERTY_HANDOFF_RACE_QEMU
    int race_paused = 0;
#endif
    if (!context || context != mhi2q_probe_manager()) {
        *error = -6714;
        return NULL;
    }
    if (!callback_enter()) {
        *error = -6714;
        return NULL;
    }
    sync_mutex = (pthread_mutex_t *)((char *)context + 0x1a0);
    pthread_mutex_lock(&property_gate);
    if (context != mhi2q_probe_manager() ||
        __sync_val_compare_and_swap(&property_quiesced, 0, 0)) {
        pthread_mutex_unlock(&property_gate);
        callback_leave();
        *error = -6714;
        return NULL;
    }
    pthread_mutex_lock(sync_mutex);
    *(uint8_t *)((char *)context + 0x191) = 0;
    pthread_mutex_unlock(sync_mutex);
#ifdef MHI2Q_PROPERTY_HANDOFF_RACE_QEMU
    if (__sync_val_compare_and_swap(&property_race_armed, 0, 0)) {
        race_paused = 1;
        __sync_add_and_fetch(&property_race_inflight, 1);
        usleep(100000);
    }
#endif
    value = original_copy_property(server, key, qualifier, error, context);
    pthread_mutex_lock(sync_mutex);
    if (*(void **)((char *)context + 0x1f0) == value) {
        *(void **)((char *)context + 0x1f0) = NULL;
    } else {
        __sync_lock_test_and_set(&property_failed, 1);
        (void)write(2, "[dio-property] handoff slot mismatch\n",
                    sizeof("[dio-property] handoff slot mismatch\n") - 1);
        value = NULL;
        *error = -6714;
    }
    pthread_mutex_unlock(sync_mutex);
#ifdef MHI2Q_PROPERTY_HANDOFF_RACE_QEMU
    if (race_paused)
        __sync_sub_and_fetch(&property_race_inflight, 1);
#endif
    pthread_mutex_unlock(&property_gate);
    callback_leave();
    return value;
}

unsigned int mhi2q_property_handoff_ready(void)
{
    return __sync_val_compare_and_swap(&property_installed, 0, 0) &&
           !__sync_val_compare_and_swap(&property_failed, 0, 0) &&
           !__sync_val_compare_and_swap(&property_quiesced, 0, 0);
}

unsigned int mhi2q_property_handoff_is_quiesced(void)
{
    return __sync_val_compare_and_swap(&property_quiesced, 0, 0);
}

unsigned int mhi2q_property_handoff_quiesce(void)
{
    /* A later property-result mismatch blocks START but must not prevent a
     * controlled takeover: the installed wrappers can still drain safely. */
    if (!__sync_val_compare_and_swap(&property_installed, 0, 0))
        return 0;
    pthread_mutex_lock(&callback_gate);
    __sync_lock_test_and_set(&property_quiesced, 1);
    while (callbacks_inflight)
        pthread_cond_wait(&callback_drained, &callback_gate);
    pthread_mutex_unlock(&callback_gate);
    return 1;
}

void AirPlayReceiverServerSetDelegate(void *server, const void *delegate)
{
    static set_delegate_fn real;
#ifdef MHI2Q_BRIDGE_STOCK210
    void *slots[15];
#else
    void *slots[7];
#endif
    if (!real)
        real = (set_delegate_fn)dlsym(RTLD_NEXT, "AirPlayReceiverServerSetDelegate");
    if (!real)
        return;
    const char *mode=getenv("MHI2Q_DIO_OWNER_MODE");
    if(mode && !strcmp(mode,"wired")) {real(server,delegate);return;}
    if (!__progname || strcmp(__progname, "dio_manager") != 0 ||
        (uintptr_t)__builtin_return_address(0) != 0x0015c6c8u || !delegate) {
        real(server, delegate);
        return;
    }
    if (!exact_property_image() || !delegate_copy_matches(real) ||
        !mhi2q_probe_manager() ||
        ((void *const *)delegate)[0] != mhi2q_probe_manager() ||
        ((void *const *)delegate)[2] != NULL ||
        ((void *const *)delegate)[3] != (void *)0x00158690u ||
        ((void *const *)delegate)[4] != NULL ||
        ((void *const *)delegate)[5] != (void *)0x00157a04u ||
        ((void *const *)delegate)[6] != (void *)0x001549f0u) {
        __sync_lock_test_and_set(&property_failed, 1);
        (void)write(2, "[dio-property] image/delegate guard failed\n",
                    sizeof("[dio-property] image/delegate guard failed\n") - 1);
        real(server, delegate);
        return;
    }
    memcpy(slots, delegate, sizeof(slots));
    original_copy_property = (copy_property_fn)slots[3];
    original_session_created = (session_created_fn)slots[5];
    original_session_failed = (session_failed_fn)slots[6];
    slots[3] = (void *)handoff_copy_property;
    slots[5] = (void *)handoff_session_created;
    slots[6] = (void *)handoff_session_failed;
    real(server, slots);
    __sync_lock_test_and_set(&property_installed, 1);
    (void)write(2, "[dio-property] handoff installed\n",
                sizeof("[dio-property] handoff installed\n") - 1);
}
