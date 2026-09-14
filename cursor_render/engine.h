#ifndef CURSOR_ENGINE_H
#define CURSOR_ENGINE_H
#include "cursor_plane.h"
#include "../common/prism_handoff.h"
/* Pure native scene state. Heartbeats never count as input activity. */
typedef struct {
    cursor_frame command;
    handoff_t handoff;
    uint64_t started, stepped, activity_at, dismiss_at, motion_at;
    float motion_x, motion_y, target_x, target_y;
    unsigned dismiss_alpha;
    uint32_t activity;
    int initialized, shown, touching, done, dismissing;
} cursor_engine;
void cursor_engine_reset(cursor_engine *e);
void cursor_engine_apply(cursor_engine *e, const cursor_frame *command, uint64_t now);
void cursor_engine_frame(cursor_engine *e, uint64_t now, cursor_frame *frame);
unsigned cursor_engine_click_ttl(const cursor_engine *e, uint64_t now);
#endif
