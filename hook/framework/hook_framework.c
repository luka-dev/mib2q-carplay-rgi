/*
 * CarPlay Hook Framework - Implementation
 */

#include "hook_framework.h"
#include "../coverart/coverart_hook.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

DEFINE_LOG_MODULE(HOOK);

#define MAX_MODULES 16
#define INJECT_QUEUE_CAPACITY 8
#define INJECT_FRAME_CAPACITY 512
#define INJECT_SHUTDOWN_WAIT_MS 500
/* True only for the main dio_manager process.  dio_manager exec()s helper processes that
 * inherit the LD_PRELOAD; each loads the hook and, on its first hooked call, would open its
 * OWN connection to Java's single-accept bus (127.0.0.1:19810).  Those competing connects
 * churn the server (Java keeps only the last) and steal it from dio_manager — where iAP2 +
 * libairplay + the cover-art tap actually run — so RGI/cover-art frames from dio_manager's
 * connection are silently dropped.  Gate the bus to dio_manager only.  Fail-open if /proc is
 * unreadable (unchanged behaviour). */
int hook_process_is_dio_manager(void) {
    char buf[256];
    int fd = open("/proc/self/cmdline", O_RDONLY);
    int n;
    if (fd < 0) return 1;
    n = (int)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 1;
    buf[n] = 0;
    return strstr(buf, "dio_manager") != NULL;
}

struct hook_module {
    hook_module_def_t def;
    bool active;
};

/* Framework state */
static struct {
    hook_module_t modules[MAX_MODULES];
    int module_count;
    hook_context_t ctx;
    pthread_mutex_t lock;
    bool initialized;
    bool shutting_down;
    volatile bool bus_started;
    volatile bool bus_disabled;
    bool bus_starting;
    uint64_t bus_retry_after_ms;

    /* Live dio-owned ICinemoIAP, retained once by the hook.  Captured at the
     * real dio -> libNmeSDK CinemoCreateIAP PLT boundary. */
    void* cinemo_iap;
    int cinemo_iap_owner_pid;

    /* Function pointers (only the Cinemo NME seams we actually act on).
     * write/writev/MsgSend/MsgSendv were interposed but did nothing except
     * pass through — pure liability (a dlsym miss made them return -1 and fail
     * a real write mid-handshake). Removed. */
    int (*real_decode)(void*, const uint8_t*, int);
    int (*real_encode)(const void*, void*);
    int (*real_transport_send)(void*, const uint8_t*, unsigned int, unsigned int*);
    /* NmeTransport::Recv(NmeArray<uchar>&) — narrow cover-art receive seam
     * (symmetric to real_transport_send). */
    int (*real_transport_recv)(void*, void*);
    int (*real_cinemo_create_iap)(void*);
    int (*real_iap_addref)(void*);
    int (*real_iap_release)(void*);
    int (*real_iap_send_iap2)(void*, int, const void*, int);
    hook_transport_recv_sink_t transport_recv_sink;
    hook_transport_recv_reset_t transport_recv_reset;
} g_fw = {
    .initialized = false,
    .shutting_down = false,
    .bus_started = false,
    .bus_disabled = false
};

/* ICinemoIAP::SendIAP2 is a synchronous stock call with no timeout.  It must
 * never run on NmeTransport's thread, the 1 Hz bus timer, or an ELF destructor:
 * a stuck Cinemo call on any of those paths used to make bus_shutdown() join a
 * non-returning thread and SI eventually reported TIMEOUT_WATCHDOG.  A single
 * process-lifetime worker contains that risk.  The queue stores only immutable
 * frame bytes plus the captured session generation; the worker acquires its own
 * transient COM ref immediately before the stock call. */
typedef struct inject_item_s {
    uint8_t frame[INJECT_FRAME_CAPACITY];
    uint16_t len;
    uint8_t link_session;
    uint32_t generation;
} inject_item_t;

static pthread_mutex_t g_inject_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_inject_cond = PTHREAD_COND_INITIALIZER;
static pthread_t g_inject_tid;
static inject_item_t g_inject_queue[INJECT_QUEUE_CAPACITY];
static unsigned int g_inject_head;
static unsigned int g_inject_tail;
static unsigned int g_inject_count;
static bool g_inject_created;
static bool g_inject_shutdown;
static bool g_inject_exited;

static pthread_once_t g_fw_lock_once = PTHREAD_ONCE_INIT;

static void init_fw_lock_once(void) {
    pthread_mutex_init(&g_fw.lock, NULL);
}

static void ensure_fw_lock_init(void) {
    pthread_once(&g_fw_lock_once, init_fw_lock_once);
}

