/* MU1316 dio_manager-only capture of its real CDIOManager instance.
 *
 * This is an offline probe, not the receiver bridge. It never starts/stops
 * AirPlay and never returns a receiver-ready acknowledgement. The default
 * build opens no socket; compile-time QEMU experiments may probe localhost.
 * It is intended for QEMU verification before a bridge is built.
 */
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#ifdef MHI2Q_CAPTURE_SIGSEGV_QEMU
#include <signal.h>
#include <ucontext.h>
#endif
#ifdef MHI2Q_CAPTURE_START_EXPERIMENT
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#endif
#include <string.h>
#include <unistd.h>

#define MANAGER_NEW_RETURN ((uintptr_t)0x00171c48u)
#define NEXT_NEW_RETURN    ((uintptr_t)0x00171c6cu)
#define DIO_MANAGER_VPTR   ((uintptr_t)0x001a4848u)
#define DIO_MANAGER_DTOR   ((uintptr_t)0x0015b314u)
#define PENDING_SLOTS      8

static volatile unsigned int capture_enabled;
static volatile uintptr_t captured_manager;
extern char *__progname;

#ifdef MHI2Q_CAPTURE_HTTP_DELEGATE_QEMU
/* Diagnostic only: verify whether stock 210's own HTTPServerCreate PLT call
 * can be interposed before any pairing-route work is attempted. */
static int (*stock_http_handle)(void *, void *, void *);
static int trace_http_handle(void *connection, void *message, void *context)
{
    static const char marker[] = "[http-delegate-qemu] route-called\n";
    (void)write(2, marker, sizeof(marker) - 1);
    return stock_http_handle(connection, message, context);
}

int HTTPServerCreate(void **out_server, const void *delegate)
{
    static int (*next)(void **, const void *);
    if (!next)
        next = (int (*)(void **, const void *))dlsym(RTLD_NEXT, "HTTPServerCreate");
    if (!next)
        return -1;
    const uintptr_t *slots = (const uintptr_t *)delegate;
    char line[192];
    int used = snprintf(line, sizeof(line),
                        "[http-delegate-qemu] create slot0=%p slot6=%p slot7=%p slot9=%p\n",
                        delegate ? (void *)slots[0] : NULL,
                        delegate ? (void *)slots[6] : NULL,
                        delegate ? (void *)slots[7] : NULL,
                        delegate ? (void *)slots[9] : NULL);
    if (used > 0 && used < (int)sizeof(line))
        (void)write(2, line, (size_t)used);
    uintptr_t copied[11];
    Dl_info owner = {0};
    if (delegate && dladdr((void *)slots[9], &owner) && owner.dli_fbase &&
        slots[9] - (uintptr_t)owner.dli_fbase == 0x302e0u) {
        memcpy(copied, delegate, sizeof(copied));
        stock_http_handle = (int (*)(void *, void *, void *))copied[9];
        copied[9] = (uintptr_t)trace_http_handle;
        delegate = copied;
        static const char installed[] = "[http-delegate-qemu] route-wrapped\n";
        (void)write(2, installed, sizeof(installed) - 1);
    }
    return next(out_server, delegate);
}
#endif

#ifdef MHI2Q_CAPTURE_QEMU_REPEAT_MEDIA
/* Diagnose whether the stock main loop is stopped through its exported CF API. */
void CFRunLoopStop(void *loop)
{
    static void (*next)(void *);
    void *caller = __builtin_return_address(0);
    char line[128];
    int used = snprintf(line, sizeof(line),
                        "[dio-capture] CFRunLoopStop caller=%p loop=%p\n",
                        caller, loop);
    if (used > 0 && used < (int)sizeof(line))
        (void)write(2, line, (size_t)used);
#ifdef MHI2Q_CAPTURE_QEMU_HOLD_STOCK_RUNLOOP
    if ((uintptr_t)caller == 0x0015897cu) {
        static const char held[] =
            "[dio-capture] QEMU held stock CFRunLoopStop\n";
        (void)write(2, held, sizeof(held) - 1);
        return;
    }
#endif
    if (!next)
        next = (void (*)(void *))dlsym(RTLD_NEXT, "CFRunLoopStop");
    if (next)
        next(loop);
}
#endif

#ifdef MHI2Q_CAPTURE_BT_PROPERTY_TRACE_QEMU
/* QEMU diagnosis only. The stock call at 0x15c6c4 passes a 0x3c-byte
 * delegate by address; the receiver copies it. Wrap only its copyProperty
 * slot (+0x0c) and preserve every other slot and the original context. */
typedef void *(*server_copy_property_fn)(void *, void *, const void *, int *, void *);
static server_copy_property_fn stock_server_copy_property;
static unsigned int bt_property_trace_count;
#ifdef MHI2Q_CAPTURE_BT_PROPERTY_RESET_QEMU
static pthread_mutex_t bt_property_gate = PTHREAD_MUTEX_INITIALIZER;
#ifdef MHI2Q_CAPTURE_BT_PROPERTY_HANDOFF_QEMU
static volatile unsigned int bt_property_handoff_mismatch;
#endif
#endif

static void *trace_server_copy_property(void *server, void *key,
                                        const void *qualifier, int *out_error,
                                        void *context)
{
    uint32_t before = context ? *(const uint32_t *)((const char *)context + 0x288) : 0;
    uintptr_t before_data = context ? *(const uintptr_t *)((const char *)context + 0x28c) : 0;
    unsigned int signal_before = context ? *(const uint8_t *)((const char *)context + 0x191) : 0;
    uintptr_t slot_before = context ? *(const uintptr_t *)((const char *)context + 0x1f0) : 0;
    int slot_error_before = context ? *(const int *)((const char *)context + 0x1f4) : 0;
    int error = 0;
    void *value;
#ifdef MHI2Q_CAPTURE_BT_PROPERTY_DIRECT_QEMU
    /* A/B experiment: use the exact worker-side stock value builder while
     * bypassing its shared signal/result slots. Do not ship this thread change
     * without validating every property and shutdown path on the unit. */
    (void)server;
    (void)qualifier;
    /* 0x160a74..0x160a88 passes (manager, NULL, key, NULL, &err).
     * The older three-argument reconstruction is ABI-inaccurate. */
    value = ((void *(*)(void *, void *, const void *, void *, int *))0x0015fe1cu)(
        context, NULL, key, NULL, out_error ? out_error : &error);
#else
#ifdef MHI2Q_CAPTURE_BT_PROPERTY_RESET_QEMU
    if (context && (uintptr_t)context == captured_manager) {
        /* The stock signal/wait functions use +0x1a0 as the sync mutex and
         * +0x191 as the signalled byte. Hold our own gate across post/wait so
         * only one copy-property request uses the shared result slots. */
        pthread_mutex_t *sync_mutex = (pthread_mutex_t *)((char *)context + 0x1a0);
        pthread_mutex_lock(&bt_property_gate);
        pthread_mutex_lock(sync_mutex);
        *(uint8_t *)((char *)context + 0x191) = 0;
        pthread_mutex_unlock(sync_mutex);
        value = stock_server_copy_property(server, key, qualifier,
                                           out_error ? out_error : &error,
                                           context);
#ifdef MHI2Q_CAPTURE_BT_PROPERTY_HANDOFF_QEMU
        /* The SDK consumes/releases a returned CF object. Stock CDIOManager
         * D1 also releases the shared slot at 0x15b498..0x15b4a4. Transfer
         * ownership only when this callback received the exact worker value. */
        pthread_mutex_lock(sync_mutex);
        if (*(void **)((char *)context + 0x1f0) == value) {
            *(void **)((char *)context + 0x1f0) = NULL;
        } else {
            __sync_add_and_fetch(&bt_property_handoff_mismatch, 1);
            (void)write(2, "[dio-btprop] handoff slot mismatch\n",
                        sizeof("[dio-btprop] handoff slot mismatch\n") - 1);
        }
        pthread_mutex_unlock(sync_mutex);
#endif
        pthread_mutex_unlock(&bt_property_gate);
    } else {
        value = stock_server_copy_property(server, key, qualifier,
                                           out_error ? out_error : &error,
                                           context);
    }
#else
    value = stock_server_copy_property(server, key, qualifier,
                                       out_error ? out_error : &error,
                                       context);
#endif
#endif
    if (bt_property_trace_count++ < 80) {
        char line[300];
        int length = snprintf(line, sizeof(line),
            "[dio-btprop] key=%p manager=%u vec=%u/%p->%u/%p sig=%u->%u slot=%p/%d->%p/%d value=%p err=%d\n",
            key, context && (uintptr_t)context == captured_manager,
            before, (void *)before_data,
            context ? *(const uint32_t *)((const char *)context + 0x288) : 0,
            (void *)(context ? *(const uintptr_t *)((const char *)context + 0x28c) : 0),
            signal_before,
            context ? *(const uint8_t *)((const char *)context + 0x191) : 0,
            (void *)slot_before, slot_error_before,
            (void *)(context ? *(const uintptr_t *)((const char *)context + 0x1f0) : 0),
            context ? *(const int *)((const char *)context + 0x1f4) : 0,
            value, out_error ? *out_error : error);
        if (length > 0 && (size_t)length < sizeof(line))
            (void)write(2, line, (size_t)length);
    }
    return value;
}

