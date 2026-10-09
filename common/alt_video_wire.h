/* Local AltScreen transport. Explicit byte encoding; no packed ARM structs. */
#ifndef ALT_VIDEO_WIRE_H
#define ALT_VIDEO_WIRE_H
#include <stdint.h>
#include <string.h>
#define ALT_WIRE_MAGIC UINT32_C(0x41563246)
#define ALT_WIRE_HELLO UINT32_C(0x41563248)
#define ALT_WIRE_STATUS UINT32_C(0x41563253)
#define ALT_WIRE_STATUS_SIZE 12u
#define ALT_HELLO_STATUS_CAPABILITY 3u /* metadata remains v2; old hooks fall back to legacy */
#define ALT_WIRE_ACK UINT32_C(0x41563241)
#define ALT_WIRE_ACK_SIZE 12u
#define ALT_WIRE_INTERNAL_ACK 0x8000u /* reader-only, never an AU flag */
#define ALT_WIRE_VERSION 2u
#define ALT_WIRE_HEADER_SIZE 40u
#define ALT_WIRE_HELLO_SIZE 8u
#define ALT_WIRE_MAX_PAYLOAD (4u * 1024u * 1024u)
#define ALT_WIRE_CONFIG 1u
#define ALT_WIRE_IDR 2u
typedef struct {
    uint32_t generation;
    uint16_t flags;
    uint16_t version;
    uint64_t sequence, sender_pts, received_us;
} alt_frame_meta_t;
typedef struct {
    uint32_t generation;
    uint16_t version;
    uint64_t sequence;
} alt_wire_state_t;
/* Failed records never change stream identity or the sequence baseline. */
static inline int alt_wire_accept(alt_wire_state_t *s, uint32_t len, const alt_frame_meta_t *m) {
    uint16_t version;
    if (!s || !m)
        return -1;
    if (!len)
        return m->version ? -1 : 0;
    if (m->version && m->version != ALT_WIRE_VERSION)
        return -1;
    if (m->flags & ~(ALT_WIRE_CONFIG | ALT_WIRE_IDR))
        return -1;
    version = m->version ? ALT_WIRE_VERSION : 1;
    if (s->version && s->version != version)
        return -1;
    if (m->version) {
        if (!s->version) {
            if (m->flags != ALT_WIRE_CONFIG || m->sequence || !m->generation)
                return -1;
        } else if (m->generation != s->generation || (m->flags & ALT_WIRE_CONFIG) || !m->sequence ||
                   m->sequence <= s->sequence)
            return -1;
        s->generation = m->generation;
        s->sequence = m->sequence;
    }
    s->version = version;
    return 0;
}
static inline uint32_t alt_wire_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static inline uint64_t alt_wire_u64(const uint8_t *p) {
    return ((uint64_t)alt_wire_u32(p) << 32) | alt_wire_u32(p + 4);
}
static inline void alt_wire_put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static inline void alt_wire_put64(uint8_t *p, uint64_t v) {
    alt_wire_put32(p, (uint32_t)(v >> 32));
    alt_wire_put32(p + 4, (uint32_t)v);
}
static inline int alt_wire_pts_delta(uint64_t before, uint64_t after, uint64_t *us) {
    uint64_t d = after - before;
    if (!us || !before || !after || !d || d > (UINT64_C(10) << 32))
        return -1;
    *us = (d >> 32) * UINT64_C(1000000) + ((d & UINT64_C(0xffffffff)) * UINT64_C(1000000) >> 32);
    return 0;
}
static inline int alt_wire_decode(const uint8_t *p, uint32_t size, uint32_t *len,
                                  alt_frame_meta_t *m) {
    uint32_t flags;
    if (!p || !len || !m || size != ALT_WIRE_HEADER_SIZE || alt_wire_u32(p) != ALT_WIRE_MAGIC ||
        p[4] != 0 || p[5] != ALT_WIRE_VERSION)
        return -1;
    flags = ((uint32_t)p[6] << 8) | p[7];
    *len = alt_wire_u32(p + 8);
    if (!*len || *len > ALT_WIRE_MAX_PAYLOAD || !alt_wire_u32(p + 12) ||
        (flags & ~(ALT_WIRE_CONFIG | ALT_WIRE_IDR)) ||
        ((flags & ALT_WIRE_CONFIG) && (flags & ALT_WIRE_IDR)) ||
        ((flags & ALT_WIRE_CONFIG) && (*len > 4096 || alt_wire_u64(p + 16) != 0)) ||
        (!(flags & ALT_WIRE_CONFIG) && !alt_wire_u64(p + 16)))
        return -1;
    memset(m, 0, sizeof(*m));
    m->version = ALT_WIRE_VERSION;
    m->flags = (uint16_t)flags;
    m->generation = alt_wire_u32(p + 12);
    m->sequence = alt_wire_u64(p + 16);
    m->sender_pts = alt_wire_u64(p + 24);
    m->received_us = alt_wire_u64(p + 32);
    return 0;
}
static inline void alt_wire_encode(uint8_t *p, uint32_t len, const alt_frame_meta_t *m) {
    memset(p, 0, ALT_WIRE_HEADER_SIZE);
    alt_wire_put32(p, ALT_WIRE_MAGIC);
    p[5] = ALT_WIRE_VERSION;
    p[6] = (uint8_t)(m->flags >> 8);
    p[7] = (uint8_t)m->flags;
    alt_wire_put32(p + 8, len);
    alt_wire_put32(p + 12, m->generation);
    alt_wire_put64(p + 16, m->sequence);
    alt_wire_put64(p + 24, m->sender_pts);
    alt_wire_put64(p + 32, m->received_us);
}
#endif