static void inject_deadline_after_ms(struct timespec* deadline, long ms) {
    clock_gettime(CLOCK_REALTIME, deadline);
    deadline->tv_sec += ms / 1000L;
    deadline->tv_nsec += (ms % 1000L) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

static void* inject_worker_main(void* arg) {
    (void)arg;
    LOG_INFO(LOG_MODULE, "iAP2 injection worker started");
    for (;;) {
        inject_item_t item;
        void* iap = NULL;
        int (*send_iap2)(void*, int, const void*, int) = NULL;
        int (*release_iap)(void*) = NULL;
        int ret;

        pthread_mutex_lock(&g_inject_lock);
        while (!g_inject_shutdown && g_inject_count == 0)
            pthread_cond_wait(&g_inject_cond, &g_inject_lock);
        if (g_inject_shutdown) {
            pthread_mutex_unlock(&g_inject_lock);
            break;
        }
        item = g_inject_queue[g_inject_head];
        g_inject_head = (g_inject_head + 1u) % INJECT_QUEUE_CAPACITY;
        --g_inject_count;
        pthread_mutex_unlock(&g_inject_lock);

        /* Revalidate immediately before sending.  A queued frame from an old
         * phone generation is simply discarded after Identify/reconnect. */
        ensure_fw_lock_init();
        pthread_mutex_lock(&g_fw.lock);
        if (!g_fw.shutting_down && g_fw.initialized &&
            g_fw.ctx.inject.valid &&
            g_fw.ctx.inject.generation == item.generation &&
            g_fw.ctx.inject.link_session == item.link_session &&
            g_fw.cinemo_iap && g_fw.cinemo_iap_owner_pid == (int)getpid() &&
            g_fw.real_iap_send_iap2 && g_fw.real_iap_addref &&
            g_fw.real_iap_release) {
            iap = g_fw.cinemo_iap;
            g_fw.real_iap_addref(iap);
            send_iap2 = g_fw.real_iap_send_iap2;
            release_iap = g_fw.real_iap_release;
        }
        pthread_mutex_unlock(&g_fw.lock);

        if (!iap) {
            LOG_INFO(LOG_MODULE,
                     "Dropped stale semantic frame gen=%u session=%u len=%u",
                     (unsigned)item.generation, (unsigned)item.link_session,
                     (unsigned)item.len);
            continue;
        }

        /* No hook/framework lock is held across the stock synchronous call. */
        ret = send_iap2(iap, (int)item.link_session, item.frame, (int)item.len);
        release_iap(iap);
        if (ret != 0) {
            LOG_WARN(LOG_MODULE,
                     "Stock ICinemoIAP::SendIAP2 failed session=%u len=%u rc=%d",
                     (unsigned)item.link_session, (unsigned)item.len, ret);
        } else {
            LOG_INFO(LOG_MODULE,
                     "Injected semantic frame via stock SendIAP2 session=%u len=%u",
                     (unsigned)item.link_session, (unsigned)item.len);
        }
    }

    pthread_mutex_lock(&g_inject_lock);
    g_inject_exited = true;
    pthread_cond_broadcast(&g_inject_cond);
    pthread_mutex_unlock(&g_inject_lock);
    LOG_INFO(LOG_MODULE, "iAP2 injection worker exiting");
    return NULL;
}

static hook_result_t inject_worker_enqueue(const uint8_t* frame, size_t frame_len,
                                           uint8_t link_session,
                                           uint32_t generation) {
    hook_result_t result = HOOK_OK;

    pthread_mutex_lock(&g_inject_lock);
    if (g_inject_shutdown) {
        result = HOOK_ERR_INIT;
    } else {
        if (!g_inject_created) {
            g_inject_exited = false;
            if (pthread_create(&g_inject_tid, NULL, inject_worker_main, NULL) != 0) {
                result = HOOK_ERR_INIT;
            } else {
                g_inject_created = true;
            }
        }
        if (result == HOOK_OK) {
            if (g_inject_count >= INJECT_QUEUE_CAPACITY) {
                result = HOOK_ERR_BUSY;
            } else {
                inject_item_t* item = &g_inject_queue[g_inject_tail];
                memcpy(item->frame, frame, frame_len);
                item->len = (uint16_t)frame_len;
                item->link_session = link_session;
                item->generation = generation;
                g_inject_tail = (g_inject_tail + 1u) % INJECT_QUEUE_CAPACITY;
                ++g_inject_count;
                pthread_cond_signal(&g_inject_cond);
            }
        }
    }
    pthread_mutex_unlock(&g_inject_lock);
    return result;
}

/* Called only from the process-exit framework destructor.  Never wait forever
 * for a stock SendIAP2 which is already wedged: after the bounded wait the OS
 * will reclaim the process and its threads. */
static void inject_worker_shutdown(void) {
    pthread_t tid;
    bool join_worker = false;
    bool exited = false;
    struct timespec deadline;
    int wait_rc = 0;

    pthread_mutex_lock(&g_inject_lock);
    g_inject_shutdown = true;
    g_inject_head = g_inject_tail = g_inject_count = 0;
    pthread_cond_broadcast(&g_inject_cond);
    if (g_inject_created) {
        tid = g_inject_tid;
        inject_deadline_after_ms(&deadline, INJECT_SHUTDOWN_WAIT_MS);
        while (!g_inject_exited && wait_rc != ETIMEDOUT)
            wait_rc = pthread_cond_timedwait(&g_inject_cond, &g_inject_lock,
                                             &deadline);
        exited = g_inject_exited;
        join_worker = exited;
        if (exited) g_inject_created = false;
    }
    pthread_mutex_unlock(&g_inject_lock);

    if (join_worker) {
        pthread_join(tid, NULL);
    } else if (g_inject_created) {
        LOG_WARN(LOG_MODULE,
                 "iAP2 injection worker did not exit within %d ms; process-exit cleanup will reclaim it",
                 INJECT_SHUTDOWN_WAIT_MS);
    }
}

static void resolve_functions(void) {
    if (!g_fw.real_decode)
        g_fw.real_decode = (int(*)(void*, const uint8_t*, int))dlsym(RTLD_NEXT, "_ZN14NmeIAP2Message6DecodeEPKhi");
    if (!g_fw.real_encode)
        g_fw.real_encode = (int(*)(const void*, void*))dlsym(RTLD_NEXT, "_ZNK14NmeIAP2Message6EncodeER8NmeArrayIhE");
    if (!g_fw.real_transport_send)
        g_fw.real_transport_send = (int(*)(void*, const uint8_t*, unsigned int, unsigned int*))dlsym(RTLD_NEXT, "_ZN12NmeTransport4SendEPKhjPj");
    if (!g_fw.real_transport_recv)
        g_fw.real_transport_recv = (int(*)(void*, void*))dlsym(RTLD_NEXT, "_ZN12NmeTransport4RecvER8NmeArrayIhE");
    if (!g_fw.real_cinemo_create_iap)
        g_fw.real_cinemo_create_iap = (int(*)(void*))dlsym(RTLD_NEXT, "CinemoCreateIAP");
    if (!g_fw.real_iap_addref)
        g_fw.real_iap_addref = (int(*)(void*))dlsym(RTLD_NEXT, "ICinemoIAP_AddRef");
    if (!g_fw.real_iap_release)
        g_fw.real_iap_release = (int(*)(void*))dlsym(RTLD_NEXT, "ICinemoIAP_Release");
    if (!g_fw.real_iap_send_iap2)
        g_fw.real_iap_send_iap2 = (int(*)(void*, int, const void*, int))dlsym(RTLD_NEXT, "ICinemoIAP_SendIAP2");
}

void hook_set_transport_recv_sink(hook_transport_recv_sink_t sink) {
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    g_fw.transport_recv_sink = sink;
    pthread_mutex_unlock(&g_fw.lock);
}

void hook_set_transport_recv_reset(hook_transport_recv_reset_t reset) {
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    g_fw.transport_recv_reset = reset;
    pthread_mutex_unlock(&g_fw.lock);
}

static hook_transport_recv_sink_t hook_get_transport_recv_sink(void) {
    hook_transport_recv_sink_t sink;
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    sink = g_fw.transport_recv_sink;
    pthread_mutex_unlock(&g_fw.lock);
    return sink;
}

static void hook_call_transport_recv_reset(void) {
    hook_transport_recv_reset_t reset;
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    reset = g_fw.transport_recv_reset;
    pthread_mutex_unlock(&g_fw.lock);
    if (reset) reset();
}

static bool is_known_52xx_msg(uint16_t msgid) {
    switch (msgid) {
        case IAP2_MSG_ROUTE_GUIDANCE_START:
        case IAP2_MSG_ROUTE_GUIDANCE_UPDATE:
        case IAP2_MSG_ROUTE_GUIDANCE_MANEUVER:
        case IAP2_MSG_ROUTE_GUIDANCE_STOP:
        case IAP2_MSG_ROUTE_GUIDANCE_LANE:
            return true;
        default:
            return false;
    }
}

static void log_unknown_52xx_msg(const uint8_t* buf, size_t len, uint16_t msgid, msg_direction_t dir) {
    if (!buf || len < 6) return;
    if ((msgid & 0xFF00) != 0x5200) return;
    if (is_known_52xx_msg(msgid)) return;

    const char* dir_str = (dir == MSG_DIR_INCOMING) ? "IN" : "OUT";
    LOG_WARN(LOG_MODULE, "Unknown 0x52xx msgid=0x%04X dir=%s len=%zu", msgid, dir_str, len);
    LOG_HEXDUMP(LOG_MODULE, "IAP2 0x52xx raw", buf, len);
}

/* Capture only the active iAP2 link-session id.  We intentionally do NOT copy
 * the FF5A prefix: its seq/ack belong to the stock link state machine and may
 * already be stale by the time the bus heartbeat emits 0x5200. */
static void store_injection_context(const uint8_t* buf, size_t len,
                                    size_t frame_offset) {
    injection_ctx_t* inj = &g_fw.ctx.inject;
    iap2_link_header_t hdr;
    bool have_link = false;

    if (buf && frame_offset >= 9 && frame_offset <= len)
        have_link = iap2_parse_link_header(buf, frame_offset, &hdr);

    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    if (have_link) {
        inj->link_session = hdr.session;
        inj->generation++;
        if (inj->generation == 0) inj->generation = 1;
        inj->valid = (g_fw.cinemo_iap != NULL &&
                      g_fw.cinemo_iap_owner_pid == (int)getpid());
    }
    pthread_mutex_unlock(&g_fw.lock);
}

static void clear_injection_context(void) {
    injection_ctx_t* inj = &g_fw.ctx.inject;
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    memset(inj, 0, sizeof(*inj));
    pthread_mutex_unlock(&g_fw.lock);
}

/* Notify modules of state change */
static void notify_state(int event, void* event_data) {
    for (int i = 0; i < g_fw.module_count; i++) {
        hook_module_t* mod = &g_fw.modules[i];
        if (mod->active && mod->def.on_state) {
            g_fw.ctx.current_module = mod;
            mod->def.on_state(&g_fw.ctx, event, event_data);
        }
    }
    g_fw.ctx.current_module = NULL;
}

/* Notify modules of transport send */
static void notify_transport_send(uint16_t msgid) {
    for (int i = 0; i < g_fw.module_count; i++) {
        hook_module_t* mod = &g_fw.modules[i];
        if (mod->active && mod->def.on_transport_send) {
            g_fw.ctx.current_module = mod;
            mod->def.on_transport_send(&g_fw.ctx, msgid);
        }
    }
    g_fw.ctx.current_module = NULL;
}

static bool module_wants_message(hook_module_t* mod, uint16_t msgid) {
    if (!mod->def.msg_filter || mod->def.msg_filter_count == 0) return true;
    for (size_t i = 0; i < mod->def.msg_filter_count; i++) {
        if (mod->def.msg_filter[i] == msgid) return true;
    }
    return false;
}

static bool dispatch_message(const iap2_frame_t* frame) {
    bool consumed = false;
    for (int i = 0; i < g_fw.module_count && !consumed; i++) {
        hook_module_t* mod = &g_fw.modules[i];
        if (!mod->active || !mod->def.on_message) continue;
        if (!module_wants_message(mod, frame->msgid)) continue;
        g_fw.ctx.current_module = mod;
        g_fw.ctx.msgid = frame->msgid;
        consumed = mod->def.on_message(&g_fw.ctx, frame);
    }
    g_fw.ctx.current_module = NULL;
    return consumed;
}

static void handle_state_messages(uint16_t msgid) {
    switch (msgid) {
        case IAP2_MSG_IDENTIFY_START:
            /* A new phone/session may reuse the same dio_manager process.
             * Re-arm Identify patching and discard any request queued for the
             * previous iAP2 link before modules observe the boundary. */
            g_fw.ctx.identify_patched = false;
            g_fw.ctx.identify_accepted = false;
            g_fw.ctx.auth_done = false;
            g_fw.ctx.session_active = false;
            g_fw.ctx.rgd_component_valid = false;
            clear_injection_context();
            /* New session boundary — let the transport-recv sink (cover art)
             * drop any half-reassembled stream from a prior session. */
            hook_call_transport_recv_reset();
            notify_state(HOOK_EVENT_IDENTIFY_START, NULL);
            break;
        case IAP2_MSG_IDENTIFY_ACCEPTED:
            g_fw.ctx.identify_accepted = true;
            LOG_INFO(LOG_MODULE, "Identify accepted (0x1D02)");
            notify_state(HOOK_EVENT_IDENTIFY_OK, NULL);
            break;
        case IAP2_MSG_IDENTIFY_END:
            if (!g_fw.ctx.session_active) {
                notify_state(HOOK_EVENT_IDENTIFY_END, NULL);
            }
            break;
        case IAP2_MSG_AUTH_COMPLETE:
            g_fw.ctx.auth_done = true;
            LOG_INFO(LOG_MODULE, "Auth complete (0xAA05)");
            notify_state(HOOK_EVENT_AUTH_DONE, NULL);
            break;
        case IAP2_MSG_STOP_LOCATION:
            /*
             * IAP2_MSG_STOP_LOCATION (0xFFFC) tells the accessory to stop
             * dispatching location updates — it is NOT a session-end signal.
             * iOS sends it for many transient reasons:
             *   - Maps app suspends (CarPlay user opens Music etc).
             *   - iOS detects external nav already active on the cluster
             *     and decides to pause its own RG stream.
             *   - Internal restart of the iOS location subsystem.
             * Real session end / RG end is signalled via 0x5201
             * route_state=0 + source_supports_rg=0 (already debounced
             * downstream in rgd_hook).  Treating STOP_LOCATION as
             * HOOK_EVENT_DISCONNECT caused tight cycle ping-pong
             * (Java teardown → iOS sends new 0x5201 → Java reactivates →
             * STOP_LOCATION fires again → ...), which made native nav
             * BAP cancel commands get blocked during our active windows.
             */
            LOG_INFO(LOG_MODULE,
                     "iAP2 STOP_LOCATION received (location stream paused — "
                     "not a disconnect, ignoring)");
            break;
    }
}

static bool parse_array_bytes(void* arr, uint8_t** out_data, unsigned int* out_len) {
    if (!arr || !out_data || !out_len) return false;
    *out_data = *(uint8_t**)((char*)arr + 0);
    *out_len = *(unsigned int*)((char*)arr + 4);
    return (*out_data != NULL && *out_len > 0);
}

static uint64_t framework_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* bus_init() used to be fire-and-forget.  A single pthread/socket failure then
 * permanently disabled Java/RGI for the lifetime of dio_manager because the
 * framework was already marked initialized.  Elect one retry owner under the
 * framework lock and retry at most once per second from later real hook calls. */
static void framework_try_start_bus(void) {
    uint64_t now;
    hook_result_t rc;

    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    if (g_fw.shutting_down || g_fw.bus_started || g_fw.bus_disabled) {
        pthread_mutex_unlock(&g_fw.lock);
        return;
    }
    pthread_mutex_unlock(&g_fw.lock);
    if (!hook_process_is_dio_manager()) {
        pthread_mutex_lock(&g_fw.lock);
        g_fw.bus_disabled = true;
        pthread_mutex_unlock(&g_fw.lock);
        return;
    }

    now = framework_now_ms();
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    if (g_fw.shutting_down || g_fw.bus_started || g_fw.bus_disabled || g_fw.bus_starting ||
        (now != 0 && g_fw.bus_retry_after_ms != 0 &&
         now < g_fw.bus_retry_after_ms)) {
        pthread_mutex_unlock(&g_fw.lock);
        return;
    }
    g_fw.bus_starting = true;
    pthread_mutex_unlock(&g_fw.lock);

    rc = bus_init();

    pthread_mutex_lock(&g_fw.lock);
    g_fw.bus_starting = false;
    if (rc == HOOK_OK || rc == HOOK_ERR_BUSY) {
        g_fw.bus_started = true;
        g_fw.bus_retry_after_ms = 0;
    } else {
        /* If CLOCK_MONOTONIC itself failed, do not create a permanent retry
         * lockout: leave the deadline at zero and retry on the next hook call. */
        g_fw.bus_retry_after_ms = (now != 0) ? (now + 1000ULL) : 0;
    }
    pthread_mutex_unlock(&g_fw.lock);

    if (rc != HOOK_OK && rc != HOOK_ERR_BUSY)
        LOG_WARN(LOG_MODULE, "bus_init failed rc=%d; retrying after 1000 ms", rc);
}

/* Public API */

hook_result_t hook_framework_init(void) {
    bool already_initialized;

    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    if (g_fw.shutting_down) {
        pthread_mutex_unlock(&g_fw.lock);
        return HOOK_ERR_INIT;
    }
    already_initialized = g_fw.initialized;

    if (!already_initialized) {
        /* Don't call log_init here - logging is lazy (auto-init on first write) */

        resolve_functions();

        memset(&g_fw.ctx, 0, sizeof(g_fw.ctx));
        g_fw.ctx.rgd_component_id = 0x0010;

        g_fw.initialized = true;
    }
    pthread_mutex_unlock(&g_fw.lock);

    /* Register the cover-art receive sink from the same lazy boundary as the
     * rest of the framework.  coverart_runtime_init() is pthread_once-backed,
     * so concurrent first Decode/Encode/Send/Recv calls all wait until the
     * sink is completely installed.  No thread is created here. */
    coverart_runtime_init();

    /* Start/retry the TCP bus only in dio_manager.  Safe here: init is lazy on
     * a real Cinemo call, not in the LD_PRELOAD constructor. */
    framework_try_start_bus();

    /* Don't log in constructor - open() may fail during LD_PRELOAD init */
    /* LOG_INFO will happen on first hooked function call */

    return HOOK_OK;
}

void hook_framework_shutdown(void) {
    void* iap_to_release = NULL;
    int iap_owner_pid = 0;
    bool was_initialized;

    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    was_initialized = g_fw.initialized;
    if (!was_initialized) {
        iap_to_release = g_fw.cinemo_iap;
        iap_owner_pid = g_fw.cinemo_iap_owner_pid;
        g_fw.cinemo_iap = NULL;
        g_fw.cinemo_iap_owner_pid = 0;
        pthread_mutex_unlock(&g_fw.lock);
        if (iap_to_release && iap_owner_pid == (int)getpid() && g_fw.real_iap_release)
            g_fw.real_iap_release(iap_to_release);
        return;
    }

    /* Publish the terminal state while holding the framework lock, then drop
     * it BEFORE callbacks, worker shutdown, bus shutdown, or pthread_join.
     * An in-flight timer injection which has not acquired this lock will now
     * observe shutting_down and return instead of deadlocking with our join. */
    g_fw.shutting_down = true;
    g_fw.initialized = false;
    memset(&g_fw.ctx.inject, 0, sizeof(g_fw.ctx.inject));
    pthread_mutex_unlock(&g_fw.lock);

    notify_state(HOOK_EVENT_SHUTDOWN, NULL);

    /* No module may enqueue another stock transport call after the terminal
     * state above.  Bound the only already-running synchronous SendIAP2. */
    inject_worker_shutdown();

    /* Stop accepting cover-art callbacks and collect its optional worker while
     * the bus is still alive.  Its shutdown path is independently bounded. */
    coverart_runtime_shutdown();

    /* Stop TCP bus */
    bus_shutdown();

    pthread_mutex_lock(&g_fw.lock);
    for (int i = 0; i < g_fw.module_count; i++) {
        g_fw.modules[i].active = false;
    }
    g_fw.module_count = 0;
    g_fw.bus_started = false;
    g_fw.bus_starting = false;
    g_fw.bus_retry_after_ms = 0;
    iap_to_release = g_fw.cinemo_iap;
    iap_owner_pid = g_fw.cinemo_iap_owner_pid;
    g_fw.cinemo_iap = NULL;
    g_fw.cinemo_iap_owner_pid = 0;

    pthread_mutex_unlock(&g_fw.lock);
    if (iap_to_release && iap_owner_pid == (int)getpid() && g_fw.real_iap_release)
        g_fw.real_iap_release(iap_to_release);
    LOG_INFO(LOG_MODULE, "=== CarPlay Hook Stopped ===");
    log_shutdown();
}

hook_result_t hook_framework_register_module(const hook_module_def_t* def) {
    if (!def || !def->name) return HOOK_ERR_PARAM;

    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);

    if (g_fw.module_count >= MAX_MODULES) {
        pthread_mutex_unlock(&g_fw.lock);
        return HOOK_ERR_BUSY;
    }

    /* Check duplicate */
    for (int i = 0; i < g_fw.module_count; i++) {
        if (strcmp(g_fw.modules[i].def.name, def->name) == 0) {
            pthread_mutex_unlock(&g_fw.lock);
            return HOOK_OK;
        }
    }

    /* Insert sorted by priority */
    int insert_idx = g_fw.module_count;
    for (int i = 0; i < g_fw.module_count; i++) {
        if (def->priority < g_fw.modules[i].def.priority) {
            insert_idx = i;
            break;
        }
    }

    for (int i = g_fw.module_count; i > insert_idx; i--) {
        g_fw.modules[i] = g_fw.modules[i - 1];
    }

    g_fw.modules[insert_idx].def = *def;
    g_fw.modules[insert_idx].active = true;
    g_fw.module_count++;

    pthread_mutex_unlock(&g_fw.lock);
    LOG_INFO(LOG_MODULE, "Registered module '%s'", def->name);
    return HOOK_OK;
}

