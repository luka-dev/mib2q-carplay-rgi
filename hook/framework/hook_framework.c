/*
 * CarPlay Hook Framework - Implementation
 */

#include "hook_framework.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

DEFINE_LOG_MODULE(HOOK);

#define MAX_MODULES 16
#define MAX_PENDING_FRAME 512

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

/* Semantic injection queue.  We cannot safely call NmeTransport::Send with a
 * copied FF5A prefix: that replays the previous link seq/ack and can either be
 * dropped or desynchronise unrelated iAP2 services.  Instead we replace one
 * outgoing LocationInfo Encode result and let the stock stack wrap it. */
static struct {
    pthread_mutex_t lock;
    bool valid;
    uint8_t frame[MAX_PENDING_FRAME];
    size_t len;
    uint16_t msgid;
} g_pending = {
    .lock = PTHREAD_MUTEX_INITIALIZER
};

/* Framework state */
static struct {
    hook_module_t modules[MAX_MODULES];
    int module_count;
    hook_context_t ctx;
    pthread_mutex_t lock;
    bool initialized;

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
    hook_transport_recv_sink_t transport_recv_sink;
    hook_transport_recv_reset_t transport_recv_reset;
} g_fw = {
    .initialized = false
};

static bool g_fw_lock_initialized = false;

static void ensure_fw_lock_init(void) {
    if (!g_fw_lock_initialized) {
        pthread_mutex_init(&g_fw.lock, NULL);
        g_fw_lock_initialized = true;
    }
}

static void clear_pending_frame(void) {
    pthread_mutex_lock(&g_pending.lock);
    g_pending.valid = false;
    g_pending.len = 0;
    g_pending.msgid = 0;
    pthread_mutex_unlock(&g_pending.lock);
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
}

void hook_set_transport_recv_sink(hook_transport_recv_sink_t sink) {
    g_fw.transport_recv_sink = sink;
}

