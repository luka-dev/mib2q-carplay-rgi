/*
 * OpenGL rendering — shared between macOS and QNX.
 * Uses GLES2-compatible subset (shaders, no fixed-function).
 */

#ifndef CR_RENDER_H
#define CR_RENDER_H

#include <stdint.h>

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

/* Toggle 3D extrusion. raised=1 → extruded, raised=0 → flat on ground. */
void render_set_raised(int raised);

#endif /* CR_RENDER_H */