void AirPlayReceiverServerSetDelegate(void *server, const void *delegate)
{
    typedef void (*set_delegate_fn)(void *, const void *);
    static set_delegate_fn real;
    void *slots[15];
    if (!real)
        real = (set_delegate_fn)dlsym(RTLD_NEXT, "AirPlayReceiverServerSetDelegate");
    if (!real)
        return;
    if (!capture_enabled || !__progname || strcmp(__progname, "dio_manager") != 0 ||
        (uintptr_t)__builtin_return_address(0) != 0x0015c6c8u || !delegate) {
        real(server, delegate);
        return;
    }
    memcpy(slots, delegate, sizeof(slots));
    stock_server_copy_property = (server_copy_property_fn)slots[3];
    slots[3] = (void *)trace_server_copy_property;
#ifdef MHI2Q_CAPTURE_BT_PROPERTY_DIRECT_QEMU
    (void)write(2, "[dio-btprop] direct stock value builder active\n",
                sizeof("[dio-btprop] direct stock value builder active\n") - 1);
#elif defined(MHI2Q_CAPTURE_BT_PROPERTY_HANDOFF_QEMU)
    (void)write(2, "[dio-btprop] stock signal-reset ownership handoff active\n",
                sizeof("[dio-btprop] stock signal-reset ownership handoff active\n") - 1);
#elif defined(MHI2Q_CAPTURE_BT_PROPERTY_RESET_QEMU)
    (void)write(2, "[dio-btprop] stock signal-reset gate active\n",
                sizeof("[dio-btprop] stock signal-reset gate active\n") - 1);
#else
    (void)write(2, "[dio-btprop] stock server callback wrapped\n",
                sizeof("[dio-btprop] stock server callback wrapped\n") - 1);
#endif
    real(server, slots);
}
#endif

#if defined(MHI2Q_CAPTURE_AIRPLAY280_TRACE) && !defined(MHI2Q_CAPTURE_AIRPLAY280_ABI_FIXED)
/* QEMU diagnosis only: log the caller and object before the SDK dereferences it. */
const void *CFDictionaryGetTypedValue(const void *object, const void *key,
                                      uint32_t type, int *error)
{
    typedef const void *(*get_fn)(const void *, const void *, uint32_t, int *);
    static get_fn real;
    if (!real) real = (get_fn)dlsym(RTLD_NEXT, "CFDictionaryGetTypedValue");
    char line[160];
    int length = snprintf(line, sizeof(line),
                          "[airplay280] CFDictionaryGetTypedValue object=%p key=%p caller=%p\n",
                          object, key, __builtin_return_address(0));
    if (length > 0 && length < (int)sizeof(line)) (void)write(2, line, (size_t)length);
    return real(object, key, type, error);
}

const void *CFDictionaryGetValue(const void *object, const void *key)
{
    typedef const void *(*get_fn)(const void *, const void *);
    static get_fn real;
    if (!real) real = (get_fn)dlsym(RTLD_NEXT, "CFDictionaryGetValue");
    char line[160];
    int length = snprintf(line, sizeof(line),
                          "[airplay280] CFDictionaryGetValue object=%p key=%p caller=%p\n",
                          object, key, __builtin_return_address(0));
    if (length > 0 && length < (int)sizeof(line)) (void)write(2, line, (size_t)length);
    return real(object, key);
}

int CFLDictionaryGetValue(void *object, const void *key, void *out_value)
{
    typedef int (*get_fn)(void *, const void *, void *);
    static get_fn real;
    if (!real) real = (get_fn)dlsym(RTLD_NEXT, "CFLDictionaryGetValue");
    char line[160];
    int length = snprintf(line, sizeof(line),
                          "[airplay280] CFLDictionaryGetValue object=%p key=%p caller=%p\n",
                          object, key, __builtin_return_address(0));
    if (length > 0 && length < (int)sizeof(line)) (void)write(2, line, (size_t)length);
    return real(object, key, out_value);
}
#endif

#ifdef MHI2Q_CAPTURE_SCREEN_DATA_QEMU
/* The virtual BSP has no QNX pipe resource manager. Give the 280 SDK's
 * NetSocket cancellation path a selectable loopback pair for this probe. */
int pipe(int ends[2])
{
    struct sockaddr_in address;
    socklen_t address_length = sizeof(address);
    int listener = -1, writer = -1, reader = -1;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) goto fail;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) ||
        getsockname(listener, (struct sockaddr *)&address, &address_length) ||
        listen(listener, 1)) goto fail;
    writer = socket(AF_INET, SOCK_STREAM, 0);
    if (writer < 0 || connect(writer, (struct sockaddr *)&address,
                              sizeof(address))) goto fail;
    reader = accept(listener, NULL, NULL);
    if (reader < 0) goto fail;
    close(listener);
    ends[0] = reader;
    ends[1] = writer;
    {
        static const char marker[] = "[screen-data-qemu] pipe via loopback pair\n";
        (void)write(2, marker, sizeof(marker) - 1);
    }
    return 0;
fail:
    if (reader >= 0) close(reader);
    if (writer >= 0) close(writer);
    if (listener >= 0) close(listener);
    return -1;
}

int ScreenStreamStart(void *stream)
{
    typedef int (*start_fn)(void *);
    static start_fn real;
    int result;
    char line[128];
    int length;
    if (!real) real = (start_fn)dlsym(RTLD_NEXT, "ScreenStreamStart");
    result = real ? real(stream) : -1;
    length = snprintf(line, sizeof(line),
        "[screen-data-qemu] ScreenStreamStart result=%d\n", result);
    if (length > 0 && (size_t)length < sizeof(line))
        (void)write(2, line, (size_t)length);
    return result;
}

int AirPlayReceiverSessionScreen_ProcessFrames(void *session, void *socket,
                                               int timeout)
{
    typedef int (*frames_fn)(void *, void *, int);
    static frames_fn real;
    int result;
    char line[128];
    int length;
    if (!real) real = (frames_fn)dlsym(RTLD_NEXT,
        "AirPlayReceiverSessionScreen_ProcessFrames");
    {
        static const char entered[] = "[screen-data-qemu] ProcessFrames entered\n";
        (void)write(2, entered, sizeof(entered) - 1);
    }
    result = real ? real(session, socket, timeout) : -1;
    length = snprintf(line, sizeof(line),
        "[screen-data-qemu] ProcessFrames result=%d\n", result);
    if (length > 0 && (size_t)length < sizeof(line))
        (void)write(2, line, (size_t)length);
    return result;
}
#endif

