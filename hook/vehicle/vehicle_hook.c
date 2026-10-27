/*
 * Vehicle data to the iPhone - see vehicle_hook.h.
 *
 * Wire formats:
 *   VehicleInformationComponent 20 / VehicleStatusComponent 21, StartVehicleStatusUpdates
 *   0xA100, VehicleStatusUpdate 0xA101, StopVehicleStatusUpdates 0xA102: MFi R29 tables
 *   97-19..97-21 and 97-161..97-163 (Range u16 km, OutsideTemperature i16 degC, RangeWarning
 *   bool; at most 1 Hz, on change).
 *   PASCD: MFi R32 table 41-4 and the 41.3.3 examples (tests/vehicle_hook_test.c checks the
 *   first one byte for byte); LocationInformation 0xFFFB param 0 = one NUL-terminated
 *   sentence; StartLocationInformation 0xFFFA param 4 = VehicleSpeedData.
 *
 * Every message here is an extra one through the stock SendIAP2 worker; stock's own location,
 * power and identification traffic is never replaced.  Component 21 goes into the Identify only
 * when the Identify also declares its three messages, which is what iOS checks before accepting
 * it, so the phone has no reason to reject it.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "vehicle_hook.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

DEFINE_LOG_MODULE(VEHICLE);

#define MSG_START_VEHICLE_STATUS    0xA100
#define MSG_VEHICLE_STATUS_UPDATE   0xA101
#define MSG_STOP_VEHICLE_STATUS     0xA102

#define IDENT_VEHICLE_INFORMATION   20
#define IDENT_VEHICLE_STATUS        21
#define IDENT_LOCATION_INFORMATION  22
#define LOCATION_VEHICLE_SPEED      20  /* inside component 22 */
#define START_LOCATION_SPEED        4   /* in 0xFFFA */

/* Component identifiers must be unique inside the Identify; stock and route guidance
 * (0x0010) use low values. */
#define VEH_INFO_COMPONENT_ID       0x0F20
#define VEH_STATUS_COMPONENT_ID     0x0F21

#define STATUS_MIN_INTERVAL_MS      1000u
#define PASCD_BATCH_SPAN_MS         800u    /* 5 Hz input -> one sentence per second */
#define PASCD_GAP_MS                2000u   /* older samples are dropped, not sent late */

static const uint16_t veh_msg_filter[] = {
    MSG_START_VEHICLE_STATUS, MSG_STOP_VEHICLE_STATUS,
    IAP2_MSG_START_LOCATION, IAP2_MSG_STOP_LOCATION, IAP2_MSG_LOCATION_INFO,
};

/* ---------------- pure helpers ---------------- */

static size_t put_tlv(uint8_t* out, size_t max, size_t off, uint16_t id, const void* v, size_t n) {
    size_t w;
    if (off >= max) return 0;
    w = iap2_build_tlv(out + off, max - off, id, (const uint8_t*)v, n);
    return w ? off + w : 0;
}

static size_t put_u16(uint8_t* out, size_t max, size_t off, uint16_t id, uint16_t v) {
    uint8_t be[2];
    be[0] = (uint8_t)(v >> 8); be[1] = (uint8_t)v;
    return put_tlv(out, max, off, id, be, 2);
}

static size_t put_str(uint8_t* out, size_t max, size_t off, uint16_t id, const char* s) {
    return put_tlv(out, max, off, id, s, strlen(s) + 1);
}

bool veh_parse_cmd(const uint8_t* p, size_t len, veh_cmd_t* out) {
    memset(out, 0, sizeof(*out));
    if (!p || len < VEH_CMD_LEN || p[0] != 1) return false;
    out->valid = p[1];
    out->speed_cms = (uint16_t)(p[2] | (p[3] << 8));
    out->gear = p[4] <= VEH_GEAR_PARK ? p[4] : VEH_GEAR_UNKNOWN;
    out->range_km = (uint16_t)(p[5] | (p[6] << 8));
    out->temp_dc = (int16_t)(uint16_t)(p[7] | (p[8] << 8));
    out->range_warning = p[9] != 0;
    return true;                                /* p[10..13]: Java ms, logging only */
}

