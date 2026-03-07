/*
 * CarPlay Hook Framework - Implementation
 */

#include "hook_framework.h"

DEFINE_LOG_MODULE(HOOK);

#define MAX_MODULES 16
#define MAX_PREFIX_LEN 64

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
    volatile bool in_send;  /* Prevent recursive injection */

    /* Function pointers */
    int (*real_decode)(void*, const uint8_t*, int);
    int (*real_encode)(const void*, void*);
    ssize_t (*real_write)(int, const void*, size_t);
    ssize_t (*real_writev)(int, const struct iovec*, int);
    int (*real_transport_send)(void*, const uint8_t*, unsigned int, unsigned int*);
#ifdef __QNX__
    int (*real_msgsend)(int, const void*, int, void*, int);
    int (*real_msgsendv)(int, const iov_t*, int, const iov_t*, int);
#endif
} g_fw = {
    .initialized = false,
    .in_send = false
};

static bool g_fw_lock_initialized = false;

static void ensure_fw_lock_init(void) {
    if (!g_fw_lock_initialized) {
        pthread_mutex_init(&g_fw.lock, NULL);
        g_fw_lock_initialized = true;
    }
}

static void resolve_functions(void) {
    if (!g_fw.real_write)
        g_fw.real_write = (ssize_t(*)(int, const void*, size_t))dlsym(RTLD_NEXT, "write");
    if (!g_fw.real_writev)
        g_fw.real_writev = (ssize_t(*)(int, const struct iovec*, int))dlsym(RTLD_NEXT, "writev");
    if (!g_fw.real_decode)
        g_fw.real_decode = (int(*)(void*, const uint8_t*, int))dlsym(RTLD_NEXT, "_ZN14NmeIAP2Message6DecodeEPKhi");
    if (!g_fw.real_encode)
        g_fw.real_encode = (int(*)(const void*, void*))dlsym(RTLD_NEXT, "_ZNK14NmeIAP2Message6EncodeER8NmeArrayIhE");
    if (!g_fw.real_transport_send)
        g_fw.real_transport_send = (int(*)(void*, const uint8_t*, unsigned int, unsigned int*))dlsym(RTLD_NEXT, "_ZN12NmeTransport4SendEPKhjPj");
#ifdef __QNX__
    if (!g_fw.real_msgsend)
        g_fw.real_msgsend = (int(*)(int, const void*, int, void*, int))dlsym(RTLD_NEXT, "MsgSend");
    if (!g_fw.real_msgsendv)
        g_fw.real_msgsendv = (int(*)(int, const iov_t*, int, const iov_t*, int))dlsym(RTLD_NEXT, "MsgSendv");
#endif
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

/* Store link-layer prefix for later injection */
static void store_injection_context(void* transport_self, const uint8_t* buf, size_t len, size_t frame_offset) {
    injection_ctx_t* inj = &g_fw.ctx.inject;

    ensure_fw_lock_init();
    pthread_mutex_lock(&g_fw.lock);

    /* Free old prefix */
    if (inj->prefix_buf) {
        free(inj->prefix_buf);
        inj->prefix_buf = NULL;
    }

    inj->transport_self = transport_self;
    inj->original_total_len = len;

    /* Store prefix (link header before iAP2 frame) */
    if (frame_offset > 0 && frame_offset <= MAX_PREFIX_LEN) {
        inj->prefix_buf = (uint8_t*)malloc(frame_offset);
        if (inj->prefix_buf) {
            memcpy(inj->prefix_buf, buf, frame_offset);
            inj->prefix_len = frame_offset;
        }
    } else {
        inj->prefix_len = 0;
    }

    /* Detect checksum algorithm from original frame */
    if (inj->cksum_algo == IAP2_CKSUM_UNKNOWN && frame_offset > 0) {
        iap2_frame_t frame;
        if (iap2_find_frame(buf, len, &frame)) {
            size_t cksum_pos = frame.offset + frame.frame_len;
            if (cksum_pos < len) {
                uint8_t expected = buf[cksum_pos];
                inj->cksum_algo = iap2_detect_cksum_algo(buf + frame.offset, frame.frame_len, expected);
                if (inj->cksum_algo != IAP2_CKSUM_UNKNOWN) {
                    LOG_INFO(LOG_MODULE, "Detected checksum algo=%d", inj->cksum_algo);
                }
            }
        }
    }

    inj->valid = (transport_self != NULL);
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
            if (g_fw.ctx.session_active) {
                g_fw.ctx.session_active = false;
                notify_state(HOOK_EVENT_DISCONNECT, NULL);
            }
            break;
    }
}

