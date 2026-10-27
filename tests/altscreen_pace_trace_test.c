#include "../hook/altscreen/pace_trace.h"
#include <assert.h>
#include <stdio.h>

static uint64_t ntp_us(uint64_t us)
{
    return (us / 1000000 << 32) + ((us % 1000000) << 32) / 1000000;
}

int main(void)
{
    alt_pace_trace_t s;
    unsigned i;
    /* Independent sender cadence and local receive/processing costs. */
    memset(&s, 0, sizeof(s));
    for (i = 0; i < 300; i++)
        alt_pace_record(&s, 1000000 + i * 50000, 49700, 300,
                        ntp_us(1000000 + i * 50000), 1);
    assert(s.frames == 300 && s.forwarded == 300 && s.arrival.count == 299);
    assert(alt_pace_mean(&s.arrival) == 50000);
    assert(alt_pace_mean(&s.pts) >= 49999 && alt_pace_mean(&s.pts) <= 50000);
    assert(alt_pace_mean(&s.read) == 49700 && alt_pace_mean(&s.work) == 300);
    alt_pace_reset_window(&s);
    assert(!s.frames && !s.arrival.count);
    alt_pace_record(&s, 16000000, 49700, 300, ntp_us(16000000), 1);
    assert(s.arrival.count == 1 && alt_pace_mean(&s.arrival) == 50000);

    /* A 30-fps sender can arrive at 20 fps because we spend 50 ms forwarding.
     * The probe must show the two clocks independently, including burst zeros. */
    memset(&s, 0, sizeof(s));
    for (i = 0; i < 300; i++)
        alt_pace_record(&s, 1000000 + i * 50000, 0, 50000,
                        ntp_us(1000000 + i * 33333), i != 12);
    assert(alt_pace_mean(&s.arrival) == 50000 && alt_pace_mean(&s.work) == 50000);
    assert(alt_pace_mean(&s.pts) >= 33332 && alt_pace_mean(&s.pts) <= 33333);
    assert(s.forwarded == 299);
    memset(&s, 0, sizeof(s));
    alt_pace_record(&s, 1000000, 0, 0, 0, 1);
    alt_pace_record(&s, 1000000, 0, 0, 0, 1);
    alt_pace_record(&s, 1100000, 100000, 0, 0, 1);
    assert(s.arrival.count == 2 && s.arrival.zero == 1);
    assert(alt_pace_mean(&s.arrival) == 50000); /* Old metric falsely said 100 ms. */
    assert(!s.pts.count && s.pts_invalid == 3);

    /* Backwards, absent and discontinuous sender PTS cannot be called FPS. */
    alt_pace_record(&s, 1150000, 0, 0, ntp_us(2000000), 1);
    alt_pace_record(&s, 1200000, 0, 0, ntp_us(1000000), 1);
    alt_pace_record(&s, 1250000, 0, 0, ntp_us(30000000), 1);
    assert(!s.pts.count && s.pts_invalid == 6);
    puts("altscreen_pace_trace_test: sender cadence, local stalls, bursts, invalid PTS PASS");
    return 0;
}