uint8_t veh_parse_start_status(const uint8_t* payload, size_t len) {
    iap2_tlv_t t;
    size_t off = 0;
    uint8_t want = 0;
    while (payload && off < len && iap2_parse_tlv(payload + off, len - off, &t)) {
        if (t.id == 3) want |= VEH_WANT_RANGE;
        else if (t.id == 4) want |= VEH_WANT_TEMP;
        else if (t.id == 6) want |= VEH_WANT_WARNING;
        off += t.len;
    }
    return want;
}

/* 0.1 degC -> whole degC, half away from zero (-0.6 -> -1, -0.4 -> 0). */
static int temp_c(int16_t dc) {
    return dc >= 0 ? (dc + 5) / 10 : -((5 - dc) / 10);
}

size_t veh_build_status_update(uint8_t* out, size_t out_max, uint8_t want, const veh_cmd_t* st) {
    size_t off = 0;
    if ((want & VEH_WANT_RANGE) && (st->valid & VEH_VALID_RANGE))
        if (!(off = put_u16(out, out_max, off, 3, st->range_km))) return 0;
    if ((want & VEH_WANT_TEMP) && (st->valid & VEH_VALID_TEMP)) {
        if (!(off = put_u16(out, out_max, off, 4, (uint16_t)(int16_t)temp_c(st->temp_dc)))) return 0;
    }
    if ((want & VEH_WANT_WARNING) && (st->valid & VEH_VALID_WARNING)) {
        uint8_t b = st->range_warning ? 1 : 0;
        if (!(off = put_tlv(out, out_max, off, 6, &b, 1))) return 0;
    }
    return off;
}

bool veh_location_wants_speed(const uint8_t* payload, size_t len) {
    iap2_tlv_t t;
    size_t off = 0;
    while (payload && off < len && iap2_parse_tlv(payload + off, len - off, &t)) {
        if (t.id == START_LOCATION_SPEED) return true;
        off += t.len;
    }
    return false;
}

size_t veh_build_identify_components(uint8_t* out, size_t out_max, bool add_info, uint8_t engine) {
    static const uint8_t status_params[] = { 3, 4, 6 };   /* Range, OutsideTemperature, RangeWarning */
    uint8_t g[96];
    size_t off = 0, n = 0, i;
    if (add_info) {
        if (!(n = put_u16(g, sizeof(g), 0, 0, VEH_INFO_COMPONENT_ID))) return 0;
        if (!(n = put_str(g, sizeof(g), n, 1, "Vehicle"))) return 0;
        if (!(n = put_tlv(g, sizeof(g), n, 2, &engine, 1))) return 0;
        if (!(n = put_str(g, sizeof(g), n, 6, "Audi"))) return 0;
        if (!(off = put_tlv(out, out_max, off, IDENT_VEHICLE_INFORMATION, g, n))) return 0;
    }
    if (!(n = put_u16(g, sizeof(g), 0, 0, VEH_STATUS_COMPONENT_ID))) return 0;
    if (!(n = put_str(g, sizeof(g), n, 1, "VehicleStatus"))) return 0;
    for (i = 0; i < sizeof(status_params); i++)
        if (!(n = put_tlv(g, sizeof(g), n, status_params[i], NULL, 0))) return 0;
    return put_tlv(out, out_max, off, IDENT_VEHICLE_STATUS, g, n);
}

static char pascd_state(const veh_sample_t* s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) if (s[i].gear == VEH_GEAR_REVERSE) return 'R';
    switch (s[n - 1].gear) {
        case VEH_GEAR_DRIVE:   return 'D';
        case VEH_GEAR_NEUTRAL: return 'N';
        case VEH_GEAR_PARK:    return 'P';
        default:               return 'U';
    }
}

