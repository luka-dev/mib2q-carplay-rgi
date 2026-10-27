/*
 * CarPlay Telephony Hook Module - see tel_hook.h.
 *
 * Wire formats: StartListUpdates 0x4170 and ListUpdate 0x4171 as Cinemo builds
 * and parses them (libNmeVfs.so MU1316 @0xFBA50 / @0xF9880, recents entry
 * @0xF7F70), byte-identical to xPaiiN's car-verified request and captures
 * (tests/tel_hook_test.c).  HoldStatusUpdate 0x415F and MuteStatusUpdate
 * 0x4160 carry the status as param 0: iOS accessoryd reads only param 0 and
 * drops the message without it (iOS 27 __holdStatusUpdateHandler /
 * __muteStatusUpdateHandler).  Cinemo's unused builders @0xFB3C4 / @0xFA834 put
 * it in param 1, which iOS ignores.  Under CarPlay iOS caps each list at 12.  Stock dio_manager never calls any of them; the hook sends
 * them as extra messages through the stock SendIAP2 worker and never replaces
 * a stock message.
 *
 * If iOS rejects 0x4170 in Identify, Cinemo drops it from the identification
 * pool by itself (OnIdentificationRejected @0xE438C) and the subscription below
 * simply gets no answer.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "tel_hook.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DEFINE_LOG_MODULE(TELEPHONY);

#define MSG_START_CALL_STATE    0x4154  /* stock subscription = link is up */
#define MSG_COMM_UPDATE         0x4158  /* stock subscription, read-only here */
#define MSG_ACCEPT_CALL         0x415B
#define MSG_END_CALL            0x415C
#define MSG_SWAP_CALLS          0x415D
#define MSG_HOLD_STATUS_UPDATE  0x415F
#define MSG_MUTE_STATUS_UPDATE  0x4160
#define MSG_START_LIST_UPDATES  0x4170
#define MSG_LIST_UPDATE         0x4171
#define MSG_START_POWER_UPDATES 0xAE00  /* stock subscription, extended once */
#define MSG_POWER_UPDATE        0xAE01  /* stock receives it, read-only here */
#define POWER_CHARGING_STATE    5
#define POWER_CHARGE_LEVEL      6

/* CMD_TEL actions (Java -> hook), payload [u8 action][args]. */
#define TEL_CMD_MUTE            1       /* [u8 on] */
#define TEL_CMD_HOLD            2       /* [u8 on] */
#define TEL_CMD_ACCEPT          3       /* [u8 action]: 0 accept / hold-and-accept */
#define TEL_CMD_END             4       /* [u8 action]: 0 end, 1 end all */
#define TEL_CMD_SWAP            5       /* [u8 ignored]: SwapCalls has no params */

static const uint16_t tel_msg_filter[] = { MSG_START_CALL_STATE, MSG_COMM_UPDATE, MSG_LIST_UPDATE,
                                           MSG_START_POWER_UPDATES, MSG_POWER_UPDATE };

static pthread_mutex_t tel_lock = PTHREAD_MUTEX_INITIALIZER;
static bool tel_lists_requested;        /* once per Identify cycle */
static bool tel_power_requested;        /* once per Identify cycle */
/* Extended StartPowerUpdates waiting for the phone's first PowerUpdate after stock's own. */
static uint8_t tel_power_union[96];
static size_t tel_power_union_len;
static bool tel_battery_enabled;        /* IAP2_PHONE_BATTERY, read at init */

/* On by value: unset, "", 0, off, false, no = off (an exported "0" means off). */
static bool tel_env_on(const char* name, bool def) {
    const char* v = getenv(name);
    if (!v) return def;
    return !(!*v || !strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "false") || !strcmp(v, "no"));
}

/* ---------------- builders ---------------- */

static size_t put_tlv(uint8_t* out, size_t max, size_t off, uint16_t id,
                      const void* v, size_t n) {
    size_t w;
    if (off >= max) return 0;
    w = iap2_build_tlv(out + off, max - off, id, (const uint8_t*)v, n);
    return w ? off + w : 0;
}