hook_result_t hook_framework_unregister_module(const char* name) {
    if (!name) return HOOK_ERR_PARAM;

    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    for (int i = 0; i < g_fw.module_count; i++) {
        if (strcmp(g_fw.modules[i].def.name, name) == 0) {
            g_fw.modules[i].active = false;
            pthread_mutex_unlock(&g_fw.lock);
            return HOOK_OK;
        }
    }
    pthread_mutex_unlock(&g_fw.lock);
    return HOOK_ERR_NOT_FOUND;
}

hook_context_t* hook_framework_get_context(void) {
    return &g_fw.ctx;
}

/* Send an additional iAP2 frame without consuming the stock carrier. */
hook_result_t hook_inject_frame(const uint8_t* frame, size_t frame_len) {
    uint8_t link_session;
    uint32_t generation;
    hook_result_t queued;

    if (!frame || frame_len < 6 || frame_len > 512 ||
        !iap2_validate_frame(frame, frame_len) ||
        read_be16(frame + 2) != frame_len)
        return HOOK_ERR_PARAM;
    if (!g_fw.initialized || g_fw.shutting_down) return HOOK_ERR_INIT;

    resolve_functions();
    if (!g_fw.real_iap_send_iap2 || !g_fw.real_iap_addref || !g_fw.real_iap_release)
        return HOOK_ERR_INIT;

    /* Snapshot only immutable routing state.  The injection worker revalidates
     * it and acquires the transient COM reference immediately before sending. */
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    if (g_fw.shutting_down || !g_fw.initialized ||
        !g_fw.ctx.inject.valid || !g_fw.cinemo_iap ||
        g_fw.cinemo_iap_owner_pid != (int)getpid()) {
        pthread_mutex_unlock(&g_fw.lock);
        LOG_WARN(LOG_MODULE, "No valid stock ICinemoIAP/link-session context");
        return HOOK_ERR_IO;
    }
    link_session = g_fw.ctx.inject.link_session;
    generation = g_fw.ctx.inject.generation;
    pthread_mutex_unlock(&g_fw.lock);

    queued = inject_worker_enqueue(frame, frame_len, link_session, generation);
    if (queued != HOOK_OK) {
        LOG_WARN(LOG_MODULE,
                 "Semantic injection queue rejected session=%u gen=%u len=%zu rc=%d",
                 (unsigned)link_session, (unsigned)generation, frame_len,
                 queued);
    }
    return queued;
}

