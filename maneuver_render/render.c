/*
 * OpenGL rendering -- single-FBO painter's algorithm architecture.
 *
 * 2 mask FBOs (flat 2D, no lighting):
 *   FBO_ROAD  -- combined road: white outline drawn first, grey fill on top
 *               (painter's algorithm -- fill overwrites interior, border remains)
 *   FBO_ROUTE -- blue active route
 *
 * Compositing pipeline:
 *   1. Render 3D ground-plane quad textured with road FBO (tex_mode 7 reads FBO RGB)
 *   2. Route shadow on asphalt
 *   3. Route mesh (extruded 3D)
 *
 * All maneuver coordinates remain 2D (x, y). Internally mapped to 3D:
 *   2D x -> 3D x (left-right)
 *   2D y -> 3D z (depth: y=-0.55 near camera, y=0.55 far)
 *   3D y  = extrusion height (0=ground, H=top)
 *
 * Uses GLES2-compatible subset (shaders, no fixed-function).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "gl_compat.h"
#define GLPC_DIR "/mnt/persist/var/app/luka_carplay_maneuver"
#include "gl_program_cache.h"
#include "render.h"
#include "protocol.h"
#include "maneuver.h"
#include "visible_area.h"
#include "lane_panel.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * 3D Configuration
 * ================================================================ */

#define EXTRUDE_H    0.03f     /* active extrusion height */
#define RAISE_BASE   0.03f    /* active floats this much above grey */
#define Z_BIAS_STEP  0.00001f  /* depth bias per draw (layer ordering) */

/* Camera -- perspective mode */
#define CAM_EYE_X    0.0f
#define CAM_EYE_Y    0.95f
#define CAM_EYE_Z   -1.40f
#define CAM_CTR_X    0.0f
#define CAM_CTR_Y   -0.50f
#define CAM_CTR_Z    0.25f
#define CAM_FOV_DEG  40.0f

typedef struct {
    float base_color[4];
    float surface[4];  /* ambient floor, diffuse strength, spec strength, spec power */
    float fx[4];       /* fresnel strength, fresnel power, clearcoat strength, clearcoat power */
} material_preset_t;

typedef struct {
    float key_dir[3];
    float key_color[3];
    float fill_color[3];
    float sky_color[3];
    float ground_color[3];
    float spec_color[3];
} lighting_state_t;

/* ================================================================
 * 4×4 matrix math (column-major, OpenGL convention)
 *
 * Layout: m[col*4 + row]
 * ================================================================ */

static void mat4_zero(float *m) { memset(m, 0, 16 * sizeof(float)); }

static void mat4_mul(float *out, const float *a, const float *b) {
    float tmp[16];
    int c, r, k;
    for (c = 0; c < 4; c++)
        for (r = 0; r < 4; r++) {
            float s = 0;
            for (k = 0; k < 4; k++)
                s += a[k * 4 + r] * b[c * 4 + k];
            tmp[c * 4 + r] = s;
        }
    memcpy(out, tmp, sizeof(tmp));
}

static void mat4_perspective(float *m, float fov_rad, float aspect, float zn, float zf) {
    float f = 1.0f / tanf(fov_rad * 0.5f);
    mat4_zero(m);
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = 2.0f * zf * zn / (zn - zf);
}

static void mat4_ortho(float *m, float l, float r, float b, float t, float n, float f) {
    mat4_zero(m);
    m[0]  =  2.0f / (r - l);
    m[5]  =  2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] =  1.0f;
}

static void mat4_lookAt(float *m,
                        float ex, float ey, float ez,
                        float cx, float cy, float cz,
                        float ux, float uy, float uz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;

    float sx = fy * uz - fz * uy;
    float sy = fz * ux - fx * uz;
    float sz = fx * uy - fy * ux;
    float sl = sqrtf(sx*sx + sy*sy + sz*sz);
    sx /= sl; sy /= sl; sz /= sl;

    float tux = sy * fz - sz * fy;
    float tuy = sz * fx - sx * fz;
    float tuz = sx * fy - sy * fx;

    /* Horizontal flip: negate side vector only (keep up unchanged). */
    mat4_zero(m);
    m[0]  = -sx;  m[4]  = -sy;  m[8]  = -sz;   m[12] = (sx*ex + sy*ey + sz*ez);
    m[1]  = tux;  m[5]  = tuy;  m[9]  = tuz;   m[13] = -(tux*ex + tuy*ey + tuz*ez);
    m[2]  = -fx;  m[6]  = -fy;  m[10] = -fz;   m[14] =  (fx*ex + fy*ey + fz*ez);
    m[3]  = 0;    m[7]  = 0;    m[11] = 0;     m[15] = 1.0f;
}

/* ================================================================
 * Shader program -- 3D with directional lighting
 *
 * Three pass families replace the former all-in-one mode switch:
 *   lit: 3D route geometry and the textured road FBO;
 *   flat: masks, overlay, arrival sprite and final framebuffer blit;
 *   shadow: route cast shadow and lower-face contact AO.
 * ================================================================ */

static GLuint g_program = 0;
static GLuint g_flat_program = 0, g_shadow_program = 0;
enum { PASS_LIT = 0, PASS_FLAT = 1, PASS_SHADOW = 2 };
static int g_active_pass = PASS_LIT;
static GLint g_flat_attr_pos = -1, g_flat_attr_uv = -1;
static GLint g_flat_mvp = -1, g_flat_zbias = -1, g_flat_mode = -1;
static GLint g_flat_color = -1, g_flat_tex = -1, g_flat_alpha = -1, g_flat_entry_fade = -1;
static float g_flat_entry_value[4];
static GLint g_shadow_attr_pos = -1, g_shadow_attr_norm = -1;
static GLint g_shadow_mvp = -1, g_shadow_zbias = -1, g_shadow_mode = -1;
static GLint g_shadow_mask_scale = -1, g_shadow_light = -1, g_shadow_tex = -1;
static GLint g_shadow_alpha = -1, g_shadow_opacity = -1, g_shadow_thickness = -1;
static float g_light_vector[3] = { 0.0f, 1.0f, 0.0f };
static GLuint g_cutout_program = 0;
static GLint g_cutout_attr = -1, g_cutout_size = -1;
static GLint g_cutout_rect = -1, g_cutout_feather = -1, g_cutout_opacity = -1;
static GLint  g_attr_pos   = -1;
static GLint  g_attr_norm  = -1;
static GLint g_attr_progress=-1, g_uni_progress=-1;
void render_set_mask_direction_fade(float x,float y,float dx,float dy,float span) {
    g_flat_entry_value[0]=dx;
    g_flat_entry_value[1]=dy;
    g_flat_entry_value[2]=-(x*dx+y*dy);
    g_flat_entry_value[3]=span>1e-6f?1.0f/span:0.0f;
    if(g_active_pass==PASS_FLAT)
        glUniform4fv(g_flat_entry_fade,1,g_flat_entry_value);
}
void render_set_mask_entry_fade(float start,float span) {
    render_set_mask_direction_fade(0,start,0,1,span);
}
static float g_progress_fill=0, g_progress_path_weight=0, g_progress_glow=0;
void render_set_route_progress(float fill, float path_weight, float glow) {
    g_progress_fill=cr_route_progress_front(fill);
    g_progress_path_weight=path_weight; g_progress_glow=glow;
}
static GLint  g_uni_color  = -1;
static GLint  g_uni_mvp    = -1;
static GLint  g_uni_light  = -1;
static GLint  g_uni_light_key_color = -1;
static GLint  g_uni_light_fill_color = -1;
static GLint  g_uni_light_sky_color = -1;
static GLint  g_uni_light_ground_color = -1;
static GLint  g_uni_light_spec_color = -1;
static GLint  g_uni_zbias  = -1;
static GLint  g_uni_eye    = -1;
static GLint  g_uni_mat_surface = -1;
static GLint  g_uni_mat_fx = -1;
static GLint  g_uni_tex_mode = -1;
static GLint  g_uni_tex      = -1;
static GLint  g_uni_mask_scale = -1;  /* vec2: 1/(2*hw), 1/(2*hh) for mask UV */
static GLint  g_uni_global_alpha = -1;
static float  g_global_alpha = 1.0f;

static int   g_perspective = 1;   /* target: 0=ortho, 1=perspective */
static float g_persp_t = 1.0f;   /* animated blend: 0.0=ortho, 1.0=perspective */
#define PERSP_ANIM_SPEED 0.033f   /* per-frame step (~1s at 30fps) */
static int g_raised = 1;
static int g_fb_w = 640, g_fb_h = 400;
static int g_win_w = 640, g_win_h = 400;  /* actual window buffer size (pre-SSAA) */
float g_3d_offset_adjust = -0.10f;  /* extra Y offset in 3D mode (tuned) */
static float g_z_bias = 0.0f;

/* Mask cache dirty flag */
static int g_masks_dirty = 1;

/* Camera pan offset (maneuver space) -- shifts entire scene to follow arrow */
static float g_cam_pan_x = 0.0f;
static float g_cam_pan_z = 0.0f;  /* z in 3D = y in maneuver 2D */
static float g_cam_rot = 0.0f;
static float g_light_rot = 0.0f;
static render_material_t g_active_material = RENDER_MAT_GENERIC_SOLID;

/* Stored MVPs for composite pass */
static float g_mvp_current[16];    /* perspective-blended MVP (for 3D composite) */
static cr_rect_animation_t g_visible_area = {
    {CR_POPUP_X, CR_POPUP_Y, CR_POPUP_W, CR_POPUP_H},
    {CR_POPUP_X, CR_POPUP_Y, CR_POPUP_W, CR_POPUP_H},
    {CR_POPUP_X, CR_POPUP_Y, CR_POPUP_W, CR_POPUP_H}, 0, 0};
static cr_rect_animation_t g_content_offset;

const cr_route_progress_map_t *render_prepare_route_progress(const cr_route_progress_point_t *points, int count) {
    static cr_route_progress_map_t projected;
    if(g_progress_path_weight<=0) return NULL;
    cr_rect_t visible=g_visible_area.current;
    /* The row erases the entry below its top edge. Keep the progress origin
     * at the visible arrow, interpolating with the same camera presentation. */
    visible.h=fmaxf(1,visible.h-g_content_offset.current.h);
    return cr_route_progress_project(&projected,points,count,g_mvp_current,visible)
        ? &projected : NULL;
}

static double viewport_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (double)t.tv_sec+(double)t.tv_nsec/1000000000.0;
}
static float g_mvp_ortho_2d[16];   /* pure orthographic for mask rendering */

/* ================================================================
 * Multi-FBO system
 * ================================================================ */

enum { FBO_ROAD = 0, FBO_ROUTE = 1, FBO_COUNT = 2 };
static GLuint g_fbos[FBO_COUNT];
static GLuint g_fbo_texs[FBO_COUNT];
static GLuint g_fbo_depths[FBO_COUNT];
static int g_fbo_w = 0, g_fbo_h = 0;
static GLint g_default_fbo = 0;  /* saved at init -- may not be 0 on macOS */
static int g_route_mask_ready = 0;

/* 2x supersample FBO — render at double resolution, blit down with GL_LINEAR */
#ifdef PLATFORM_MACOS
#define SSAA_SCALE 1  /* macOS Retina provides 2x framebuffer */
#else
#define SSAA_SCALE 2  /* QNX: explicit 2x supersample */
#endif
#define FXAA_ENABLED 1 /* FXAA on both platforms */
static GLuint g_ss_fbo = 0;
static GLuint g_ss_tex = 0;
static GLuint g_ss_depth = 0;
static int g_ss_w = 0, g_ss_h = 0;

/* FXAA post-process */
static GLuint g_fxaa_fbo = 0;
static GLuint g_fxaa_tex = 0;
static GLuint g_fxaa_prog = 0;
static GLint  g_fxaa_attr_pos = -1;
static GLint  g_fxaa_uni_tex = -1;
static GLint  g_fxaa_uni_rcp = -1;  /* 1/width, 1/height */
static float g_mask_half_w = 1.6f;
static float g_mask_half_h = 1.0f;


