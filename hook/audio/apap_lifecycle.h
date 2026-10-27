/*
 * apap_lifecycle.h -- buffered AirPlay packet ownership and FLUSH ranges
 *
 * This object deliberately has no lock and creates no thread.  The type-103
 * render lock is the single owner of every call in the hook; keeping the state
 * machine independent makes its ordering and wraparound rules host-testable.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef CARPLAY_APAP_LIFECYCLE_H
#define CARPLAY_APAP_LIFECYCLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct spa_apap_flush_range {
    int have_from_sequence;
    uint32_t from_sequence;
    int have_until_sequence;
    uint32_t until_sequence;
    int have_from_rtp;
    uint32_t from_rtp;
    int have_until_rtp;
    uint32_t until_rtp;
} spa_apap_flush_range_t;

typedef struct spa_apap_queued_packet spa_apap_queued_packet_t;

typedef struct spa_apap_lifecycle {
    spa_apap_queued_packet_t *head;
    spa_apap_queued_packet_t *tail;
    spa_apap_queued_packet_t *inflight;
    int inflight_discard;
    size_t queued_bytes;
    size_t queued_packets;
    size_t byte_limit;
    int playing;
    int flush_pending;
    spa_apap_flush_range_t flush;
    uint64_t current_flush_dropped;
    uint64_t total_flush_dropped;
    uint64_t allocation_failures;
} spa_apap_lifecycle_t;

enum spa_apap_enqueue_result {
    SPA_APAP_ENQUEUE_QUEUED = 1,
    SPA_APAP_ENQUEUE_DROPPED_FLUSH = 0,
    SPA_APAP_ENQUEUE_FULL = -1,
    SPA_APAP_ENQUEUE_NO_MEMORY = -2
};

void spa_apap_lifecycle_init(spa_apap_lifecycle_t *state,
                             size_t byte_limit);
void spa_apap_lifecycle_dispose(spa_apap_lifecycle_t *state);
void spa_apap_lifecycle_set_rate(spa_apap_lifecycle_t *state,
                                 int64_t rate);
int spa_apap_lifecycle_playing(const spa_apap_lifecycle_t *state);
int spa_apap_lifecycle_saturated(const spa_apap_lifecycle_t *state);

int spa_apap_flush_range_contains(const spa_apap_flush_range_t *range,
                                  uint32_t sequence, uint32_t rtp);
int spa_apap_flush_range_after_upper(const spa_apap_flush_range_t *range,
                                     uint32_t sequence, uint32_t rtp);

/* Authenticate the APAP record before calling either of these functions. */
int spa_apap_lifecycle_observe(spa_apap_lifecycle_t *state,
                               uint32_t sequence, uint32_t rtp,
                               int *crossed_upper);
int spa_apap_lifecycle_enqueue(spa_apap_lifecycle_t *state,
                               const uint8_t *bytes, size_t length,
                               uint32_t sequence, uint32_t rtp,
                               int *crossed_upper);

/* One packet may be in flight outside the caller's lock.  A concurrent FLUSH
 * marks it for disposal but never frees memory still used by the decoder. */
const uint8_t *spa_apap_lifecycle_take(spa_apap_lifecycle_t *state,
                                       size_t *length,
                                       uint32_t *sequence,
                                       uint32_t *rtp);
void spa_apap_lifecycle_finish(spa_apap_lifecycle_t *state,
                               int requeue_front);

void spa_apap_lifecycle_begin_flush(spa_apap_lifecycle_t *state,
                                    const spa_apap_flush_range_t *range,
                                    size_t *removed_packets,
                                    size_t *removed_bytes);

#endif