size_t veh_format_pascd(char* out, size_t out_max, const veh_sample_t* s, size_t n) {
    uint64_t ref;
    size_t w, i;
    int r;
    uint8_t ck = 0;
    if (!out || !s || n == 0 || n > VEH_PASCD_MAX_SAMPLES) return 0;
    ref = s[0].t_ms % 86400000u;                /* daily rollover, spec 41-4 field 2 */
    r = snprintf(out, out_max, "$PASCD,%lu.%03u,C,%c,0,%u",
                 (unsigned long)(ref / 1000u), (unsigned)(ref % 1000u), pascd_state(s, n), (unsigned)n);
    if (r < 0 || (size_t)r >= out_max) return 0;
    w = (size_t)r;
    for (i = 0; i < n; i++) {
        uint64_t dt = s[i].t_ms >= s[0].t_ms ? s[i].t_ms - s[0].t_ms : 0;
        unsigned long cs = (unsigned long)((dt + 5u) / 10u);
        r = snprintf(out + w, out_max - w, ",%lu.%02lu,%lu.%03lu", cs / 100u, cs % 100u,
                     (unsigned long)(s[i].speed_mms / 1000u), (unsigned long)(s[i].speed_mms % 1000u));
        if (r < 0 || (size_t)r >= out_max - w) return 0;
        w += (size_t)r;
    }
    for (i = 1; i < w; i++) ck ^= (uint8_t)out[i];
    r = snprintf(out + w, out_max - w, "*%02X", ck);
    if (r < 0 || (size_t)r >= out_max - w) return 0;
    return w + (size_t)r;
}

/* ---------------- module state ---------------- */

static pthread_mutex_t veh_lock = PTHREAD_MUTEX_INITIALIZER;
static bool veh_status_on, veh_speed_on;
static uint8_t veh_engine = VEH_ENGINE_GASOLINE;
static uint8_t veh_want;                    /* VehicleStatus subscription, 0 = none */
static veh_cmd_t veh_now_state, veh_sent_state;
static bool veh_have_state, veh_sent_any;
static uint64_t veh_status_last_ms;
static bool veh_pascd_active, veh_stock_pascd;
static veh_sample_t veh_buf[VEH_PASCD_MAX_SAMPLES];
static size_t veh_buf_n;
/* Our last sentences: Cinemo's encoder may hand our own extra 0xFFFB back to on_message,
 * which must not be mistaken for stock PASCD. */
#define VEH_SENT_RING 4
static char veh_sent_ring[VEH_SENT_RING][200];
static unsigned veh_sent_next;
static unsigned veh_pascd_sent;

static uint64_t veh_clock_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
static uint64_t (*veh_now_ms)(void) = veh_clock_ms;

#ifdef VEHICLE_HOOK_TESTING
void veh_test_set_clock(uint64_t (*fn)(void)) { veh_now_ms = fn ? fn : veh_clock_ms; }
void veh_test_set_config(bool status_on, bool speed_on, uint8_t engine) {
    veh_status_on = status_on; veh_speed_on = speed_on; veh_engine = engine;
}
#endif

static bool veh_env_on(const char* name, bool def) {
    const char* v = getenv(name);
    if (!v) return def;
    return !(!*v || !strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "false") || !strcmp(v, "no"));
}

static uint8_t veh_env_engine(void) {
    const char* v = getenv("IAP2_VEHICLE_ENGINE");
    if (!v || !strcmp(v, "gasoline")) return VEH_ENGINE_GASOLINE;
    if (!strcmp(v, "diesel")) return VEH_ENGINE_DIESEL;
    if (!strcmp(v, "electric")) return VEH_ENGINE_ELECTRIC;
    if (!strcmp(v, "cng")) return VEH_ENGINE_CNG;
    LOG_WARN(LOG_MODULE, "IAP2_VEHICLE_ENGINE='%s' unknown; using gasoline", v);
    return VEH_ENGINE_GASOLINE;
}

static bool status_changed(const veh_cmd_t* a, const veh_cmd_t* b) {
    uint8_t m = VEH_VALID_RANGE | VEH_VALID_TEMP | VEH_VALID_WARNING;
    if ((a->valid & m) != (b->valid & m)) return true;
    if ((a->valid & VEH_VALID_RANGE) && a->range_km != b->range_km) return true;
    if ((a->valid & VEH_VALID_TEMP) && temp_c(a->temp_dc) != temp_c(b->temp_dc)) return true;
    return (a->valid & VEH_VALID_WARNING) && a->range_warning != b->range_warning;
}

