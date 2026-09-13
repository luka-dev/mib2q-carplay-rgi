#ifndef CR_LANE_PANEL_H
#define CR_LANE_PANEL_H
#include "lane_guidance.h"
#include "visible_area.h"
#include "scene/scene.h"
#ifdef __cplusplus
extern "C" {
#endif

enum { CR_LANE_PANEL_DIRECTIONS = 3 };
enum { CR_LANE_PANEL_HEIGHT=32, CR_LANE_PANEL_FADE_X=8, CR_LANE_PANEL_FADE_Y=12 };
typedef struct {
    int source, primary, count; /* direction buckets -4..4; primary=9 is unknown */
    int directions[CR_LANE_PANEL_DIRECTIONS];
    int recommended, overflow;
} cr_lane_cell_t;
typedef struct {
    int count, capacity, first;
    float width, height;
    cr_lane_cell_t cells[CR_LANE_CAPACITY];
} cr_lane_panel_layout_t;

/* Pure layout: original event order (position order when all positions are
 * usable), no maneuver/route input and no invented direction for unknowns. */
void cr_lane_panel_layout(cr_lane_panel_layout_t *out,
                          const cr_lane_guidance_t *lanes, float visible_width);
typedef struct cr_lane_panel cr_lane_panel_t;
cr_lane_panel_t *cr_lane_panel_create(void);
void cr_lane_panel_destroy(cr_lane_panel_t *panel);
void cr_lane_panel_clear(cr_lane_panel_t *panel);
int cr_lane_panel_update(cr_lane_panel_t *panel, const cr_lane_guidance_t *lanes,
                         float visible_width, double now);
int cr_lane_panel_animating(const cr_lane_panel_t *panel, double now);
void cr_lane_panel_draw(const cr_lane_panel_t *panel, cr_rect_t visible, double now);
/* Same small-frame composition for both stages. Always dolly when lanes show,
 * then fit important geometry above the feathered row, including the full tip. */
void cr_lane_panel_framing(cr_lane_panel_t *panel,const cr_scene_t *current,
                           const cr_scene_t *next,float out[3]);

#ifdef __cplusplus
}
#endif
#endif
