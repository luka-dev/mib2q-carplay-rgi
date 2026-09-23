/* Compact passive startup tracing; see state_trace.h. */
#include "state_trace.h"

#if ENABLE_STATE_TRACE

#include "iap2_protocol.h"
#include "logging.h"

#define STATE_TRACE_RX_CAP 65536u
#define STATE_TRACE_ACK_EVENTS_MAX 32u
#define STATE_TRACE_HID_EVENTS_MAX 256u

static volatile uint32_t g_trace_event_sequence;
static volatile uint32_t g_trace_generation;
static pthread_mutex_t g_trace_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_trace_rx[STATE_TRACE_RX_CAP];
static size_t g_trace_rx_len;
static int g_trace_waiting_5000_ack;
static uint8_t g_trace_5000_sequence;
static uint8_t g_trace_5000_session;
static unsigned g_trace_ack_events;
static int g_trace_saw_5200;
static int g_trace_saw_520x;
static volatile uint32_t g_trace_hid_events;

static uint64_t state_trace_monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000ULL +
           (uint64_t)ts.tv_nsec / 1000000ULL;
}

void state_trace_emit(const char *name, const char *fmt, ...)
{
    char details[640];
    va_list ap;
    uint32_t event_sequence;

    details[0] = '\0';
    if (fmt && fmt[0]) {
        va_start(ap, fmt);
        (void)vsnprintf(details, sizeof(details), fmt, ap);
        va_end(ap);
        details[sizeof(details) - 1] = '\0';
    }
    event_sequence = __sync_add_and_fetch(&g_trace_event_sequence, 1);
    log_write(LOG_LEVEL_WARN, "STATE",
              "ST ev=%u mono_ms=%llu pid=%ld gen=%u name=%s%s%s",
              (unsigned)event_sequence,
              (unsigned long long)state_trace_monotonic_ms(),
              (long)getpid(),
              (unsigned)g_trace_generation,
              name ? name : "UNKNOWN",
              details[0] ? " " : "", details);
}

uint32_t state_trace_generation(void)
{
    return g_trace_generation;
}

uint32_t state_trace_begin_generation(void)
{
    uint32_t generation = __sync_add_and_fetch(&g_trace_generation, 1);
    if (generation == 0)
        generation = __sync_add_and_fetch(&g_trace_generation, 1);

    pthread_mutex_lock(&g_trace_lock);
    g_trace_rx_len = 0;
    g_trace_waiting_5000_ack = 0;
    g_trace_5000_sequence = 0;
    g_trace_5000_session = 0;
    g_trace_ack_events = 0;
    g_trace_saw_5200 = 0;
    g_trace_saw_520x = 0;
    g_trace_hid_events = 0;
    pthread_mutex_unlock(&g_trace_lock);
    state_trace_emit("IAP_IDENTIFY_START", "physical_generation=%u",
                     (unsigned)generation);
    return generation;
}