hook_result_t hook_inject_message(uint16_t msgid, const uint8_t* payload, size_t payload_len) {
    uint8_t frame[512];
    size_t frame_len = iap2_build_frame(frame, sizeof(frame), msgid, payload, payload_len);
    if (frame_len == 0) return HOOK_ERR_PARAM;
    return hook_inject_frame(frame, frame_len);
}

bool hook_is_ready(void) {
    return g_fw.ctx.identify_patched &&
           g_fw.ctx.identify_accepted &&
           g_fw.ctx.auth_done &&
           g_fw.ctx.rgd_component_valid;
}

bool hook_is_active(void) {
    return g_fw.ctx.session_active;
}

uint16_t hook_get_component_id(void) {
    return g_fw.ctx.rgd_component_id;
}

/* Hooked Functions */

/* dio_manager calls this factory through its PLT (dio 0x17d330).  Capture the
 * returned ICinemoIAP at that genuine executable -> libNmeSDK boundary and
 * retain exactly one hook-owned reference.  This is the stable object whose
 * SendIAP2 method enters Cinemo's normal link state machine; no libairplay or
 * libNmeSDK-internal interposition is assumed here. */
int CinemoCreateIAP(void* args) {
    void* new_iap = NULL;
    void* old_iap = NULL;
    int old_owner_pid = 0;
    int ret;

    ensure_fw_lock_init();
    resolve_functions();
    if (!g_fw.real_cinemo_create_iap) {
        LOG_ERROR(LOG_MODULE, "CinemoCreateIAP real symbol UNRESOLVED");
        return -1;
    }

    ret = g_fw.real_cinemo_create_iap(args);
    if (ret != 0 || !args || !g_fw.real_iap_addref || !g_fw.real_iap_release)
        return ret;

    new_iap = *(void**)args; /* CinemoIAPArgs::iapOut @ +0x00 */
    if (!new_iap) return ret;

    /* AddRef before publication: once visible under g_fw.lock, every injector
     * can safely acquire its own transient ref. */
    g_fw.real_iap_addref(new_iap);
    pthread_mutex_lock(&g_fw.lock);
    old_iap = g_fw.cinemo_iap;
    old_owner_pid = g_fw.cinemo_iap_owner_pid;
    g_fw.cinemo_iap = new_iap;
    g_fw.cinemo_iap_owner_pid = (int)getpid();
    if (g_fw.ctx.inject.generation != 0)
        g_fw.ctx.inject.valid = true;
    pthread_mutex_unlock(&g_fw.lock);

    /* Drop the previous persistent capture ref after the atomic swap.  In a
     * forked child, never mutate the parent's duplicated COM refcount state. */
    if (old_iap && old_owner_pid == (int)getpid())
        g_fw.real_iap_release(old_iap);

    LOG_INFO(LOG_MODULE, "Captured stock ICinemoIAP=%p via CinemoCreateIAP", new_iap);
    return ret;
}