/* Under veh_lock: the VehicleStatusUpdate due now, if any. */
static size_t status_due_locked(uint64_t now, uint8_t* out, size_t out_max) {
    size_t n;
    if (!veh_want || !veh_have_state) return 0;
    if (veh_sent_any && !status_changed(&veh_now_state, &veh_sent_state)) return 0;
    if (veh_sent_any && now - veh_status_last_ms < STATUS_MIN_INTERVAL_MS) return 0;
    n = veh_build_status_update(out, out_max, veh_want, &veh_now_state);
    if (!n) return 0;
    veh_sent_state = veh_now_state;
    veh_sent_any = true;
    veh_status_last_ms = now;
    return n;
}

static void send_status(const uint8_t* p, size_t n) {
    char hex[3 * 32 + 1];
    size_t i;
    if (!n) return;
    if (hook_inject_message(MSG_VEHICLE_STATUS_UPDATE, p, n) != HOOK_OK) {
        LOG_WARN(LOG_MODULE, "VehicleStatusUpdate not queued");
        return;
    }
    for (i = 0; i < n && i < 32; i++) snprintf(hex + 3 * i, 4, "%02X ", p[i]);
    hex[3 * (i ? i : 0)] = 0;
    LOG_WARN(LOG_MODULE, "VehicleStatusUpdate sent: %s", hex);
}

/* Under veh_lock: append one speed sample, return the sentence length to send (0 = none). */
static size_t pascd_feed_locked(uint64_t now, const veh_cmd_t* c, char* out, size_t out_max) {
    size_t n;
    if (!veh_speed_on || !veh_pascd_active || veh_stock_pascd || !(c->valid & VEH_VALID_SPEED)) {
        veh_buf_n = 0;
        return 0;
    }
    if (veh_buf_n && (now < veh_buf[veh_buf_n - 1].t_ms || now - veh_buf[0].t_ms > PASCD_GAP_MS))
        veh_buf_n = 0;
    veh_buf[veh_buf_n].t_ms = now;
    veh_buf[veh_buf_n].speed_mms = (uint32_t)c->speed_cms * 10u;
    veh_buf[veh_buf_n].gear = (c->valid & VEH_VALID_GEAR) ? c->gear : VEH_GEAR_UNKNOWN;
    veh_buf_n++;
    if (veh_buf_n < VEH_PASCD_MAX_SAMPLES && now - veh_buf[0].t_ms < PASCD_BATCH_SPAN_MS) return 0;
    n = veh_format_pascd(out, out_max, veh_buf, veh_buf_n);
    veh_buf_n = 0;
    if (n && n < sizeof(veh_sent_ring[0])) {
        memcpy(veh_sent_ring[veh_sent_next % VEH_SENT_RING], out, n + 1);
        veh_sent_next++;
    }
    return n;
}

static void send_pascd(const char* s, size_t n) {
    uint8_t payload[220];
    size_t len;
    if (!n) return;
    len = put_tlv(payload, sizeof(payload), 0, 0, s, n + 1);  /* NUL-terminated, spec 41.1 */
    if (!len || hook_inject_message(IAP2_MSG_LOCATION_INFO, payload, len) != HOOK_OK) {
        LOG_WARN(LOG_MODULE, "PASCD not queued");
        return;
    }
    if (++veh_pascd_sent == 1) LOG_WARN(LOG_MODULE, "first PASCD sent: %s", s);
    else if (veh_pascd_sent % 60 == 0) LOG_WARN(LOG_MODULE, "PASCD #%u: %s", veh_pascd_sent, s);
}

static void veh_on_cmd(uint16_t type, uint8_t flags, const uint8_t* p, uint32_t len, void* ctx) {
    veh_cmd_t c;
    uint8_t status[32];
    char sentence[200];
    size_t sn, pn;
    uint64_t now;
    (void)type; (void)flags; (void)ctx;
    if (!veh_parse_cmd(p, len, &c)) return;
    now = veh_now_ms();
    pthread_mutex_lock(&veh_lock);
    veh_now_state = c;
    veh_have_state = true;
    sn = status_due_locked(now, status, sizeof(status));
    pn = pascd_feed_locked(now, &c, sentence, sizeof(sentence));
    pthread_mutex_unlock(&veh_lock);
    send_status(status, sn);
    send_pascd(sentence, pn);
}

