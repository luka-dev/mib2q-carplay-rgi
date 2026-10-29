/* Bounded command/zoom scheduling. Pure policy: caller owns refs and locks. */
#ifndef ALTSCREEN_COMMAND_POLICY_H
#define ALTSCREEN_COMMAND_POLICY_H
#include <stdint.h>
#include <string.h>
#define ALT_COMMAND_CAPACITY 32u
#define ALT_COMMAND_RESERVED 8u
#define ALT_ZOOM_PENDING_MAX 8
#define ALT_ZOOM_INTERVAL_US UINT64_C(50000)
#define ALT_ZOOM_TAIL_US UINT64_C(250000)
enum {
    ALT_COMMAND_GENERIC = 0,
    ALT_COMMAND_IDR,
    ALT_COMMAND_SHOW_UI,
    ALT_COMMAND_VIEW_AREA,
    ALT_COMMAND_NIGHT_MODE
};
enum { ALT_COMMAND_NONE = -1, ALT_COMMAND_ZOOM = -2 };
typedef struct {
    uint32_t gen;
    void *cmd, *completion, *ctx;
    unsigned key;
    uint64_t sequence;
} alt_command_item_t;
typedef struct {
    uint32_t generation;
    int live;
    alt_command_item_t items[ALT_COMMAND_CAPACITY];
    unsigned count, urgent_streak, normal_streak;
    uint64_t sequence;
    int zoom_steps, zoom_direction;
    uint64_t zoom_revision, zoom_last_input_us, zoom_due_us;
    uint64_t zoom_interval_us, zoom_tail_us;
} alt_command_policy_t;
typedef struct {
    uint32_t gen;
    int direction;
    uint64_t revision, input_us;
} alt_zoom_dispatch_t;
static inline void alt_command_init(alt_command_policy_t *q) {
    memset(q, 0, sizeof(*q));
    q->zoom_interval_us = ALT_ZOOM_INTERVAL_US;
    q->zoom_tail_us = ALT_ZOOM_TAIL_US;
}
static inline unsigned alt_command_priority(unsigned key) {
    return key == ALT_COMMAND_IDR                                         ? 0u
           : (key == ALT_COMMAND_SHOW_UI || key == ALT_COMMAND_VIEW_AREA) ? 1u
                                                                          : 2u;
}
static inline int alt_command_replaceable(unsigned key) {
    return key == ALT_COMMAND_VIEW_AREA || key == ALT_COMMAND_NIGHT_MODE;
}
/* Caller supplies room for 32 retired items and completes/releases them outside
 * its mutex. A generation can only advance; revoke never clears a newer owner. */
static inline unsigned alt_command_generation(alt_command_policy_t *q, uint32_t gen, int live,
                                              alt_command_item_t retired[ALT_COMMAND_CAPACITY]) {
    unsigned count = 0;
    if (!gen || gen < q->generation)
        return 0;
    if (gen == q->generation && live)
        return 0; /* cannot revive a revoked generation */
    if (gen > q->generation || !live) {
        count = q->count;
        memcpy(retired, q->items, count * sizeof(*retired));
        memset(q->items, 0, sizeof(q->items));
        q->count = 0;
        q->zoom_steps = 0;
        ++q->zoom_revision;
        q->urgent_streak = q->normal_streak = 0;
        q->zoom_last_input_us = q->zoom_due_us = 0;
        q->zoom_direction = 0;
        q->generation = gen;
        q->live = live ? 1 : 0;
    }
    return count;
}
/* 0 accepted, -1 stale/inactive, -2 capacity. Replacement cancels the older
 * pending completion; already dispatched requests are never replaced. */
