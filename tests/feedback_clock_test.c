/* Regression coverage for the detached QSA feedback clock. */
#include "audio/feedback_clock.h"

#include <assert.h>
#include <stdio.h>

#define PERIOD_FRAMES UINT64_C(768)
#define PERIOD_NS UINT64_C(16000000)

static void feed_exact_clock(spa_feedback_clock_t *clock, uint64_t *now)
{
    unsigned i;
    int event = 0;

    for (i = 0; event != 1 &&
         i <= SPA_FEEDBACK_CLOCK_WARMUP_WRITES +
              SPA_FEEDBACK_CLOCK_WINDOW_WRITES;
         i++) {
        *now += PERIOD_NS;
        event = spa_feedback_clock_note_write(clock, *now, PERIOD_FRAMES, 0);
    }
    assert(event == 1);
}

static void feed_implausible_clock(spa_feedback_clock_t *clock, uint64_t *now)
{
    unsigned i;
    int event = 0;

    for (i = 0; event != -1 &&
         i <= SPA_FEEDBACK_CLOCK_WINDOW_WRITES;
         i++) {
        *now += UINT64_C(1000000);
        event = spa_feedback_clock_note_write(clock, *now, PERIOD_FRAMES, 0);
    }
    assert(event == -1);
}

int main(void)
{
    spa_feedback_clock_t clock;
    uint64_t now = 0;
    double rate;
    int valid;

    spa_feedback_clock_reset(&clock);
    assert(spa_feedback_clock_get(&clock, &valid) == 0.0 && !valid);

    feed_exact_clock(&clock, &now);
    rate = spa_feedback_clock_get(&clock, &valid);
    assert(valid);
    assert(rate > 47999.999 && rate < 48000.001);

    /* One scheduler-sized hole invalidates the old result and restarts
     * warmup instead of publishing the delayed window as a device clock. */
    now += SPA_FEEDBACK_CLOCK_MAX_GAP_NS + 1;
    assert(spa_feedback_clock_note_write(&clock, now, PERIOD_FRAMES, 0) == -1);
    assert(spa_feedback_clock_get(&clock, &valid) == 0.0 && !valid);

    feed_exact_clock(&clock, &now);
    assert(spa_feedback_clock_get(&clock, &valid) > 47999.999 && valid);

    /* A hidden write==0 -> prepare -> successful retry is also a new clock
     * epoch even though the final plugin write returned the full byte count. */
    now += PERIOD_NS;
    assert(spa_feedback_clock_note_write(&clock, now, PERIOD_FRAMES, 1) == -1);
    assert(spa_feedback_clock_get(&clock, &valid) == 0.0 && !valid);

    /* An implausible completed window must start a full new warmup; it must
     * not immediately measure another window from a bad pacing epoch. */
    while (clock.warmupWrites < SPA_FEEDBACK_CLOCK_WARMUP_WRITES) {
        now += PERIOD_NS;
        (void)spa_feedback_clock_note_write(&clock, now, PERIOD_FRAMES, 0);
    }
    feed_implausible_clock(&clock, &now);
    assert(!clock.valid);
    assert(clock.warmupWrites == 1);
    assert(clock.windowWrites == 0);

    /* An invalid timestamp wipes the epoch rather than preserving a stale
     * measured value. */
    assert(spa_feedback_clock_note_write(&clock, 0, PERIOD_FRAMES, 0) == -1);
    assert(clock.warmupWrites == 0 && clock.lastWriteNs == 0);

    puts("OK  detached feedback clock warmup, measurement, all reset paths");
    return 0;
}