/* ---------------- iAP2 ---------------- */

static bool payload_has_foreign_pascd(const uint8_t* payload, size_t len) {
    iap2_tlv_t t;
    size_t off = 0;
    while (payload && off < len && iap2_parse_tlv(payload + off, len - off, &t)) {
        if (t.id == 0 && t.value_len >= 6 && !memcmp(t.value, "$PASCD", 6)) {
            bool ours = false;
            unsigned i;
            for (i = 0; i < VEH_SENT_RING && !ours; i++) {
                size_t n = strlen(veh_sent_ring[i]);
                ours = n && t.value_len >= n && !memcmp(t.value, veh_sent_ring[i], n);
            }
            if (!ours) return true;
        }
        off += t.len;
    }
    return false;
}

static bool veh_on_message(hook_context_t* ctx, const iap2_frame_t* f) {
    uint8_t status[32];
    size_t sn = 0;
    if (ctx->direction == MSG_DIR_INCOMING) {
        pthread_mutex_lock(&veh_lock);
        switch (f->msgid) {
            case MSG_START_VEHICLE_STATUS:
                veh_want = veh_status_on ? veh_parse_start_status(f->payload, f->payload_len) : 0;
                veh_sent_any = false;
                LOG_WARN(LOG_MODULE, "phone started VehicleStatus updates: range=%d temp=%d warning=%d",
                         !!(veh_want & VEH_WANT_RANGE), !!(veh_want & VEH_WANT_TEMP),
                         !!(veh_want & VEH_WANT_WARNING));
                sn = status_due_locked(veh_now_ms(), status, sizeof(status));
                break;
            case MSG_STOP_VEHICLE_STATUS:
                veh_want = 0;
                LOG_INFO(LOG_MODULE, "phone stopped VehicleStatus updates");
                break;
            case IAP2_MSG_START_LOCATION:
                veh_pascd_active = veh_location_wants_speed(f->payload, f->payload_len);
                veh_buf_n = 0;
                LOG_WARN(LOG_MODULE, "StartLocationInformation: VehicleSpeedData %s%s",
                         veh_pascd_active ? "requested" : "not requested",
                         veh_pascd_active && !veh_speed_on ? " (IAP2_VEHICLE_SPEED off)" : "");
                break;
            case IAP2_MSG_STOP_LOCATION:
                veh_pascd_active = false;
                veh_buf_n = 0;
                break;
            default:
                break;
        }
        pthread_mutex_unlock(&veh_lock);
        send_status(status, sn);
    } else if (f->msgid == IAP2_MSG_LOCATION_INFO) {
        pthread_mutex_lock(&veh_lock);
        if (!veh_stock_pascd && payload_has_foreign_pascd(f->payload, f->payload_len)) {
            veh_stock_pascd = true;
            LOG_WARN(LOG_MODULE, "stock already sends PASCD; the hook's sentences stay off");
        }
        pthread_mutex_unlock(&veh_lock);
    }
    return false;                                   /* stock keeps every message */
}

/* LocationInformationComponent advertises VehicleSpeedData (dio_manager.json nmea bit 0x08). */
static bool identify_has_speed(const uint8_t* buf, size_t len) {
    uint16_t clen = 0;
    iap2_tlv_t t;
    size_t off = iap2_find_tlv_offset(buf + 6, len - 6, IDENT_LOCATION_INFORMATION, &clen);
    if (clen < 4) return false;
    return iap2_find_tlv(buf + 6 + off + 4, (size_t)clen - 4, LOCATION_VEHICLE_SPEED, &t);
}

/* The Identify's message list `list` (6 = sent by us, 7 = received from the phone) holds `id`. */
static bool identify_lists_msg(const uint8_t* buf, size_t len, uint16_t list, uint16_t id) {
    iap2_tlv_t t;
    size_t i;
    if (!iap2_find_tlv(buf + 6, len - 6, list, &t)) return false;
    for (i = 0; i + 1 < t.value_len; i += 2)
        if ((uint16_t)((t.value[i] << 8) | t.value[i + 1]) == id) return true;
    return false;
}

