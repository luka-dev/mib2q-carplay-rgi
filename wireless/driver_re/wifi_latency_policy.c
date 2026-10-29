#include "wifi_latency_policy.h"

static int backwards(uint32_t now, uint32_t before) {
    return (now - before) >= UINT32_C(0x80000000);
}

int wl_rx_age_create(struct wl_rx_age *s, uint32_t generation) {
    if (!s || !generation) return WL_INVALID;
    if (s->alive) return WL_BUSY;
    if (generation <= s->generation) return WL_STALE;
    s->generation = generation;
    s->clock_valid = s->last_now = s->clock_fault = 0;
    s->present[0] = s->present[1] = 0;
    s->alive = 1;
    return WL_OK;
}

int wl_rx_age_retire(struct wl_rx_age *s, uint32_t generation) {
    if (!s || !s->alive || s->generation != generation) return WL_STALE;
    s->alive = 0;
    s->present[0] = s->present[1] = 0;
    return WL_OK;
}

int wl_rx_age_observe(struct wl_rx_age *s, uint32_t generation, uint32_t now,
                     uint32_t start_sequence, const uint32_t *slots, uint32_t window,
                     uint32_t incoming_packet, uint32_t incoming_sequence,
                     uint32_t incoming_arrived, struct wl_rx_age_result *out) {
    uint32_t next[2] = {0, 0}, max_age = 0;
    if (!out) return WL_INVALID;
    out->valid = out->due = out->expired = out->oldest_packet = out->pending = 0;
    if (!s || !slots || !window || window > WL_RX_MAX_WINDOW || start_sequence >= 4096
        || (incoming_packet && incoming_sequence >= 4096)) return WL_INVALID;
    if (!s->alive || s->generation != generation) return WL_STALE;
    if (s->clock_fault || (s->clock_valid && backwards(now, s->last_now))
        || (incoming_packet && backwards(now, incoming_arrived))) {
        s->clock_fault = 1;
        return WL_CLOCK_FAULT;
    }
    s->clock_valid = 1;
    s->last_now = now;
    for (uint32_t i = 0; i < window; ++i) {
        uint32_t packet = slots[i];
        if (!packet) continue;
        uint32_t sequence = (start_sequence + i) & 4095u;
        uint32_t index = sequence & (WL_RX_MAX_WINDOW - 1);
        uint32_t word = index >> 5, bit = UINT32_C(1) << (index & 31);
        struct wl_rx_stamp *entry = &s->entry[index];
        if (!(s->present[word] & bit) || entry->packet != packet || entry->sequence != sequence) {
            entry->packet = packet;
            entry->sequence = sequence;
            entry->arrived = now;
        }
        /* Explicit accepted-input identity defeats allocator-address reuse,
         * including a sequence wrap. Rejected input is absent from slots.
         */
        if (packet == incoming_packet && sequence == incoming_sequence)
            entry->arrived = incoming_arrived;
        uint32_t age = now - entry->arrived;
        if (age >= UINT32_C(0x80000000)) {
            s->clock_fault = 1;
            out->valid = 0;
            return WL_CLOCK_FAULT;
        }
        next[word] |= bit;
        ++out->pending;
        if (!out->valid || age > max_age) {
            out->valid = 1;
            max_age = age;
            out->due = entry->arrived + WL_RX_AGE_TICKS;
            out->oldest_packet = packet;
        }
    }
    s->present[0] = next[0];
    s->present[1] = next[1];
    out->expired = out->valid && max_age >= WL_RX_AGE_TICKS;
    return WL_OK;
}

/* One atomic word: generation32 | again1 | alive1 | phase2 | serial28.
 * A spinlock held by a preempted lower-priority QNX thread would be unsafe.
 * ARMv7-A emits LDREXD/STREXD; reject targets needing a hidden library lock.
 */
_Static_assert(__atomic_always_lock_free(8, 0), "TX policy requires lock-free 64-bit CAS");
#define TX_PHASE_SHIFT 28u
#define TX_PHASE_MASK (UINT64_C(3) << TX_PHASE_SHIFT)
#define TX_ALIVE (UINT64_C(1) << 30)
#define TX_AGAIN (UINT64_C(1) << 31)
#define TX_GEN(v) ((uint32_t)((v) >> 32))
#define TX_SERIAL(v) ((uint32_t)((v) & WL_TX_SERIAL_MAX))
#define TX_PHASE(v) ((uint32_t)(((v) >> TX_PHASE_SHIFT) & 3u))

static int matches(uint64_t control, struct wl_tx_ticket t) {
    return t.generation && t.serial && TX_GEN(control) == t.generation && TX_SERIAL(control) == t.serial;
}
static int exchange(struct wl_tx_wake *s, uint64_t *old, uint64_t next) {
    return atomic_compare_exchange_weak_explicit(&s->control, old, next,
                                                 memory_order_acq_rel, memory_order_acquire);
}
static void clear_ticket(struct wl_tx_ticket *out) {
    out->generation = out->serial = 0;
}
static void set_ticket(struct wl_tx_ticket *out, uint64_t state) {
    out->generation = TX_GEN(state);
    out->serial = TX_SERIAL(state);
}

void wl_tx_init(struct wl_tx_wake *s) {
    atomic_init(&s->control, 0);
}