#ifdef MHI2Q_CAPTURE_SCREEN_AUDIO_QEMU
/* This call follows successful main-audio RTP authentication and jitter
 * insertion in the 280 receiver. Log only its result, never packet bytes. */
int RTPJitterBufferPutBusyNode(void *context, void *node)
{
    typedef int (*put_fn)(void *, void *);
    static put_fn real;
    char line[96];
    int result, length;
    if (!real) real = (put_fn)dlsym(RTLD_NEXT, "RTPJitterBufferPutBusyNode");
    result = real ? real(context, node) : -1;
    length = snprintf(line, sizeof(line),
                      "[audio-qemu] RTPJitterBufferPutBusyNode result=%d\n",
                      result);
    if (length > 0 && (size_t)length < sizeof(line))
        (void)write(2, line, (size_t)length);
    return result;
}

int AirPlayReceiverSessionReadAudio(void *session, int type,
                                   uint32_t sample_time, uint64_t host_time,
                                   void *buffer, size_t length)
{
    typedef int (*read_fn)(void *, int, uint32_t, uint64_t, void *, size_t);
    static read_fn real;
    static int reported;
    const uint8_t *bytes = (const uint8_t *)buffer;
    size_t i;
    int result;
    if (!real) real = (read_fn)dlsym(RTLD_NEXT, "AirPlayReceiverSessionReadAudio");
    result = real ? real(session, type, sample_time, host_time,
                         buffer, length) : -1;
    if (result == 0 && type == 100 && !reported) {
        for (i = 0; i < length; ++i) {
            if (bytes[i] != 0) {
                if (__sync_bool_compare_and_swap(&reported, 0, 1)) {
                    static const char marker[] =
                        "[audio-qemu] main PCM callback contained nonzero samples\n";
                    (void)write(2, marker, sizeof(marker) - 1);
                }
                break;
            }
        }
    }
    return result;
}
#endif

typedef void *(*nothrow_new_fn)(unsigned int, const void *);
typedef void (*delete_fn)(void *);
typedef struct {
    pthread_t thread;
    uintptr_t manager;
    int occupied;
} PendingManager;

static volatile unsigned int capture_count;
static volatile unsigned int release_count;
static volatile unsigned int capture_error;
static volatile unsigned int airplay_bonjour_port;
static volatile unsigned int airplay_bonjour_successes;
static volatile uintptr_t observed_airplay_server;
unsigned int mhi2q_probe_airplay_bonjour_port(void);
unsigned int mhi2q_probe_airplay_bonjour_successes(void);
void *mhi2q_probe_airplay_server(void);
void mhi2q_probe_record_airplay_bonjour(unsigned int port);
void mhi2q_probe_record_airplay_server(void *server);
#ifdef MHI2Q_CAPTURE_BRIDGE
extern void mhi2q_bridge_on_manager(void *manager);
extern void mhi2q_bridge_on_manager_release(void *manager);
#endif
static pthread_mutex_t pending_lock = PTHREAD_MUTEX_INITIALIZER;
static PendingManager pending[PENDING_SLOTS];

/* Exported by the QNX 6.5 libc.so.3 staged with dio_manager. */

#ifdef MHI2Q_CAPTURE_SIGSEGV_QEMU
static void trace_segv(int signo, siginfo_t *info, void *context)
{
    const ucontext_t *state = context;
    const uint32_t *r = state->uc_mcontext.cpu.gpr;
    char line[512];
    int length = snprintf(line, sizeof(line),
        "[dio-crash-qemu] sig=%d addr=%p pc=%08x lr=%08x sp=%08x "
        "fp=%08x r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x "
        "r5=%08x r6=%08x r7=%08x r8=%08x r9=%08x r10=%08x r12=%08x\n",
        signo, info->si_addr, r[15], r[14], r[13], r[11],
        r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
        r[9], r[10], r[12]);
    if (length > 0 && (size_t)length < sizeof(line))
        (void)write(2, line, (size_t)length);
    /* At this fault the preceding load from r5+0x0c succeeded. Keep this
     * diagnostic QEMU-only and read only the same live object and stack. */
    if ((uintptr_t)info->si_addr == 4 && r[8] == 0 && r[5] && r[13]) {
        const uint32_t *object = (const uint32_t *)r[5];
        const uint32_t *stack = (const uint32_t *)r[13];
        length = snprintf(line, sizeof(line),
            "[dio-crash-qemu] object=%08x %08x %08x %08x %08x %08x %08x %08x "
            "%08x %08x %08x %08x stack=%08x %08x %08x %08x %08x %08x %08x %08x "
            "%08x %08x %08x %08x %08x %08x %08x %08x\n",
            object[0], object[1], object[2], object[3],
            object[4], object[5], object[6], object[7],
            object[8], object[9], object[10], object[11],
            stack[0], stack[1], stack[2], stack[3],
            stack[4], stack[5], stack[6], stack[7],
            stack[8], stack[9], stack[10], stack[11],
            stack[12], stack[13], stack[14], stack[15]);
        if (length > 0 && (size_t)length < sizeof(line))
            (void)write(2, line, (size_t)length);
        {
            static const unsigned offsets[] = {7, 9, 11, 13, 14};
            unsigned index;
            for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
                Dl_info owner;
                unsigned slot = offsets[index];
                if (!dladdr((void *)(uintptr_t)stack[slot], &owner))
                    continue;
                length = snprintf(line, sizeof(line),
                    "[dio-crash-qemu] stack[%u]=%08x module=%s base=%p symbol=%s\n",
                    slot, stack[slot], owner.dli_fname ? owner.dli_fname : "?",
                    owner.dli_fbase, owner.dli_sname ? owner.dli_sname : "?");
                if (length > 0 && (size_t)length < sizeof(line))
                    (void)write(2, line, (size_t)length);
            }
        }
    }
    _exit(128 + signo);
}
#endif

#ifdef MHI2Q_CAPTURE_UNICODE_DTOR_QEMU
/* QEMU-only trace for the libutil destructor reached during screen-session
 * shutdown. The entry is imported by stock dio_manager through its PLT. */
void mhi2q_trace_unicode8_d1(void *object)
    __asm__("_ZN4util14UnicodeString8D1Ev");
void mhi2q_trace_unicode8_d1(void *object)
{
    typedef void (*destructor_fn)(void *);
    static destructor_fn real;
    char line[160];
    int length;
    unsigned int flags = object ? ((const unsigned char *)object)[6] : 0;
    uint32_t backing = object ? ((const uint32_t *)object)[3] : 0;
    if (!real)
        real = (destructor_fn)dlsym(RTLD_NEXT, "_ZN4util14UnicodeString8D1Ev");
    length = snprintf(line, sizeof(line),
        "[unicode-dtor-qemu] D1 this=%p caller=%p flags=%02x backing=%08x\n",
        object, __builtin_return_address(0), flags, backing);
    if (length > 0 && (size_t)length < sizeof(line))
        (void)write(2, line, (size_t)length);
    if (real) real(object);
}
#endif

#ifdef MHI2Q_CAPTURE_DIAGNOSTIC
#define CAPTURE_TRACE(message) ((void)write(2, message, sizeof(message) - 1))
#else
#define CAPTURE_TRACE(message) ((void)0)
#endif