int _ZN14NmeIAP2Message6DecodeEPKhi(void* self, const uint8_t* buf, int len) {
    if (!g_fw.initialized || (!g_fw.bus_started && !g_fw.bus_disabled))
        hook_framework_init();
    resolve_functions();

    int ret = 0;
    if (g_fw.real_decode) ret = g_fw.real_decode(self, buf, len);

    if (ret != 0 || !buf || len < 6) return ret;
    if (buf[0] != 0x40 || buf[1] != 0x40) return ret;

    uint16_t frame_len = read_be16(buf + 2);
    if (frame_len < 6 || frame_len > (uint16_t)len) return ret;

    uint16_t msgid = read_be16(buf + 4);

    g_fw.ctx.direction = MSG_DIR_INCOMING;
    g_fw.ctx.raw_buf = buf;
    g_fw.ctx.raw_len = (size_t)len;

    {
        size_t dump_len = (frame_len <= (uint16_t)len) ? (size_t)frame_len : (size_t)len;
        log_unknown_52xx_msg(buf, dump_len, msgid, MSG_DIR_INCOMING);
    }

    handle_state_messages(msgid);

    iap2_frame_t frame = {
        .offset = 0,
        .frame_len = frame_len,
        .msgid = msgid,
        .payload = (frame_len > 6) ? (buf + 6) : NULL,
        .payload_len = (frame_len > 6) ? (frame_len - 6) : 0
    };
    dispatch_message(&frame);

    return ret;
}