int wl_tx_bind(struct wl_tx_wake *s, uint32_t generation) {
    if (!s || !generation) return WL_INVALID;
    uint64_t old = atomic_load_explicit(&s->control, memory_order_acquire);
    for (;;) {
        if ((old & TX_ALIVE) || TX_PHASE(old) != WL_TX_IDLE) return WL_BUSY;
        /* Strict monotonic generation; never wrap to a retained old ticket. */
        if (generation <= TX_GEN(old)) return WL_STALE;
        uint64_t next = ((uint64_t)generation << 32) | TX_ALIVE;
        if (exchange(s, &old, next)) return WL_OK;
    }
}

int wl_tx_retire(struct wl_tx_wake *s, uint32_t generation) {
    if (!s) return WL_INVALID;
    uint64_t old = atomic_load_explicit(&s->control, memory_order_acquire);
    for (;;) {
        if (!generation || TX_GEN(old) != generation) return WL_STALE;
        uint64_t next = old & ~(TX_ALIVE | TX_AGAIN);
        if (exchange(s, &old, next)) return TX_PHASE(next) == WL_TX_IDLE ? WL_OK : WL_BUSY;
    }
}

int wl_tx_request(struct wl_tx_wake *s, uint32_t generation,
                  uint32_t stopped, uint32_t free_slot, struct wl_tx_ticket *out) {
    if (!out) return WL_TX_NONE;
    clear_ticket(out);
    if (!s || !stopped || !free_slot) return WL_TX_NONE;
    uint64_t old = atomic_load_explicit(&s->control, memory_order_acquire);
    for (;;) {
        if (!(old & TX_ALIVE) || TX_GEN(old) != generation) return WL_TX_NONE;
        uint32_t phase = TX_PHASE(old);
        if (phase == WL_TX_QUEUED) return WL_TX_NONE;
        if (phase == WL_TX_RUNNING) {
            if (old & TX_AGAIN) return WL_TX_NONE;
            if (exchange(s, &old, old | TX_AGAIN)) return WL_TX_NONE;
        } else {
            if (TX_SERIAL(old) == WL_TX_SERIAL_MAX) return WL_TX_NONE;
            uint64_t next = (old + 1u) | ((uint64_t)WL_TX_QUEUED << TX_PHASE_SHIFT);
            if (exchange(s, &old, next)) { set_ticket(out, next); return WL_TX_SUBMIT; }
        }
    }
}

int wl_tx_begin(struct wl_tx_wake *s, struct wl_tx_ticket ticket) {
    if (!s) return WL_TX_NONE;
    uint64_t old = atomic_load_explicit(&s->control, memory_order_acquire);
    for (;;) {
        if (!matches(old, ticket) || TX_PHASE(old) != WL_TX_QUEUED) return WL_TX_NONE;
        uint64_t next = old & ~(TX_PHASE_MASK | TX_AGAIN);
        int result = WL_TX_DISCARD;
        if (old & TX_ALIVE) {
            next |= (uint64_t)WL_TX_RUNNING << TX_PHASE_SHIFT;
            result = WL_TX_RUN;
        }
        if (exchange(s, &old, next)) return result;
    }
}

int wl_tx_finish(struct wl_tx_wake *s, struct wl_tx_ticket ticket, struct wl_tx_ticket *next_ticket) {
    if (!next_ticket) return WL_TX_NONE;
    clear_ticket(next_ticket);
    if (!s) return WL_TX_NONE;
    uint64_t old = atomic_load_explicit(&s->control, memory_order_acquire);
    for (;;) {
        if (!matches(old, ticket) || TX_PHASE(old) != WL_TX_RUNNING) return WL_TX_NONE;
        uint64_t next = old & ~(TX_PHASE_MASK | TX_AGAIN);
        int result = WL_TX_NONE;
        if ((old & (TX_ALIVE | TX_AGAIN)) == (TX_ALIVE | TX_AGAIN)
            && TX_SERIAL(old) < WL_TX_SERIAL_MAX) {
            ++next;
            next |= (uint64_t)WL_TX_QUEUED << TX_PHASE_SHIFT;
            result = WL_TX_SUBMIT;
        }
        if (exchange(s, &old, next)) {
            if (result == WL_TX_SUBMIT) set_ticket(next_ticket, next);
            return result;
        }
    }
}

int wl_tx_submit_failed(struct wl_tx_wake *s, struct wl_tx_ticket ticket) {
    if (!s) return WL_STALE;
    uint64_t old = atomic_load_explicit(&s->control, memory_order_acquire);
    for (;;) {
        if (!matches(old, ticket) || TX_PHASE(old) != WL_TX_QUEUED) return WL_STALE;
        if (exchange(s, &old, old & ~(TX_PHASE_MASK | TX_AGAIN))) return WL_OK;
    }
}

int wl_tx_reclaimable(struct wl_tx_wake *s) {
    if (!s) return 0;
    uint64_t state = atomic_load_explicit(&s->control, memory_order_acquire);
    return !(state & TX_ALIVE) && TX_PHASE(state) == WL_TX_IDLE;
}

int wl_tx_serial_exhausted(struct wl_tx_wake *s) {
    if (!s) return 0;
    uint64_t state = atomic_load_explicit(&s->control, memory_order_acquire);
    return TX_PHASE(state) == WL_TX_IDLE && TX_SERIAL(state) == WL_TX_SERIAL_MAX;
}

void wl_policy_abi(uint32_t out[5]) {
    out[0] = sizeof(struct wl_rx_age);
    out[1] = sizeof(struct wl_rx_age_result);
    out[2] = sizeof(struct wl_tx_wake);
    out[3] = sizeof(struct wl_tx_ticket);
    out[4] = WL_RX_AGE_TICKS;
}
