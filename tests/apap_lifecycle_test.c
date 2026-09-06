/* Host regression for the type-103 stopped queue and exact FLUSH ranges. */
#include "audio/apap_lifecycle.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void enqueue_byte(spa_apap_lifecycle_t *state, uint8_t value,
                         uint32_t sequence, uint32_t rtp)
{
    int crossed = -1;
    assert(spa_apap_lifecycle_enqueue(state, &value, 1, sequence, rtp,
                                      &crossed) == SPA_APAP_ENQUEUE_QUEUED);
    assert(crossed == 0);
}

static uint8_t take_byte(spa_apap_lifecycle_t *state,
                         uint32_t expected_sequence)
{
    const uint8_t *bytes;
    size_t length = 0;
    uint32_t sequence = 0;

    bytes = spa_apap_lifecycle_take(state, &length, &sequence, 0);
    assert(bytes != 0);
    assert(length == 1);
    assert(sequence == expected_sequence);
    return bytes[0];
}

static void assert_stopped_queue_and_order(void)
{
    spa_apap_lifecycle_t state;

    spa_apap_lifecycle_init(&state, 1024);
    assert(!spa_apap_lifecycle_playing(&state));
    enqueue_byte(&state, 1, 10, 1000);
    enqueue_byte(&state, 2, 11, 2000);
    enqueue_byte(&state, 3, 12, 3000);
    assert(spa_apap_lifecycle_take(&state, 0, 0, 0) == 0);

    spa_apap_lifecycle_set_rate(&state, 1);
    assert(take_byte(&state, 10) == 1);
    spa_apap_lifecycle_finish(&state, 0);
    assert(take_byte(&state, 11) == 2);
    /* A rate transition racing a decode returns ownership to the front. */
    spa_apap_lifecycle_set_rate(&state, 0);
    spa_apap_lifecycle_finish(&state, 1);
    assert(spa_apap_lifecycle_take(&state, 0, 0, 0) == 0);
    spa_apap_lifecycle_set_rate(&state, 1);
    assert(take_byte(&state, 11) == 2);
    spa_apap_lifecycle_finish(&state, 0);
    assert(take_byte(&state, 12) == 3);
    spa_apap_lifecycle_finish(&state, 0);
    assert(state.queued_packets == 0);
    spa_apap_lifecycle_dispose(&state);
}

static void assert_prefix_flush(void)
{
    spa_apap_lifecycle_t state;
    spa_apap_flush_range_t range;
    size_t packets = 0, bytes = 0;
    uint8_t value = 9;
    int crossed = 0;

    spa_apap_lifecycle_init(&state, 1024);
    enqueue_byte(&state, 10, 10, 1000);
    enqueue_byte(&state, 11, 11, 1100);
    enqueue_byte(&state, 12, 12, 1200);
    enqueue_byte(&state, 13, 13, 1300);
    memset(&range, 0, sizeof(range));
    range.have_until_sequence = 1;
    range.until_sequence = 12;
    spa_apap_lifecycle_begin_flush(&state, &range, &packets, &bytes);
    assert(packets == 3 && bytes == 3);
    assert(state.queued_packets == 1);
    assert(state.flush_pending);

    assert(spa_apap_lifecycle_enqueue(&state, &value, 1, 12, 1200,
                                      &crossed) ==
           SPA_APAP_ENQUEUE_DROPPED_FLUSH);
    assert(!crossed);
    assert(spa_apap_lifecycle_enqueue(&state, &value, 1, 13, 1300,
                                      &crossed) == SPA_APAP_ENQUEUE_QUEUED);
    assert(crossed);
    assert(!state.flush_pending);
    spa_apap_lifecycle_set_rate(&state, 1);
    assert(take_byte(&state, 13) == 13);
    spa_apap_lifecycle_finish(&state, 0);
    assert(take_byte(&state, 13) == 9);
    spa_apap_lifecycle_finish(&state, 0);
    spa_apap_lifecycle_dispose(&state);
}

static void assert_bounded_and_wrapping_flush(void)
{
    spa_apap_lifecycle_t state;
    spa_apap_flush_range_t range;
    size_t packets;

    spa_apap_lifecycle_init(&state, 1024);
    enqueue_byte(&state, 1, UINT32_C(0x00fffffd), 1);
    enqueue_byte(&state, 2, UINT32_C(0x00fffffe), 2);
    enqueue_byte(&state, 3, UINT32_C(0x00ffffff), 3);
    enqueue_byte(&state, 4, 0, 4);
    enqueue_byte(&state, 5, 1, 5);
    enqueue_byte(&state, 6, 2, 6);
    memset(&range, 0, sizeof(range));
    range.have_from_sequence = 1;
    range.from_sequence = UINT32_C(0x00fffffe);
    range.have_until_sequence = 1;
    range.until_sequence = 1;
    assert(!spa_apap_flush_range_contains(&range,
                                          UINT32_C(0x00fffffd), 0));
    assert(spa_apap_flush_range_contains(&range,
                                         UINT32_C(0x00fffffe), 0));
    assert(spa_apap_flush_range_contains(&range,
                                         UINT32_C(0x00ffffff), 0));
    assert(spa_apap_flush_range_contains(&range, 0, 0));
    assert(spa_apap_flush_range_contains(&range, 1, 0));
    assert(!spa_apap_flush_range_contains(&range, 2, 0));
    spa_apap_lifecycle_begin_flush(&state, &range, &packets, 0);
    assert(packets == 4);
    assert(state.queued_packets == 2);
    spa_apap_lifecycle_set_rate(&state, 1);
    assert(take_byte(&state, UINT32_C(0x00fffffd)) == 1);
    spa_apap_lifecycle_finish(&state, 0);
    assert(take_byte(&state, 2) == 6);
    spa_apap_lifecycle_finish(&state, 0);
    spa_apap_lifecycle_dispose(&state);
}