int _ZNK14NmeIAP2Message6EncodeER8NmeArrayIhE(const void* self, void* out_array) {
    if (!g_fw.initialized || (!g_fw.bus_started && !g_fw.bus_disabled))
        hook_framework_init();
    resolve_functions();

    int ret = -1;
    if (g_fw.real_encode) ret = g_fw.real_encode(self, out_array);
    if (ret != 0) return ret;

    uint8_t* data = NULL;
    unsigned int len = 0;
    if (!parse_array_bytes(out_array, &data, &len) || len < 6) return ret;

    int msgid = iap2_parse_msgid(data, len);
    if (msgid < 0) return ret;

    /* Identify patching */
    if (msgid == IAP2_MSG_IDENTIFY && !g_fw.ctx.identify_patched) {
        bool patched = g_fw.ctx.identify_patched;
        g_fw.ctx._priv = out_array;

        for (int i = 0; i < g_fw.module_count; i++) {
            hook_module_t* mod = &g_fw.modules[i];
            if (!mod->active || !mod->def.on_identify) continue;

            g_fw.ctx.current_module = mod;
            unsigned int cap = *(unsigned int*)((char*)out_array + 8);
            size_t new_len = mod->def.on_identify(&g_fw.ctx, data, len, cap);

            if (new_len != len) {
                *(unsigned int*)((char*)out_array + 4) = (unsigned int)new_len;
                len = (unsigned int)new_len;
                parse_array_bytes(out_array, &data, &len);
                patched = true;
            } else if (g_fw.ctx.identify_patched) {
                patched = true;
            }
        }

        g_fw.ctx.current_module = NULL;
        g_fw.ctx._priv = NULL;
        if (patched) {
            g_fw.ctx.identify_patched = true;
        }
    }

    iap2_frame_t frame = {
        .offset = 0,
        .frame_len = read_be16(data + 2),
        .msgid = (uint16_t)msgid,
        .payload = (len > 6) ? (data + 6) : NULL,
        .payload_len = (len > 6) ? (len - 6) : 0
    };

    g_fw.ctx.direction = MSG_DIR_OUTGOING;
    g_fw.ctx.raw_buf = data;
    g_fw.ctx.raw_len = len;

    {
        size_t dump_len = (frame.frame_len <= len) ? frame.frame_len : len;
        log_unknown_52xx_msg(data, dump_len, frame.msgid, MSG_DIR_OUTGOING);
    }

    dispatch_message(&frame);

    return ret;
}