static const material_preset_t k_material_presets[RENDER_MAT_COUNT] = {
    [RENDER_MAT_GENERIC_SOLID] = {
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.20f, 0.56f, 0.06f, 10.0f },
        { 0.02f, 3.5f, 0.0f, 1.0f }
    },
    [RENDER_MAT_ROAD_ASPHALT] = {
        { 0.20f, 0.21f, 0.24f, 1.0f },       /* darker, slightly blue-tinted asphalt */
        { 0.18f, 0.52f, 0.14f, 12.0f },       /* tighter spec, more diffuse response */
        { 0.05f, 4.0f, 0.02f, 28.0f }         /* subtle clearcoat (wet look) */
    },
    [RENDER_MAT_ROUTE_ACTIVE] = {
        { 0.35f, 0.67f, 0.90f, 1.0f },   /* soft blue route material */
        { 0.10f, 0.68f, 0.50f,  6.0f },   /* broad spec lobe (power 6 — visible at N·H=0.82) */
        { 0.22f, 3.0f, 0.40f, 12.0f }     /* clearcoat candy gloss (power 12, wide enough for camera angle) */
    }
};

static const lighting_state_t k_lighting_state = {
    { 0.18f, 0.97f, -0.14f },
    { 0.92f, 0.87f, 0.80f },
    { 0.10f, 0.14f, 0.20f },
    { 0.14f, 0.18f, 0.26f },
    { 0.04f, 0.03f, 0.02f },
    { 0.82f, 0.80f, 0.76f }
};

static const char *k_vert_src =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "attribute float a_progress;\n"
    "uniform mat4 u_mvp;\n"
    "uniform float u_z_bias;\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_world_pos;\n"
    "varying float v_progress;\n"
    "varying float v_progress_w;\n"
    "void main() {\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "  gl_Position.z -= u_z_bias;\n"
    "  v_normal = a_normal;\n"
    /* GLES2 has no noperspective qualifier. Cancel perspective interpolation
     * so equal projected lengths stay equal inside long shaft triangles too. */
    "  v_progress = a_progress * gl_Position.w;\n"
    "  v_progress_w = gl_Position.w;\n"
    "  v_world_pos = a_pos;\n"
    "}\n";

static const char *k_frag_src_body =
    "uniform vec4 u_color;\n"
    "uniform vec4 u_progress;\n"
    "uniform vec3 u_light_dir;\n"
    "uniform vec3 u_light_key_color;\n"
    "uniform vec3 u_light_fill_color;\n"
    "uniform vec3 u_light_sky_color;\n"
    "uniform vec3 u_light_ground_color;\n"
    "uniform vec3 u_light_spec_color;\n"
    "uniform vec3 u_eye;\n"
    "uniform vec4 u_mat_surface;\n"
    "uniform vec4 u_mat_fx;\n"
    "uniform float u_tex_mode;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_mask_scale;\n"
    "uniform float u_global_alpha;\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_world_pos;\n"
    "varying float v_progress;\n"
    "varying float v_progress_w;\n"
    "vec4 shade_surface(vec4 base, float alpha) {\n"
    /* Applied to the actual route mesh only. Keep normal lighting, depth,
     * shadows and alpha; crossings carry their own arc-length coordinates. */
    "  if (u_progress.w > 0.0) {\n"
    /* Bright white resting arrow; compensate for the existing cool lighting.
     * Keep the established HUD-blue fill/blink endpoint unchanged. */
    "    vec3 quiet = vec3(1.000000, 0.980392, 0.909804);\n"
    "    vec3 ice = vec3(0.32, 0.77, 1.0);\n"
    "    float f = u_progress.x;\n"
    "    float filled = 0.0;\n"
    "    if (u_progress.y > 0.0) {\n"
    "      filled = 1.0 - smoothstep(f-0.055, f+0.055, v_progress/v_progress_w);\n"
    "      if (f <= -0.055) filled = 0.0;\n"
    "      else if (f >= 1.055) filled = 1.0;\n"
    "    }\n"
    "    vec3 tint = mix(quiet,ice,filled*u_progress.y+u_progress.z);\n"
    "    base.rgb = mix(base.rgb,tint,u_progress.w);\n"
    "  }\n"
    "  vec3 N = normalize(v_normal);\n"
    "  vec3 L = normalize(u_light_dir);\n"
    "  vec3 fill_dir = normalize(vec3(-L.x * 0.55, 0.45, -L.z * 0.55));\n"
    "  vec3 V = normalize(u_eye - v_world_pos);\n"
    /* directional lights */
    "  float key_n = max(dot(N, L), 0.0);\n"
    "  float fill_n = max(dot(N, fill_dir), 0.0);\n"
    "  float hemi_t = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);\n"
    "  vec3 hemi = mix(u_light_ground_color, u_light_sky_color, hemi_t);\n"
    "  vec3 diffuse_light = hemi;\n"
    "  diffuse_light += u_light_key_color * (0.45 * u_mat_surface.x + u_mat_surface.y * key_n);\n"
    "  diffuse_light += u_light_fill_color * fill_n * (0.08 + 0.18 * u_mat_surface.y);\n"
    /* specular */
    "  vec3 H = normalize(L + V);\n"
    "  float ndotv = max(dot(N, V), 0.0);\n"
    "  float fres = pow(1.0 - ndotv, u_mat_fx.y);\n"
    "  float spec_lobe = pow(max(dot(N, H), 0.0), u_mat_surface.w);\n"
    "  float coat_lobe = pow(max(dot(N, H), 0.0), u_mat_fx.w);\n"
    "  float spec = u_mat_surface.z * spec_lobe * (0.10 + 0.30 * fres);\n"
    "  float clearcoat = u_mat_fx.z * coat_lobe * (0.10 + 0.34 * key_n);\n"
    "  vec3 spec_tint = mix(u_light_spec_color, base.rgb, 0.48);\n"
    /* car-paint color shift at grazing angles */
    "  vec3 paint = base.rgb + base.rgb * vec3(0.12, 0.06, -0.04) * fres;\n"
    /* diffuse color */
    "  vec3 color = paint * diffuse_light;\n"
    /* specular + clearcoat */
    "  color += spec_tint * (spec + clearcoat);\n"
    /* fresnel rim */
    "  color += paint * (u_mat_fx.x * fres * 0.70);\n"
    /* environment reflection (gradient with red taillight ambience below horizon) */
    "  vec3 R = reflect(-V, N);\n"
    "  float env_t = clamp(R.y * 0.5 + 0.5, 0.0, 1.0);\n"
    "  vec3 env_lo = vec3(0.10, 0.035, 0.025);\n"   /* warm red below horizon -- taillight reflections */
    "  vec3 env_mid = vec3(0.14, 0.12, 0.15);\n"   /* horizon -- neutral */
    "  vec3 env_hi = vec3(0.09, 0.12, 0.22);\n"    /* sky -- cool blue */
    "  vec3 env_col = (env_t < 0.5)\n"
    "    ? mix(env_lo, env_mid, env_t * 2.0)\n"
    "    : mix(env_mid, env_hi, (env_t - 0.5) * 2.0);\n"
    "  float env_fres = pow(1.0 - ndotv, 2.0);\n"
    "  float env_str = u_mat_surface.z + u_mat_fx.z * 0.5 + 0.08;\n"
    "  color += env_col * (0.40 + env_fres * 1.20) * env_str;\n"
    /* edge AO -- subtle darkening on side faces */
    "  float edge_ao = mix(0.85, 1.0, smoothstep(-0.1, 0.25, N.y));\n"
    "  color *= edge_ao;\n"
    /* tone map + saturation */
    "  color = clamp(color, 0.0, 1.0);\n"
    "  {\n"
    "    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));\n"
    "    color = mix(vec3(luma), color, 1.22);\n"
    "  }\n"
    "  return vec4(color, alpha * base.a);\n"
    "}\n"
    "void main() {\n"
    /* tex_mode 7: lit 3D blit with FBO color as base (painter's algorithm road FBO). */
    "  if (u_tex_mode > 6.5 && u_tex_mode < 7.5) {\n"
    "    vec2 uv = vec2(v_world_pos.x * u_mask_scale.x + 0.5,\n"
    "                    v_world_pos.z * u_mask_scale.y + 0.5);\n"
    "    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) discard;\n"
    "    vec4 mask = texture2D(u_tex, uv);\n"
    "    if (mask.a < 0.01) discard;\n"
    "    gl_FragColor = shade_surface(vec4(mask.rgb, 1.0), mask.a);\n"
    "    gl_FragColor.a *= u_global_alpha;\n"
    "    return;\n"
    "  }\n"
    /* tex_mode 0: normal 3D geometry with lighting */
    "  gl_FragColor = shade_surface(u_color, 1.0);\n"
    "  gl_FragColor.a *= u_global_alpha;\n"
    "}\n";

/* Mask/overlay, sprite and final blit share only texture/flat-color operations.
 * None of these pixels needs the lighting, progress or material shader. */
static const char *k_flat_vert_src =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform float u_z_bias;\n"
    "uniform float u_mode;\n"
    "varying vec3 v_world_pos;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "  if (u_mode > 0.5 && u_mode < 1.5) gl_Position=vec4(a_pos.xy,0.0,1.0);\n"
    "  else { gl_Position=u_mvp*vec4(a_pos,1.0); gl_Position.z-=u_z_bias; }\n"
    "  v_world_pos=a_pos;\n"
    "  v_uv=a_normal.xy;\n"
    "}\n";
