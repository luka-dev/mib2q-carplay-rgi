/* Shared control records; explicit BE bytes, fixed storage, no packed structs. */
#ifndef ALT_CONTROL_WIRE_H
#define ALT_CONTROL_WIRE_H
#include "alt_video_wire.h"
#include <stddef.h>
#define ALT_CONTROL_MAX_SIZE ALT_WIRE_STATUS_SIZE
#define ALT_CONTROL_ACK_CAPABILITY 1u
typedef struct {
    uint32_t magic, generation, value;
} alt_control_record_t;
typedef struct {
    uint8_t bytes[ALT_CONTROL_MAX_SIZE];
    unsigned used;
    int failed;
} alt_control_stream_t;
static inline unsigned alt_control_size(uint32_t magic) {
    return magic == ALT_WIRE_HELLO                               ? ALT_WIRE_HELLO_SIZE
           : (magic == ALT_WIRE_ACK || magic == ALT_WIRE_STATUS) ? ALT_WIRE_STATUS_SIZE
                                                                 : 0;
}
static inline int alt_control_valid(const alt_control_record_t *r) {
    if (!r)
        return 0;
    if (r->magic == ALT_WIRE_HELLO)
        return !r->generation &&
               (r->value == ALT_WIRE_VERSION || r->value == ALT_HELLO_STATUS_CAPABILITY);
    return r->generation && ((r->magic == ALT_WIRE_ACK && r->value == ALT_CONTROL_ACK_CAPABILITY) ||
                             (r->magic == ALT_WIRE_STATUS && r->value <= 1u));
}
static inline unsigned alt_control_encode(uint8_t *bytes, size_t capacity,
                                          const alt_control_record_t *r) {
    unsigned size;
    if (!bytes || !alt_control_valid(r) || capacity < (size = alt_control_size(r->magic)))
        return 0;
    alt_wire_put32(bytes, r->magic);
    if (r->magic == ALT_WIRE_HELLO)
        alt_wire_put32(bytes + 4, r->value);
    else {
        alt_wire_put32(bytes + 4, r->generation);
        alt_wire_put32(bytes + 8, r->value);
    }
    return size;
}
static inline int alt_control_decode(const uint8_t *bytes, size_t size, alt_control_record_t *out) {
    alt_control_record_t r = {0};
    if (!bytes || !out || size < 4 || size != alt_control_size(alt_wire_u32(bytes)))
        return -1;
    r.magic = alt_wire_u32(bytes);
    if (r.magic == ALT_WIRE_HELLO)
        r.value = alt_wire_u32(bytes + 4);
    else {
        r.generation = alt_wire_u32(bytes + 4);
        r.value = alt_wire_u32(bytes + 8);
    }
    if (!alt_control_valid(&r))
        return -1;
    *out = r;
    return 0;
}
static inline void alt_control_reset(alt_control_stream_t *s) { memset(s, 0, sizeof(*s)); }
static inline unsigned alt_control_needed(const alt_control_stream_t *s) {
    unsigned total;
    if (!s || s->failed || s->used > ALT_CONTROL_MAX_SIZE)
        return 0;
    total = s->used < 4 ? 4u : alt_control_size(alt_wire_u32(s->bytes));
    return total > s->used ? total - s->used : 0;
}
/* The caller has just read n bytes directly at bytes + used. Commit returns
 * 0=incomplete, 1=one complete record, -1=invalid until explicit reset. */
static inline int alt_control_commit(alt_control_stream_t *s, size_t n, alt_control_record_t *out) {
    unsigned total;
    if (!s || !out)
        return -1;
    if (s->failed || s->used > ALT_CONTROL_MAX_SIZE || n > ALT_CONTROL_MAX_SIZE - s->used) {
        s->failed = 1;
        return -1;
    }
    s->used += (unsigned)n;
    if (s->used < 4)
        return 0;
    total = alt_control_size(alt_wire_u32(s->bytes));
    if (!total || s->used > total ||
        (s->used == total && alt_control_decode(s->bytes, total, out))) {
        s->failed = 1;
        return -1;
    }
    if (s->used < total)
        return 0;
    s->used = 0;
    return 1;
}
/* Feed a chunk through the same parser used by socket I/O. Stops at one record
 * so the caller can process it and feed the unconsumed remainder. */
static inline int alt_control_feed(alt_control_stream_t *s, const uint8_t *bytes, size_t size,
                                   size_t *consumed, alt_control_record_t *out) {
    if (!consumed || !s || !out || (!bytes && size))
        return -1;
    *consumed = 0;
    if (s->failed)
        return -1;
    while (*consumed < size) {
        unsigned needed = alt_control_needed(s);
        size_t take = size - *consumed;
        int result;
        if (!needed) {
            s->failed = 1;
            return -1;
        }
        if (take > needed)
            take = needed;
        memcpy(s->bytes + s->used, bytes + *consumed, take);
        *consumed += take;
        result = alt_control_commit(s, take, out);
        if (result)
            return result;
    }
    return 0;
}
#endif
