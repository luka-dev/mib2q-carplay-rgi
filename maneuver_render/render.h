/*
 * OpenGL rendering -- shared between macOS and QNX.
 * Uses GLES2-compatible subset (shaders, no fixed-function).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef CR_RENDER_H
#define CR_RENDER_H

#include <stdint.h>
#include "route_progress.h"
#include "visible_area.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RENDER_MAT_GENERIC_SOLID = 0,
    RENDER_MAT_ROAD_ASPHALT,
    RENDER_MAT_ROUTE_ACTIVE,
    RENDER_MAT_COUNT
} render_material_t;

/* Initialize GL state (shaders, default textures).
 * Call after platform_init(). Returns 0 on success. */
int render_init(int fb_width, int fb_height);

/* Begin frame -- clear screen. */
void render_begin_frame(void);

/* Draw a colored rectangle in NDC coordinates. */
void render_rect(float x, float y, float w, float h,
                 float r, float g, float b, float a);

/* Draw a colored triangle. */
void render_triangle(float x0, float y0, float x1, float y1, float x2, float y2,
                     float r, float g, float b, float a);

/* End frame (no-op, swap is done by platform). */
void render_end_frame(void);

/* Cleanup GL resources. */
void render_shutdown(void);

/* Update viewport (call on resize). */
void render_set_viewport(int fb_width, int fb_height);

/* Thick line segment (rendered as quad). */
void render_thick_line(float x0, float y0, float x1, float y1, float thickness,
                       float r, float g, float b, float a);

/* Filled circle. */
void render_disc(float cx, float cy, float radius, int segments,
                 float r, float g, float b, float a);

/* Arc (thick ring segment). Angles in radians, drawn CCW from start to end. */
void render_arc(float cx, float cy, float radius, float thickness,
                float start_rad, float end_rad, int segments,
                float r, float g, float b, float a);

/* Full circle ring (convenience for render_arc 0..2pi). */
void render_circle(float cx, float cy, float radius, float thickness, int segments,
                   float r, float g, float b, float a);

/* Toggle perspective on/off. enabled=0 -> flat (no perspective). */
void render_set_perspective(int enabled);

/* Debug grid: colored checkerboard + red popup crop outline. */
void render_set_debug_grid(int on);
void render_debug_grid(void);
/* Draw only the exact current visible-area border (no checkerboard). */
void render_debug_visible_area(void);
void render_debug_small_area(void);

/* Set camera pan offset in maneuver space (shifts entire scene). */
void render_set_camera_pan(float x, float y);

/* Set camera rotation around the maneuver plane. 0 keeps the default view. */
void render_set_camera_rotation(float angle_rad);

/* Rotate the directional light basis around the maneuver plane. */
void render_set_light_rotation(float angle_rad);

/* Recompute camera-dependent matrices/uniforms for the current frame. */
void render_sync_camera(void);
/* Animation step of this frame in 30 fps frames (1.0 at 30 fps, 1.5 at 20 fps).
 * Every per-frame animation constant is tuned for 30 fps and multiplies by it. */
void render_set_frame_step(float frames);
float render_frame_step(void);
/* Pure settled projection for scene compilation; does not move the live camera. */
void render_get_layout_matrix(float out[16]);
void render_build_layout_matrix(float out[16],float aspect);
void render_get_camera_pose(float *x,float *y,float *rotation);
/* Animated camera dolly plus source-pixel lens shift, additive to native travel. */
void render_set_content_framing(float x,float y,float dolly);
void render_reset_content_offset(void);
void render_get_content_framing(float *x,float *y,float *dolly);
void render_build_framing_matrix(float out[16],float aspect,float dolly);

/* Select the shading preset used by subsequent 3D geometry draws. */
void render_set_material(render_material_t material);

/* Returns 1 if a transition animation is in progress. */
int render_is_animating(void);

/* Toggle 3D extrusion. raised=1 -> extruded, raised=0 -> flat on ground. */
void render_set_raised(int raised);

