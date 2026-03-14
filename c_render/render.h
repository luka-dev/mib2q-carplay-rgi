/*
 * OpenGL rendering — shared between macOS and QNX.
 * Uses GLES2-compatible subset (shaders, no fixed-function).
 */

#ifndef CR_RENDER_H
#define CR_RENDER_H

#include <stdint.h>

typedef enum {
    RENDER_MAT_GENERIC_SOLID = 0,
    RENDER_MAT_ROAD_ASPHALT,
    RENDER_MAT_ROAD_BORDER_PAINT,
    RENDER_MAT_ROUTE_ACTIVE,
    RENDER_MAT_COUNT
} render_material_t;

/* Initialize GL state (shaders, default textures).
 * Call after platform_init(). Returns 0 on success. */
int render_init(int fb_width, int fb_height);

/* Begin frame — clear screen. */
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

/* Arrowhead triangle. Base at (bx,by), tip extends in direction angle_rad by size. */
void render_arrowhead(float bx, float by, float angle_rad, float size,
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

/* Toggle perspective on/off. enabled=0 → flat (no perspective). */
void render_set_perspective(int enabled);

/* Set camera pan offset in maneuver space (shifts entire scene). */
void render_set_camera_pan(float x, float y);

/* Set camera rotation around the maneuver plane. 0 keeps the default view. */
void render_set_camera_rotation(float angle_rad);

/* Rotate the directional light basis around the maneuver plane. */
void render_set_light_rotation(float angle_rad);

/* Recompute camera-dependent matrices/uniforms for the current frame. */
void render_sync_camera(void);

/* Select the shading preset used by subsequent 3D geometry draws. */
void render_set_material(render_material_t material);

/* Returns 1 if a transition animation is in progress. */
int render_is_animating(void);

/* Toggle 3D extrusion. raised=1 → extruded, raised=0 → flat on ground. */
void render_set_raised(int raised);

/* ================================================================
 * Multi-layer mask rendering API
 *
 * 3 mask FBOs rendered in flat 2D (no lighting, no perspective):
 *   OUTLINE — white border lines
 *   FILL    — grey road fill
 *   ROUTE   — blue active route
 *
 * Compositing applies subtraction, materials, and perspective.
 * ================================================================ */

/* Mask rendering passes */
void render_begin_outline_mask(void);   /* bind FBO_OUTLINE, ortho 2D, flat white */
void render_end_outline_mask(void);

void render_begin_fill_mask(void);      /* bind FBO_FILL, ortho 2D, flat grey */
void render_end_fill_mask(void);

/* Resume masks — bind without clearing (append content to existing mask) */
void render_resume_outline_mask(void);
void render_resume_fill_mask(void);

void render_begin_route_mask(void);     /* bind FBO_ROUTE, ortho 2D, flat blue */
void render_end_route_mask(void);

/* Composite masks → screen with subtraction, materials, perspective */
void render_composite(void);

/* Mark masks as needing re-render (call on maneuver state change) */
void render_invalidate_masks(void);

/* Apply/remove a 2D rigid transform for mask rendering (second maneuver). */
void render_push_mask_transform(float tx, float ty, float cos_r, float sin_r);
void render_pop_mask_transform(void);

/* Mask append mode: 1 = begin_mask doesn't clear FBO (draw on top). */
void render_set_mask_append(int append);

/* Returns 1 if masks need re-rendering */
int render_masks_dirty(void);

/* Legacy stub pass API — redirects to outline mask internally */
void render_begin_stubs(void);
void render_end_stubs(void);

/* ================================================================
 * Flag sprite API
 * ================================================================ */

/* Load flag atlas texture. Call after render_init(). Returns 0 on success. */
int render_load_flag_atlas(const char *path, int frame_w, int frame_h, int frame_count);

/* Draw flag sprite at maneuver-space position (x, y).
 * size = half-extent in maneuver space.
 * frame = animation frame index (0..frame_count-1). */
void render_sprite_flag(float x, float y, float size, int frame);

/* ================================================================
 * Vertex buffer — exposed for route_path.c mesh drawing
 * ================================================================ */

void vb_reset(void);
void vb_v(float x, float y, float z, float nx, float ny, float nz);
void vb_quad(float x0, float y0, float z0,
             float x1, float y1, float z1,
             float x2, float y2, float z2,
             float x3, float y3, float z3,
             float nx, float ny, float nz);
void vb_flush(float r, float g, float b, float a);

#endif /* CR_RENDER_H */