static size_t veh_on_identify(hook_context_t* ctx, uint8_t* buf, size_t len, size_t max_len) {
    uint8_t add[192];
    uint16_t info_len = 0, status_len = 0;
    size_t n, new_len;
    if (!buf || len < 6) return len;
    if (veh_speed_on && !identify_has_speed(buf, len))
        LOG_WARN(LOG_MODULE, "Identify has no VehicleSpeedData: set dio_manager.json \"nmea\" bit 0x08 (installer) for PASCD");
    if (!veh_status_on) return len;
    (void)iap2_find_tlv_offset(buf + 6, len - 6, IDENT_VEHICLE_INFORMATION, &info_len);
    (void)iap2_find_tlv_offset(buf + 6, len - 6, IDENT_VEHICLE_STATUS, &status_len);
    if (status_len) {
        LOG_WARN(LOG_MODULE, "stock Identify already carries a VehicleStatusComponent; left as is");
        return len;
    }
    /* iOS rejects component 21 unless all three messages are declared (CoreAccessories
     * _checkIdentificationInfo_19, iOS 26.1); the installer adds them to dio_manager.json. */
    if (!identify_lists_msg(buf, len, 6, MSG_VEHICLE_STATUS_UPDATE) ||
        !identify_lists_msg(buf, len, 7, MSG_START_VEHICLE_STATUS) ||
        !identify_lists_msg(buf, len, 7, MSG_STOP_VEHICLE_STATUS)) {
        LOG_WARN(LOG_MODULE, "Identify does not declare 0xA100/0xA101/0xA102 (dio_manager.json not patched): no VehicleStatus");
        return len;
    }
    n = veh_build_identify_components(add, sizeof(add), info_len == 0, veh_engine);
    if (!n) return len;
    new_len = len + n;
    if (new_len > max_len) {
        size_t cap = 0;
        uint8_t* grown = hook_identify_grow(ctx, len, new_len, &cap);
        if (!grown) {
            LOG_WARN(LOG_MODULE, "Identify too small for the vehicle components; skipped this cycle");
            return len;
        }
        buf = grown;
    }
    memcpy(buf + len, add, n);
    write_be16(buf + 2, (uint16_t)new_len);
    if (ctx && ctx->_priv) *(unsigned int*)((char*)ctx->_priv + 4) = (unsigned int)new_len;
    LOG_WARN(LOG_MODULE, "Identify: +VehicleStatus%s (%zu -> %zu bytes)",
             info_len ? "" : " +VehicleInformation", len, new_len);
    return new_len;
}

static void veh_on_state(hook_context_t* ctx, int event, void* data) {
    (void)ctx; (void)data;
    if (event != HOOK_EVENT_IDENTIFY_START && event != HOOK_EVENT_DISCONNECT) return;
    pthread_mutex_lock(&veh_lock);
    veh_want = 0;
    veh_sent_any = false;
    veh_pascd_active = false;
    veh_stock_pascd = false;
    veh_buf_n = 0;
    memset(veh_sent_ring, 0, sizeof(veh_sent_ring));
    pthread_mutex_unlock(&veh_lock);
}

static void veh_init(void) {
    veh_status_on = veh_env_on("IAP2_VEHICLE_STATUS", false);
    veh_speed_on = veh_env_on("IAP2_VEHICLE_SPEED", false);
    veh_engine = veh_env_engine();
    bus_on(CMD_VEHICLE, veh_on_cmd, NULL);
    LOG_WARN(LOG_MODULE, "vehicle data: status=%d engine=%u speed=%d",
             veh_status_on, (unsigned)veh_engine, veh_speed_on);
}

static void veh_shutdown(void) {
    bus_off(CMD_VEHICLE);
}

const hook_module_def_t vehicle_module_def = {
    .name = "vehicle",
    .priority = HOOK_PRIORITY_LOW,
    .msg_filter = veh_msg_filter,
    .msg_filter_count = sizeof(veh_msg_filter) / sizeof(veh_msg_filter[0]),
    .on_message = veh_on_message,
    .on_identify = veh_on_identify,
    .on_state = veh_on_state,
    .on_init = veh_init,
    .on_shutdown = veh_shutdown,
};
