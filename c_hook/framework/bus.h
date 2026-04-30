/*
 * CarPlay Hook Bus - C side public API
 *
 * Full-duplex TCP bus between this shared library (inside dio_manager)
 * and the Java lsd process.  See bus_protocol.h for wire format.
 *
 * Threading model:
 *   - bus_init()  spawns a listener thread (accepts clients on 19810)
 *                 and a writer thread (drains the send queue).
 *   - bus_send()  is thread-safe: enqueues a frame and wakes the writer.
 *                 Never blocks on network I/O.  Safe from iAP2/OMX hook
 *                 threads.
 *   - bus_on()    registers a dispatcher callback for a given inbound
 *                 type.  Callback is invoked on the reader thread - must
 *                 return quickly or post to another queue.
 *
 * Payload helpers for text (PPS-style key:type:value) builder are
 * provided so route guidance / cover art publishers can be migrated
 * with minimal diff.
 */

#ifndef CARPLAY_BUS_H
#define CARPLAY_BUS_H

#include "common.h"
#include "bus_protocol.h"

/* ============================================================
 * Send policies
 *   Affect only enqueue behaviour when the send queue is full.
 * ============================================================ */
typedef enum {
    BUS_POLICY_RELIABLE  = 0,  /* drop-newest; log warning             */
    BUS_POLICY_LOSSY     = 1,  /* overwrite last frame of same type    */
    BUS_POLICY_DROP_OLD  = 2   /* evict oldest to make room            */
} bus_policy_t;

/* ============================================================
 * Inbound dispatch
 * ============================================================ */
typedef void (*bus_handler_t)(uint16_t type, uint8_t flags,
                              const uint8_t* payload, uint32_t len,
                              void* ctx);

/* ============================================================
 * Lifecycle
 * ============================================================ */
hook_result_t bus_init(void);
void bus_shutdown(void);
bool bus_is_connected(void);

/* ============================================================
 * Per-type configuration
 *   Must be called before first bus_send of that type for effect.
 * ============================================================ */
void bus_set_sticky(uint16_t type, bool sticky);
void bus_set_policy(uint16_t type, bus_policy_t policy);

/* ============================================================
 * Inbound handlers
 *
 * bus_on(type, h, ctx) registers a handler for a given inbound type.
 *   The ctx pointer is stored verbatim and passed to h() on dispatch.
 *
 * bus_off(type) is **synchronous**: it blocks until any in-flight
 *   dispatch for this type has finished and no further invocation can
 *   start.  After bus_off() returns, the caller may safely free(ctx).
 *
 * bus_on() replacing an existing handler has the same contract: the
 *   call waits for any active dispatch of the previous handler to
 *   finish before installing the new one.
 * ============================================================ */
hook_result_t bus_on(uint16_t type, bus_handler_t handler, void* ctx);
void bus_off(uint16_t type);

/* ============================================================
 * Send API
 *   bus_send copies payload; caller retains ownership.
 * ============================================================ */
hook_result_t bus_send(uint16_t type, uint8_t flags,
                       const uint8_t* payload, uint32_t len);

/* ============================================================
 * Periodic tick — register a callback fired ~1 Hz from the bus
 * heartbeat thread.  Avoids spawning extra worker threads in
 * other modules just to drive a low-frequency timer (e.g., debounce
 * windows that need to flush even with no incoming traffic).
 *
 * Single registered callback per process (last call wins).  Callback
 * runs on the heartbeat thread; keep it short (microseconds).
 * Pass NULL to disable.
 * ============================================================ */
typedef void (*bus_tick_cb_t)(void);
void bus_set_periodic_tick(bus_tick_cb_t cb);

/* ============================================================
 * Text payload builder (PPS-compatible key:type:value format)
 *
 *   bus_text_builder_t b;
 *   bus_text_begin(&b, "routeguidance");
 *   bus_text_int (&b, "route_state", 1);
 *   bus_text_str (&b, "destination", "Home");
 *   bus_send_text(EVT_RGD_UPDATE, BUS_FLAG_STICKY, &b);
 *
 *   Builder is stack-allocated with a fixed buffer.  For route guidance
 *   (~10 KB worst case) use BUS_TEXT_BUILDER_LARGE.
 * ============================================================ */
#define BUS_TEXT_BUILDER_DEFAULT_CAP  4096
#define BUS_TEXT_BUILDER_LARGE_CAP    (64 * 1024)

typedef struct {
    uint8_t* buf;
    uint32_t cap;
    uint32_t len;
    bool     own_buf;       /* true if builder allocated its own heap buffer */
    bool     overflow;      /* sticky: any append past cap sets this */
} bus_text_builder_t;

/* Begin a new builder with a caller-owned buffer (recommended, stack-friendly). */
void bus_text_begin_with(bus_text_builder_t* b, const char* object_name,
                         uint8_t* buf, uint32_t cap);

/* Begin with an internally-allocated buffer (freed by bus_send_text / bus_text_free). */
hook_result_t bus_text_begin_heap(bus_text_builder_t* b, const char* object_name, uint32_t cap);

/* Release any internally-allocated buffer.  Safe to call on stack builders. */
void bus_text_free(bus_text_builder_t* b);

/* Appenders.  All fail silently if overflow bit set. */
void bus_text_str (bus_text_builder_t* b, const char* key, const char* value);
void bus_text_int (bus_text_builder_t* b, const char* key, int64_t  value);
void bus_text_uint(bus_text_builder_t* b, const char* key, uint64_t value);
void bus_text_bool(bus_text_builder_t* b, const char* key, bool     value);
void bus_text_fmt (bus_text_builder_t* b, const char* key, char type,
                   const char* fmt, ...) __attribute__((format(printf, 4, 5)));
void bus_text_raw (bus_text_builder_t* b, const char* line);

/* Send the assembled text as a frame.  Always frees heap buffer on return. */
hook_result_t bus_send_text(uint16_t type, uint8_t flags, bus_text_builder_t* b);

#endif /* CARPLAY_BUS_H */
