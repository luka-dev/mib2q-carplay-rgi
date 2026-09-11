/* Copyright (c) 2026, original project author. */
#ifndef SHARE_VIEW_SWITCH_H
#define SHARE_VIEW_SWITCH_H
#include <stddef.h>
#include <stdint.h>

#define VIEW_AREA_UNKNOWN SIZE_MAX
typedef struct { uint64_t generation, serial; size_t area; } view_request;
typedef struct {
    uint64_t generation, serial;
    size_t count, desired, applied;
    int active, pending;
    view_request request;
} view_switch;

/* Educational owner-thread state model, inspired by local generation/ACK guards.
 * All calls must be serialized on one owner thread. Post callback tokens there
 * by value; this helper does not keep session pointers or invoke callbacks.
 * Initialize once before use; do not reinitialize while old tokens can arrive.
 */
int view_switch_init(view_switch *state, size_t area_count, size_t initial_area);
/* Begin only after the previous session ended; desired survives reconnects.
 * Call begin only when your own session adapter can accept requests.
 */
int view_switch_begin(view_switch *state);
void view_switch_end(view_switch *state);
int view_switch_select(view_switch *state, size_t area);
/* One request in flight; coalesce selections while pending. Return 1 and copy
 * a token when needed, 0 for no work, -1 for invalid input/counter exhaustion.
 * A returned token is pending immediately, before any external send call.
 */
int view_switch_next(view_switch *state, view_request *out);
/* Return 1 for a current completion, 0 for stale/duplicate/invalid tokens.
 * success is nonzero only for confirmed application. Failed/uncertain requests
 * leave applied UNKNOWN; retry timing and budget are the caller's responsibility.
 */
int view_switch_complete(view_switch *state, view_request request, int success);
#endif