static void assert_rtp_range_and_inflight_flush(void)
{
    spa_apap_lifecycle_t state;
    spa_apap_flush_range_t range;
    size_t packets;

    spa_apap_lifecycle_init(&state, 1024);
    enqueue_byte(&state, 1, 10, 100);
    enqueue_byte(&state, 2, 11, 200);
    enqueue_byte(&state, 3, 12, 300);
    memset(&range, 0, sizeof(range));
    range.have_from_rtp = 1;
    range.from_rtp = 200;
    range.have_until_rtp = 1;
    range.until_rtp = 200;

    spa_apap_lifecycle_set_rate(&state, 1);
    assert(take_byte(&state, 10) == 1);
    spa_apap_lifecycle_finish(&state, 0);
    assert(take_byte(&state, 11) == 2);
    spa_apap_lifecycle_begin_flush(&state, &range, &packets, 0);
    assert(packets == 1); /* the in-flight object is marked, never freed here */
    spa_apap_lifecycle_finish(&state, 1);
    assert(state.queued_packets == 1);
    assert(take_byte(&state, 12) == 3);
    spa_apap_lifecycle_finish(&state, 0);
    spa_apap_lifecycle_dispose(&state);
}

static void assert_bounded_then_prefix_matches_vehicle_shape(void)
{
    spa_apap_lifecycle_t state;
    spa_apap_flush_range_t range;
    size_t packets;

    spa_apap_lifecycle_init(&state, 1024);
    enqueue_byte(&state, 9, 9, 900);
    enqueue_byte(&state, 10, 10, 1000);
    enqueue_byte(&state, 11, 11, 1100);
    enqueue_byte(&state, 12, 12, 1200);
    enqueue_byte(&state, 13, 13, 1300);

    memset(&range, 0, sizeof(range));
    range.have_from_sequence = 1;
    range.from_sequence = 10;
    range.have_until_sequence = 1;
    range.until_sequence = 12;
    range.have_from_rtp = 1;
    range.from_rtp = 1000;
    range.have_until_rtp = 1;
    range.until_rtp = 1200;
    spa_apap_lifecycle_begin_flush(&state, &range, &packets, 0);
    assert(packets == 3);
    assert(state.queued_packets == 2); /* 9 and post-range 13 survive */

    /* iOS vehicle traces follow 6--16 ms later with an until-only flush at
     * the same upper boundary.  Its final state must be only post-12. */
    memset(&range, 0, sizeof(range));
    range.have_until_sequence = 1;
    range.until_sequence = 12;
    range.have_until_rtp = 1;
    range.until_rtp = 1200;
    spa_apap_lifecycle_begin_flush(&state, &range, &packets, 0);
    assert(packets == 1);
    assert(state.queued_packets == 1);
    spa_apap_lifecycle_set_rate(&state, 1);
    assert(take_byte(&state, 13) == 13);
    spa_apap_lifecycle_finish(&state, 0);
    spa_apap_lifecycle_dispose(&state);
}

static void assert_advertised_limit_backpressure(void)
{
    spa_apap_lifecycle_t state;
    uint8_t four[4] = { 1, 2, 3, 4 };

    spa_apap_lifecycle_init(&state, 5);
    assert(spa_apap_lifecycle_enqueue(&state, four, sizeof(four), 1, 1, 0) ==
           SPA_APAP_ENQUEUE_QUEUED);
    assert(!spa_apap_lifecycle_saturated(&state));
    assert(spa_apap_lifecycle_enqueue(&state, four, sizeof(four), 2, 2, 0) ==
           SPA_APAP_ENQUEUE_QUEUED);
    assert(spa_apap_lifecycle_saturated(&state));
    assert(state.queued_bytes == 8); /* limit plus one complete record */
    assert(spa_apap_lifecycle_enqueue(&state, four, sizeof(four), 3, 3, 0) ==
           SPA_APAP_ENQUEUE_FULL);
    spa_apap_lifecycle_dispose(&state);
}

int main(void)
{
    assert_stopped_queue_and_order();
    assert_prefix_flush();
    assert_bounded_and_wrapping_flush();
    assert_rtp_range_and_inflight_flush();
    assert_bounded_then_prefix_matches_vehicle_shape();
    assert_advertised_limit_backpressure();
    puts("OK  stopped queue, ordered resume, exact FLUSH ranges, wrap and backpressure");
    return 0;
}