static uint32_t state_trace_hash(const uint8_t *data, size_t len)
{
    uint32_t hash = 2166136261u;
    size_t i;
    for (i = 0; data && i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static int state_trace_summarize_tlvs(const uint8_t *data, size_t len,
                                      uint32_t *id_mask,
                                      uint32_t *nonzero_mask,
                                      size_t *value_bytes)
{
    size_t offset = 0;
    int count = 0;
    if (id_mask) *id_mask = 0;
    if (nonzero_mask) *nonzero_mask = 0;
    if (value_bytes) *value_bytes = 0;
    while (offset + 4u <= len) {
        uint16_t item_len = read_be16(data + offset);
        uint16_t item_id = read_be16(data + offset + 2u);
        size_t item_value_len;
        size_t i;
        int any_nonzero = 0;
        if (item_len < 4u || item_len > len - offset) return -1;
        item_value_len = (size_t)item_len - 4u;
        if (item_id < 32u) {
            if (id_mask) *id_mask |= UINT32_C(1) << item_id;
            for (i = 0; i < item_value_len; i++) {
                if (data[offset + 4u + i] != 0) {
                    any_nonzero = 1;
                    break;
                }
            }
            if (any_nonzero && nonzero_mask)
                *nonzero_mask |= UINT32_C(1) << item_id;
        }
        if (value_bytes) *value_bytes += item_value_len;
        offset += item_len;
        count++;
    }
    return offset == len ? count : -1;
}

static void state_trace_count_now_playing_groups(
    const uint8_t *payload, size_t payload_len,
    int *media_groups, int *media_attributes,
    int *playback_groups, int *playback_attributes,
    int *unknown_groups,
    uint32_t *media_mask, uint32_t *media_nonzero_mask,
    size_t *media_value_bytes,
    uint32_t *playback_mask, uint32_t *playback_nonzero_mask,
    size_t *playback_value_bytes)
{
    size_t offset = 0;
    *media_groups = *playback_groups = *unknown_groups = 0;
    *media_attributes = *playback_attributes = 0;
    *media_mask = *media_nonzero_mask = 0;
    *playback_mask = *playback_nonzero_mask = 0;
    *media_value_bytes = *playback_value_bytes = 0;
    while (payload && offset + 4u <= payload_len) {
        uint16_t item_len = read_be16(payload + offset);
        uint16_t item_id = read_be16(payload + offset + 2u);
        int nested;
        uint32_t nested_mask = 0, nested_nonzero_mask = 0;
        size_t nested_value_bytes = 0;
        if (item_len < 4u || item_len > payload_len - offset) {
            *unknown_groups = -1;
            return;
        }
        nested = state_trace_summarize_tlvs(payload + offset + 4u,
                                            (size_t)item_len - 4u,
                                            &nested_mask,
                                            &nested_nonzero_mask,
                                            &nested_value_bytes);
        if (item_id == 0u) {
            (*media_groups)++;
            if (nested < 0) *media_attributes = -1;
            else if (*media_attributes >= 0) *media_attributes += nested;
            *media_mask |= nested_mask;
            *media_nonzero_mask |= nested_nonzero_mask;
            *media_value_bytes += nested_value_bytes;
        } else if (item_id == 1u) {
            (*playback_groups)++;
            if (nested < 0) *playback_attributes = -1;
            else if (*playback_attributes >= 0) *playback_attributes += nested;
            *playback_mask |= nested_mask;
            *playback_nonzero_mask |= nested_nonzero_mask;
            *playback_value_bytes += nested_value_bytes;
        } else {
            (*unknown_groups)++;
        }
        offset += item_len;
    }
    if (offset != payload_len) *unknown_groups = -1;
}

void state_trace_note_iap_semantic(msg_direction_t direction, uint16_t msgid,
                                   const uint8_t *payload, size_t payload_len)
{
    const char *dir = direction == MSG_DIR_OUTGOING ? "out" : "in";
    int emit_rgi = 0;

    if (msgid == IAP2_MSG_NOW_PLAYING_START ||
        msgid == IAP2_MSG_NOW_PLAYING_UPDATE) {
        int media_groups, media_attributes, playback_groups;
        int playback_attributes, unknown_groups;
        uint32_t media_mask, media_nonzero_mask;
        uint32_t playback_mask, playback_nonzero_mask;
        size_t media_value_bytes, playback_value_bytes;
        state_trace_count_now_playing_groups(
            payload, payload_len, &media_groups, &media_attributes,
            &playback_groups, &playback_attributes, &unknown_groups,
            &media_mask, &media_nonzero_mask, &media_value_bytes,
            &playback_mask, &playback_nonzero_mask, &playback_value_bytes);
        state_trace_emit(msgid == IAP2_MSG_NOW_PLAYING_START ?
                         "IAP_NOW_PLAYING_START" : "IAP_NOW_PLAYING_UPDATE",
                         "dir=%s msg=0x%04x payload_bytes=%u payload_hash=%08x "
                         "media_groups=%d media_attrs=%d playback_groups=%d "
                         "playback_attrs=%d unknown_groups=%d "
                         "media_mask=%08x media_nonzero=%08x media_value_bytes=%u "
                         "playback_mask=%08x playback_nonzero=%08x "
                         "playback_value_bytes=%u",
                         dir, (unsigned)msgid, (unsigned)payload_len,
                         (unsigned)state_trace_hash(payload, payload_len),
                         media_groups, media_attributes, playback_groups,
                         playback_attributes, unknown_groups,
                         (unsigned)media_mask, (unsigned)media_nonzero_mask,
                         (unsigned)media_value_bytes,
                         (unsigned)playback_mask,
                         (unsigned)playback_nonzero_mask,
                         (unsigned)playback_value_bytes);
        if (msgid == IAP2_MSG_NOW_PLAYING_UPDATE) {
            pthread_mutex_lock(&g_trace_lock);
            g_trace_waiting_5000_ack = 0;
            pthread_mutex_unlock(&g_trace_lock);
        }
        return;
    }

    if (msgid < IAP2_MSG_ROUTE_GUIDANCE_START ||
        msgid > IAP2_MSG_ROUTE_GUIDANCE_LANE) return;

    pthread_mutex_lock(&g_trace_lock);
    if (msgid == IAP2_MSG_ROUTE_GUIDANCE_START && !g_trace_saw_5200) {
        g_trace_saw_5200 = 1;
        emit_rgi = 1;
    } else if (msgid != IAP2_MSG_ROUTE_GUIDANCE_START && !g_trace_saw_520x) {
        g_trace_saw_520x = 1;
        emit_rgi = 1;
    }
    pthread_mutex_unlock(&g_trace_lock);
    if (emit_rgi)
        state_trace_emit("IAP_RGI_FIRST", "dir=%s msg=0x%04x payload_bytes=%u",
                         dir, (unsigned)msgid, (unsigned)payload_len);
}

void state_trace_note_iap_transport_tx(uint16_t msgid, int result, int sent,
                                       int header_valid, uint8_t control,
                                       uint8_t sequence, uint8_t acknowledgement,
                                       uint8_t session, uint16_t packet_length)
{
    if (msgid != IAP2_MSG_NOW_PLAYING_START &&
        msgid != IAP2_MSG_ROUTE_GUIDANCE_START) return;

    if (msgid == IAP2_MSG_NOW_PLAYING_START && result == 0 && header_valid) {
        pthread_mutex_lock(&g_trace_lock);
        g_trace_waiting_5000_ack = 1;
        g_trace_5000_sequence = sequence;
        g_trace_5000_session = session;
        g_trace_ack_events = 0;
        pthread_mutex_unlock(&g_trace_lock);
    }
    state_trace_emit("IAP_LINK_TX",
                     "msg=0x%04x result=%d sent=%d header=%d ctrl=0x%02x "
                     "seq=%u ack=%u session=%u packet_len=%u",
                     (unsigned)msgid, result, sent, header_valid,
                     (unsigned)control, (unsigned)sequence,
                     (unsigned)acknowledgement, (unsigned)session,
                     (unsigned)packet_length);
}

static void state_trace_process_rx_packet_locked(const uint8_t *packet,
                                                  size_t packet_len)
{
    iap2_link_header_t header;
    iap2_frame_t semantic;
    int semantic_msg = -1;

    if (!iap2_parse_link_header(packet, packet_len, &header)) return;
    if (iap2_find_frame(packet, packet_len, &semantic))
        semantic_msg = semantic.msgid;
    if (g_trace_waiting_5000_ack &&
        g_trace_ack_events < STATE_TRACE_ACK_EVENTS_MAX) {
        int ack_eq_seq = header.ack == g_trace_5000_sequence;
        int ack_eq_next = header.ack == (uint8_t)(g_trace_5000_sequence + 1u);
        g_trace_ack_events++;
        state_trace_emit("IAP_LINK_RX_AFTER_5000",
                         "tx_seq=%u tx_session=%u ctrl=0x%02x seq=%u ack=%u "
                         "session=%u packet_len=%u semantic_msg=%d "
                         "ack_eq_seq=%d ack_eq_next=%d observation=%u",
                         (unsigned)g_trace_5000_sequence,
                         (unsigned)g_trace_5000_session,
                         (unsigned)header.ctrl, (unsigned)header.seq,
                         (unsigned)header.ack, (unsigned)header.session,
                         (unsigned)header.length, semantic_msg,
                         ack_eq_seq, ack_eq_next, g_trace_ack_events);
    }
}

void state_trace_feed_iap_transport_rx(const uint8_t *data, size_t len)
{
    size_t processed = 0;

    if (!data || !len) return;
    pthread_mutex_lock(&g_trace_lock);

    if (len > STATE_TRACE_RX_CAP - g_trace_rx_len) {
        state_trace_emit("IAP_RX_REASSEMBLY_RESET",
                         "buffered=%u incoming=%u cap=%u",
                         (unsigned)g_trace_rx_len, (unsigned)len,
                         (unsigned)STATE_TRACE_RX_CAP);
        g_trace_rx_len = 0;
        if (len > STATE_TRACE_RX_CAP) {
            data += len - STATE_TRACE_RX_CAP;
            len = STATE_TRACE_RX_CAP;
        }
    }
    memcpy(g_trace_rx + g_trace_rx_len, data, len);
    g_trace_rx_len += len;

    while (g_trace_rx_len - processed >= 2u) {
        uint8_t *packet = g_trace_rx + processed;
        uint16_t packet_len;
        size_t available;
        if (packet[0] != IAP2_LINK_SYNC1 || packet[1] != IAP2_LINK_SYNC2) {
            processed++;
            continue;
        }
        available = g_trace_rx_len - processed;
        if (available < 9u) break;
        packet_len = read_be16(packet + 2u);
        if (packet_len < 9u) {
            processed++;
            continue;
        }
        if (available < packet_len) break;
        state_trace_process_rx_packet_locked(packet, packet_len);
        processed += packet_len;
    }

    if (processed) {
        g_trace_rx_len -= processed;
        if (g_trace_rx_len)
            memmove(g_trace_rx, g_trace_rx + processed, g_trace_rx_len);
    }
    pthread_mutex_unlock(&g_trace_lock);
}

void state_trace_reset_iap_transport_rx(void)
{
    pthread_mutex_lock(&g_trace_lock);
    g_trace_rx_len = 0;
    pthread_mutex_unlock(&g_trace_lock);
}

/* Stock MHI2Q dio_manager control seam, recovered from the exact dio_manager image:
 *
 *   CDIOManager_sendButtonHIDReport @ 0x1566c8
 *     HIDRemotePlaybackFillReport(report, usage, state == 0, descriptorBits)
 *     -> queues exactly one byte on success
 *
 *   CHIDService_onJob_sendHIDReport @ 0x176f70
 *     HIDDevicePostReport(device, blob->data@+4, blob->length@+8)
 *
 * The generic post marker also observes touchscreen/knob reports.  It records
 * only length, first byte and a payload hash, and is bounded per physical
 * generation so input activity cannot turn the compact state trace into a
 * packet capture.  Both wrappers exist only in an ENABLE_STATE_TRACE build.
 */
typedef int (*state_trace_hid_remote_fill_t)(uint8_t *, uint32_t, int,
                                              const uint8_t *);
typedef int (*state_trace_hid_post_t)(void *, const void *, size_t);

HOOK_EXPORT int HIDRemotePlaybackFillReport(uint8_t *report, uint32_t usage, int asserted,
                                const uint8_t *descriptor_bits)
{
    static state_trace_hid_remote_fill_t real;
    int result;

    if (!real)
        real = (state_trace_hid_remote_fill_t)
            dlsym(RTLD_NEXT, "HIDRemotePlaybackFillReport");
    if (!real) {
        state_trace_emit("HID_REMOTE_FILL",
                         "usage=0x%02x asserted=%d result=-1 resolved=0",
                         (unsigned)usage, asserted ? 1 : 0);
        return -1;
    }

    result = real(report, usage, asserted, descriptor_bits);
    state_trace_emit("HID_REMOTE_FILL",
                     "usage=0x%02x asserted=%d result=%d resolved=1 "
                     "report_valid=%d report_byte=0x%02x",
                     (unsigned)usage, asserted ? 1 : 0, result,
                     result == 0 && report ? 1 : 0,
                     result == 0 && report ? (unsigned)report[0] : 0u);
    return result;
}

HOOK_EXPORT int HIDDevicePostReport(void *hid_device, const void *report, size_t report_len)
{
    static state_trace_hid_post_t real;
    uint32_t ordinal;
    int result;

    if (!real)
        real = (state_trace_hid_post_t)dlsym(RTLD_NEXT,
                                              "HIDDevicePostReport");
    if (!real) {
        state_trace_emit("HID_REPORT_POST",
                         "ordinal=0 len=%u result=-1 resolved=0",
                         (unsigned)report_len);
        return -1;
    }

    result = real(hid_device, report, report_len);
    ordinal = __sync_add_and_fetch(&g_trace_hid_events, 1);
    if (ordinal <= STATE_TRACE_HID_EVENTS_MAX) {
        const uint8_t *bytes = (const uint8_t *)report;
        state_trace_emit("HID_REPORT_POST",
                         "ordinal=%u len=%u result=%d resolved=1 "
                         "device_valid=%d report_valid=%d first=0x%02x "
                         "report_hash=%08x",
                         (unsigned)ordinal, (unsigned)report_len, result,
                         hid_device ? 1 : 0, report ? 1 : 0,
                         bytes && report_len ? (unsigned)bytes[0] : 0u,
                         (unsigned)state_trace_hash(bytes, report_len));
    } else if (ordinal == STATE_TRACE_HID_EVENTS_MAX + 1u) {
        state_trace_emit("HID_REPORT_POST_LIMIT",
                         "limit=%u", (unsigned)STATE_TRACE_HID_EVENTS_MAX);
    }
    return result;
}

#endif /* ENABLE_STATE_TRACE */