size_t tel_build_start_list_updates(uint8_t* out, size_t out_max,
                                    uint16_t recents_max, uint16_t favorites_max) {
    /* Every property, exactly as the car-verified MST2 request asks. */
    static const uint8_t recent_props[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    static const uint8_t favorite_props[] = { 0, 1, 2, 3, 4, 5 };
    uint8_t grp[64];
    uint8_t be[2];
    uint8_t one = 1;
    size_t g = 0, off = 0, i;

    for (i = 0; i < sizeof(recent_props); i++)
        if (!(g = put_tlv(grp, sizeof(grp), g, recent_props[i], NULL, 0))) return 0;
    if (!(off = put_tlv(out, out_max, 0, 1, grp, g))) return 0;
    be[0] = (uint8_t)(recents_max >> 8); be[1] = (uint8_t)recents_max;
    if (!(off = put_tlv(out, out_max, off, 3, be, 2))) return 0;
    if (!(off = put_tlv(out, out_max, off, 4, &one, 1))) return 0;      /* combine */

    g = 0;
    for (i = 0; i < sizeof(favorite_props); i++)
        if (!(g = put_tlv(grp, sizeof(grp), g, favorite_props[i], NULL, 0))) return 0;
    if (!(off = put_tlv(out, out_max, off, 6, grp, g))) return 0;
    be[0] = (uint8_t)(favorites_max >> 8); be[1] = (uint8_t)favorites_max;
    return put_tlv(out, out_max, off, 8, be, 2);
}

size_t tel_build_status(uint8_t* out, size_t out_max, bool on) {
    uint8_t v = on ? 1 : 0;
    return put_tlv(out, out_max, 0, 0, &v, 1);
}

/* ---------------- parser ---------------- */

static void copy_str(char* dst, const iap2_tlv_t* t) {
    size_t n = t->value_len;
    if (n >= TEL_TEXT_MAX) n = TEL_TEXT_MAX - 1;
    memcpy(dst, t->value, n);
    dst[n] = 0;
    dst[strcspn(dst, "\r\n")] = 0;      /* one bus text line per value */
}

static uint64_t be_n(const uint8_t* p, size_t n) {
    uint64_t v = 0;
    while (n--) v = (v << 8) | *p++;
    return v;
}

static void parse_entry(const uint8_t* p, size_t len, tel_entry_t* e) {
    iap2_tlv_t t;
    size_t off = 0;
    memset(e, 0, sizeof(*e));
    while (off < len && iap2_parse_tlv(p + off, len - off, &t)) {
        switch (t.id) {
            case 0: if (t.value_len == 2) e->index = (uint16_t)be_n(t.value, 2); break;
            case 1: copy_str(e->a, &t); break;
            case 2: copy_str(e->b, &t); break;
            case 6: if (t.value_len == 1) e->type = t.value[0]; break;
            case 7: if (t.value_len == 8) e->timestamp = be_n(t.value, 8); break;
            case 9: if (t.value_len == 1) e->occurrences = t.value[0]; break;
            default: break;
        }
        off += t.len;
    }
}

static void add_entry(tel_list_t* l, const iap2_tlv_t* t) {
    if (l->n < TEL_LIST_MAX) parse_entry(t->value, t->value_len, &l->e[l->n++]);
}

static void set_count(tel_list_t* l, const iap2_tlv_t* t) {
    if (t->value_len != 2) return;
    l->count_set = true;
    l->count = (uint16_t)be_n(t->value, 2);
}

bool tel_parse_list_update(const uint8_t* payload, size_t len, tel_lists_t* out) {
    iap2_tlv_t t;
    size_t off = 0;
    memset(out, 0, sizeof(*out));
    if (!payload) return false;
    while (off < len && iap2_parse_tlv(payload + off, len - off, &t)) {
        switch (t.id) {
            case 1: add_entry(&out->recents, &t); break;
            case 2: set_count(&out->recents, &t); break;
            case 6: add_entry(&out->favorites, &t); break;
            case 7: set_count(&out->favorites, &t); break;
            default: break;                     /* 0/5 availability: implied by the count */
        }
        off += t.len;
    }
    return off == len;
}

static void merge_list(tel_list_t* last, const tel_list_t* upd) {
    size_t i;
    if (upd->count_set) last->n = upd->count < TEL_LIST_MAX ? upd->count : TEL_LIST_MAX;
    for (i = 0; i < upd->n; i++) {
        size_t at = upd->e[i].index;
        if (at >= TEL_LIST_MAX) continue;
        last->e[at] = upd->e[i];
        if (last->n <= at) last->n = at + 1;
    }
}

void tel_merge_lists(tel_lists_t* last, const tel_lists_t* upd) {
    merge_list(&last->recents, &upd->recents);
    merge_list(&last->favorites, &upd->favorites);
}

bool tel_parse_comm_update(const uint8_t* payload, size_t len, tel_comm_t* out) {
    iap2_tlv_t t;
    size_t off = 0;
    memset(out, 0, sizeof(*out));
    if (!payload) return false;
    while (off < len && iap2_parse_tlv(payload + off, len - off, &t)) {
        switch (t.id) {
            case 0: if (t.value_len == 1) { out->has_signal = true; out->signal = t.value[0]; } break;
            case 1: if (t.value_len == 1) { out->has_registration = true; out->registration = t.value[0]; } break;
            case 2: if (t.value_len == 1) { out->has_airplane = true; out->airplane = t.value[0] != 0; } break;
            case 4: out->has_carrier = true; copy_str(out->carrier, &t); break;
            default: break;
        }
        off += t.len;
    }
    return off == len;
}

void tel_merge_comm(tel_comm_t* last, const tel_comm_t* upd) {
    if (upd->has_signal) { last->has_signal = true; last->signal = upd->signal; }
    if (upd->has_registration) { last->has_registration = true; last->registration = upd->registration; }
    if (upd->has_airplane) { last->has_airplane = true; last->airplane = upd->airplane; }
    if (upd->has_carrier) { last->has_carrier = true; memcpy(last->carrier, upd->carrier, sizeof(last->carrier)); }
}

bool tel_parse_power_update(const uint8_t* payload, size_t len, tel_power_t* out) {
    iap2_tlv_t t;
    size_t off = 0;
    memset(out, 0, sizeof(*out));
    if (!payload) return false;
    while (off < len && iap2_parse_tlv(payload + off, len - off, &t)) {
        if (t.id == POWER_CHARGE_LEVEL && t.value_len == 2) {
            uint16_t v = (uint16_t)be_n(t.value, 2);
            out->has_level = true;
            out->level = v > 100 ? 100 : v;
        } else if (t.id == POWER_CHARGING_STATE && t.value_len == 1) {
            out->has_state = true;
            out->state = t.value[0];
        }
        off += t.len;
    }
    return off == len;
}

void tel_merge_power(tel_power_t* last, const tel_power_t* upd) {
    if (upd->has_level) { last->has_level = true; last->level = upd->level; }
    if (upd->has_state) { last->has_state = true; last->state = upd->state; }
}

size_t tel_build_power_union(const uint8_t* stock, size_t stock_len, uint8_t* out, size_t out_max) {
    iap2_tlv_t t;
    size_t off = 0;
    bool has_state = false, has_level = false;
    while (stock && off < stock_len && iap2_parse_tlv(stock + off, stock_len - off, &t)) {
        if (t.id == POWER_CHARGING_STATE) has_state = true;
        if (t.id == POWER_CHARGE_LEVEL) has_level = true;
        off += t.len;
    }
    if (off != stock_len || (has_state && has_level) || stock_len > out_max) return 0;
    if (stock_len) memcpy(out, stock, stock_len);
    off = stock_len;
    if (!has_state && !(off = put_tlv(out, out_max, off, POWER_CHARGING_STATE, NULL, 0))) return 0;
    if (!has_level && !(off = put_tlv(out, out_max, off, POWER_CHARGE_LEVEL, NULL, 0))) return 0;
    return off;
}

/* ---------------- bus ---------------- */

static void add_list(bus_text_builder_t* b, const char* p, const tel_entry_t* e, size_t n,
                     bool recents) {
    char key[24];
    size_t i;
    snprintf(key, sizeof(key), "%s.n", p);
    bus_text_int(b, key, (int64_t)n);
    for (i = 0; i < n; i++) {
        snprintf(key, sizeof(key), "%s.%u.a", p, (unsigned)i); bus_text_str(b, key, e[i].a);
        snprintf(key, sizeof(key), "%s.%u.b", p, (unsigned)i); bus_text_str(b, key, e[i].b);
        if (recents) {
            snprintf(key, sizeof(key), "%s.%u.type", p, (unsigned)i); bus_text_int(b, key, e[i].type);
            snprintf(key, sizeof(key), "%s.%u.ts", p, (unsigned)i); bus_text_uint(b, key, e[i].timestamp);
            snprintf(key, sizeof(key), "%s.%u.occ", p, (unsigned)i); bus_text_int(b, key, e[i].occurrences);
        }
    }
}

/* Running lists; the sticky frame always carries both. */
static tel_lists_t tel_last;

static void publish_lists(const tel_lists_t* upd) {
    bus_text_builder_t b;
    pthread_mutex_lock(&tel_lock);
    if (!upd) memset(&tel_last, 0, sizeof(tel_last));
    else tel_merge_lists(&tel_last, upd);
    if (bus_text_begin_heap(&b, "telephony", BUS_TEXT_BUILDER_DEFAULT_CAP * 4) == HOOK_OK) {
        add_list(&b, "r", tel_last.recents.e, tel_last.recents.n, true);
        add_list(&b, "f", tel_last.favorites.e, tel_last.favorites.n, false);
        bus_send_text(EVT_TEL_LISTS, BUS_FLAG_STICKY, &b);
    }
    LOG_INFO(LOG_MODULE, "lists: recents=%u favorites=%u",
             (unsigned)tel_last.recents.n, (unsigned)tel_last.favorites.n);
    pthread_mutex_unlock(&tel_lock);
}

/* Running CommunicationsUpdate and PowerUpdate state; -1 = not reported yet. */
static tel_comm_t tel_comm;
static tel_power_t tel_power;

/* upd == NULL and pwr == NULL resets both (new Identify, disconnect, init). */
static void publish_comm(const tel_comm_t* upd, const tel_power_t* pwr) {
    bus_text_builder_t b;
    pthread_mutex_lock(&tel_lock);
    if (!upd && !pwr) {
        memset(&tel_comm, 0, sizeof(tel_comm));
        memset(&tel_power, 0, sizeof(tel_power));
    }
    if (upd) tel_merge_comm(&tel_comm, upd);
    if (pwr) tel_merge_power(&tel_power, pwr);
    if (bus_text_begin_heap(&b, "telephony_comm", BUS_TEXT_BUILDER_DEFAULT_CAP) == HOOK_OK) {
        bus_text_int(&b, "signal", tel_comm.has_signal ? tel_comm.signal : -1);
        bus_text_int(&b, "registration", tel_comm.has_registration ? tel_comm.registration : -1);
        bus_text_bool(&b, "airplane", tel_comm.airplane);
        bus_text_str(&b, "carrier", tel_comm.carrier);
        bus_text_int(&b, "battery", tel_power.has_level ? tel_power.level : -1);
        bus_text_int(&b, "charging", tel_power.has_state ? tel_power.state : -1);
        bus_send_text(EVT_TEL_COMM, BUS_FLAG_STICKY, &b);
    }
    pthread_mutex_unlock(&tel_lock);
}

static void tel_on_cmd(uint16_t type, uint8_t flags, const uint8_t* p, uint32_t len, void* ctx) {
    uint8_t payload[16];
    size_t n;
    hook_result_t r;
    uint16_t msg;
    (void)type; (void)flags; (void)ctx;
    if (len < 2) return;
    switch (p[0]) {
        case TEL_CMD_MUTE:   msg = MSG_MUTE_STATUS_UPDATE; break;
        case TEL_CMD_HOLD:   msg = MSG_HOLD_STATUS_UPDATE; break;
        /* AcceptCall / EndCall carry Action as param 0 and no CallUUID: iOS then
         * takes the incoming call / the frontmost call (26.1
         * ACCCommunicationsFeaturePlugin acceptCallWithAction / endCallWithAction,
         * "No call UUID specified - falling back to ...").  Unlike CarPlay's HID
         * telephony keys this works in every call state, dialing included, and
         * does not depend on the car's HID descriptor. */
        case TEL_CMD_ACCEPT: msg = MSG_ACCEPT_CALL; break;
        case TEL_CMD_END:    msg = MSG_END_CALL; break;
        case TEL_CMD_SWAP:   msg = MSG_SWAP_CALLS; break;       /* iOS swapCalls */
        default: return;
    }
    /* Every other command is one u8 as param 0 (status or action; both use 0/1). */
    n = msg == MSG_SWAP_CALLS ? 0 : tel_build_status(payload, sizeof(payload), p[1] != 0);
    r = (n || msg == MSG_SWAP_CALLS) ? hook_inject_message(msg, payload, n) : HOOK_ERR_PARAM;
    if (r != HOOK_OK)
        LOG_WARN(LOG_MODULE, "cmd %u/%u not sent rc=%d", (unsigned)p[0], (unsigned)p[1], (int)r);
}

/* ---------------- module ---------------- */

static bool tel_on_message(hook_context_t* ctx, const iap2_frame_t* f) {
    if (ctx->direction == MSG_DIR_OUTGOING && f->msgid == MSG_START_CALL_STATE) {
        uint8_t payload[128];
        size_t n;
        bool send;
        pthread_mutex_lock(&tel_lock);
        send = !tel_lists_requested;
        tel_lists_requested = true;
        pthread_mutex_unlock(&tel_lock);
        if (!send) return false;
        n = tel_build_start_list_updates(payload, sizeof(payload), TEL_LIST_MAX, TEL_LIST_MAX);
        if (!n || hook_inject_message(MSG_START_LIST_UPDATES, payload, n) != HOOK_OK) {
            LOG_WARN(LOG_MODULE, "StartListUpdates not queued");
            pthread_mutex_lock(&tel_lock);
            tel_lists_requested = false;        /* next stock 0x4154 retries */
            pthread_mutex_unlock(&tel_lock);
        }
    } else if (ctx->direction == MSG_DIR_INCOMING && f->msgid == MSG_COMM_UPDATE) {
        tel_comm_t upd;
        if (tel_parse_comm_update(f->payload, f->payload_len, &upd)) publish_comm(&upd, NULL);
        else LOG_WARN(LOG_MODULE, "malformed CommunicationsUpdate len=%u", (unsigned)f->payload_len);
    } else if (ctx->direction == MSG_DIR_OUTGOING && f->msgid == MSG_START_POWER_UPDATES) {
        /* Stock subscribes to PowerUpdate for its own charging logic.  If it does not ask
         * for the battery, its request is repeated once with the battery params added: every
         * stock param stays in, so iOS keeps sending what stock asked for whether a second
         * StartPowerUpdates replaces or extends the first.  This runs in Encode, before stock's
         * message is on the wire, and the injection worker could overtake it; the extended
         * request therefore waits for the phone's first PowerUpdate, which proves stock's own
         * request has arrived. */
        size_t n;
        if (!tel_battery_enabled) return false;
        n = tel_build_power_union(f->payload, f->payload_len, tel_power_union, sizeof(tel_power_union));
        pthread_mutex_lock(&tel_lock);
        tel_power_union_len = tel_power_requested ? 0 : n;
        pthread_mutex_unlock(&tel_lock);
        {
            char hex[3 * 24 + 1];
            size_t i;
            for (i = 0; i < f->payload_len && i < 24; i++) snprintf(hex + 3 * i, 4, "%02X ", f->payload[i]);
            hex[3 * i] = 0;
            LOG_WARN(LOG_MODULE, "stock StartPowerUpdates [%s] -> %s", hex,
                     n ? "battery params added after the first PowerUpdate" : "already asks for the battery");
        }
    } else if (ctx->direction == MSG_DIR_INCOMING && f->msgid == MSG_POWER_UPDATE) {
        tel_power_t pwr;
        uint8_t payload[sizeof(tel_power_union)];
        size_t n;
        if (!tel_battery_enabled) return false;
        pthread_mutex_lock(&tel_lock);
        n = tel_power_union_len;
        if (n) memcpy(payload, tel_power_union, n);
        tel_power_union_len = 0;
        if (n) tel_power_requested = true;
        pthread_mutex_unlock(&tel_lock);
        if (n) {
            if (hook_inject_message(MSG_START_POWER_UPDATES, payload, n) == HOOK_OK) {
                LOG_WARN(LOG_MODULE, "StartPowerUpdates extended with battery level/state (%u bytes)", (unsigned)n);
            } else {
                LOG_WARN(LOG_MODULE, "StartPowerUpdates extension not queued");
                pthread_mutex_lock(&tel_lock);
                tel_power_requested = false;    /* the next stock 0xAE00 retries */
                pthread_mutex_unlock(&tel_lock);
            }
        }
        if (tel_parse_power_update(f->payload, f->payload_len, &pwr)) {
            static int last_level = -1, last_state = -1;    /* Cinemo's receive thread only */
            static unsigned seen;
            if (seen++ < 5) {
                char hex[3 * 24 + 1];
                size_t i;
                for (i = 0; i < f->payload_len && i < 24; i++) snprintf(hex + 3 * i, 4, "%02X ", f->payload[i]);
                hex[3 * i] = 0;
                LOG_WARN(LOG_MODULE, "PowerUpdate [%s]", hex);
            }
            if ((pwr.has_level && pwr.level != last_level) || (pwr.has_state && pwr.state != last_state)) {
                if (pwr.has_level) last_level = pwr.level;
                if (pwr.has_state) last_state = pwr.state;
                LOG_WARN(LOG_MODULE, "iPhone battery %d%% charging=%d (PowerUpdate len=%u)",
                         last_level, last_state, (unsigned)f->payload_len);
            }
            if (pwr.has_level || pwr.has_state) publish_comm(NULL, &pwr);
        } else {
            LOG_WARN(LOG_MODULE, "malformed PowerUpdate len=%u", (unsigned)f->payload_len);
        }
    } else if (ctx->direction == MSG_DIR_INCOMING && f->msgid == MSG_LIST_UPDATE) {
        static tel_lists_t upd;                 /* Cinemo's receive thread only */
        if (tel_parse_list_update(f->payload, f->payload_len, &upd)) publish_lists(&upd);
        else LOG_WARN(LOG_MODULE, "malformed ListUpdate len=%u", (unsigned)f->payload_len);
    }
    return false;                               /* stock keeps every message */
}

static void tel_on_state(hook_context_t* ctx, int event, void* data) {
    (void)ctx; (void)data;
    if (event == HOOK_EVENT_IDENTIFY_START || event == HOOK_EVENT_DISCONNECT) {
        pthread_mutex_lock(&tel_lock);
        tel_lists_requested = false;
        tel_power_requested = false;
        tel_power_union_len = 0;
        pthread_mutex_unlock(&tel_lock);
        publish_lists(NULL);
        publish_comm(NULL, NULL);
    }
}

static void tel_init(void) {
    bus_set_sticky(EVT_TEL_LISTS, true);
    bus_set_sticky(EVT_TEL_COMM, true);
    bus_on(CMD_TEL, tel_on_cmd, NULL);
    tel_battery_enabled = tel_env_on("IAP2_PHONE_BATTERY", true);
    publish_lists(NULL);                        /* known baseline for Java */
    publish_comm(NULL, NULL);
}

static void tel_shutdown(void) {
    bus_off(CMD_TEL);
}

const hook_module_def_t tel_module_def = {
    .name = "telephony",
    .priority = HOOK_PRIORITY_LOW,
    .msg_filter = tel_msg_filter,
    .msg_filter_count = sizeof(tel_msg_filter) / sizeof(tel_msg_filter[0]),
    .on_message = tel_on_message,
    .on_state = tel_on_state,
    .on_init = tel_init,
    .on_shutdown = tel_shutdown,
};
