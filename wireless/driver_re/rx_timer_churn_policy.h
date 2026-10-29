/* OFFLINE prototype ABI; not a production driver patch or allocator. */
#ifndef RX_TIMER_CHURN_POLICY_H
#define RX_TIMER_CHURN_POLICY_H
#include <stdint.h>
enum rx_timer_churn_state_word {
    RXTC_GENERATION, RXTC_ALIVE, RXTC_VIRTUAL_VALID, RXTC_VIRTUAL_DUE,
    RXTC_ARMED, RXTC_PHYSICAL_DUE, RXTC_ARM_EPOCH, RXTC_STATE_WORDS
};
enum rx_timer_churn_input_word {
    RXTC_EVENT, RXTC_NOW_TICK, RXTC_PENDING, RXTC_PROGRESSED,
    RXTC_INPUT_GENERATION, RXTC_AGE_CAP_VALID, RXTC_OLDEST_AGE_DUE,
    RXTC_CAPTURED_ARM_EPOCH, RXTC_INPUT_WORDS
};
enum rx_timer_churn_event { RXTC_RX, RXTC_EXPIRY, RXTC_DELETE, RXTC_CREATE };
enum rx_timer_churn_action { RXTC_ARM = 1, RXTC_STOP = 2, RXTC_FLUSH = 4, RXTC_IGNORE = 8 };
/* State initially zero. Caller must:
 * - validate bounded/trusted native table snapshots under lifecycle exclusion;
 * - serialize updates and the resulting native timer/flush operation;
 * - keep sidecar alive until every queued/captured immutable callback cookie
 *   {state_ref, generation, arm_epoch} is canceled with acknowledgment or run;
 * - prohibit generation/epoch alias while an older cookie may survive;
 * - execute ARM at state[RXTC_PHYSICAL_DUE], not that many milliseconds;
 * - on missing registry entry/capacity or unsupported clock use stock path.
 * Deadlines are uint32 monotonic native ticks with every comparison <2^31.
 * A valid age cap must belong to the post-RX pending set, never old slots.
 * IGNORE has no state mutation; it must never cancel a newer physical timer.
 */
uint32_t rx_timer_churn_policy(uint32_t state[RXTC_STATE_WORDS],
                               const uint32_t input[RXTC_INPUT_WORDS]);
#endif