int _ZN12NmeTransport4SendEPKhjPj(void* self, const uint8_t* buf, unsigned int len, unsigned int* sent) {
    iap2_frame_t frame;
    bool have_frame = false;
    int ret;

    if (!g_fw.initialized || (!g_fw.bus_started && !g_fw.bus_disabled))
        hook_framework_init();
    resolve_functions();

    if (!g_fw.real_transport_send) return -1;

    if (buf && len >= 6)
        have_frame = iap2_find_frame(buf, (size_t)len, &frame);

    /* The stock message must be committed first.  In particular LocationInfo
     * is never consumed or delayed as an injection carrier. */
    ret = g_fw.real_transport_send(self, buf, len, sent);
    if (ret != 0 || !have_frame) return ret;

    store_injection_context(buf, (size_t)len, frame.offset);
    notify_transport_send(frame.msgid);

    return ret;
}

/* NmeTransport::Recv(NmeArray<uchar>&) — receive counterpart to Send.
 * Cover art taps the raw FF5A link bytes here instead of globally interposing
 * libc read()/recv().  We call the real Recv, then hand the freshly-filled
 * NmeArray<uchar> (data@+0, len@+4 — same layout Encode uses) to the sink.
 * Fail-safe mirrors the Send hook: if the real symbol didn't resolve we return
 * -1 (same behaviour already deployed for Send). */