#ifdef MHI2Q_CAPTURE_START_EXPERIMENT
static void log_hid_result(const char *name, int status);
#ifdef MHI2Q_CAPTURE_MOCK_MAC_QEMU
unsigned int if_nametoindex(const char *name)
{
    static unsigned int (*real)(const char *);
    unsigned int index;
    char line[128];
    int count;
    if (!real)
        real = (unsigned int (*)(const char *))
            dlsym(RTLD_NEXT, "if_nametoindex");
    if (!real)
        return 0;
    index = real(name);
    if (__progname && strcmp(__progname, "dio_manager") == 0) {
        count = snprintf(line, sizeof(line),
                         "[dio-capture] if_nametoindex name=%.80s index=%u\n",
                         name ? name : "(null)", index);
        if (count > 0 && (size_t)count < sizeof(line))
            (void)write(2, line, (size_t)count);
#ifndef MHI2Q_CAPTURE_IFACE_LO0_QEMU
        if (name && *name && index == 0) {
            index = real("lo0");
            log_hid_result("QEMU loopback interface index", (int)index);
        }
#endif
    }
    return index;
}

int getifaddrs(struct ifaddrs **out)
{
    static int (*real)(struct ifaddrs **);
    static unsigned int logged;
    int status;
    if (!real)
        real = (int (*)(struct ifaddrs **))dlsym(RTLD_NEXT, "getifaddrs");
    if (!real)
        return -1;
    status = real(out);
    if (logged++ < 5 && __progname &&
        strcmp(__progname, "dio_manager") == 0) {
        const struct ifaddrs *entry;
        log_hid_result("QEMU getifaddrs status", status);
        for (entry = out && status == 0 ? *out : NULL;
             entry; entry = entry->ifa_next) {
            char line[120];
            int count = snprintf(line, sizeof(line),
                                 "[dio-capture] iface=%s flags=%x family=%d\n",
                                 entry->ifa_name ? entry->ifa_name : "?",
                                 entry->ifa_flags,
                                 entry->ifa_addr ? entry->ifa_addr->sa_family : -1);
            if (count > 0 && (size_t)count < sizeof(line))
                (void)write(2, line, (size_t)count);
        }
    }
    return status;
}

#ifndef MHI2Q_CAPTURE_BRIDGE
int DNSServiceRegister(void **out_ref, uint32_t flags, uint32_t interface_index,
                       const char *name, const char *regtype,
                       const char *domain, const char *host, uint16_t port,
                       uint16_t txt_length, const void *txt_record,
                       void *callback, void *context)
{
    typedef int (*register_fn)(void **, uint32_t, uint32_t, const char *,
                               const char *, const char *, const char *,
                               uint16_t, uint16_t, const void *, void *, void *);
    static register_fn real;
    int status;
    char line[160];
    int count = snprintf(line, sizeof(line),
                         "[dio-capture] DNSServiceRegister name=%s type=%s port=%u\n",
                         name ? name : "(null)", regtype ? regtype : "(null)",
                         (unsigned)ntohs(port));
    if (count > 0 && (size_t)count < sizeof(line))
        (void)write(2, line, (size_t)count);
#ifdef MHI2Q_CAPTURE_DUMP_TXT_QEMU
    if (regtype && strcmp(regtype, "_airplay._tcp.") == 0 &&
        txt_record && txt_length <= 1024) {
        static const char digits[] = "0123456789abcdef";
        static const char prefix[] = "[dio-capture] bonjour-txt-hex=";
        char hex[sizeof(prefix) + 2048];
        const unsigned char *bytes = txt_record;
        size_t index;
        memcpy(hex, prefix, sizeof(prefix) - 1);
        for (index = 0; index < txt_length; ++index) {
            hex[sizeof(prefix) - 1 + index * 2] = digits[bytes[index] >> 4];
            hex[sizeof(prefix) + index * 2] = digits[bytes[index] & 15];
        }
        hex[sizeof(prefix) - 1 + txt_length * 2] = '\n';
        (void)write(2, hex, sizeof(prefix) + txt_length * 2);
    }
#endif
    if (!real)
        real = (register_fn)dlsym(RTLD_NEXT, "DNSServiceRegister");
    if (!real)
        return -1;
    status = real(out_ref, flags, interface_index, name, regtype, domain,
                  host, port, txt_length, txt_record, callback, context);
    log_hid_result("DNSServiceRegister status", status);
    if (status == 0 && regtype && strcmp(regtype, "_airplay._tcp.") == 0) {
        mhi2q_probe_record_airplay_bonjour(ntohs(port));
    }
    return status;
}
#endif

uint64_t GetPrimaryMACAddress(uint8_t out_mac[6], int *out_error)
{
    static uint64_t (*real)(uint8_t[6], int *);
    uint64_t value = 0;
    int error = 0;
    if (!real)
        real = (uint64_t (*)(uint8_t[6], int *))
            dlsym(RTLD_NEXT, "GetPrimaryMACAddress");
    if (real)
        value = real(out_mac, &error);
    log_hid_result("stock primary MAC error", error);
    if (__progname && strcmp(__progname, "dio_manager") == 0) {
        static const uint8_t synthetic[6] = { 0x02, 0, 0, 0, 0, 1 };
        if (out_mac)
            memcpy(out_mac, synthetic, sizeof(synthetic));
        if (out_error)
            *out_error = 0;
        CAPTURE_TRACE("[dio-capture] QEMU synthetic primary MAC\n");
        return UINT64_C(0x020000000001);
    }
    if (out_error)
        *out_error = error;
    return value;
}
#endif

#ifdef MHI2Q_CAPTURE_STAGE_MDNSD_QEMU
int setsockopt(int fd, int level, int option, const void *value, socklen_t length)
{
    static int (*real)(int, int, int, const void *, socklen_t);
    int status;
    if (!real)
        real = (int (*)(int, int, int, const void *, socklen_t))
            dlsym(RTLD_NEXT, "setsockopt");
    if (!real)
        return -1;
    status = real(fd, level, option, value, length);
    if (__progname && strcmp(__progname, "mdnsd") == 0) {
        log_hid_result("mdnsd setsockopt level", level);
        log_hid_result("mdnsd setsockopt option", option);
        log_hid_result("mdnsd setsockopt status", status);
        if (status < 0)
            log_hid_result("mdnsd setsockopt errno", errno);
        if (status < 0 && level == IPPROTO_IP && option == 0x19) {
            /* Factory mdnsd requires IP_PKTINFO (25); the QEMU io-pkt
             * lacks it. Only this emulator path reports support so we can
             * observe the next discovery dependency. */
            CAPTURE_TRACE("[dio-capture] QEMU mdnsd IP_PKTINFO shim\n");
            return 0;
        }
    }
    return status;
}

