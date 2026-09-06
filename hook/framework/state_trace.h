/*
 * Compact, generation-scoped CarPlay startup trace.
 *
 * ENABLE_STATE_TRACE is independent of ENABLE_LOGGING: the diagnostic build
 * can compile every ordinary log/capture out while retaining this bounded
 * event stream through logging.c's asynchronous writer.
 */
#ifndef CARPLAY_STATE_TRACE_H
#define CARPLAY_STATE_TRACE_H

#include "common.h"

#ifndef ENABLE_STATE_TRACE
#define ENABLE_STATE_TRACE 0
#endif

#if ENABLE_STATE_TRACE

void state_trace_emit(const char *name, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
uint32_t state_trace_begin_generation(void);
uint32_t state_trace_generation(void);
void state_trace_note_iap_semantic(msg_direction_t direction, uint16_t msgid,
                                   const uint8_t *payload, size_t payload_len);
void state_trace_note_iap_transport_tx(uint16_t msgid, int result, int sent,
                                       int header_valid, uint8_t control,
                                       uint8_t sequence, uint8_t acknowledgement,
                                       uint8_t session, uint16_t packet_length);
void state_trace_feed_iap_transport_rx(const uint8_t *data, size_t len);
void state_trace_reset_iap_transport_rx(void);

#define STATE_TRACE(name, fmt, ...) \
    state_trace_emit((name), (fmt), ##__VA_ARGS__)

#else

static inline uint32_t state_trace_begin_generation(void) { return 0; }
static inline uint32_t state_trace_generation(void) { return 0; }
static inline void state_trace_note_iap_semantic(msg_direction_t direction,
                                                  uint16_t msgid,
                                                  const uint8_t *payload,
                                                  size_t payload_len)
{
    (void)direction; (void)msgid; (void)payload; (void)payload_len;
}
static inline void state_trace_note_iap_transport_tx(
    uint16_t msgid, int result, int sent, int header_valid, uint8_t control,
    uint8_t sequence, uint8_t acknowledgement, uint8_t session,
    uint16_t packet_length)
{
    (void)msgid; (void)result; (void)sent; (void)header_valid;
    (void)control; (void)sequence; (void)acknowledgement; (void)session;
    (void)packet_length;
}
static inline void state_trace_feed_iap_transport_rx(const uint8_t *data,
                                                       size_t len)
{
    (void)data; (void)len;
}
static inline void state_trace_reset_iap_transport_rx(void) {}

#define STATE_TRACE(name, fmt, ...) ((void)0)

#endif

#endif /* CARPLAY_STATE_TRACE_H */
