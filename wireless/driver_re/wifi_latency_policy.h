#ifndef WIFI_LATENCY_POLICY_H
#define WIFI_LATENCY_POLICY_H

#include <stdint.h>
#include <stdatomic.h>

/* These types are the offline patch ABI. No original driver padding is used.
 * RX calls require external serialization with table create/delete/expiry.
 * TX uses lock-free 64-bit CAS transitions, never holds a lock across preemption.
 */
#define WL_RX_MAX_WINDOW 64u
#define WL_RX_AGE_TICKS 6u

struct wl_rx_stamp { uint32_t packet, sequence, arrived; };
struct wl_rx_age {
    uint32_t generation, alive, clock_valid, last_now, clock_fault;
    uint32_t present[2];
    struct wl_rx_stamp entry[WL_RX_MAX_WINDOW];
};
struct wl_rx_age_result {
    uint32_t valid, due, expired, oldest_packet, pending;
};
enum { WL_OK = 0, WL_STALE = 1, WL_INVALID = 2, WL_CLOCK_FAULT = 3, WL_BUSY = 4 };

int wl_rx_age_create(struct wl_rx_age *, uint32_t generation);
int wl_rx_age_retire(struct wl_rx_age *, uint32_t generation);
int wl_rx_age_observe(struct wl_rx_age *, uint32_t generation, uint32_t now,
                     uint32_t start_sequence, const uint32_t *slots, uint32_t window,
                     uint32_t incoming_packet, uint32_t incoming_sequence,
                     uint32_t incoming_arrived, struct wl_rx_age_result *);

struct wl_tx_ticket { uint32_t generation, serial; };
#define WL_TX_SERIAL_MAX UINT32_C(0x0fffffff)
enum { WL_TX_IDLE = 0, WL_TX_QUEUED = 1, WL_TX_RUNNING = 2 };
enum { WL_TX_NONE = 0, WL_TX_SUBMIT = 1, WL_TX_RUN = 2, WL_TX_DISCARD = 3 };
struct wl_tx_wake {
    _Atomic uint64_t control;
};

void wl_tx_init(struct wl_tx_wake *);
int wl_tx_bind(struct wl_tx_wake *, uint32_t generation);
/* Retire never frees a queued/captured/running ticket. BUSY means keep the
 * sidecar and original interface target alive until the owner drains it.
 */
int wl_tx_retire(struct wl_tx_wake *, uint32_t generation);
int wl_tx_request(struct wl_tx_wake *, uint32_t generation,
                  uint32_t stopped, uint32_t free_slot, struct wl_tx_ticket *);
int wl_tx_begin(struct wl_tx_wake *, struct wl_tx_ticket);
int wl_tx_finish(struct wl_tx_wake *, struct wl_tx_ticket, struct wl_tx_ticket *next);
/* Only call after a definite transport enqueue failure or cancellation ACK.
 * Keep the original driver's one-tick fallback; this is best-effort early wake.
 */
int wl_tx_submit_failed(struct wl_tx_wake *, struct wl_tx_ticket);
int wl_tx_reclaimable(struct wl_tx_wake *);
/* No serial wrap: once the last ticket drains, runtime restores native wake. */
int wl_tx_serial_exhausted(struct wl_tx_wake *);
/* Runtime ABI receipt for the independent ARM harness/build guard. */
void wl_policy_abi(uint32_t out[5]);

#endif
