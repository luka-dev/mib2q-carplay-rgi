/* Passive iAP2 artwork reassembly. No acknowledgments or transport writes.
 * Caller serializes feed/reset/dispose; zero initialization is sufficient. */
#ifndef CARPLAY_COVERART_STREAM_H
#define CARPLAY_COVERART_STREAM_H
#include <stddef.h>
#include <stdint.h>

#define COVERART_STREAM_MAX_FILE 800000u
#define COVERART_STREAM_MAX_ACTIVE 4u
#define COVERART_STREAM_MAX_PENDING (1024u * 1024u)

typedef struct {
    uint8_t *data;
    size_t len, cap;
    uint64_t expected;
    uint32_t order;
    uint8_t session, kind, started;
} coverart_file_t;

typedef struct {
    uint8_t *data;
    size_t len;
} coverart_packet_t;

/* Metadata-only receive diagnostics; never retain media or control contents. */
typedef struct {
    uint32_t packets, gaps, old_packets, bad_checksum;
    uint32_t setups, rejected_setups, starts, completed;
    uint64_t setup_prefix;
    uint16_t setup_len;
    uint16_t setup_type;
    uint8_t control, seq, session, session_kind, file, op;
} coverart_stream_stats_t;

typedef struct {
    uint8_t *raw;
    size_t raw_len;
    coverart_file_t files[256];
    coverart_packet_t pending[256];
    size_t pending_bytes;
    uint32_t order, last_image_order;
    uint8_t next_seq, have_seq, have_image_order;
    /* 0 unknown, 1 control, 2 file transfer, 3 external accessory. */
    uint8_t session_kind[256];
    int confirmed;
    coverart_stream_stats_t stats;
} coverart_stream_t;

/* Returns the newest complete image in this delivery, malloc-owned by caller.
 * Partial successors remain buffered. Completion is the file opcode, never a
 * byte pattern inside the image. Setup may be size-only (8 bytes) or include
 * a type (10+ bytes). Size 0 in Setup means unknown length. */
void coverart_stream_feed(coverart_stream_t *s, const uint8_t *bytes, size_t len,
                         uint8_t **image, size_t *image_len);
/* Identify resets transfers and sequencing but keeps negotiated session IDs. */
void coverart_stream_reset(coverart_stream_t *s);
void coverart_stream_dispose(coverart_stream_t *s);
#endif