static inline int alt_command_offer(alt_command_policy_t *q, alt_command_item_t item,
                                    alt_command_item_t *replaced, int *has_replaced) {
    unsigned i;
    *has_replaced = 0;
    if (!q->live || !item.gen || item.gen != q->generation)
        return -1;
    if (alt_command_replaceable(item.key))
        for (i = 0; i < q->count; ++i) {
            if (q->items[i].key == item.key) {
                *replaced = q->items[i];
                *has_replaced = 1;
                item.sequence = ++q->sequence;
                q->items[i] = item;
                return 0;
            }
        }
    if (q->count >= ALT_COMMAND_CAPACITY ||
        (alt_command_priority(item.key) == 2 &&
         q->count >= ALT_COMMAND_CAPACITY - ALT_COMMAND_RESERVED))
        return -2;
    item.sequence = ++q->sequence;
    q->items[q->count++] = item;
    return 0;
}
static inline void alt_zoom_expire(alt_command_policy_t *q, uint64_t now) {
    if (q->zoom_steps &&
        (!now || now < q->zoom_last_input_us || now - q->zoom_last_input_us > q->zoom_tail_us)) {
        q->zoom_steps = 0;
        ++q->zoom_revision;
    }
}
static inline int alt_zoom_offer(alt_command_policy_t *q, uint32_t gen, int steps, uint64_t now) {
    int same;
    if (!q->live || gen != q->generation || !steps || !now)
        return -1;
    alt_zoom_expire(q, now);
    same = !q->zoom_direction || q->zoom_direction == (steps > 0 ? 1 : -1);
    if (!same) {
        q->zoom_steps = 0;
        ++q->zoom_revision;
        q->zoom_due_us = now;
    }
    if (steps > ALT_ZOOM_PENDING_MAX)
        steps = ALT_ZOOM_PENDING_MAX;
    if (steps < -ALT_ZOOM_PENDING_MAX)
        steps = -ALT_ZOOM_PENDING_MAX;
    q->zoom_steps += steps;
    if (q->zoom_steps > ALT_ZOOM_PENDING_MAX)
        q->zoom_steps = ALT_ZOOM_PENDING_MAX;
    if (q->zoom_steps < -ALT_ZOOM_PENDING_MAX)
        q->zoom_steps = -ALT_ZOOM_PENDING_MAX;
    if (!q->zoom_last_input_us)
        q->zoom_due_us = now;
    q->zoom_last_input_us = now;
    q->zoom_direction = steps > 0 ? 1 : -1;
    return 0;
}
static inline int alt_zoom_ready(const alt_command_policy_t *q, uint64_t now) {
    return q->live && q->zoom_steps && now && now >= q->zoom_due_us &&
           now >= q->zoom_last_input_us && now - q->zoom_last_input_us <= q->zoom_tail_us;
}
static inline int alt_command_select(alt_command_policy_t *q, uint64_t now) {
    int best[3] = {-1, -1, -1};
    unsigned i;
    alt_zoom_expire(q, now);
    for (i = 0; i < q->count; ++i) {
        unsigned rank = alt_command_priority(q->items[i].key);
        if (best[rank] < 0 || q->items[i].sequence < q->items[best[rank]].sequence)
            best[rank] = (int)i;
    }
    if (best[0] >= 0)
        return best[0]; /* recovery cannot be starved by fairness */
    if (best[1] >= 0 && (q->urgent_streak < 4 || (best[2] < 0 && !alt_zoom_ready(q, now))))
        return best[1];
    if (alt_zoom_ready(q, now) && (best[2] < 0 || q->normal_streak >= 2))
        return ALT_COMMAND_ZOOM;
    if (best[2] >= 0)
        return best[2];
    if (best[1] >= 0)
        return best[1];
    return alt_zoom_ready(q, now) ? ALT_COMMAND_ZOOM : ALT_COMMAND_NONE;
}
static inline void alt_command_take(alt_command_policy_t *q, unsigned index,
                                    alt_command_item_t *out) {
    *out = q->items[index];
    if (alt_command_priority(out->key) < 2) {
        if (q->urgent_streak < 4)
            ++q->urgent_streak;
    } else {
        q->urgent_streak = 0;
        if (q->normal_streak < 2)
            ++q->normal_streak;
    }
    --q->count;
    memmove(q->items + index, q->items + index + 1, (q->count - index) * sizeof(*q->items));
    memset(q->items + q->count, 0, sizeof(*q->items));
}
static inline void alt_zoom_take(alt_command_policy_t *q, uint64_t now, alt_zoom_dispatch_t *out) {
    out->gen = q->generation;
    out->direction = q->zoom_steps > 0 ? 1 : 0;
    out->revision = q->zoom_revision;
    /* This selected step keeps its own input age; a later same-direction
     * offer may refresh the pending tail but cannot refresh this stamp. */
    out->input_us = q->zoom_last_input_us;
    q->zoom_steps += q->zoom_steps > 0 ? -1 : 1;
    q->zoom_due_us = now + q->zoom_interval_us;
    q->urgent_streak = q->normal_streak = 0;
}
static inline int alt_zoom_dispatch_valid(const alt_command_policy_t *q,
                                          const alt_zoom_dispatch_t *dispatch, uint64_t now) {
    return q->live && q->generation == dispatch->gen && q->zoom_revision == dispatch->revision &&
           dispatch->input_us && now >= dispatch->input_us &&
           now - dispatch->input_us <= q->zoom_tail_us && now && now >= q->zoom_last_input_us &&
           now - q->zoom_last_input_us <= q->zoom_tail_us;
}
#endif