extern const uint8_t mhi2q_mdnsd_blob[];
extern const uint8_t mhi2q_mdnsd_blob_end[];
static void stage_factory_mdnsd(void)
{
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
    if(access("/dev/shmem/mdnsd_bin",F_OK)==0)return;
#endif
    size_t length = (size_t)(mhi2q_mdnsd_blob_end - mhi2q_mdnsd_blob);
    size_t total = 0;
    int output;
    if (length != 464588u) {
        log_hid_result("mdnsd blob size", (int)length);
        return;
    }
    output = open("/dev/shmem/mdnsd_bin", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (output < 0) {
        log_hid_result("mdnsd output errno", errno);
        return;
    }
    while (total < length) {
        ssize_t step = write(output, mhi2q_mdnsd_blob + total,
                             length - total > 4096u ? 4096u : length - total);
        if (step <= 0) {
            log_hid_result("mdnsd write errno", errno);
            close(output);
            return;
        }
        total += (size_t)step;
    }
    close(output);
    log_hid_result("mdnsd bytes restored", (int)total);
    int pid = spawnl(P_NOWAIT, "/dev/shmem/mdnsd_bin", "mdnsd", "-f", (char *)0);
    if (pid < 0)
        log_hid_result("mdnsd spawn errno", errno);
    else
        log_hid_result("mdnsd spawned pid", pid);
}
#endif
#ifdef MHI2Q_CAPTURE_TRACKF_GATE_QEMU
static volatile int fail_getifaddrs_after_start;

int getifaddrs(struct ifaddrs **out)
{
    static int (*real)(struct ifaddrs **);
    if (fail_getifaddrs_after_start) {
        if (out)
            *out = NULL;
        errno = ENXIO;
        return -1;
    }
    if (!real)
        real = (int (*)(struct ifaddrs **))dlsym(RTLD_NEXT, "getifaddrs");
    return real ? real(out) : -1;
}
#endif

#ifndef MHI2Q_CAPTURE_BRIDGE_DRIVES_START
#ifdef MHI2Q_CAPTURE_AIRPLAY280_TRACE
extern void mhi2q_probe_pairing_full(void);
#endif

static void probe_info_listener(void)
{
    static const char request[] = "GET /info HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                                  "Connection: close\r\n\r\n";
    struct sockaddr_in address;
    struct timeval timeout = { 1, 0 };
    char reply[1024];
    int attempt;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    for (attempt = 0; attempt < 25; ++attempt) {
        int port_index;
        sleep(1);
        for (port_index = 0; port_index < 2; ++port_index) {
            int fd;
            ssize_t count;
            address.sin_port = htons(port_index == 0 ? 7000 : 5000);
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) {
                if (attempt == 0)
                    log_hid_result("probe socket errno", errno);
                continue;
            }
            (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                             sizeof(timeout));
            if (connect(fd, (struct sockaddr *)&address,
                        sizeof(address)) != 0) {
                if (attempt == 0)
                    log_hid_result(port_index == 0 ? "probe 7000 errno" :
                                   "probe 5000 errno", errno);
                close(fd);
                continue;
            }
            if (port_index == 0)
                CAPTURE_TRACE("[dio-capture] TCP7000 connected\n");
            else
                CAPTURE_TRACE("[dio-capture] TCP5000 connected\n");
            if (write(fd, request, sizeof(request) - 1) ==
                (ssize_t)(sizeof(request) - 1)) {
                count = read(fd, reply, sizeof(reply) - 1);
                if (count > 0) {
                    size_t used = (size_t)count;
                    reply[used] = '\0';
                    if (strstr(reply, " 200 ")) {
                        if (port_index == 0)
                            CAPTURE_TRACE("[dio-capture] GET /info 7000 HTTP 200\n");
                        else
                            CAPTURE_TRACE("[dio-capture] GET /info 5000 HTTP 200\n");
                    }
#ifdef MHI2Q_CAPTURE_DUMP_INFO_QEMU
                    while (used < sizeof(reply)) {
                        count = read(fd, reply + used, sizeof(reply) - used);
                        if (count <= 0)
                            break;
                        used += (size_t)count;
                    }
                    {
                        const char *prefix = port_index == 0 ?
                            "[dio-capture] info7000-hex=" :
                            "[dio-capture] info5000-hex=";
                        size_t prefix_length = strlen(prefix);
                        static const char digits[] = "0123456789abcdef";
                        char line[64 + sizeof(reply) * 2];
                        size_t index;
                        memcpy(line, prefix, prefix_length);
                        for (index = 0; index < used; ++index) {
                            line[prefix_length + index * 2] =
                                digits[((uint8_t)reply[index]) >> 4];
                            line[prefix_length + index * 2 + 1] =
                                digits[((uint8_t)reply[index]) & 15];
                        }
                        line[prefix_length + used * 2] = '\n';
                        (void)write(2, line, prefix_length + used * 2 + 1);
                    }
#endif
                }
            }
            close(fd);
#ifdef MHI2Q_CAPTURE_WIRELESS_HOOK_QEMU
            {
                int marker = open("/dev/shmem/independent_srp_ready",
                                  O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (marker >= 0) close(marker);
            }
#endif
#ifdef MHI2Q_CAPTURE_AIRPLAY280_TRACE
            mhi2q_probe_pairing_full();
#ifdef MHI2Q_CAPTURE_INDEPENDENT_SRP_QEMU
            {
                int marker = open("/dev/shmem/independent_srp_ready",
                                  O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (marker >= 0) close(marker);
            }
#endif
#endif
            return;
        }
    }
    CAPTURE_TRACE("[dio-capture] TCP7000/5000 unavailable after 25s\n");
}
#endif

#ifdef MHI2Q_CAPTURE_SOFT_RESTART_QEMU
static int probe_5000_connect(void)
{
    struct sockaddr_in address;
    int fd;
    int status;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(5000);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 0;
    status = connect(fd, (struct sockaddr *)&address, sizeof(address));
    close(fd);
    return status == 0;
}

static void probe_soft_restart(uintptr_t manager)
{
    typedef void *(*create_string_fn)(void *, const char *, unsigned int);
    typedef int (*control_fn)(void *, uint32_t, void *, void *, void *, void *);
    typedef void (*release_fn)(void *);
    create_string_fn create_string = (create_string_fn)
        dlsym(RTLD_DEFAULT, "CFStringCreateWithCString");
    control_fn control = (control_fn)
        dlsym(RTLD_DEFAULT, "AirPlayReceiverServerControl");
    release_fn release = (release_fn)dlsym(RTLD_DEFAULT, "CFRelease");
    uintptr_t server = __sync_val_compare_and_swap(&observed_airplay_server, 0, 0);
    void *stop_command;
    void *start_command;
    if (!server || !create_string || !control || !release ||
        __sync_val_compare_and_swap(&captured_manager, manager, manager)
        != manager) {
        CAPTURE_TRACE("[dio-capture] QEMU soft restart prerequisites absent\n");
        return;
    }
    stop_command = create_string(NULL, "stopServer", 0x08000100u);
    start_command = create_string(NULL, "startServer", 0x08000100u);
    if (!stop_command || !start_command) {
        CAPTURE_TRACE("[dio-capture] QEMU soft restart command alloc failed\n");
    } else {
        CAPTURE_TRACE("[dio-capture] QEMU direct stopServer\n");
        log_hid_result("QEMU direct stopServer",
                       control((void *)server, 1, stop_command,
                               NULL, NULL, NULL));
        sleep(2);
        log_hid_result("QEMU stopped TCP5000 connected", probe_5000_connect());
        CAPTURE_TRACE("[dio-capture] QEMU direct startServer\n");
        log_hid_result("QEMU direct startServer",
                       control((void *)server, 1, start_command,
                               NULL, NULL, NULL));
        sleep(2);
        log_hid_result("QEMU restarted TCP5000 connected", probe_5000_connect());
        CAPTURE_TRACE("[dio-capture] QEMU probing restarted /info\n");
        probe_info_listener();
        log_hid_result("QEMU AirPlay Bonjour successes after restart",
                       (int)mhi2q_probe_airplay_bonjour_successes());
        log_hid_result("QEMU manager retained",
                       __sync_val_compare_and_swap(&captured_manager,
                                                   manager, manager) == manager);
    }
    if (stop_command)
        release(stop_command);
    if (start_command)
        release(start_command);
}
#endif

int bind(int fd, const struct sockaddr *address, socklen_t length)
{
    static int (*real)(int, const struct sockaddr *, socklen_t);
    int status;
    if (!real)
        real = (int (*)(int, const struct sockaddr *, socklen_t))
            dlsym(RTLD_NEXT, "bind");
    if (!real)
        return -1;
    status = real(fd, address, length);
    if (address && address->sa_family == AF_INET &&
        length >= sizeof(struct sockaddr_in)) {
        log_hid_result("bind IPv4 port",
                       ntohs(((const struct sockaddr_in *)address)->sin_port));
        log_hid_result("bind IPv4 status", status);
        if (status < 0)
            log_hid_result("bind IPv4 errno", errno);
    } else if (address && address->sa_family == AF_INET6 &&
               length >= sizeof(struct sockaddr_in6)) {
        log_hid_result("bind IPv6 port",
                       ntohs(((const struct sockaddr_in6 *)address)->sin6_port));
        log_hid_result("bind IPv6 status", status);
        if (status < 0)
            log_hid_result("bind IPv6 errno", errno);
    } else if (address && address->sa_family == AF_UNIX &&
               length >= sizeof(sa_family_t)) {
        const struct sockaddr_un *local = (const struct sockaddr_un *)address;
        char line[160];
        int count = snprintf(line, sizeof(line),
                             "[dio-capture] bind Unix path=%.100s status=%d errno=%d\n",
                             local->sun_path, status, status < 0 ? errno : 0);
        if (count > 0 && (size_t)count < sizeof(line))
            (void)write(2, line, (size_t)count);
    }
    return status;
}

#ifdef MHI2Q_CAPTURE_STAGE_MDNSD_QEMU
int connect(int fd, const struct sockaddr *address, socklen_t length)
{
    static int (*real)(int, const struct sockaddr *, socklen_t);
    int status;
    if (!real)
        real = (int (*)(int, const struct sockaddr *, socklen_t))
            dlsym(RTLD_NEXT, "connect");
    if (!real)
        return -1;
    status = real(fd, address, length);
    if (address && address->sa_family == AF_UNIX &&
        length >= sizeof(sa_family_t)) {
        const struct sockaddr_un *local = (const struct sockaddr_un *)address;
        char line[160];
        int count = snprintf(line, sizeof(line),
                             "[dio-capture] connect Unix path=%.100s status=%d errno=%d\n",
                             local->sun_path, status, status < 0 ? errno : 0);
        if (count > 0 && (size_t)count < sizeof(line))
            (void)write(2, line, (size_t)count);
    }
    return status;
}
#endif

typedef int (*hid_create_fn)(void *);
typedef int (*hid_register_fn)(void *);

static void log_hid_result(const char *name, int status)
{
    char line[100];
    int count = snprintf(line, sizeof(line), "[dio-capture] %s status=%d\n",
                         name, status);
    if (count > 0 && (size_t)count < sizeof(line))
        (void)write(2, line, (size_t)count);
}

int sscanf(const char *input, const char *format, ...)
{
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    va_list args;
    int result;
    va_start(args, format);
    result = vsscanf(input, format, args);
    va_end(args);
    if (caller == 0x0015c36cu || caller == 0x0015c380u)
        log_hid_result("run sscanf", result);
#ifdef MHI2Q_CAPTURE_SKIP_HID_QEMU
    if (caller == 0x0015c380u) {
        memset((void *)0x001a9874u, 0, 6);
        memset((void *)0x001a9864u, 0, 3);
        CAPTURE_TRACE("[dio-capture] skipped QEMU virtual HID branch\n");
    }
#endif
    return result;
}

int ScreenCreate(void *out_screen)
{
#if defined(MHI2Q_CAPTURE_AIRPLAY280_TRACE) && !defined(MHI2Q_CAPTURE_AIRPLAY280_ABI_FIXED)
    static int (*real)(void *, const void *);
    if (!real)
        real = (int (*)(void *, const void *))dlsym(RTLD_NEXT, "ScreenCreate");
    if (!real)
        return -1;
    int status = real(out_screen, NULL);
#else
    static int (*real)(void *);
    if (!real)
        real = (int (*)(void *))dlsym(RTLD_NEXT, "ScreenCreate");
    if (!real)
        return -1;
    int status = real(out_screen);
#endif
    log_hid_result("ScreenCreate", status);
    return status;
}

int ScreenRegister(void *screen)
{
    static int (*real)(void *);
    if (!real)
        real = (int (*)(void *))dlsym(RTLD_NEXT, "ScreenRegister");
    if (!real)
        return -1;
    int status = real(screen);
    log_hid_result("ScreenRegister", status);
    return status;
}

#ifndef MHI2Q_CAPTURE_BRIDGE
int AirPlayReceiverServerCreate(void *out_server)
{
    static int (*real)(void *);
    CAPTURE_TRACE("[dio-capture] AirPlayReceiverServerCreate entered\n");
    if (!real)
        real = (int (*)(void *))dlsym(RTLD_NEXT,
                                     "AirPlayReceiverServerCreate");
    if (!real)
        return -1;
    int status = real(out_server);
    log_hid_result("AirPlayReceiverServerCreate", status);
    if (status == 0 && out_server && *(void **)out_server)
        mhi2q_probe_record_airplay_server(*(void **)out_server);
    return status;
}
#endif

int AirPlayReceiverServerControl(void *server, uint32_t flags, void *command,
                                 void *qualifier, void *params, void *out_params)
{
    typedef int (*control_fn)(void *, uint32_t, void *, void *, void *, void *);
    static control_fn real;
    CAPTURE_TRACE("[dio-capture] AirPlayReceiverServerControl entered\n");
    if (!real)
        real = (control_fn)dlsym(RTLD_NEXT, "AirPlayReceiverServerControl");
    if (!real)
        return -1;
    int status = real(server, flags, command, qualifier, params, out_params);
    log_hid_result("AirPlayReceiverServerControl", status);
    return status;
}

int HIDDeviceCreateVirtual(void *out_device)
{
    static hid_create_fn real;
    if (!real)
        real = (hid_create_fn)dlsym(RTLD_NEXT, "HIDDeviceCreateVirtual");
    if (!real)
        return -1;
    int status = real(out_device);
    log_hid_result("HIDDeviceCreateVirtual", status);
    return status;
}

int HIDRegisterDevice(void *device)
{
    static hid_register_fn real;
    if (!real)
        real = (hid_register_fn)dlsym(RTLD_NEXT, "HIDRegisterDevice");
    if (!real)
        return -1;
    int status = real(device);
    log_hid_result("HIDRegisterDevice", status);
    return status;
}

/* QEMU-only experiment: post the stock event after the mesh has settled.
 * The manager is real; optional A/B flags seed display words or bypass the
 * virtual HID branch to isolate emulator dependencies. */
#ifdef MHI2Q_CAPTURE_SEED_BT_IDS_QEMU
/* Exercise the stock DSI device-list copier, not a hand-written manager
 * vector. The outer count/pointer and the inner count/pointer are the two
 * levels read at 0x15a798..0x15a8c4. Each source entry has a 12-byte stride
 * and carries a UnicodeString8 pointer at +4 (0x15a8d4, 0x15a984). */
static int seed_qemu_bluetooth_id(uintptr_t manager)
{
    typedef void (*us8_ctor_fn)(void *, const char *, int, unsigned int,
                                int, unsigned int *);
    typedef void (*us8_dtor_fn)(void *);
    struct source_entry { uint32_t tag; void *value; uint32_t reserved; };
    struct source_list { uint32_t count; struct source_entry *entries; };
    struct source_outer { uint32_t count; struct source_list *lists; };
    static const char id[] = "02:00:00:00:00:01";
    uint32_t string_words[10] = {0}; /* sizeof UnicodeString8 == 0x28 */
    struct source_entry entry = {0, string_words, 0};
    struct source_list list = {1, &entry};
    struct source_outer outer = {1, &list};
    us8_ctor_fn ctor = (us8_ctor_fn)dlsym(RTLD_DEFAULT,
        "_ZN4util14UnicodeString8C1EPKcNS_20UnicodeString_common8EncodingEjbPj");
    us8_dtor_fn dtor = (us8_dtor_fn)dlsym(RTLD_DEFAULT,
        "_ZN4util14UnicodeString8D1Ev");
    if (!ctor || !dtor || *(uint32_t *)(manager + 0x288u) != 0 ||
        *(uintptr_t *)(manager + 0x28cu) != 0) {
        CAPTURE_TRACE("[dio-capture] QEMU Bluetooth ID seed precondition failed\n");
        return -1;
    }
    ctor(string_words, id, 3, (unsigned int)(sizeof(id) - 1u), 0, NULL);
    ((void (*)(void *, const void *))0x0015a758u)((void *)manager, &outer);
    dtor(string_words);
    if (*(uint32_t *)(manager + 0x288u) != 1 ||
        *(uintptr_t *)(manager + 0x28cu) == 0) {
        CAPTURE_TRACE("[dio-capture] QEMU Bluetooth ID stock copy failed\n");
        return -1;
    }
    CAPTURE_TRACE("[dio-capture] QEMU Bluetooth ID stock copy count=1\n");
    return 0;
}
#endif
static void *post_start_event(void *arg)
{
    uintptr_t manager = (uintptr_t)arg;
    char state[220];
    int length;
#ifdef MHI2Q_CAPTURE_STAGE_MDNSD_QEMU
    stage_factory_mdnsd();
#endif
#ifdef MHI2Q_CAPTURE_QEMU_REPEAT_MEDIA
    sleep(1);
#else
    sleep(8);
#endif
    if (__sync_val_compare_and_swap(&captured_manager, manager, manager)
        != manager) {
        CAPTURE_TRACE("[dio-capture] start experiment skipped: manager gone\n");
        return NULL;
    }
#ifdef MHI2Q_CAPTURE_SEED_BT_IDS_QEMU
    if (seed_qemu_bluetooth_id(manager) != 0) {
        __sync_add_and_fetch(&capture_error, 1);
        return NULL;
    }
#endif
#ifdef MHI2Q_CAPTURE_SEED_DISPLAY
    /* A/B QEMU probe only: the four DSI-written display words. Never use
     * these guessed dimensions as a production session configuration. */
    *(uint32_t *)(manager + 0x224u) = 800;
    *(uint32_t *)(manager + 0x228u) = 480;
    *(uint32_t *)(manager + 0x22cu) = 800;
    *(uint32_t *)(manager + 0x230u) = 480;
    CAPTURE_TRACE("[dio-capture] seeded QEMU-only 800x480 display\n");
#endif
    length = snprintf(state, sizeof(state),
                      "[dio-capture] state flags=%02x dims=%u,%u/%u,%u "
                      "hid=%08x,%08x,%08x,%08x alt=%u,%u\n",
                      *(uint8_t *)(manager + 0x174u),
                      *(uint32_t *)(manager + 0x224u),
                      *(uint32_t *)(manager + 0x228u),
                      *(uint32_t *)(manager + 0x22cu),
                      *(uint32_t *)(manager + 0x230u),
                      *(uint32_t *)(manager + 0xb4u),
                      *(uint32_t *)(manager + 0xb8u),
                      *(uint32_t *)(manager + 0xbcu),
                      *(uint32_t *)(manager + 0xc0u),
                      *(uint32_t *)(manager + 0x16cu),
                      *(uint32_t *)(manager + 0x170u));
    if (length > 0 && (size_t)length < sizeof(state))
        (void)write(2, state, (size_t)length);
#ifdef MHI2Q_CAPTURE_TRACKF_GATE_QEMU
    fail_getifaddrs_after_start = 1;
    CAPTURE_TRACE("[dio-capture] armed QEMU Track-F network gate\n");
#endif
#ifdef MHI2Q_CAPTURE_BRIDGE_DRIVES_START
    CAPTURE_TRACE("[dio-capture] QEMU bridge owns first StartAirPlay\n");
#else
    CAPTURE_TRACE("[dio-capture] posting stock StartAirPlay event\n");
    ((void (*)(void *, int))0x00157460u)((void *)manager, 0);
    CAPTURE_TRACE("[dio-capture] stock StartAirPlay enqueue returned\n");
    probe_info_listener();
    log_hid_result("observed AirPlay Bonjour port",
                   (int)mhi2q_probe_airplay_bonjour_port());
    log_hid_result("observed AirPlay Bonjour successes",
                   (int)mhi2q_probe_airplay_bonjour_successes());
#ifdef MHI2Q_CAPTURE_SOFT_RESTART_QEMU
    probe_soft_restart(manager);
#endif
#if defined(MHI2Q_CAPTURE_STOP_QEMU) || defined(MHI2Q_CAPTURE_RESTART_QEMU)
    if (__sync_val_compare_and_swap(&captured_manager, manager, manager)
        == manager) {
        uintptr_t first_thread = *(uintptr_t *)(manager + 0x168u);
        CAPTURE_TRACE("[dio-capture] QEMU calling stock AirPlay stop\n");
        ((void (*)(void *))0x00154ad8u)((void *)manager);
        log_hid_result("QEMU stopped thread pointer retained",
                       *(uintptr_t *)(manager + 0x168u) == first_thread);
        if (first_thread)
            log_hid_result("QEMU stopped util thread pointer",
                           (int)*(uintptr_t *)(first_thread + 4u));
#ifdef MHI2Q_CAPTURE_RESTART_QEMU
        CAPTURE_TRACE("[dio-capture] QEMU posting second StartAirPlay event\n");
        ((void (*)(void *, int))0x00157460u)((void *)manager, 0);
        sleep(3);
        log_hid_result("QEMU second start thread pointer retained",
                       *(uintptr_t *)(manager + 0x168u) == first_thread);
        if (first_thread)
            log_hid_result("QEMU second start util thread pointer",
                           (int)*(uintptr_t *)(first_thread + 4u));
#endif
    }
#endif
#endif
    return NULL;
}
#endif

/* Optional exact-image interface repair. Stock dio hardcodes carplay0 even
 * though the MHI2Q AP observed on the unit is uap0. The compile-time target
 * is lo0 for QEMU or uap0 for a later production bridge. The stock property
 * API still owns validation and storage; this only replaces its value at the
 * single identified call site. The default capture-only build omits it. */
#ifdef MHI2Q_CAPTURE_INTERFACE_NAME
int CFObjectSetPropertyCString(void *object, void *queue, void *setter,
                               uint32_t flags, void *property, void *qualifier,
                               const void *value, size_t length)
{
    typedef int (*setter_fn)(void *, void *, void *, uint32_t, void *,
                             void *, const void *, size_t);
    static setter_fn real;
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    int substitute = capture_enabled && __progname &&
                     strcmp(__progname, "dio_manager") == 0 &&
                     caller == 0x0015c580u &&
                     property == (void *)0x00195ef8u &&
                     value && strcmp((const char *)value, "carplay0") == 0;
    if (!real)
        real = (setter_fn)dlsym(RTLD_NEXT, "CFObjectSetPropertyCString");
    if (!real)
        return -1;
    if (substitute)
        CAPTURE_TRACE("[dio-capture] interfaceName carplay0 -> "
                      MHI2Q_CAPTURE_INTERFACE_NAME "\n");
    return real(object, queue, setter, flags, property, qualifier,
                substitute ? MHI2Q_CAPTURE_INTERFACE_NAME : value, length);
}
#endif

/* The ELF constructor only enables the exact-image guard. Unlike the earlier
 * vtable-slot experiment, no stock data or text is modified. QEMU showed that
 * the controller's init is called directly, bypassing its vtable slot. */
__attribute__((constructor)) static void enable_capture(void)
{
    static const uint8_t init_prefix[8] = {
        0xf0, 0x45, 0x2d, 0xe9, 0x00, 0x40, 0xa0, 0xe1
    };
    static const uint8_t manager_call[12] = {
        0x03, 0xa1, 0xff, 0xeb, 0x00, 0x00, 0x56, 0xe3,
        0x38, 0x60, 0x84, 0xe5
    };
    static const uint8_t manager_new[20] = {
        0x2f, 0x0e, 0xa0, 0xe3, 0x03, 0x70, 0x95, 0xe7,
        0x07, 0x10, 0xa0, 0xe1, 0x5f, 0x83, 0xfe, 0xeb,
        0x40, 0x10, 0x84, 0xe2
    };
    static const uint8_t next_new[20] = {
        0x70, 0x00, 0xa0, 0xe3, 0x07, 0x10, 0xa0, 0xe1,
        0x56, 0x83, 0xfe, 0xeb, 0x00, 0x60, 0xa0, 0xe1,
        0x75, 0xc8, 0xff, 0xeb
    };
    if (!__progname || strcmp(__progname, "dio_manager") != 0)
        return;
    if (memcmp((const void *)0x00171c10u, init_prefix,
               sizeof(init_prefix)) != 0 ||
        memcmp((const void *)0x00171c38u, manager_new,
               sizeof(manager_new)) != 0 ||
        memcmp((const void *)0x00171c50u, manager_call,
               sizeof(manager_call)) != 0 ||
        memcmp((const void *)0x00171c60u, next_new,
               sizeof(next_new)) != 0 ||
        *(const uintptr_t *)DIO_MANAGER_VPTR != DIO_MANAGER_DTOR) {
        capture_error = 1;
        return;
    }
    capture_enabled = 1;
    CAPTURE_TRACE("[dio-capture] image guard passed\n");
#ifdef MHI2Q_CAPTURE_SIGSEGV_QEMU
    {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_sigaction = trace_segv;
        action.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &action, NULL);
    }
#endif
#ifdef MHI2Q_CAPTURE_EXISTING_HOOK_QEMU
    {
        void *symbol = dlsym(RTLD_DEFAULT, "AirPlayCopyServerInfo");
        Dl_info owner = {0};
        if (!symbol || !dladdr(symbol, &owner) || !owner.dli_fname ||
            !strstr(owner.dli_fname, "libcarplay_hook.so")) {
            capture_error = 1;
            CAPTURE_TRACE("[dio-capture] existing hook preload owner=0\n");
            return;
        }
        CAPTURE_TRACE("[dio-capture] existing hook preload owner=1\n");
    }
#endif
}

/* The stock controller allocates its manager at 0x171c44 and returns to
 * 0x171c48. It calls the ctor, stores controller+0x38, then allocates its
 * next object at 0x171c68 (return 0x171c6c). At that second return the
 * manager's constructor has completed. The two return addresses, size and
 * vptr avoid treating an unrelated allocation as a manager. A bounded
 * per-thread table prevents two concurrent controller inits from crossing. */
void *_ZnwjRKSt9nothrow_t(unsigned int size, const void *nothrow_tag)
{
    static nothrow_new_fn real_new;
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    void *result;
    pthread_t self;
    unsigned int i;

    if (!real_new)
        real_new = (nothrow_new_fn)dlsym(RTLD_NEXT, "_ZnwjRKSt9nothrow_t");
    if (!real_new)
        return NULL;
    result = real_new(size, nothrow_tag);
    if (!capture_enabled || !result)
        return result;
    if ((size != 0x2f0u || caller != MANAGER_NEW_RETURN) &&
        (size != 0x70u || caller != NEXT_NEW_RETURN))
        return result;

    self = pthread_self();
    pthread_mutex_lock(&pending_lock);
    if (caller == MANAGER_NEW_RETURN) {
        for (i = 0; i < PENDING_SLOTS; ++i) {
            if (!pending[i].occupied || pthread_equal(pending[i].thread, self)) {
                pending[i].thread = self;
                pending[i].manager = (uintptr_t)result;
                pending[i].occupied = 1;
                break;
            }
        }
        if (i == PENDING_SLOTS)
            __sync_add_and_fetch(&capture_error, 1);
        CAPTURE_TRACE("[dio-capture] manager allocation observed\n");
    } else {
        uintptr_t manager = 0;
        for (i = 0; i < PENDING_SLOTS; ++i) {
            if (pending[i].occupied && pthread_equal(pending[i].thread, self)) {
                manager = pending[i].manager;
                pending[i].manager = 0;
                pending[i].occupied = 0;
                break;
            }
        }
        pthread_mutex_unlock(&pending_lock);
        if (manager && *(const uintptr_t *)manager == DIO_MANAGER_VPTR) {
            __sync_lock_test_and_set(&captured_manager, manager);
            __sync_add_and_fetch(&capture_count, 1);
            CAPTURE_TRACE("[dio-capture] manager captured\n");
#ifdef MHI2Q_CAPTURE_BRIDGE
            mhi2q_bridge_on_manager((void *)manager);
#endif
#ifdef MHI2Q_CAPTURE_START_EXPERIMENT
            pthread_t worker;
            if (pthread_create(&worker, NULL, post_start_event,
                               (void *)manager) == 0)
                pthread_detach(worker);
            else
                __sync_add_and_fetch(&capture_error, 1);
#endif
        } else {
            __sync_add_and_fetch(&capture_error, 1);
            CAPTURE_TRACE("[dio-capture] manager validation failed\n");
        }
        return result;
    }
    pthread_mutex_unlock(&pending_lock);
    return result;
}

/* In the ordinary deleting-destructor path, clear the pointer before the
 * underlying operator delete frees the manager. Other ownership paths still
 * need tracing, and a future bridge must synchronize pointer use with stop. */
void _ZdlPv(void *ptr)
{
    static delete_fn real_delete;
    if (!real_delete)
        real_delete = (delete_fn)dlsym(RTLD_NEXT, "_ZdlPv");
    if (!real_delete) {
        __sync_add_and_fetch(&capture_error, 1);
        return;
    }
    if (ptr && __sync_bool_compare_and_swap(&captured_manager,
                                            (uintptr_t)ptr, 0)) {
        __sync_add_and_fetch(&release_count, 1);
        CAPTURE_TRACE("[dio-capture] manager released\n");
#ifdef MHI2Q_CAPTURE_BRIDGE
        mhi2q_bridge_on_manager_release(ptr);
#endif
    }
    real_delete(ptr);
}

/* Read-only exports for the future QEMU bridge integration. */
void *mhi2q_probe_manager(void)
{
    return (void *)__sync_val_compare_and_swap(&captured_manager, 0, 0);
}

unsigned int mhi2q_probe_captures(void)
{
    return __sync_val_compare_and_swap(&capture_count, 0, 0);
}

unsigned int mhi2q_probe_releases(void)
{
    return __sync_val_compare_and_swap(&release_count, 0, 0);
}

unsigned int mhi2q_probe_error(void)
{
    return __sync_val_compare_and_swap(&capture_error, 0, 0);
}

/* Read-only QEMU evidence for a future START readiness gate. A successful
 * DNSServiceRegister call alone does not prove multicast announcement. */
unsigned int mhi2q_probe_airplay_bonjour_port(void)
{
    return __sync_val_compare_and_swap(&airplay_bonjour_port, 0, 0);
}

unsigned int mhi2q_probe_airplay_bonjour_successes(void)
{
    return __sync_val_compare_and_swap(&airplay_bonjour_successes, 0, 0);
}

void *mhi2q_probe_airplay_server(void)
{
    return (void *)__sync_val_compare_and_swap(&observed_airplay_server, 0, 0);
}

void mhi2q_probe_record_airplay_bonjour(unsigned int port)
{
    __sync_lock_test_and_set(&airplay_bonjour_port, port);
    __sync_add_and_fetch(&airplay_bonjour_successes, 1);
}

void mhi2q_probe_record_airplay_server(void *server)
{
    __sync_lock_test_and_set(&observed_airplay_server, (uintptr_t)server);
}

#ifdef MHI2Q_CAPTURE_WIRELESS_MEDIA_QEMU
#include "stock210_media_qemu.c"
#endif