static bool parse_array_bytes(void* arr, uint8_t** out_data, unsigned int* out_len) {
    if (!arr || !out_data || !out_len) return false;
    *out_data = *(uint8_t**)((char*)arr + 0);
    *out_len = *(unsigned int*)((char*)arr + 4);
    return (*out_data != NULL && *out_len > 0);
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
    g_fw.ctx.inject.cksum_algo = IAP2_CKSUM_UNKNOWN;

    g_fw.initialized = true;
    pthread_mutex_unlock(&g_fw.lock);

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

    /* Free injection context */
    if (g_fw.ctx.inject.prefix_buf) {
        free(g_fw.ctx.inject.prefix_buf);
        g_fw.ctx.inject.prefix_buf = NULL;
    }

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

/* Inject frame with proper link-layer handling */
hook_result_t hook_inject_frame(const uint8_t* frame, size_t frame_len) {
    if (!frame || frame_len < 6) return HOOK_ERR_PARAM;
    if (!g_fw.initialized) return HOOK_ERR_INIT;

    injection_ctx_t* inj = &g_fw.ctx.inject;

    if (!inj->valid || !inj->transport_self || !g_fw.real_transport_send) {
        LOG_WARN(LOG_MODULE, "No valid injection context");
        return HOOK_ERR_IO;
    }

    g_fw.in_send = true;

    hook_result_t result = HOOK_OK;

    if (inj->prefix_len > 0 && inj->prefix_buf) {
        /* Build full frame: prefix + iAP2 frame + checksum */
        size_t total = inj->prefix_len + frame_len + 1;
        uint8_t* out = (uint8_t*)malloc(total);
        if (!out) {
            g_fw.in_send = false;
            return HOOK_ERR_MEMORY;
        }

        /* Copy prefix */
        memcpy(out, inj->prefix_buf, inj->prefix_len);

        /* Copy frame */
        memcpy(out + inj->prefix_len, frame, frame_len);

        /* Calculate and append checksum */
        iap2_cksum_algo_t algo = (inj->cksum_algo != IAP2_CKSUM_UNKNOWN) ? inj->cksum_algo : IAP2_CKSUM_SUM;
        out[inj->prefix_len + frame_len] = iap2_calc_cksum(frame, frame_len, algo);

        /* Patch length in prefix (link header) */
        iap2_patch_link_header(out, inj->prefix_len, total);

        unsigned int sent = 0;
        int ret = g_fw.real_transport_send(inj->transport_self, out, (unsigned int)total, &sent);
        if (ret != 0) {
            result = HOOK_ERR_IO;
        }

        free(out);
        LOG_INFO(LOG_MODULE, "Injected frame with prefix (len=%zu)", total);
    } else {
        /* No prefix - send raw frame */
        unsigned int sent = 0;
        int ret = g_fw.real_transport_send(inj->transport_self, frame, (unsigned int)frame_len, &sent);
        if (ret != 0) {
            result = HOOK_ERR_IO;
        }
        LOG_INFO(LOG_MODULE, "Injected raw frame (len=%zu)", frame_len);
    }

    g_fw.in_send = false;
    return result;
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

    /* Skip if we're injecting */
    if (g_fw.in_send) {
        return g_fw.real_transport_send(self, buf, len, sent);
    }

    /* Find iAP2 frame and store injection context */
    if (buf && len >= 6) {
        iap2_frame_t frame;
        if (iap2_find_frame(buf, (size_t)len, &frame)) {
            /* Store context for injection */
            store_injection_context(self, buf, (size_t)len, frame.offset);

            /* Notify modules - they can trigger injection here */
            notify_transport_send(frame.msgid);
        }
    }

    return g_fw.real_transport_send(self, buf, len, sent);
}

ssize_t write(int fd, const void* buf, size_t count) {
    if (!g_fw.initialized) hook_framework_init();
    resolve_functions();
    if (!g_fw.real_write) return -1;
    return g_fw.real_write(fd, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    if (!g_fw.initialized) hook_framework_init();
    resolve_functions();
    if (!g_fw.real_writev) return -1;
    return g_fw.real_writev(fd, iov, iovcnt);
}

#ifdef __QNX__
int MsgSend(int coid, const void* smsg, int sbytes, void* rmsg, int rbytes) {
    if (!g_fw.initialized) hook_framework_init();
    resolve_functions();
    if (!g_fw.real_msgsend) return -1;
    return g_fw.real_msgsend(coid, smsg, sbytes, rmsg, rbytes);
}

int MsgSendv(int coid, const iov_t* siov, int sparts, const iov_t* riov, int rparts) {
    if (!g_fw.initialized) hook_framework_init();
    resolve_functions();
    if (!g_fw.real_msgsendv) return -1;
    return g_fw.real_msgsendv(coid, siov, sparts, riov, rparts);
}
#endif

__attribute__((constructor))
static void hook_lib_init(void) {
    hook_framework_init();
}

__attribute__((destructor))
static void hook_lib_fini(void) {
    hook_framework_shutdown();
}