void hook_set_transport_recv_reset(hook_transport_recv_reset_t reset) {
    g_fw.transport_recv_reset = reset;
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
            clear_pending_frame();
            /* New session boundary — let the transport-recv sink (cover art)
             * drop any half-reassembled stream from a prior session. */
            if (g_fw.transport_recv_reset) g_fw.transport_recv_reset();
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

/* Substitute one expendable LocationInfo message at the NmeIAP2Message
 * boundary.  At this point the bytes are still the 0x4040 session message;
 * Cinemo has not assigned the FF5A link seq/ack yet. */
static bool replace_location_with_pending(void* out_array,
                                          uint8_t** io_data,
                                          unsigned int* io_len,
                                          uint16_t carrier_msgid,
                                          uint16_t* out_msgid) {
    if (!out_array || !io_data || !io_len || !out_msgid)
        return false;
    if (carrier_msgid != IAP2_MSG_LOCATION_INFO)
        return false;

    pthread_mutex_lock(&g_pending.lock);
    if (!g_pending.valid || g_pending.len < IAP2_MIN_FRAME) {
        pthread_mutex_unlock(&g_pending.lock);
        return false;
    }

    uint8_t* base = *(uint8_t**)((char*)out_array + 0);
    unsigned int cap = *(unsigned int*)((char*)out_array + 8);
    if (cap < g_pending.len) {
        size_t new_cap = g_pending.len + 64;
        uint8_t* grown = (uint8_t*)realloc(base, new_cap);
        if (!grown) {
            pthread_mutex_unlock(&g_pending.lock);
            LOG_ERROR(LOG_MODULE,
                      "Semantic iAP2 carrier resize failed: need=%zu cap=%u",
                      g_pending.len, cap);
            return false;
        }
        base = grown;
        *(uint8_t**)((char*)out_array + 0) = grown;
        *(unsigned int*)((char*)out_array + 8) = (unsigned int)new_cap;
    }

    memcpy(base, g_pending.frame, g_pending.len);
    *(unsigned int*)((char*)out_array + 4) = (unsigned int)g_pending.len;
    *io_data = base;
    *io_len = (unsigned int)g_pending.len;
    *out_msgid = g_pending.msgid;

    uint16_t injected_msgid = g_pending.msgid;
    size_t injected_len = g_pending.len;
    g_pending.valid = false;
    g_pending.len = 0;
    g_pending.msgid = 0;
    pthread_mutex_unlock(&g_pending.lock);

    LOG_INFO(LOG_MODULE,
             "Semantic iAP2 send: replaced carrier 0x%04X with 0x%04X len=%zu; stock owns FF5A seq/ack",
             carrier_msgid, injected_msgid, injected_len);
    return true;
}

/* Public API */

hook_result_t hook_framework_init(void) {
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    if (g_fw.initialized) {
        pthread_mutex_unlock(&g_fw.lock);
        return HOOK_OK;
    }

    /* Don't call log_init here - logging is lazy (auto-init on first write) */

    resolve_functions();

    memset(&g_fw.ctx, 0, sizeof(g_fw.ctx));
    g_fw.ctx.rgd_component_id = 0x0010;
    clear_pending_frame();

    g_fw.initialized = true;
    pthread_mutex_unlock(&g_fw.lock);

    /* Start TCP bus listener — ONLY in the main dio_manager process (helper processes that
     * inherit the LD_PRELOAD must not open competing connections; see hook_process_is_dio_manager).
     * Safe here - hook_framework_init is lazy (first hooked call), not the LD_PRELOAD constructor. */
    if (hook_process_is_dio_manager()) {
        bus_init();
    }

    /* Don't log in constructor - open() may fail during LD_PRELOAD init */
    /* LOG_INFO will happen on first hooked function call */

    return HOOK_OK;
}

void hook_framework_shutdown(void) {
    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);
    if (!g_fw.initialized) {
        pthread_mutex_unlock(&g_fw.lock);
        return;
    }

    notify_state(HOOK_EVENT_SHUTDOWN, NULL);

    /* Stop TCP bus */
    bus_shutdown();

    clear_pending_frame();

    for (int i = 0; i < g_fw.module_count; i++) {
        g_fw.modules[i].active = false;
    }
    g_fw.module_count = 0;
    g_fw.initialized = false;

    pthread_mutex_unlock(&g_fw.lock);
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

/* Queue a semantic iAP2 frame for the stock Encode -> link-layer path. */
hook_result_t hook_inject_frame(const uint8_t* frame, size_t frame_len) {
    if (!frame || frame_len < 6) return HOOK_ERR_PARAM;
    if (!g_fw.initialized) return HOOK_ERR_INIT;
    if (frame_len > MAX_PENDING_FRAME || !iap2_validate_frame(frame, frame_len)
            || read_be16(frame + 2) != frame_len)
        return HOOK_ERR_PARAM;

    uint16_t msgid = read_be16(frame + 4);
    pthread_mutex_lock(&g_pending.lock);
    if (g_pending.valid && g_pending.msgid != msgid) {
        pthread_mutex_unlock(&g_pending.lock);
        return HOOK_ERR_BUSY;
    }
    memcpy(g_pending.frame, frame, frame_len);
    g_pending.len = frame_len;
    g_pending.msgid = msgid;
    g_pending.valid = true;
    pthread_mutex_unlock(&g_pending.lock);

    LOG_INFO(LOG_MODULE, "Queued semantic iAP2 msg=0x%04X len=%zu for stock Encode path",
             msgid, frame_len);
    return HOOK_OK;
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

int _ZN14NmeIAP2Message6DecodeEPKhi(void* self, const uint8_t* buf, int len) {
    if (!g_fw.initialized) hook_framework_init();
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
    if (!g_fw.initialized) hook_framework_init();
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

    /* Route-guidance requests must enter before Cinemo's link layer.  Replace
     * a single periodic LocationInfo carrier rather than issuing a second raw
     * NmeTransport::Send with a duplicated link sequence number. */
    {
        uint16_t outgoing_msgid = (uint16_t)msgid;
        if (replace_location_with_pending(out_array, &data, &len,
                                          (uint16_t)msgid, &outgoing_msgid)) {
            msgid = (int)outgoing_msgid;
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
    if (!g_fw.initialized) hook_framework_init();
    resolve_functions();

    if (!g_fw.real_transport_send) return -1;

    /* Observe the final stock-produced link packet.  Injection has already
     * happened at NmeIAP2Message::Encode, so this function never recursively
     * calls Send and never manufactures an FF5A header. */
    if (buf && len >= 6) {
        iap2_frame_t frame;
        if (iap2_find_frame(buf, (size_t)len, &frame)) {
            /* Notify modules.  A LocationInfo may queue a request for the
             * next LocationInfo Encode; the current packet is already built. */
            notify_transport_send(frame.msgid);
        }
    }

    return g_fw.real_transport_send(self, buf, len, sent);
}

/* NmeTransport::Recv(NmeArray<uchar>&) — receive counterpart to Send.
 * Cover art taps the raw FF5A link bytes here instead of globally interposing
 * libc read()/recv().  We call the real Recv, then hand the freshly-filled
 * NmeArray<uchar> (data@+0, len@+4 — same layout Encode uses) to the sink.
 * Fail-safe mirrors the Send hook: if the real symbol didn't resolve we return
 * -1 (same behaviour already deployed for Send). */
int _ZN12NmeTransport4RecvER8NmeArrayIhE(void* self, void* out_array) {
    if (!g_fw.initialized) hook_framework_init();
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

    hook_transport_recv_sink_t sink = g_fw.transport_recv_sink;
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

__attribute__((constructor))
static void hook_lib_init(void) {
    hook_framework_init();
}

__attribute__((destructor))
static void hook_lib_fini(void) {
    hook_framework_shutdown();
}
