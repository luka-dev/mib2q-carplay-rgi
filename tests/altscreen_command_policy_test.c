#include "../hook/altscreen/command_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    alt_command_policy_t q;
    alt_command_item_t retired[ALT_COMMAND_CAPACITY], replaced, item = {0};
    int has;
    alt_zoom_dispatch_t dispatch;
    uint64_t now = 1000000;
    alt_command_init(&q);
    assert(!alt_command_generation(&q, 1, 1, retired));
    assert(!alt_zoom_offer(&q, 1, 3, now));
    assert(alt_command_select(&q, now) == ALT_COMMAND_ZOOM);
    alt_zoom_take(&q, now, &dispatch);
    assert(dispatch.direction == 1 && q.zoom_steps == 2);
    assert(alt_command_select(&q, now + 49999) == ALT_COMMAND_NONE);
    assert(alt_command_select(&q, now + 50000) == ALT_COMMAND_ZOOM);
    assert(!alt_zoom_offer(&q, 1, -2, now + 10000));
    assert(!alt_zoom_dispatch_valid(&q, &dispatch, now + 10000));
    assert(q.zoom_steps == -2 && alt_command_select(&q, now + 10000) == ALT_COMMAND_ZOOM);
    alt_zoom_take(&q, now + 10000, &dispatch);
    assert(dispatch.direction == 0);
    assert(!alt_zoom_offer(&q, 1, -127, now + 12000));
    assert(q.zoom_steps == -ALT_ZOOM_PENDING_MAX);
    assert(alt_command_select(&q, now + 300000) == ALT_COMMAND_NONE && !q.zoom_steps);
    assert(!alt_zoom_offer(&q, 1, 1, now + 400000));
    alt_zoom_take(&q, now + 400000, &dispatch);
    assert(!alt_zoom_offer(&q, 1, -1, now + 400001));
    assert(!alt_zoom_dispatch_valid(&q, &dispatch, now + 400001));
    /* Zoom does not consume the 32 retained-command slots. */
    item.gen = 1;
    item.key = ALT_COMMAND_GENERIC;
    for (unsigned i = 0; i < ALT_COMMAND_CAPACITY - ALT_COMMAND_RESERVED; ++i)
        assert(!alt_command_offer(&q, item, &replaced, &has));
    assert(alt_command_offer(&q, item, &replaced, &has) == -2);
    item.key = ALT_COMMAND_VIEW_AREA;
    assert(!alt_command_offer(&q, item, &replaced, &has));
    item.cmd = (void *)2;
    assert(!alt_command_offer(&q, item, &replaced, &has) && has);
    assert(q.count == ALT_COMMAND_CAPACITY - ALT_COMMAND_RESERVED + 1);
    item.key = ALT_COMMAND_SHOW_UI;
    assert(!alt_command_offer(&q, item, &replaced, &has));
    item.key = ALT_COMMAND_IDR;
    assert(!alt_command_offer(&q, item, &replaced, &has));
    int choice = alt_command_select(&q, now + 400001);
    assert(choice >= 0 && q.items[choice].key == ALT_COMMAND_IDR);
    alt_command_take(&q, (unsigned)choice, &item);
    choice = alt_command_select(&q, now + 400001);
    assert(choice >= 0 && q.items[choice].key == ALT_COMMAND_VIEW_AREA);
    /* Advancing/revoking the owner clears old zoom and pending state. */
    unsigned old_count = q.count;
    assert(alt_command_generation(&q, 2, 1, retired) == old_count);
    assert(!q.count && !q.zoom_steps && q.generation == 2);
    assert(alt_zoom_offer(&q, 1, 1, now) < 0);
    assert(!alt_zoom_offer(&q, 2, 1, now));
    alt_command_generation(&q, 2, 0, retired);
    alt_command_generation(&q, 2, 1, retired);
    assert(!q.live && alt_zoom_offer(&q, 2, 1, now) < 0);
    alt_command_generation(&q, 3, 1, retired);
    alt_command_generation(&q, 2, 0, retired);
    assert(q.live && q.generation == 3);
    item.gen = 3;
    item.key = ALT_COMMAND_MAP_APPEARANCE;
    assert(!alt_command_offer(&q, item, &replaced, &has));
    item.key = ALT_COMMAND_UI_APPEARANCE;
    assert(!alt_command_offer(&q, item, &replaced, &has));
    item.key = ALT_COMMAND_MAP_APPEARANCE;
    assert(!alt_command_offer(&q, item, &replaced, &has) && has && q.count == 2);
    alt_command_generation(&q, 4, 1, retired);
    /* Sustained layout/startup work admits normal/zoom after four choices. */
    item.gen = 4;
    item.key = ALT_COMMAND_SHOW_UI;
    for (unsigned i = 0; i < 6; ++i)
        assert(!alt_command_offer(&q, item, &replaced, &has));
    assert(!alt_zoom_offer(&q, 4, 1, now));
    for (unsigned i = 0; i < 4; ++i) {
        choice = alt_command_select(&q, now);
        assert(choice >= 0);
        alt_command_take(&q, (unsigned)choice, &item);
    }
    assert(alt_command_select(&q, now) == ALT_COMMAND_ZOOM);
    /* Last step selected, zero pending: later input cannot revive its age. */
    alt_command_generation(&q, 5, 1, retired);
    assert(!alt_zoom_offer(&q, 5, 1, now));
    alt_zoom_take(&q, now, &dispatch);
    assert(!q.zoom_steps);
    assert(alt_zoom_dispatch_valid(&q, &dispatch, now + q.zoom_tail_us));
    assert(!alt_zoom_dispatch_valid(&q, &dispatch, now + q.zoom_tail_us + 1));
    assert(!alt_zoom_offer(&q, 5, 1, now + q.zoom_tail_us + 1));
    assert(!alt_zoom_dispatch_valid(&q, &dispatch, now + q.zoom_tail_us + 1));
    assert(alt_zoom_ready(&q, now + q.zoom_tail_us + 1) && q.zoom_steps == 1);
    alt_zoom_take(&q, now + q.zoom_tail_us + 1, &dispatch);
    assert(alt_zoom_dispatch_valid(&q, &dispatch, now + q.zoom_tail_us + 1));
    /* Fresh pending intent also cannot extend a selection during a burst. */
    assert(!alt_zoom_offer(&q, 5, 1, now + 2 * q.zoom_tail_us));
    assert(!alt_zoom_dispatch_valid(&q, &dispatch, now + 2 * q.zoom_tail_us + 2));
    puts("altscreen_command_policy_test: immediate/cadenced zoom, reversal, tail/cap, reserved "
         "priority, latest state, generation revocation and fairness PASS");
    return 0;
}
