/*
 * Route path — segment-based path definition + 3D mesh extrusion.
 *
 * Path = sequence of LINE and ARC segments in 2D (x, y).
 * Densified to polyline, then extruded into a 3D mesh
 * (top face + side walls + arrowhead prism).
 *
 * Coordinates are 2D maneuver space (same as mask rendering).
 * Extrusion maps 2D y → 3D z, adds 3D y for height.
 */

#ifndef CR_ROUTE_PATH_H
#define CR_ROUTE_PATH_H

/* Segment types */
#define RSEG_LINE  0
#define RSEG_ARC   1

#define RPATH_MAX_SEGS  48
#define RPATH_MAX_PTS  480

typedef struct {
    int type;
    float x0, y0, x1, y1;   /* LINE endpoints */
    float cx, cy;            /* ARC center */
    float radius;            /* ARC radius */
    float start_rad;         /* ARC start angle (math convention) */
    float end_rad;           /* ARC end angle */
} route_seg_t;

typedef struct {
    route_seg_t segs[RPATH_MAX_SEGS];
    int seg_count;
    /* Densified polyline */
    float px[RPATH_MAX_PTS], py[RPATH_MAX_PTS];
    unsigned char pt_smooth[RPATH_MAX_PTS];  /* 1 for interior arc samples */
    float dist[RPATH_MAX_PTS];  /* cumulative distance at each point */
    int pt_count;
    float total_length;
    /* Arrowhead */
    float arrow_x, arrow_y;
    float arrow_angle;       /* radians */
} route_path_t;

#define RMESH_MAX_VERTS 4800

typedef struct {
    float verts[RMESH_MAX_VERTS * 6];  /* pos(3) + normal(3) */
    int vert_count;
    int valid;
} route_mesh_t;

/* Path building */
void rpath_clear(route_path_t *p);
void rpath_add_line(route_path_t *p, float x0, float y0, float x1, float y1);
void rpath_add_arc(route_path_t *p, float cx, float cy, float radius,
                   float start_rad, float end_rad);
void rpath_densify(route_path_t *p);
void rpath_set_arrow(route_path_t *p, float x, float y, float angle_rad);

/* Mesh generation + drawing.
 * t0/t1 = animation window: extrude path from fraction t0 to t1.
 * 0.0 = path start, 1.0 = path end. */
void rpath_extrude(const route_path_t *p, route_mesh_t *m,
                   float width, float base_y, float top_y,
                   float t0, float t1);
void rpath_draw(const route_mesh_t *m,
                float r, float g, float b, float a);

/* Append segments from src to dst, applying 2D rigid transform:
 * rotated by (cos_r, sin_r) then translated by (tx, ty).
 * rot_rad is the rotation angle for adjusting arc start/end. */
void rpath_xform_append(route_path_t *dst, const route_path_t *src,
                         float tx, float ty, float cos_r, float sin_r,
                         float rot_rad);

/* Debug overlay — draws polyline with active window highlighted. */
void rpath_draw_debug(const route_path_t *p, float t0, float t1);

#endif /* CR_ROUTE_PATH_H */
