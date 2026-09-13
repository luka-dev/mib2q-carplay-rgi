#ifndef CR_PREVIEW_FIXTURES_H
#define CR_PREVIEW_FIXTURES_H
#include "maneuver.h"
#include <string.h>
typedef struct {
    const char *name;
    int icon;
    float angle;
    int direction, driving_side, junction_count;
    float junctions[MAX_JUNCTION_ANGLES];
} preview_fixture_t;
static const preview_fixture_t fixtures[] = {
    {"Follow road", ICON_APPROACH, 0, 0, 0, 0, {0}},
    {"Right turn / side streets", ICON_TURN, 90, 0, 0, 2, {-90,0}},
    {"Sharp left 160", ICON_TURN, -160, 0, 0, 0, {0}},
    {"Sharp right 160", ICON_TURN, 160, 0, 0, 0, {0}},
    {"U-turn left", ICON_UTURN, 0, -1, 0, 0, {0}},
    {"U-turn right", ICON_UTURN, 0, 1, 1, 0, {0}},
    {"Full roundabout RHT", ICON_ROUNDABOUT, 135, 0, 0, 4, {-90,0,90,135}},
    {"Full roundabout LHT", ICON_ROUNDABOUT, -135, 0, 1, 4, {-90,0,90,-135}},
    {"Highway exit right", ICON_EXIT, 0, 1, 0, 0, {0}},
    {"Merge left", ICON_MERGE, 0, -1, 0, 0, {0}},
    {"Lane change left", ICON_LANE_CHANGE, 0, -1, 0, 0, {0}},
    {"Arrival", ICON_ARRIVED, 0, 0, 0, 0, {0}},
    {"Roundabout departure RHT", ICON_ROUNDABOUT_EXIT, 0, 0, 0, 0, {0}},
    {"Clear", ICON_NONE, 0, 0, 0, 0, {0}},
    {"Slight right", ICON_TURN, 45, 0, 0, 0, {0}},
    {"Straight", ICON_TURN, 0, 0, 0, 0, {0}},
    {"Slight left", ICON_TURN, -45, 0, 0, 0, {0}},
    {"Left turn", ICON_TURN, -90, 0, 0, 0, {0}},
    {"Right turn", ICON_TURN, 90, 0, 0, 0, {0}},
    {"Sharp left 135", ICON_TURN, -135, 0, 0, 0, {0}},
    {"Sharp right 135", ICON_TURN, 135, 0, 0, 0, {0}},
    {"Follow road / side streets", ICON_APPROACH, 0, 0, 0, 2, {-90,90}},
    {"Merge right", ICON_MERGE, 0, 1, 0, 0, {0}},
    {"Highway exit left", ICON_EXIT, 0, -1, 0, 0, {0}},
    {"Lane change right", ICON_LANE_CHANGE, 0, 1, 0, 0, {0}},
    {"First exit RHT", ICON_ROUNDABOUT, 90, 0, 0, 4, {-90,0,90,180}},
    {"Straight exit RHT", ICON_ROUNDABOUT, 0, 0, 0, 4, {-90,0,90,180}},
    {"Third exit RHT", ICON_ROUNDABOUT, -90, 0, 0, 4, {-90,0,90,180}},
    {"Return RHT", ICON_ROUNDABOUT, 180, 0, 0, 4, {-90,0,90,180}},
    {"Third exit LHT", ICON_ROUNDABOUT, 90, 0, 1, 4, {-90,0,90,180}},
    {"Return LHT", ICON_ROUNDABOUT, -180, 0, 1, 4, {-90,0,90,180}},
    {"Roundabout departure LHT", ICON_ROUNDABOUT_EXIT, 0, 0, 1, 0, {0}},
};
#define CASES ((int)(sizeof(fixtures)/sizeof(fixtures[0])))
static maneuver_state_t sample_maneuver(int c) {
    const preview_fixture_t *f=&fixtures[c];
    maneuver_state_t m; memset(&m,0,sizeof(m));
    m.icon=f->icon; m.exit_angle=f->angle; m.direction=f->direction;
    m.driving_side=f->driving_side; m.bap_geometry=1;
    m.junction_angle_count=f->junction_count;
    memcpy(m.junction_angles,f->junctions,sizeof(m.junction_angles));
    return m;
}
#endif