int _ZN12NmeTransport4RecvER8NmeArrayIhE(void* self, void* out_array) {
    if (!g_fw.initialized || (!g_fw.bus_started && !g_fw.bus_disabled))
        hook_framework_init();
    resolve_functions();

    if (!g_fw.real_transport_recv) {
        /* Deployment prerequisite: if OUR symbol interposes Recv but the real
         * one didn't resolve via RTLD_NEXT, we cannot forward the receive and
         * -1 will likely wedge the Cinemo transport.  Log once so this is
         * diagnosable rather than a silent dead CarPlay. */
        static volatile int warned = 0;
        if (!warned) { warned = 1; LOG_ERROR(LOG_MODULE, "NmeTransport::Recv real symbol UNRESOLVED — transport receive will fail"); }
        return -1;
    }
    int ret = g_fw.real_transport_recv(self, out_array);

    hook_transport_recv_sink_t sink = hook_get_transport_recv_sink();
    uint8_t* data = NULL;
    unsigned int len = 0;
    bool have = (out_array && parse_array_bytes(out_array, &data, &len));

    /* One-shot on-unit validation of the Recv return convention: log ret +
     * first bytes.  Confirms 0==success (we tap only on ret==0) and that the
     * array carries FF 5A link frames.  If ret is a positive byte-count here,
     * this line reveals it and the tap gate must change. */
    static volatile int logged = 0;
    if (!logged && have && len >= 4) {
        logged = 1;
        LOG_INFO(LOG_MODULE, "NmeTransport::Recv ret=%d len=%u [%02X %02X %02X %02X]",
                 ret, len, data[0], data[1], data[2], data[3]);
    }

    /* Tap only on success.  On an error return the array may hold stale bytes;
     * re-feeding them would corrupt the reassembler with duplicates. */
    if (ret == 0 && sink && have) {
        sink(data, len);
    }
    return ret;
}

/* write/writev/MsgSend/MsgSendv are intentionally NOT interposed anymore:
 * they carried no hook logic, only added a dlsym-miss -> return -1 failure
 * mode onto every process write. Left to libc directly. */

/* No LD_PRELOAD constructor.  hook_framework_init() — and therefore bus_init() plus
 * the process-wide fault handlers it installs — must NOT run during dio_manager's
 * dlopen/link, before dio has initialised itself.  Every hooked NmeIAP2Message entry
 * (Decode/Encode/Send/Recv) lazily calls hook_framework_init() on its first invocation,
 * and each module self-registers via its own constructor, so init happens on the first
 * real iAP2 message — exactly the lazy behaviour the bus_init comment above assumes. */
__attribute__((destructor))
static void hook_lib_fini(void) {
    hook_framework_shutdown();
}
