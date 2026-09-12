#ifndef CR_MANEUVER_COMMAND_H
#define CR_MANEUVER_COMMAND_H
#include "protocol.h"
#include "maneuver.h"
#include <string.h>

/* A refresh coalesced behind a new maneuver still owes the engine a transition. */
static inline uint8_t cr_merge_maneuver_flags(int have_previous, uint8_t previous, uint8_t latest) {
    if (have_previous && !(previous & MAN_FLAG_REFRESH)) latest &= (uint8_t)~MAN_FLAG_REFRESH;
    return latest;
}

/* Shared by the real TCP reader and the host protocol/geometry regression test. */
static inline void cr_decode_maneuver(const cr_cmd_t *cmd, maneuver_state_t *out) {
    const uint8_t *p = cmd->payload;
    int i, count;
    float scale = (cmd->flags & MAN_FLAG_BAP_GEOMETRY) ? 0.5f : 1.0f;
    memset(out, 0, sizeof(*out));
    out->icon = CR_MAN_ICON(p);
    if (out->icon >= ICON_COUNT) out->icon = ICON_NONE;
    out->direction = CR_MAN_DIRECTION(p);
    out->exit_angle = CR_MAN_EXIT_ANGLE(p) * scale;
    out->driving_side = CR_MAN_DRIVING_SIDE(p) == 1 ? 1 : 0;
    out->bap_geometry = (cmd->flags & MAN_FLAG_BAP_GEOMETRY) != 0
        && !(cmd->flags & MAN_FLAG_SNAP_TO_ROAD);
    count = CR_MAN_JUNC_COUNT(p);
    if (count > MAX_JUNCTION_ANGLES) count = MAX_JUNCTION_ANGLES;
    out->junction_angle_count = count;
    for (i = 0; i < count; i++) out->junction_angles[i] = CR_MAN_JUNC_ANGLE(p, i) * scale;
}
#endif