/* Set global opacity (0.0=invisible, 1.0=fully opaque). For fade-in. */
void render_set_global_alpha(float alpha);
float render_get_global_alpha(void);

/* Source-pixel viewport (same rectangle the VC crops). Returns 1 if changed. */
int render_set_visible_area(int x, int y, int w, int h);
void render_get_visible_area(cr_rect_t *current, cr_rect_t *target);
/* Screen-space panel pass, source pixels with top-left origin. Independent of
 * camera travel and route opacity; clipped to the supplied rectangle. */
void render_begin_overlay(cr_rect_t clip);
void render_overlay_mesh(const float *xy, int count, float x, float y,
                         float r, float g, float b, float a);
/* Destination RGBA attenuation in the overlay pass, before drawing the row. */
void render_overlay_cutout(cr_rect_t area,float feather_x,float feather_y,float alpha);
void render_end_overlay(void);
/* ================================================================
 * Single-FBO painter's algorithm mask rendering API
 *
 * 2 mask FBOs rendered in flat 2D (no lighting, no perspective):
 *   ROAD  -- white outline first, grey fill on top (painter's algorithm)
 *   ROUTE -- blue active route
 *
 * Compositing applies materials and perspective (no subtraction).
 * ================================================================ */

/* Mask rendering passes */
void render_begin_outline_mask(void);   /* bind ROAD FBO, ortho 2D, flat white */
void render_end_outline_mask(void);

/* Optional backward road fade in local maneuver coordinates. span=0 disables. */
void render_set_mask_entry_fade(float start,float span);
/* Flat-mask alpha rises from 0 to 1 along a unit local direction over span.
 * span=0 disables; usable for any road end without subdividing its mesh. */
void render_set_mask_direction_fade(float x,float y,float dx,float dy,float span);

void render_begin_route_mask(void);     /* bind FBO_ROUTE, ortho 2D, flat blue */

/* Composite masks -> screen with materials and perspective */
void render_composite(void);

/* Reset depth buffer + z-bias (call between independent composite passes). */
void render_reset_depth(void);

/* Mark masks as needing re-render (call on maneuver state change) */
void render_invalidate_masks(void);

/* Apply/remove a 2D rigid transform for mask rendering (second maneuver). */
void render_push_mask_transform(float tx, float ty, float cos_r, float sin_r);
void render_pop_mask_transform(void);

/* Returns 1 if masks need re-rendering */
int render_masks_dirty(void);

/* ================================================================
 * Flag sprite API
 * ================================================================ */

/* Load flag atlas texture. Call after render_init(). Returns 0 on success. */
int render_load_flag_atlas(const char *path, int frame_w, int frame_h, int frame_count);

/* Returns the number of frames in the loaded flag atlas (0 if none loaded). */
int render_get_flag_frame_count(void);

/* Draw flag sprite at maneuver-space position (x, y).
 * size = half-extent in maneuver space.
 * frame = animation frame index (0..frame_count-1). */
void render_sprite_flag(float x, float y, float size, int frame);
void render_sprite_flag_ex(float x, float y, float size, int frame, int flip_x);

/* ================================================================
 * Vertex buffer -- exposed for route_path.c mesh drawing
 * ================================================================ */

void vb_reset(void);
void vb_v(float x, float y, float z, float nx, float ny, float nz);
void vb_route_v(float x, float y, float z, float nx, float ny, float nz, float progress);
/* Brightness is path_weight * spatial_fill + glow, applied only to the route. */
void render_set_route_progress(float fill, float path_weight, float glow);
/* Preclipped receiver triangles: pos(3), signed edge distance/height gap/onset fade(3). */
void render_contact_shadow(const float *verts,int count,float alpha,float thickness);
const cr_route_progress_map_t *render_prepare_route_progress(const cr_route_progress_point_t *points, int count);
void vb_quad(float x0, float y0, float z0,
             float x1, float y1, float z1,
             float x2, float y2, float z2,
             float x3, float y3, float z3,
             float nx, float ny, float nz);
void vb_flush(float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif /* CR_RENDER_H */