static const char *k_flat_frag_body =
    "uniform float u_mode;\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_entry_fade;\n"
    "uniform float u_global_alpha;\n"
    "uniform sampler2D u_tex;\n"
    "varying vec3 v_world_pos;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "  if (u_mode > 0.5 && u_mode < 1.5) {\n"
    "    gl_FragColor=texture2D(u_tex,v_world_pos.xy*0.5+0.5); return;\n"
    "  }\n"
    "  if (u_mode > 3.5 && u_mode < 4.5) {\n"
    "    gl_FragColor=texture2D(u_tex,v_uv);\n"
    "    gl_FragColor.a*=u_global_alpha; return;\n"
    "  }\n"
    "  gl_FragColor=u_color;\n"
    "  if (u_entry_fade.w > 0.0)\n"
    "    gl_FragColor.a*=smoothstep(0.0,1.0,(dot(v_world_pos.xz,u_entry_fade.xy)+u_entry_fade.z)*u_entry_fade.w);\n"
    "}\n";

/* Shadow pixels are multiplicative and never call the lit material function. */
static const char *k_shadow_vert_src =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform float u_z_bias;\n"
    "varying vec3 v_world_pos;\n"
    "varying vec3 v_normal;\n"
    "void main() {\n"
    "  gl_Position=u_mvp*vec4(a_pos,1.0); gl_Position.z-=u_z_bias;\n"
    "  v_world_pos=a_pos; v_normal=a_normal;\n"
    "}\n";
static const char *k_shadow_frag_body =
    "uniform float u_mode;\n"
    "uniform vec2 u_mask_scale;\n"
    "uniform vec3 u_light_dir;\n"
    "uniform float u_global_alpha;\n"
    "uniform float u_opacity;\n"
    "uniform float u_contact_thickness;\n"
    "uniform sampler2D u_tex;\n"
    "varying vec3 v_world_pos;\n"
    "varying vec3 v_normal;\n"
    "void main() {\n"
    "  if (u_mode > 8.5 && u_mode < 9.5) {\n"
    "    vec2 uv=vec2(v_world_pos.x*u_mask_scale.x+0.5,v_world_pos.z*u_mask_scale.y+0.5);\n"
    "    if (uv.x<0.0 || uv.x>1.0 || uv.y<0.0 || uv.y>1.0) discard;\n"
    "    vec2 shoff=vec2(u_light_dir.x,u_light_dir.z)*0.02;\n"
    "    vec4 rmask=texture2D(u_tex,uv-shoff);\n"
    "    if (rmask.a<0.01) discard;\n"
    "    float sa=smoothstep(0.0,0.4,rmask.a)*0.38;\n"
    "    gl_FragColor=vec4(0.0,0.0,0.0,sa*u_global_alpha); return;\n"
    "  }\n"
    "  float width=max(0.001,v_normal.z);\n"
    "  float edge=1.0-smoothstep(0.0,1.0,abs(v_normal.x)/width);\n"
    "  float gap=1.0-smoothstep(0.06,0.14,max(0.0,v_normal.y-u_contact_thickness));\n"
    "  float onset=smoothstep(0.0,0.15,v_normal.z);\n"
    "  gl_FragColor=vec4(0.0,0.0,0.0,0.42*edge*gap*onset*u_opacity*u_global_alpha);\n"
    "}\n";

/* ================================================================
 * Vertex buffer -- interleaved pos(3) + normal(3), 6 floats/vert
 * ================================================================ */

#define MAX_VERTS 1200
static float g_vbuf[MAX_VERTS * 6];
static float g_vprogress[MAX_VERTS];
static int g_route_batch=0;
static int g_vcount;

void vb_reset(void) { g_vcount = 0; g_route_batch=0; }

void vb_v(float x, float y, float z, float nx, float ny, float nz) {
    if (g_vcount >= MAX_VERTS) return;
    int i = g_vcount * 6;
    g_vbuf[i] = x; g_vbuf[i+1] = y; g_vbuf[i+2] = z;
    g_vbuf[i+3] = nx; g_vbuf[i+4] = ny; g_vbuf[i+5] = nz;
    g_vprogress[g_vcount]=0;
    g_vcount++;
}

void vb_route_v(float x,float y,float z,float nx,float ny,float nz,float progress) {
    if(g_vcount>=MAX_VERTS) return;
    vb_v(x,y,z,nx,ny,nz);
    g_vprogress[g_vcount-1]=progress;
    g_route_batch=1;
}

/* Push a quad as 2 triangles with flat normal */
void vb_quad(float x0, float y0, float z0,
                    float x1, float y1, float z1,
                    float x2, float y2, float z2,
                    float x3, float y3, float z3,
                    float nx, float ny, float nz) {
    vb_v(x0,y0,z0, nx,ny,nz);  vb_v(x1,y1,z1, nx,ny,nz);  vb_v(x2,y2,z2, nx,ny,nz);
    vb_v(x0,y0,z0, nx,ny,nz);  vb_v(x2,y2,z2, nx,ny,nz);  vb_v(x3,y3,z3, nx,ny,nz);
}

static const material_preset_t *material_preset(render_material_t material) {
    if (material < 0 || material >= RENDER_MAT_COUNT)
        return &k_material_presets[RENDER_MAT_GENERIC_SOLID];
    return &k_material_presets[material];
}

static void apply_material(const material_preset_t *preset,
                           float r, float g, float b, float a) {
    glUniform4f(g_uni_color, r, g, b, a);
    glUniform4f(g_uni_mat_surface,
                preset->surface[0], preset->surface[1],
                preset->surface[2], preset->surface[3]);
    glUniform4f(g_uni_mat_fx,
                preset->fx[0], preset->fx[1],
                preset->fx[2], preset->fx[3]);
}

static void use_lit_program(void) {
    glUseProgram(g_program);
    g_active_pass=PASS_LIT;
    glUniform1f(g_uni_tex_mode,0.0f);
    glUniform1f(g_uni_global_alpha,g_global_alpha);
    glUniform4f(g_uni_progress,0,0,0,0);
}

static void use_flat_program(float mode) {
    glUseProgram(g_flat_program);
    g_active_pass=PASS_FLAT;
    glUniform1f(g_flat_mode,mode);
    glUniform1f(g_flat_alpha,g_global_alpha);
    glUniform4fv(g_flat_entry_fade,1,g_flat_entry_value);
    glUniform1i(g_flat_tex,0);
}

static void use_shadow_program(float mode) {
    glUseProgram(g_shadow_program);
    g_active_pass=PASS_SHADOW;
    glUniform1f(g_shadow_mode,mode);
    glUniform1f(g_shadow_alpha,g_global_alpha);
    glUniformMatrix4fv(g_shadow_mvp,1,GL_FALSE,g_mvp_current);
    glUniform2f(g_shadow_mask_scale,0.5f/g_mask_half_w,0.5f/g_mask_half_h);
    glUniform3fv(g_shadow_light,1,g_light_vector);
    glUniform1i(g_shadow_tex,0);
}

void vb_flush(float r, float g, float b, float a) {
    const material_preset_t *preset;

    if (g_vcount == 0) return;
    if (g_active_pass==PASS_FLAT) {
        glUniform4f(g_flat_color,r,g,b,a);
        glUniform1f(g_flat_zbias,g_z_bias);
        g_z_bias+=Z_BIAS_STEP;
        glVertexAttribPointer(g_flat_attr_pos,3,GL_FLOAT,GL_FALSE,24,g_vbuf);
        glVertexAttribPointer(g_flat_attr_uv,3,GL_FLOAT,GL_FALSE,24,g_vbuf+3);
        glEnableVertexAttribArray(g_flat_attr_pos);
        glEnableVertexAttribArray(g_flat_attr_uv);
        glDrawArrays(GL_TRIANGLES,0,g_vcount);
        return;
    }
    preset = material_preset(g_active_material);
    apply_material(preset, r, g, b, a);
    glUniform1f(g_uni_zbias, g_z_bias);
    g_z_bias += Z_BIAS_STEP;

    glVertexAttribPointer(g_attr_pos,  3, GL_FLOAT, GL_FALSE, 24, g_vbuf);
    glVertexAttribPointer(g_attr_norm, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf + 3);
    glEnableVertexAttribArray(g_attr_pos);
    glEnableVertexAttribArray(g_attr_norm);
    glUniform4f(g_uni_progress,g_progress_fill,g_progress_path_weight,g_progress_glow,
                g_route_batch ? 1.0f : 0.0f);
    if(g_route_batch) {
        glVertexAttribPointer(g_attr_progress,1,GL_FLOAT,GL_FALSE,0,g_vprogress);
        glEnableVertexAttribArray(g_attr_progress);
    }
    glDrawArrays(GL_TRIANGLES, 0, g_vcount);
    glDisableVertexAttribArray(g_attr_progress);
    glVertexAttrib1f(g_attr_progress,0);
    /* Direct mask/sprite/composite draws bypass vb_flush. */
    glUniform4f(g_uni_progress,0,0,0,0);
}

/* Multiplicative contact AO preserves framebuffer alpha and only darkens the
 * lower mesh surfaces. Depth testing keeps the upper arrow clean. */
void render_contact_shadow(const float *verts,int count,float alpha,float thickness) {
    int drawn=0;
    if(count<=0 || alpha<=0)return;
    use_shadow_program(10.0f);
    glUniform1f(g_shadow_thickness,thickness);
    glUniform1f(g_shadow_opacity,alpha);
    glUniform1f(g_shadow_zbias,g_z_bias);
    glDepthMask(GL_FALSE);
    glBlendFuncSeparate(GL_ZERO,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);
    while(drawn<count) {
        int batch=count-drawn;
        if(batch>MAX_VERTS)batch=MAX_VERTS;
        glVertexAttribPointer(g_shadow_attr_pos,3,GL_FLOAT,GL_FALSE,24,verts+drawn*6);
        glVertexAttribPointer(g_shadow_attr_norm,3,GL_FLOAT,GL_FALSE,24,verts+drawn*6+3);
        glDrawArrays(GL_TRIANGLES,0,batch);
        drawn+=batch;
    }
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    use_lit_program();
}

/* ================================================================
 * Shader compile / link
 * ================================================================ */

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "render: shader error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

/* Create, bind attributes 0..n-1 in order, and link: from the persistent
 * program-binary cache when it holds this exact program, else by compiling
 * (then saved for the next launch).  The tag names the attribute bindings,
 * which are baked into a cached binary. */
static GLuint link_program_cached(const char *tag, const char *vert, const char *frag,
                                  const char *const *attrs, int nattrs) {
    GLuint program = glCreateProgram(), vs = 0, fs = 0;
    GLint ok = 0;
    int i;
    if (!program) return 0;
    for (i = 0; i < nattrs; ++i) glBindAttribLocation(program, (GLuint)i, attrs[i]);
    if (glpc_load(program, tag, vert, frag)) return program;
    vs = compile_shader(GL_VERTEX_SHADER, vert);
    fs = compile_shader(GL_FRAGMENT_SHADER, frag);
    if (vs && fs) {
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(program, sizeof(log), NULL, log);
            fprintf(stderr, "render: %s link error: %s\n", tag, log);
        }
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    if (!ok) { glDeleteProgram(program); return 0; }
    glpc_store(program, tag, vert, frag);
    return program;
}

static int build_program(void) {
    char vert_src[4096];
    snprintf(vert_src, sizeof(vert_src), "%s%s%s",
             SHADER_HEADER, SHADER_PRECISION, k_vert_src);
    char frag_src[16384];
    snprintf(frag_src, sizeof(frag_src), "%s%s%s",
             SHADER_HEADER, SHADER_PRECISION, k_frag_src_body);

    /* GL 2.1 requires array 0 to remain the position stream. */
    static const char *const attrs[] = { "a_pos", "a_normal", "a_progress" };
    g_program = link_program_cached("maneuver:main:a_pos,a_normal,a_progress",
                                    vert_src, frag_src, attrs, 3);
    if (!g_program) return -1;

    g_attr_pos  = glGetAttribLocation(g_program, "a_pos");
    g_attr_norm = glGetAttribLocation(g_program, "a_normal");
    g_attr_progress=glGetAttribLocation(g_program,"a_progress");
    g_uni_progress=glGetUniformLocation(g_program,"u_progress");
    g_uni_color = glGetUniformLocation(g_program, "u_color");
    g_uni_mvp   = glGetUniformLocation(g_program, "u_mvp");
    g_uni_light = glGetUniformLocation(g_program, "u_light_dir");
    g_uni_light_key_color = glGetUniformLocation(g_program, "u_light_key_color");
    g_uni_light_fill_color = glGetUniformLocation(g_program, "u_light_fill_color");
    g_uni_light_sky_color = glGetUniformLocation(g_program, "u_light_sky_color");
    g_uni_light_ground_color = glGetUniformLocation(g_program, "u_light_ground_color");
    g_uni_light_spec_color = glGetUniformLocation(g_program, "u_light_spec_color");
    g_uni_zbias = glGetUniformLocation(g_program, "u_z_bias");
    g_uni_eye   = glGetUniformLocation(g_program, "u_eye");
    g_uni_mat_surface = glGetUniformLocation(g_program, "u_mat_surface");
    g_uni_mat_fx = glGetUniformLocation(g_program, "u_mat_fx");
    g_uni_tex_mode = glGetUniformLocation(g_program, "u_tex_mode");
    g_uni_tex      = glGetUniformLocation(g_program, "u_tex");
    g_uni_mask_scale = glGetUniformLocation(g_program, "u_mask_scale");
    g_uni_global_alpha = glGetUniformLocation(g_program, "u_global_alpha");
    return 0;
}

static GLuint build_pass_program(const char *name,const char *vertex_body,
                                 const char *fragment_body) {
    static const char *const attrs[] = { "a_pos", "a_normal" };
    char vertex[4096],fragment[8192],tag[64];
    snprintf(vertex,sizeof(vertex),"%s%s%s",SHADER_HEADER,SHADER_PRECISION,vertex_body);
    snprintf(fragment,sizeof(fragment),"%s%s%s",SHADER_HEADER,SHADER_PRECISION,fragment_body);
    snprintf(tag,sizeof(tag),"maneuver:%s:a_pos,a_normal",name);
    return link_program_cached(tag,vertex,fragment,attrs,2);
}

static int build_flat_program(void) {
    g_flat_program=build_pass_program("flat",k_flat_vert_src,k_flat_frag_body);
    if(!g_flat_program)return -1;
    g_flat_attr_pos=glGetAttribLocation(g_flat_program,"a_pos");
    g_flat_attr_uv=glGetAttribLocation(g_flat_program,"a_normal");
    g_flat_mvp=glGetUniformLocation(g_flat_program,"u_mvp");
    g_flat_zbias=glGetUniformLocation(g_flat_program,"u_z_bias");
    g_flat_mode=glGetUniformLocation(g_flat_program,"u_mode");
    g_flat_color=glGetUniformLocation(g_flat_program,"u_color");
    g_flat_tex=glGetUniformLocation(g_flat_program,"u_tex");
    g_flat_alpha=glGetUniformLocation(g_flat_program,"u_global_alpha");
    g_flat_entry_fade=glGetUniformLocation(g_flat_program,"u_entry_fade");
    if(g_flat_attr_pos<0 || g_flat_attr_uv<0 || g_flat_mvp<0 ||
       g_flat_zbias<0 || g_flat_mode<0 || g_flat_color<0 ||
       g_flat_tex<0 || g_flat_alpha<0 || g_flat_entry_fade<0)return -1;
    return 0;
}

static int build_shadow_program(void) {
    g_shadow_program=build_pass_program("shadow",k_shadow_vert_src,k_shadow_frag_body);
    if(!g_shadow_program)return -1;
    g_shadow_attr_pos=glGetAttribLocation(g_shadow_program,"a_pos");
    g_shadow_attr_norm=glGetAttribLocation(g_shadow_program,"a_normal");
    g_shadow_mvp=glGetUniformLocation(g_shadow_program,"u_mvp");
    g_shadow_zbias=glGetUniformLocation(g_shadow_program,"u_z_bias");
    g_shadow_mode=glGetUniformLocation(g_shadow_program,"u_mode");
    g_shadow_mask_scale=glGetUniformLocation(g_shadow_program,"u_mask_scale");
    g_shadow_light=glGetUniformLocation(g_shadow_program,"u_light_dir");
    g_shadow_tex=glGetUniformLocation(g_shadow_program,"u_tex");
    g_shadow_alpha=glGetUniformLocation(g_shadow_program,"u_global_alpha");
    g_shadow_opacity=glGetUniformLocation(g_shadow_program,"u_opacity");
    g_shadow_thickness=glGetUniformLocation(g_shadow_program,"u_contact_thickness");
    if(g_shadow_attr_pos<0 || g_shadow_attr_norm<0 || g_shadow_mvp<0 ||
       g_shadow_zbias<0 || g_shadow_mode<0 || g_shadow_mask_scale<0 ||
       g_shadow_light<0 || g_shadow_tex<0 || g_shadow_alpha<0 ||
       g_shadow_opacity<0 || g_shadow_thickness<0)return -1;
    return 0;
}

/* Lane erasure is a small screen-space program, isolated from the many 3D
 * lighting/progress branches in the road shader.  It keeps the same feather
 * and destination-alpha blend while avoiding that shader for the panel pass. */
static int build_cutout_program(void) {
    static const char *vertex_body =
        "attribute vec2 a_pos;\n"
        "uniform vec2 u_size;\n"
        "varying vec2 v_source;\n"
        "void main() {\n"
        "  v_source=a_pos;\n"
        "  gl_Position=vec4(2.0*a_pos.x/u_size.x-1.0,1.0-2.0*a_pos.y/u_size.y,0.0,1.0);\n"
        "}\n";
    static const char *fragment_body =
        "uniform vec4 u_rect;\n"
        "uniform vec2 u_feather;\n"
        "uniform float u_opacity;\n"
        "varying vec2 v_source;\n"
        "void main() {\n"
        "  float a=smoothstep(u_rect.x-u_feather.x,u_rect.x,v_source.x);\n"
        "  a*=1.0-smoothstep(u_rect.z,u_rect.z+u_feather.x,v_source.x);\n"
        "  a*=smoothstep(u_rect.y-u_feather.y,u_rect.y,v_source.y);\n"
        "  gl_FragColor=vec4(0.0,0.0,0.0,a*u_opacity);\n"
        "}\n";
    g_cutout_program=build_pass_program("cutout",vertex_body,fragment_body);
    if(!g_cutout_program)return -1;
    g_cutout_attr=glGetAttribLocation(g_cutout_program,"a_pos");
    g_cutout_size=glGetUniformLocation(g_cutout_program,"u_size");
    g_cutout_rect=glGetUniformLocation(g_cutout_program,"u_rect");
    g_cutout_feather=glGetUniformLocation(g_cutout_program,"u_feather");
    g_cutout_opacity=glGetUniformLocation(g_cutout_program,"u_opacity");
    if(g_cutout_attr<0 || g_cutout_size<0 || g_cutout_rect<0 ||
       g_cutout_feather<0 || g_cutout_opacity<0) {
        glDeleteProgram(g_cutout_program);g_cutout_program=0;
        return -1;
    }
    return 0;
}

/* ================================================================
 * FXAA post-process shader (GLES2-compatible, based on FXAA 3.11)
 * ================================================================ */

static const char *k_fxaa_vert =
    "attribute vec2 a_pos;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "  v_uv = a_pos * 0.5 + 0.5;\n"
    "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char *k_fxaa_frag =
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_rcp;\n"  /* 1/width, 1/height */
    "varying vec2 v_uv;\n"
    "\n"
    "void main() {\n"
    "  vec3 rgbM  = texture2D(u_tex, v_uv).rgb;\n"
    "  vec3 rgbNW = texture2D(u_tex, v_uv + vec2(-1.0, -1.0) * u_rcp).rgb;\n"
    "  vec3 rgbNE = texture2D(u_tex, v_uv + vec2( 1.0, -1.0) * u_rcp).rgb;\n"
    "  vec3 rgbSW = texture2D(u_tex, v_uv + vec2(-1.0,  1.0) * u_rcp).rgb;\n"
    "  vec3 rgbSE = texture2D(u_tex, v_uv + vec2( 1.0,  1.0) * u_rcp).rgb;\n"
    "\n"
    "  vec3 luma = vec3(0.299, 0.587, 0.114);\n"
    "  float lumM  = dot(rgbM,  luma);\n"
    "  float lumNW = dot(rgbNW, luma);\n"
    "  float lumNE = dot(rgbNE, luma);\n"
    "  float lumSW = dot(rgbSW, luma);\n"
    "  float lumSE = dot(rgbSE, luma);\n"
    "\n"
    "  float lumMin = min(lumM, min(min(lumNW, lumNE), min(lumSW, lumSE)));\n"
    "  float lumMax = max(lumM, max(max(lumNW, lumNE), max(lumSW, lumSE)));\n"
    "  float lumRange = lumMax - lumMin;\n"
    "\n"
    "  if (lumRange < max(0.0312, lumMax * 0.125)) {\n"
    "    gl_FragColor = texture2D(u_tex, v_uv);\n"
    "    return;\n"
    "  }\n"
    "\n"
    "  vec2 dir;\n"
    "  dir.x = -((lumNW + lumNE) - (lumSW + lumSE));\n"
    "  dir.y =  ((lumNW + lumSW) - (lumNE + lumSE));\n"
    "  float dirReduce = max((lumNW + lumNE + lumSW + lumSE) * 0.03125, 0.0078125);\n"
    "  float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);\n"
    "  dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * u_rcp;\n"
    "\n"
    /* Filter associated RGBA together. Keeping the center alpha while
     * averaging neighboring RGB can create color in transparent pixels. */
    "  vec4 rgbaA = 0.5 * (\n"
    "    texture2D(u_tex, v_uv + dir * (1.0/3.0 - 0.5)) +\n"
    "    texture2D(u_tex, v_uv + dir * (2.0/3.0 - 0.5)));\n"
    "  vec4 rgbaB = rgbaA * 0.5 + 0.25 * (\n"
    "    texture2D(u_tex, v_uv + dir * -0.5) +\n"
    "    texture2D(u_tex, v_uv + dir *  0.5));\n"
    "  float lumB = dot(rgbaB.rgb, luma);\n"
    "\n"
    "  if (lumB < lumMin || lumB > lumMax)\n"
    "    gl_FragColor = rgbaA;\n"
    "  else\n"
    "    gl_FragColor = rgbaB;\n"
    "}\n";

static int build_fxaa_program(void) {
    char vert[2048], frag[4096];
    snprintf(vert, sizeof(vert), "%s%s%s", SHADER_HEADER, SHADER_PRECISION, k_fxaa_vert);
    snprintf(frag, sizeof(frag), "%s%s%s", SHADER_HEADER, SHADER_PRECISION, k_fxaa_frag);

    g_fxaa_prog = link_program_cached("maneuver:fxaa", vert, frag, NULL, 0);
    if (!g_fxaa_prog) {
        fprintf(stderr, "render: FXAA program build failed\n");
        return -1;
    }
    g_fxaa_attr_pos = glGetAttribLocation(g_fxaa_prog, "a_pos");
    g_fxaa_uni_tex  = glGetUniformLocation(g_fxaa_prog, "u_tex");
    g_fxaa_uni_rcp  = glGetUniformLocation(g_fxaa_prog, "u_rcp");
    fprintf(stderr, "render: FXAA shader OK\n");
    return 0;
}

/* ================================================================
 * Multi-FBO management
 * ================================================================ */

static void fbos_init(int w, int h) {
    int i;
    int alloc_w = (int)ceilf((float)w * g_mask_half_h);
    int alloc_h = (int)ceilf((float)h * g_mask_half_h);

    if (alloc_w < w) alloc_w = w;
    if (alloc_h < h) alloc_h = h;
    g_fbo_w = alloc_w;
    g_fbo_h = alloc_h;

    glGenTextures(FBO_COUNT, g_fbo_texs);
    glGenRenderbuffers(FBO_COUNT, g_fbo_depths);
    glGenFramebuffers(FBO_COUNT, g_fbos);

    for (i = 0; i < FBO_COUNT; i++) {
        glBindTexture(GL_TEXTURE_2D, g_fbo_texs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_fbo_w, g_fbo_h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindRenderbuffer(GL_RENDERBUFFER, g_fbo_depths[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_FBO, g_fbo_w, g_fbo_h);

        glBindFramebuffer(GL_FRAMEBUFFER, g_fbos[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, g_fbo_texs[i], 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, g_fbo_depths[i]);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            fprintf(stderr, "render: FBO[%d] incomplete (0x%x)\n", i, status);

        /* Make initial contents deterministic even if a given mask is never rendered. */
        glViewport(0, 0, g_fbo_w, g_fbo_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_route_mask_ready = 0;
    fprintf(stderr, "render: %d FBOs init %dx%d (mask half extents %.2f x %.2f)\n",
            FBO_COUNT, g_fbo_w, g_fbo_h, g_mask_half_w, g_mask_half_h);
}

static void fbos_shutdown(void) {
    if (g_fbos[0]) { glDeleteFramebuffers(FBO_COUNT, g_fbos); memset(g_fbos, 0, sizeof(g_fbos)); }
    if (g_fbo_texs[0]) { glDeleteTextures(FBO_COUNT, g_fbo_texs); memset(g_fbo_texs, 0, sizeof(g_fbo_texs)); }
    if (g_fbo_depths[0]) { glDeleteRenderbuffers(FBO_COUNT, g_fbo_depths); memset(g_fbo_depths, 0, sizeof(g_fbo_depths)); }
    g_fbo_w = g_fbo_h = 0;
    g_route_mask_ready = 0;
}


static void fbos_resize(int w, int h) {
    int alloc_w = (int)ceilf((float)w * g_mask_half_h);
    int alloc_h = (int)ceilf((float)h * g_mask_half_h);

    if (alloc_w < w) alloc_w = w;
    if (alloc_h < h) alloc_h = h;
    if (alloc_w == g_fbo_w && alloc_h == g_fbo_h) return;
    fbos_shutdown();
    fbos_init(w, h);
    g_masks_dirty = 1;
}

/* Bind a specific FBO, clear it, set flat overwrite mode */
static void fbo_bind(int idx) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbos[idx]);
    glViewport(0, 0, g_fbo_w, g_fbo_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void fbo_unbind(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_ss_fbo);
    glViewport(0, 0, g_fb_w, g_fb_h);
}

static void update_mask_config(int fb_width, int fb_height) {
    float required_x = 0.0f;
    float required_y = 0.0f;
    float aspect = (float)fb_width / (float)fb_height;

    maneuver_get_transition_mask_bounds(&required_x, &required_y);

    g_mask_half_h = 1.0f;
    if (required_y > g_mask_half_h)
        g_mask_half_h = required_y;
    if (required_x / aspect > g_mask_half_h)
        g_mask_half_h = required_x / aspect;
    g_mask_half_w = g_mask_half_h * aspect;
}

/* ================================================================
 * Public API
 * ================================================================ */

int render_init(int fb_width, int fb_height) {
    if (build_program() < 0) return -1;
    if (build_flat_program() < 0 || build_shadow_program() < 0 ||
        build_cutout_program() < 0) {
        render_shutdown();
        return -1;
    }

    g_fb_w = fb_width;
    g_fb_h = fb_height;
    update_mask_config(fb_width, fb_height);

    glViewport(0, 0, fb_width, fb_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_DITHER);
    /* MSAA removed — replaced by FXAA post-process for better edge smoothing */
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    /* Save default framebuffer -- may not be 0 on macOS with MSAA */
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &g_default_fbo);

    /* 2x supersample: create SS FBO, then override g_fb_w/g_fb_h to 2x
     * so the entire pipeline renders at double resolution. */
    g_ss_w = fb_width * SSAA_SCALE;
    g_ss_h = fb_height * SSAA_SCALE;
    glGenTextures(1, &g_ss_tex);
    glBindTexture(GL_TEXTURE_2D, g_ss_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_ss_w, g_ss_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenRenderbuffers(1, &g_ss_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, g_ss_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_FBO, g_ss_w, g_ss_h);
    glGenFramebuffers(1, &g_ss_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_ss_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g_ss_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, g_ss_depth);
    glBindFramebuffer(GL_FRAMEBUFFER, g_default_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    fprintf(stderr, "render: SSAA %dx%d -> %dx%d\n", g_ss_w, g_ss_h, fb_width, fb_height);

    /* FXAA intermediate FBO (same resolution as SSAA) — QNX only */
#if FXAA_ENABLED
    glGenTextures(1, &g_fxaa_tex);
    glBindTexture(GL_TEXTURE_2D, g_fxaa_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_ss_w, g_ss_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &g_fxaa_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fxaa_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g_fxaa_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, g_default_fbo);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (build_fxaa_program() < 0) {
        fprintf(stderr, "render: FXAA init failed, continuing without\n");
    } else {
        fprintf(stderr, "render: FXAA FBO %dx%d tex=%u fbo=%u attr=%d\n",
                g_ss_w, g_ss_h, g_fxaa_tex, g_fxaa_fbo, g_fxaa_attr_pos);
    }
#endif

    /* Save actual window size for final blit, then override to 2x for pipeline */
    g_win_w = fb_width;
    g_win_h = fb_height;
    g_fb_w = g_ss_w;
    g_fb_h = g_ss_h;

    fbos_init(g_fb_w, g_fb_h);

    /* Init tex_mode off */
    use_lit_program();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fbo_texs[0]);
    glUniform1i(g_uni_tex, 0);

    fprintf(stderr, "render: init multi-layer %dx%d (default fbo=%d)\n",
            fb_width, fb_height, (int)g_default_fbo);
    return 0;
}

void render_set_viewport(int fb_width, int fb_height) {
    g_win_w = fb_width;
    g_win_h = fb_height;
    g_ss_w = fb_width * SSAA_SCALE;
    g_ss_h = fb_height * SSAA_SCALE;
    g_fb_w = g_ss_w;
    g_fb_h = g_ss_h;

    /* Resize SSAA texture + depth to match new supersample resolution */
    if (g_ss_tex) {
        glBindTexture(GL_TEXTURE_2D, g_ss_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_ss_w, g_ss_h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (g_ss_depth) {
        glBindRenderbuffer(GL_RENDERBUFFER, g_ss_depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_FBO, g_ss_w, g_ss_h);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
#if FXAA_ENABLED
    if (g_fxaa_tex) {
        glBindTexture(GL_TEXTURE_2D, g_fxaa_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_ss_w, g_ss_h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
#endif

    update_mask_config(g_fb_w, g_fb_h);
    glViewport(0, 0, g_fb_w, g_fb_h);
    fbos_resize(g_fb_w, g_fb_h);
}

static void build_camera_mvp(float *mvp, float aspect, float pan_x, float pan_z,
                              float rotation, float ts,float dolly) {
    float cam_cos = cosf(rotation);
    float cam_sin = sinf(rotation);
    float eye_x = pan_x + cam_cos * CAM_EYE_X - cam_sin * CAM_EYE_Z;
    float eye_z = pan_z + cam_sin * CAM_EYE_X + cam_cos * CAM_EYE_Z;
    float ctr_x = pan_x + cam_cos * CAM_CTR_X - cam_sin * CAM_CTR_Z;
    float ctr_z = pan_z + cam_sin * CAM_CTR_X + cam_cos * CAM_CTR_Z;
    float distance=1+dolly;
    float eye_y=CAM_EYE_Y;
    if(dolly!=0) {
        eye_x=ctr_x+(eye_x-ctr_x)*distance;
        eye_z=ctr_z+(eye_z-ctr_z)*distance;
        eye_y=CAM_CTR_Y+(CAM_EYE_Y-CAM_CTR_Y)*distance;
    }

    /* Compute both MVPs and lerp */
    float mvp_persp[16], mvp_ortho[16];

    {
        float proj[16], view[16];
        float fov_rad = CAM_FOV_DEG * (float)M_PI / 180.0f;
        mat4_perspective(proj, fov_rad, aspect, 0.1f, 20.0f);
        mat4_lookAt(view,
                    eye_x, eye_y, eye_z,
                    ctr_x, CAM_CTR_Y, ctr_z,
                    0.0f, 1.0f, 0.0f);
        mat4_mul(mvp_persp, proj, view);
    }
    {
        float proj[16], view[16];
        float hh = distance, hw = hh * aspect;
        mat4_ortho(proj, -hw, hw, -hh, hh, 0.1f, 20.0f);
        mat4_zero(view);
        /* Ortho view uses the inverse camera yaw so top-down motion matches
         * the perspective camera heading instead of orbiting sideways. */
        view[0]  =  cam_cos;
        view[1]  = -cam_sin;
        view[8]  =  cam_sin;
        view[9]  =  cam_cos;
        view[12] = -cam_cos * pan_x - cam_sin * pan_z;
        view[13] =  cam_sin * pan_x - cam_cos * pan_z;
        view[6]  =  1.0f;
        view[14] = -5.0f;
        view[15] =  1.0f;
        mat4_mul(mvp_ortho, proj, view);
    }

    /* Blended MVP for 3D composite */
    {
        int i;
        for (i = 0; i < 16; i++)
            mvp[i] = mvp_ortho[i] + ts * (mvp_persp[i] - mvp_ortho[i]);
    }

    /* Shift projection to center maneuver in popup crop area.
     * Full offset in 2D (flat view), reduced in 3D (perspective extends upward).
     * ts = perspective blend: 0.0=ortho, 1.0=perspective. */
    {
        const float content_h = (float)(CR_DEFAULT_HEIGHT - 1);
        const float popup_cy = CR_POPUP_Y + CR_POPUP_H * 0.5f;     /* 103.5 */
        const float content_cy = content_h * 0.5f;                   /* 90 */
        const float full_offset = -(popup_cy - content_cy) / content_cy; /* -0.15 */
        /* 2D: full offset (centers junction in popup crop).
         * 3D: half offset (compromise — centers between junction and road top). */
        /* 2D: full offset (-0.15). 3D: adjustable extra offset. */
        const float offset_y = full_offset + ts * g_3d_offset_adjust;
        int c;
        for (c = 0; c < 4; c++) {
            mvp[c*4 + 1] += mvp[c*4 + 3] * offset_y;
        }
    }

}

void render_get_layout_matrix(float out[16]) {
    render_build_layout_matrix(out,(float)g_fb_w/g_fb_h);
}

void render_build_layout_matrix(float out[16],float aspect) {
    if(out)build_camera_mvp(out,aspect,0,0,0,1,0);
}

void render_build_framing_matrix(float out[16],float aspect,float dolly) {
    if(out)build_camera_mvp(out,aspect,0,0,0,1,dolly);
}

static void sync_camera_uniforms(void) {
    /* Animate g_persp_t toward target with ease-in-out */
    {
        float target = (float)g_perspective;
        float diff = target - g_persp_t;
        if (fabsf(diff) < 0.001f) {
            g_persp_t = target;
        } else {
            g_persp_t += (diff > 0 ? 1.0f : -1.0f) * PERSP_ANIM_SPEED;
            if ((diff > 0 && g_persp_t > target) ||
                (diff < 0 && g_persp_t < target))
                g_persp_t = target;
        }
    }

    /* Quintic ease-in-out (smootherstep) */
    float t = g_persp_t;
    float ts = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);

    float cam_cos=cosf(g_cam_rot), cam_sin=sinf(g_cam_rot);
    float eye_x=g_cam_pan_x+cam_cos*CAM_EYE_X-cam_sin*CAM_EYE_Z;
    float eye_z=g_cam_pan_z+cam_sin*CAM_EYE_X+cam_cos*CAM_EYE_Z;
    float eye_y=CAM_EYE_Y;
    if(g_content_offset.current.w!=0) {
        float distance=1+g_content_offset.current.w;
        float ctr_x=g_cam_pan_x+cam_cos*CAM_CTR_X-cam_sin*CAM_CTR_Z;
        float ctr_z=g_cam_pan_z+cam_sin*CAM_CTR_X+cam_cos*CAM_CTR_Z;
        eye_x=ctr_x+(eye_x-ctr_x)*distance;eye_z=ctr_z+(eye_z-ctr_z)*distance;
        eye_y=CAM_CTR_Y+(CAM_EYE_Y-CAM_CTR_Y)*distance;
    }
    build_camera_mvp(g_mvp_current,(float)g_fb_w/g_fb_h,
                     g_cam_pan_x,g_cam_pan_z,g_cam_rot,ts,g_content_offset.current.w);
    for(int c=0;c<4;++c) {
        g_mvp_current[c*4]+=g_mvp_current[c*4+3]*2*g_content_offset.current.x/CR_DEFAULT_WIDTH;
        g_mvp_current[c*4+1]-=g_mvp_current[c*4+3]*2*g_content_offset.current.y/CR_DEFAULT_HEIGHT;
    }

    /* Pure 2D orthographic MVP for mask rendering.
     * Same top-down view as ortho mode but with no perspective blend. */
    {
        float proj[16], view[16];
        mat4_ortho(proj, -g_mask_half_w, g_mask_half_w,
                   -g_mask_half_h, g_mask_half_h, 0.1f, 20.0f);
        mat4_zero(view);
        view[0]  =  1.0f;   /* x -> x */
        view[9]  =  1.0f;   /* z -> y_screen */
        view[13] =  0.0f;
        view[6]  =  1.0f;   /* y -> z_depth */
        view[14] = -5.0f;   /* push into view */
        view[15] =  1.0f;
        mat4_mul(g_mvp_ortho_2d, proj, view);

        /* mask_scale: maps world (x,z) -> mask UV (0..1).
         * UV = world_coord * scale + 0.5 */
        glUniform2f(g_uni_mask_scale, 0.5f / g_mask_half_w, 0.5f / g_mask_half_h);
    }

    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    /* World-stable showroom lighting tuned to stay readable during camera motion. */
    float light_cos = cosf(g_light_rot);
    float light_sin = sinf(g_light_rot);
    float base_lx = k_lighting_state.key_dir[0];
    float base_lz = k_lighting_state.key_dir[2];
    float lx = light_cos * base_lx + light_sin * base_lz;
    float ly = k_lighting_state.key_dir[1];
    float lz = -light_sin * base_lx + light_cos * base_lz;
    float ll = sqrtf(lx*lx + ly*ly + lz*lz);
    g_light_vector[0]=lx/ll;
    g_light_vector[1]=ly/ll;
    g_light_vector[2]=lz/ll;
    glUniform3f(g_uni_light, lx/ll, ly/ll, lz/ll);
    glUniform3f(g_uni_light_key_color,
                k_lighting_state.key_color[0],
                k_lighting_state.key_color[1],
                k_lighting_state.key_color[2]);
    glUniform3f(g_uni_light_fill_color,
                k_lighting_state.fill_color[0],
                k_lighting_state.fill_color[1],
                k_lighting_state.fill_color[2]);
    glUniform3f(g_uni_light_sky_color,
                k_lighting_state.sky_color[0],
                k_lighting_state.sky_color[1],
                k_lighting_state.sky_color[2]);
    glUniform3f(g_uni_light_ground_color,
                k_lighting_state.ground_color[0],
                k_lighting_state.ground_color[1],
                k_lighting_state.ground_color[2]);
    glUniform3f(g_uni_light_spec_color,
                k_lighting_state.spec_color[0],
                k_lighting_state.spec_color[1],
                k_lighting_state.spec_color[2]);

    /* Camera eye position for specular/rim -- lerp between modes */
    glUniform3f(g_uni_eye,
                eye_x * ts + 0.0f * (1.0f - ts),
                eye_y * ts + 5.0f * (1.0f - ts),
                eye_z * ts + 0.0f * (1.0f - ts));

    g_z_bias = 0.0f;
}

void render_begin_frame(void) {
    double now=viewport_now();
    cr_rect_animate(&g_visible_area,now);
    cr_rect_animate(&g_content_offset,now);
    /* Render into 2x supersample FBO */
    glBindFramebuffer(GL_FRAMEBUFFER, g_ss_fbo);
    glViewport(0, 0, g_ss_w, g_ss_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    use_lit_program();
    render_set_mask_entry_fade(0,0);
    sync_camera_uniforms();
}

void render_set_global_alpha(float alpha) {
    g_global_alpha = alpha;
    if(g_active_pass==PASS_LIT)glUniform1f(g_uni_global_alpha,alpha);
    else if(g_active_pass==PASS_FLAT)glUniform1f(g_flat_alpha,alpha);
    else glUniform1f(g_shadow_alpha,alpha);
}

float render_get_global_alpha(void) {
    return g_global_alpha;
}

int render_set_visible_area(int x, int y, int w, int h) {
    int changed=cr_rect_retarget(&g_visible_area,cr_visible_area(x,y,w,h),viewport_now());
    return changed;
}

void render_get_visible_area(cr_rect_t *current, cr_rect_t *target) {
    if(current) *current=g_visible_area.current;
    if(target) *target=g_visible_area.target;
}

void render_set_content_framing(float x,float y,float dolly) {
    cr_rect_t offset={isfinite(x)?x:0,isfinite(y)?y:0,isfinite(dolly)?fminf(.18f,fmaxf(0,dolly)):0,
                      isfinite(dolly) && dolly>0?CR_LANE_PANEL_HEIGHT:0};
    cr_rect_retarget(&g_content_offset,offset,viewport_now());
}
void render_reset_content_offset(void) {memset(&g_content_offset,0,sizeof(g_content_offset));}
void render_get_content_framing(float *x,float *y,float *dolly) {
    if(x)*x=g_content_offset.current.x;
    if(y)*y=g_content_offset.current.y;
    if(dolly)*dolly=g_content_offset.current.w;
}

void render_begin_overlay(cr_rect_t clip) {
    float identity[16]={0};
    identity[0]=identity[5]=identity[10]=identity[15]=1;
    use_flat_program(3.0f);
    render_set_mask_entry_fade(0,0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glUniform1f(g_flat_zbias,0);
    glUniformMatrix4fv(g_flat_mvp,1,GL_FALSE,identity);
    /* Round inward: even during the frame animation nothing leaks past crop. */
    int left=(int)ceilf(clip.x*g_ss_w/CR_DEFAULT_WIDTH);
    int right=(int)floorf((clip.x+clip.w)*g_ss_w/CR_DEFAULT_WIDTH);
    int bottom=(int)ceilf((CR_DEFAULT_HEIGHT-clip.y-clip.h)*g_ss_h/CR_DEFAULT_HEIGHT);
    int top=(int)floorf((CR_DEFAULT_HEIGHT-clip.y)*g_ss_h/CR_DEFAULT_HEIGHT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(left,bottom,right>left?right-left:0,top>bottom?top-bottom:0);
}

void render_overlay_mesh(const float *xy,int count,float x,float y,
                         float r,float g,float b,float a) {
    int drawn=0;
    if(!xy || count<=0 || count%3 || a<=0) return;
    while(drawn<count) {
        int n=count-drawn;
        if(n>MAX_VERTS) n=MAX_VERTS-MAX_VERTS%3;
        vb_reset();
        for(int i=0;i<n;++i) {
            int j=2*(drawn+i);
            vb_v(2*(xy[j]+x)/CR_DEFAULT_WIDTH-1,
                 1-2*(xy[j+1]+y)/CR_DEFAULT_HEIGHT,0,0,0,1);
        }
        vb_flush(r,g,b,a);drawn+=n;
    }
}

void render_end_overlay(void) {
    glDisable(GL_SCISSOR_TEST);
    use_lit_program();
    glEnable(GL_DEPTH_TEST);
}

void render_overlay_cutout(cr_rect_t area,float fx,float fy,float alpha) {
    if(alpha<=0 || area.w<=0 || area.h<=0 || !g_cutout_program)return;
    float xy[]={area.x-fx,area.y-fy, area.x+area.w+fx,area.y-fy, area.x+area.w+fx,area.y+area.h,
                area.x-fx,area.y-fy, area.x+area.w+fx,area.y+area.h, area.x-fx,area.y+area.h};
    glBlendFuncSeparate(GL_ZERO,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_cutout_program);
    glUniform2f(g_cutout_size,CR_DEFAULT_WIDTH,CR_DEFAULT_HEIGHT);
    glUniform4f(g_cutout_rect,area.x,area.y,area.x+area.w,area.y+area.h);
    glUniform2f(g_cutout_feather,fx,fy);
    glUniform1f(g_cutout_opacity,alpha);
    glEnableVertexAttribArray(g_cutout_attr);
    glVertexAttribPointer(g_cutout_attr,2,GL_FLOAT,GL_FALSE,0,xy);
    glDrawArrays(GL_TRIANGLES,0,6);
    glDisableVertexAttribArray(g_cutout_attr);
    glUseProgram(g_active_pass==PASS_FLAT?g_flat_program:
                 g_active_pass==PASS_SHADOW?g_shadow_program:g_program);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
}

/* Debug grid: draw colored checkerboard over the full 328x180 content area.
 * Each cell is labeled by position so you can see which cells are visible
 * in sidescreen vs popup modes on the VC.
 * Enable with: ./maneuver_render --grid   or   CR_DEBUG_GRID=1 ./maneuver_render */
static int g_debug_grid = 0;

void render_set_debug_grid(int on) { g_debug_grid = on; }

static void debug_area(cr_rect_t r) {
    float identity[16]={0}, rects[4][4];
    int i;
    identity[0]=identity[5]=identity[10]=identity[15]=1;
    rects[0][0]=r.x;rects[0][1]=r.y;rects[0][2]=r.x+r.w;rects[0][3]=r.y+1;
    rects[1][0]=r.x;rects[1][1]=r.y+r.h-1;rects[1][2]=r.x+r.w;rects[1][3]=r.y+r.h;
    rects[2][0]=r.x;rects[2][1]=r.y;rects[2][2]=r.x+1;rects[2][3]=r.y+r.h;
    rects[3][0]=r.x+r.w-1;rects[3][1]=r.y;rects[3][2]=r.x+r.w;rects[3][3]=r.y+r.h;
    glDisable(GL_DEPTH_TEST);
    use_flat_program(3.0f);
    glUniformMatrix4fv(g_flat_mvp,1,GL_FALSE,identity);
    for(i=0;i<4;++i) {
        float x0=2*rects[i][0]/CR_DEFAULT_WIDTH-1,x1=2*rects[i][2]/CR_DEFAULT_WIDTH-1;
        float y0=1-2*rects[i][1]/CR_DEFAULT_HEIGHT,y1=1-2*rects[i][3]/CR_DEFAULT_HEIGHT;
        vb_reset();vb_quad(x0,y0,0,x1,y0,0,x1,y1,0,x0,y1,0,0,0,1);
        vb_flush(1,.04f,.08f,1);
    }
    use_lit_program();
    glEnable(GL_DEPTH_TEST);
}

void render_debug_visible_area(void) {debug_area(g_visible_area.current);}
void render_debug_small_area(void) {
    cr_rect_t small={CR_POPUP_X,CR_POPUP_Y,CR_POPUP_W,CR_POPUP_H};
    debug_area(small);
}

void render_debug_grid(void) {
    if (!g_debug_grid) return;

    /* Grid: 8 columns x 6 rows = 48 cells covering 328x180 content */
    const int COLS = 8, ROWS = 6;
    const float cell_w = 2.0f / COLS;  /* NDC width per cell */
    const float cell_h = 2.0f / ROWS;  /* NDC height per cell */
    float identity[16];
    int row, col;

    memset(identity, 0, sizeof(identity));
    identity[0] = identity[5] = identity[10] = identity[15] = 1.0f;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    use_flat_program(3.0f);
    glUniformMatrix4fv(g_flat_mvp, 1, GL_FALSE, identity);
    glUniform1f(g_flat_zbias, 0.0f);

    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            float x0 = -1.0f + col * cell_w;
            float y0 = -1.0f + row * cell_h;
            float x1 = x0 + cell_w;
            float y1 = y0 + cell_h;
            /* Inset slightly for grid lines */
            float m = 0.005f;
            float ix0 = x0 + m, iy0 = y0 + m, ix1 = x1 - m, iy1 = y1 - m;

            /* Checkerboard color */
            float r, g, b;
            int checker = (row + col) & 1;
            if (checker) { r = 0.0f; g = 0.7f; b = 0.7f; }  /* cyan */
            else         { r = 0.0f; g = 0.7f; b = 0.0f; }  /* green */
            /* Dim border cells */
            if (row == 0 || row == ROWS-1 || col == 0 || col == COLS-1) {
                r *= 0.5f; g *= 0.5f; b *= 0.5f;
            }
            /* Center cross: bright white */
            if (col == COLS/2-1 && row == ROWS/2-1) { r = 1; g = 1; b = 1; }
            if (col == COLS/2   && row == ROWS/2  ) { r = 1; g = 1; b = 1; }

            glUniform4f(g_flat_color, r, g, b, 0.6f);
            vb_reset();
            vb_v(ix0, iy0, 0, 0,0,1);
            vb_v(ix1, iy0, 0, 0,0,1);
            vb_v(ix1, iy1, 0, 0,0,1);
            vb_v(ix0, iy0, 0, 0,0,1);
            vb_v(ix1, iy1, 0, 0,0,1);
            vb_v(ix0, iy1, 0, 0,0,1);
            glVertexAttribPointer(g_flat_attr_pos, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf);
            glVertexAttribPointer(g_flat_attr_uv, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf + 3);
            glEnableVertexAttribArray(g_flat_attr_pos);
            glEnableVertexAttribArray(g_flat_attr_uv);
            glDrawArrays(GL_TRIANGLES, 0, g_vcount);
        }
    }


    glEnable(GL_DEPTH_TEST);
    use_lit_program();
}


void render_sync_camera(void) {
    use_lit_program();
    sync_camera_uniforms();
}

void render_end_frame(void) {
    static const float quad[] = { -1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1 };

    /* Pass 1: FXAA on SSAA FBO → FXAA FBO (same resolution, smoothed edges) */
#if FXAA_ENABLED
    if (g_fxaa_prog) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_fxaa_fbo);
        glViewport(0, 0, g_ss_w, g_ss_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(g_fxaa_prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_ss_tex);
        glUniform1i(g_fxaa_uni_tex, 0);
        glUniform2f(g_fxaa_uni_rcp, 1.0f / g_ss_w, 1.0f / g_ss_h);
        glVertexAttribPointer(g_fxaa_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, quad);
        glEnableVertexAttribArray(g_fxaa_attr_pos);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(g_fxaa_attr_pos);
    }
#endif

    /* Pass 2: Downsample (FXAA result on QNX / SSAA on macOS) → window */
    glBindFramebuffer(GL_FRAMEBUFFER, g_default_fbo);
    glViewport(0, 0, g_win_w, g_win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    use_flat_program(1.0f);  /* fullscreen blit passthrough */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (FXAA_ENABLED && g_fxaa_prog) ? g_fxaa_tex : g_ss_tex);
    glUniform1i(g_flat_tex, 0);
    vb_reset();
    vb_v(-1, -1, 0, 0,0,1);
    vb_v( 1, -1, 0, 0,0,1);
    vb_v( 1,  1, 0, 0,0,1);
    vb_v(-1, -1, 0, 0,0,1);
    vb_v( 1,  1, 0, 0,0,1);
    vb_v(-1,  1, 0, 0,0,1);
    vb_flush(1, 1, 1, 1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    use_lit_program();
}

void render_set_perspective(int enabled) {
    g_perspective = enabled;
}

int render_is_animating(void) {
    return fabsf(g_persp_t - (float)g_perspective) > 0.001f || g_visible_area.active || g_content_offset.active;
}

void render_set_raised(int raised) {
    g_raised = raised;
}

void render_set_camera_pan(float x, float y) {
    g_cam_pan_x = x;
    g_cam_pan_z = y;  /* maneuver y -> 3D z */
}
void render_get_camera_pose(float *x,float *y,float *rotation) {
    *x=g_cam_pan_x;*y=g_cam_pan_z;*rotation=g_cam_rot;
}

void render_set_camera_rotation(float angle_rad) {
    g_cam_rot = angle_rad;
}

void render_set_light_rotation(float angle_rad) {
    g_light_rot = angle_rad;
}

void render_set_material(render_material_t material) {
    g_active_material = material;
}

void render_invalidate_masks(void) {
    g_masks_dirty = 1;
    g_route_mask_ready = 0;
}

int render_masks_dirty(void) {
    return g_masks_dirty;
}

/* ================================================================
 * Flag sprite atlas
 * ================================================================ */

static GLuint g_flag_tex = 0;
static int g_flag_frame_count = 0;

int render_load_flag_atlas(const char *path, int frame_w, int frame_h, int frame_count) {
    int atlas_w = frame_w * frame_count;
    int atlas_h = frame_h;
    int total_bytes = atlas_w * atlas_h * 4;

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "render: flag atlas open failed: %s\n", path);
        return -1;
    }

    unsigned char *data = (unsigned char *)malloc(total_bytes);
    if (!data) { fclose(f); return -1; }

    int read = (int)fread(data, 1, total_bytes, f);
    fclose(f);
    if (read != total_bytes) {
        fprintf(stderr, "render: flag atlas short read %d/%d\n", read, total_bytes);
        free(data);
        return -1;
    }

    glGenTextures(1, &g_flag_tex);
    glBindTexture(GL_TEXTURE_2D, g_flag_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_w, atlas_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(data);
    g_flag_frame_count = frame_count;

    GLenum err = glGetError();
    fprintf(stderr, "render: flag atlas loaded %dx%d (%d frames) tex=%u gl_err=0x%x\n",
            atlas_w, atlas_h, frame_count, g_flag_tex, err);
    return 0;
}

int render_get_flag_frame_count(void) {
    return g_flag_frame_count;
}

void render_sprite_flag_ex(float x, float y, float size, int frame, int flip_x) {
    /* Camera-facing billboard quad anchored at pole base.
     * Pole base in sprite UV: u=0.22, v=0.88 (from top).
     * Quad = full sprite (size*2 x size*2), offset so pole base = (x, 0, y).
     * The handoff camera may rotate 90-180 degrees before ARRIVED commits.
     * Rotate the quad with that yaw so it never becomes edge-on and disappears. */
    float sprite_w = size * 2.0f;
    float sprite_h = size * 2.0f;
    float pole_u = 0.22f;     /* pole base X fraction in sprite */
    float pole_v_top = 0.88f; /* pole base Y fraction from top */
    float anchor_u = flip_x ? (1.0f - pole_u) : pole_u;
    float local_l = -anchor_u * sprite_w;
    float local_r = local_l + sprite_w;
    float right_x = cosf(g_cam_rot);
    float right_z = sinf(g_cam_rot);
    float up_x = -right_z;
    float up_z = right_x;

    /* 3D (perspective): vertical -- rises in world Y at fixed z.
     * 2D (ortho):       horizontal -- lies flat, extends in +z at ground level.
     * g_persp_t: 1.0 = perspective (vertical), 0.0 = ortho (horizontal).
     * Apply same quintic ease-in-out (smootherstep) as camera. */
    float t = g_persp_t;
    t = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    float flat_h = 0.07f;     /* small height above ground in 2D mode */

    /* Bottom edge: stays at pole base position, lerp y only */
    float by_3d = -(1.0f - pole_v_top) * sprite_h;
    float by_2d = flat_h;
    float bot_y = by_3d * t + by_2d * (1.0f - t);
    /* Top edge: in 3D rises in world Y; in 2D it extends along the
     * camera-relative screen-up axis on the road plane. */
    float ty_3d = by_3d + sprite_h;
    float ty_2d = flat_h;
    float top_y = ty_3d * t + ty_2d * (1.0f - t);
    float flat_top = sprite_h * (1.0f - t);

    float lx0 = x + right_x * local_l;
    float lz0 = y + right_z * local_l;
    float rx0 = x + right_x * local_r;
    float rz0 = y + right_z * local_r;
    float lx1 = lx0 + up_x * flat_top;
    float lz1 = lz0 + up_z * flat_top;
    float rx1 = rx0 + up_x * flat_top;
    float rz1 = rz0 + up_z * flat_top;

    glDisable(GL_DEPTH_TEST);
    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    if (g_flag_tex && g_flag_frame_count > 0) {
        if (frame < 0) frame = 0;
        if (frame >= g_flag_frame_count) frame = g_flag_frame_count - 1;

        float u0 = (float)frame / (float)g_flag_frame_count;
        float u1 = (float)(frame + 1) / (float)g_flag_frame_count;
        float ul = flip_x ? u1 : u0;
        float ur = flip_x ? u0 : u1;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_flag_tex);
        use_flat_program(4.0f);
        glUniformMatrix4fv(g_flat_mvp,1,GL_FALSE,g_mvp_current);
        glUniform1f(g_flat_zbias,0.0f);
        glUniform1i(g_flat_tex,0);

        vb_reset();
        vb_v(lx0, bot_y, lz0,  ul, 1.0f, 0);
        vb_v(rx0, bot_y, rz0,  ur, 1.0f, 0);
        vb_v(rx1, top_y, rz1,  ur, 0.0f, 0);
        vb_v(lx0, bot_y, lz0,  ul, 1.0f, 0);
        vb_v(rx1, top_y, rz1,  ur, 0.0f, 0);
        vb_v(lx1, top_y, lz1,  ul, 0.0f, 0);

        glVertexAttribPointer(g_flat_attr_pos,3,GL_FLOAT,GL_FALSE,24,g_vbuf);
        glVertexAttribPointer(g_flat_attr_uv,3,GL_FLOAT,GL_FALSE,24,g_vbuf+3);
        glEnableVertexAttribArray(g_flat_attr_pos);
        glEnableVertexAttribArray(g_flat_attr_uv);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        use_lit_program();
    } else {
        /* Fallback: magenta quad when atlas not loaded */
        vb_reset();
        vb_v(lx0, bot_y, lz0,  0,0,1);
        vb_v(rx0, bot_y, rz0,  0,0,1);
        vb_v(rx1, top_y, rz1,  0,0,1);
        vb_v(lx0, bot_y, lz0,  0,0,1);
        vb_v(rx1, top_y, rz1,  0,0,1);
        vb_v(lx1, top_y, lz1,  0,0,1);
        render_set_material(RENDER_MAT_GENERIC_SOLID);
        vb_flush(1.0f, 0.0f, 1.0f, 1.0f);
    }

    glEnable(GL_DEPTH_TEST);
}

void render_sprite_flag(float x, float y, float size, int frame) {
    render_sprite_flag_ex(x, y, size, frame, 0);
}

void render_shutdown(void) {
    fbos_shutdown();
    /* SSAA FBO */
    if (g_ss_fbo) { glDeleteFramebuffers(1, &g_ss_fbo); g_ss_fbo = 0; }
    if (g_ss_tex) { glDeleteTextures(1, &g_ss_tex); g_ss_tex = 0; }
    if (g_ss_depth) { glDeleteRenderbuffers(1, &g_ss_depth); g_ss_depth = 0; }
    /* FXAA FBO + program */
    if (g_fxaa_fbo) { glDeleteFramebuffers(1, &g_fxaa_fbo); g_fxaa_fbo = 0; }
    if (g_fxaa_tex) { glDeleteTextures(1, &g_fxaa_tex); g_fxaa_tex = 0; }
    if (g_fxaa_prog) { glDeleteProgram(g_fxaa_prog); g_fxaa_prog = 0; }
    if (g_cutout_program) { glDeleteProgram(g_cutout_program); g_cutout_program = 0; }
    if (g_flat_program) {glDeleteProgram(g_flat_program);g_flat_program=0;}
    if (g_shadow_program) {glDeleteProgram(g_shadow_program);g_shadow_program=0;}
    if (g_flag_tex) {
        glDeleteTextures(1, &g_flag_tex);
        g_flag_tex = 0;
    }
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
}

/* ================================================================
 * Mask rendering -- begin/end for each mask layer
 *
 * Sets up: FBO bound, flat 2D ortho MVP, tex_mode=3 (flat color),
 * no depth test, no blending, overwrite mode.
 * ================================================================ */

static void begin_mask(int fbo_idx) {
    fbo_bind(fbo_idx);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    use_flat_program(3.0f);
    glUniformMatrix4fv(g_flat_mvp,1,GL_FALSE,g_mvp_ortho_2d);
    g_z_bias = 0.0f;
}

static void end_mask(void) {
    fbo_unbind();
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    use_lit_program();
    glUniformMatrix4fv(g_uni_mvp,1,GL_FALSE,g_mvp_current);
}

/* Apply a 2D rigid transform to the mask MVP (for rendering a second maneuver).
 * Modifies g_mvp_ortho_2d so that begin_mask picks up the transform.
 * Maneuver (x,y) -> rotated by (cos_r,sin_r) then translated by (tx,ty).
 * In 3D: maneuver x->world x, maneuver y->world z. */
static float g_mvp_ortho_2d_saved[16];

void render_push_mask_transform(float tx, float ty, float cos_r, float sin_r) {
    /* Save original */
    memcpy(g_mvp_ortho_2d_saved, g_mvp_ortho_2d, sizeof(g_mvp_ortho_2d));

    /* Build 4x4 model matrix: rotate in xz-plane + translate */
    float model[16];
    mat4_zero(model);
    model[0]  =  cos_r;   /* x' = cos*x - sin*z */
    model[8]  = -sin_r;
    model[2]  =  sin_r;   /* z' = sin*x + cos*z */
    model[10] =  cos_r;
    model[5]  =  1.0f;    /* y unchanged */
    model[12] =  tx;      /* translate x */
    model[14] =  ty;      /* translate z (maneuver y) */
    model[15] =  1.0f;
    /* New ortho MVP = saved * model */
    float mvp[16];
    mat4_mul(mvp, g_mvp_ortho_2d_saved, model);
    memcpy(g_mvp_ortho_2d, mvp, sizeof(g_mvp_ortho_2d));
}

void render_pop_mask_transform(void) {
    memcpy(g_mvp_ortho_2d, g_mvp_ortho_2d_saved, sizeof(g_mvp_ortho_2d));
}

void render_begin_outline_mask(void) { begin_mask(FBO_ROAD); }
void render_end_outline_mask(void)   { end_mask(); }

void render_begin_route_mask(void) {
    g_route_mask_ready = 1;
    begin_mask(FBO_ROUTE);
}

/* ================================================================
 * Composite pipeline -- single-FBO painter's algorithm
 *
 * FBO_ROAD contains white outline + grey fill (painter's algorithm).
 * tex_mode 7 reads FBO RGB as diffuse base color for lighting.
 * No subtraction needed -- border/fill distinction is baked into FBO colors.
 * ================================================================ */

/* The road FBO supplies fill and border RGB for one lit ground-plane quad. */
static void composite_road_layer(void) {
    const material_preset_t *preset = material_preset(RENDER_MAT_ROAD_ASPHALT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fbo_texs[FBO_ROAD]);
    glUniform1i(g_uni_tex, 0);
    glUniform1f(g_uni_tex_mode, 7.0f);

    apply_material(preset,
                   preset->base_color[0], preset->base_color[1],
                   preset->base_color[2], preset->base_color[3]);
    glUniform1f(g_uni_zbias, g_z_bias);
    g_z_bias += Z_BIAS_STEP;

    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    float gx = g_mask_half_w * 3.0f, gz = g_mask_half_h * 3.0f;

    vb_reset();
    vb_v(-gx, 0.0f, -gz,  0, 1, 0);
    vb_v( gx, 0.0f, -gz,  0, 1, 0);
    vb_v( gx, 0.0f,  gz,  0, 1, 0);
    vb_v(-gx, 0.0f, -gz,  0, 1, 0);
    vb_v( gx, 0.0f,  gz,  0, 1, 0);
    vb_v(-gx, 0.0f,  gz,  0, 1, 0);

    glVertexAttribPointer(g_attr_pos,  3, GL_FLOAT, GL_FALSE, 24, g_vbuf);
    glVertexAttribPointer(g_attr_norm, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf + 3);
    glEnableVertexAttribArray(g_attr_pos);
    glEnableVertexAttribArray(g_attr_norm);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_composite(void) {
    /* Composite to screen as 3D quads with materials + perspective */
    use_lit_program();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    /* RGB is associated as it enters the transparent framebuffer. Alpha must
     * use source-over too, not SRC_ALPHA (which incorrectly squares it). */
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    /* Layer 1: Road (combined outline+fill via painter's algorithm).
     * tex_mode 7 reads FBO RGB as base color -- white border gets paint-like shading,
     * grey fill gets asphalt-like shading, all with one material preset. */
    composite_road_layer();

    /* Layer 2: Route shadow on road surface. Only sample the route mask when it
     * was actually rendered; otherwise skip the pass instead of reading an
     * uninitialized texture. */
    if (g_route_mask_ready) {
        float gx = g_mask_half_w * 3.0f, gz = g_mask_half_h * 3.0f;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_fbo_texs[FBO_ROUTE]);
        use_shadow_program(9.0f);
        glUniform1f(g_shadow_zbias, g_z_bias);
        g_z_bias += Z_BIAS_STEP;

        vb_reset();
        vb_v(-gx, 0.0005f, -gz, 0,1,0);
        vb_v( gx, 0.0005f, -gz, 0,1,0);
        vb_v( gx, 0.0005f,  gz, 0,1,0);
        vb_v(-gx, 0.0005f, -gz, 0,1,0);
        vb_v( gx, 0.0005f,  gz, 0,1,0);
        vb_v(-gx, 0.0005f,  gz, 0,1,0);
        glVertexAttribPointer(g_shadow_attr_pos,3,GL_FLOAT,GL_FALSE,24,g_vbuf);
        glVertexAttribPointer(g_shadow_attr_norm,3,GL_FLOAT,GL_FALSE,24,g_vbuf+3);
        glEnableVertexAttribArray(g_shadow_attr_pos);
        glEnableVertexAttribArray(g_shadow_attr_norm);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    use_lit_program();

    g_masks_dirty = 0;
}

void render_reset_depth(void) {
    glClear(GL_DEPTH_BUFFER_BIT);
    g_z_bias = 0.0f;
}

/* ================================================================
 * 3D Primitives -- extrude 2D shapes into boxes/prisms
 *
 * All functions take 2D coordinates (x, y_2d).
 * In mask mode (tex_mode=3): flat 2D, y_2d maps to 3D z, y=0.
 * In normal mode (tex_mode=0): extruded 3D.
 * ================================================================ */

static float cur_base(void) { return g_raised ? RAISE_BASE : 0.0f; }
static float cur_top(void)  { return g_raised ? RAISE_BASE + EXTRUDE_H : 0.0f; }

void render_thick_line(float x0, float y0, float x1, float y1, float thickness,
                       float r, float g, float b, float a) {
    float z0 = y0, z1 = y1;          /* 2D y -> 3D z */
    float bt = cur_base(), tp = cur_top();
    float dx = x1 - x0, dz = z1 - z0;
    float len = sqrtf(dx*dx + dz*dz);
    if (len < 1e-6f) return;

    float t2 = thickness * 0.5f;
    float nx = (-dz / len) * t2;     /* perpendicular offset */
    float nz = ( dx / len) * t2;

    /* 4 corners */
    float ax = x0+nx, az = z0+nz;    /* left-near */
    float bx = x0-nx, bz = z0-nz;    /* right-near */
    float cx = x1-nx, cz = z1-nz;    /* right-far */
    float ex = x1+nx, ez = z1+nz;    /* left-far */

    float snx = nx / t2, snz = nz / t2;    /* side normal (unit) */
    float fdx = dx / len, fdz = dz / len;  /* forward normal (unit) */

    vb_reset();
    vb_quad(ax,tp,az, bx,tp,bz, cx,tp,cz, ex,tp,ez,  0,1,0);                  /* top */
    vb_quad(ax,bt,az, ex,bt,ez, ex,tp,ez, ax,tp,az,    snx,0,snz);             /* left side */
    vb_quad(bx,tp,bz, cx,tp,cz, cx,bt,cz, bx,bt,bz,  -snx,0,-snz);           /* right side */
    vb_quad(ax,tp,az, ax,bt,az, bx,bt,bz, bx,tp,bz,   -fdx,0,-fdz);          /* near cap */
    vb_quad(ex,tp,ez, cx,tp,cz, cx,bt,cz, ex,bt,ez,    fdx,0,fdz);            /* far cap */
    vb_flush(r, g, b, a);
}

void render_triangle(float x0, float y0, float x1, float y1, float x2, float y2,
                     float r, float g, float b, float a) {
    float bt = cur_base(), tp = cur_top();
    float z0 = y0, z1 = y1, z2 = y2;

    /* 3 edges for side faces */
    float ex[3][4];
    ex[0][0] = x0; ex[0][1] = z0; ex[0][2] = x1; ex[0][3] = z1;
    ex[1][0] = x1; ex[1][1] = z1; ex[1][2] = x2; ex[1][3] = z2;
    ex[2][0] = x2; ex[2][1] = z2; ex[2][2] = x0; ex[2][3] = z0;

    int i;
    vb_reset();

    /* Top face */
    vb_v(x0,tp,z0, 0,1,0);  vb_v(x1,tp,z1, 0,1,0);  vb_v(x2,tp,z2, 0,1,0);

    /* 3 side faces */
    for (i = 0; i < 3; i++) {
        float edx = ex[i][2] - ex[i][0], edz = ex[i][3] - ex[i][1];
        float el = sqrtf(edx*edx + edz*edz);
        if (el < 1e-6f) continue;
        float enx = -edz / el, enz = edx / el;
        vb_quad(ex[i][0],tp,ex[i][1], ex[i][2],tp,ex[i][3],
                ex[i][2],bt,ex[i][3], ex[i][0],bt,ex[i][1],  enx,0,enz);
    }
    vb_flush(r, g, b, a);
}

void render_disc(float cx, float cy, float radius, int segments,
                 float r, float g, float b, float a) {
    if (segments < 3) segments = 3;
    if (segments > 64) segments = 64;

    float bt = cur_base(), tp = cur_top();
    float cz = cy;   /* 2D y -> 3D z */
    int i;

    vb_reset();

    /* Top face -- individual triangles */
    for (i = 0; i < segments; i++) {
        float a0 = 2.0f * (float)M_PI * i / segments;
        float a1 = 2.0f * (float)M_PI * (i + 1) / segments;
        float px0 = cx + radius * cosf(a0), pz0 = cz + radius * sinf(a0);
        float px1 = cx + radius * cosf(a1), pz1 = cz + radius * sinf(a1);
        vb_v(cx,tp,cz, 0,1,0);  vb_v(px0,tp,pz0, 0,1,0);  vb_v(px1,tp,pz1, 0,1,0);
    }

    /* Side faces */
    for (i = 0; i < segments; i++) {
        float a0 = 2.0f * (float)M_PI * i / segments;
        float a1 = 2.0f * (float)M_PI * (i + 1) / segments;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);
        float px0 = cx + radius * c0, pz0 = cz + radius * s0;
        float px1 = cx + radius * c1, pz1 = cz + radius * s1;
        float anx = (c0 + c1) * 0.5f, anz = (s0 + s1) * 0.5f;
        float al = sqrtf(anx*anx + anz*anz);
        if (al > 0.001f) { anx /= al; anz /= al; }

        vb_v(px0,tp,pz0, anx,0,anz);  vb_v(px1,tp,pz1, anx,0,anz);  vb_v(px1,bt,pz1, anx,0,anz);
        vb_v(px0,tp,pz0, anx,0,anz);  vb_v(px1,bt,pz1, anx,0,anz);  vb_v(px0,bt,pz0, anx,0,anz);
    }

    vb_flush(r, g, b, a);
}

void render_arc(float cx, float cy, float radius, float thickness,
                float start_rad, float end_rad, int segments,
                float r, float g, float b, float a) {
    if (segments < 1) segments = 1;
    if (segments > 64) segments = 64;

    float ri = radius - thickness * 0.5f;
    float ro = radius + thickness * 0.5f;
    float bt = cur_base(), tp = cur_top();
    float cz = cy;   /* 2D y -> 3D z */
    int i;

    vb_reset();

    for (i = 0; i < segments; i++) {
        float ang0 = start_rad + (end_rad - start_rad) * i / segments;
        float ang1 = start_rad + (end_rad - start_rad) * (i + 1) / segments;
        float c0 = cosf(ang0), s0 = sinf(ang0);
        float c1 = cosf(ang1), s1 = sinf(ang1);

        float ix0 = cx + ri * c0, iz0 = cz + ri * s0;
        float ox0 = cx + ro * c0, oz0 = cz + ro * s0;
        float ix1 = cx + ri * c1, iz1 = cz + ri * s1;
        float ox1 = cx + ro * c1, oz1 = cz + ro * s1;

        /* Top face */
        vb_quad(ix0,tp,iz0, ox0,tp,oz0, ox1,tp,oz1, ix1,tp,iz1,  0,1,0);

        /* Outer side */
        float onx = (c0 + c1) * 0.5f, onz = (s0 + s1) * 0.5f;
        float ol = sqrtf(onx*onx + onz*onz);
        if (ol > 0.001f) { onx /= ol; onz /= ol; }
        vb_quad(ox0,tp,oz0, ox0,bt,oz0, ox1,bt,oz1, ox1,tp,oz1,   onx,0,onz);

        /* Inner side */
        vb_quad(ix1,tp,iz1, ix1,bt,iz1, ix0,bt,iz0, ix0,tp,iz0,  -onx,0,-onz);
    }

    vb_flush(r, g, b, a);
}

void render_circle(float cx, float cy, float radius, float thickness, int segments,
                   float r, float g, float b, float a) {
    render_arc(cx, cy, radius, thickness, 0, 2.0f * (float)M_PI, segments, r, g, b, a);
}

void render_rect(float x, float y, float w, float h_rect,
                 float r, float g, float b, float a) {
    float bt = cur_base(), tp = cur_top();
    float z0 = y, z1 = y + h_rect;
    float x0 = x, x1 = x + w;

    vb_reset();
    vb_quad(x0,tp,z0, x1,tp,z0, x1,tp,z1, x0,tp,z1,   0,1,0);        /* top */
    vb_quad(x0,tp,z0, x0,bt,z0, x1,bt,z0, x1,tp,z0,    0,0,-1);       /* near */
    vb_quad(x1,tp,z1, x1,bt,z1, x0,bt,z1, x0,tp,z1,    0,0,1);        /* far */
    vb_quad(x0,tp,z1, x0,bt,z1, x0,bt,z0, x0,tp,z0,   -1,0,0);        /* left */
    vb_quad(x1,tp,z0, x1,bt,z0, x1,bt,z1, x1,tp,z1,    1,0,0);        /* right */
    vb_flush(r, g, b, a);
}
