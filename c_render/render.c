/*
 * OpenGL rendering — multi-layer mask-based architecture.
 *
 * 3 mask FBOs (flat 2D, no lighting):
 *   FBO_OUTLINE — white road borders
 *   FBO_FILL    — grey road fill
 *   FBO_ROUTE   — blue active route
 *   FBO_COMPOSITE — scratch for subtraction result
 *
 * Compositing pipeline:
 *   1. Subtract fill from outline → border-only ring in FBO_COMPOSITE
 *   2. Render 3D ground-plane quads textured with each mask, with perspective + lighting
 *   3. Layer order: fill (ground) → outline border → route (raised)
 *
 * All maneuver coordinates remain 2D (x, y). Internally mapped to 3D:
 *   2D x → 3D x (left-right)
 *   2D y → 3D z (depth: y=-0.55 near camera, y=0.55 far)
 *   3D y  = extrusion height (0=ground, H=top)
 *
 * Uses GLES2-compatible subset (shaders, no fixed-function).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gl_compat.h"
#include "render.h"
#include "maneuver.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * 3D Configuration
 * ================================================================ */

#define EXTRUDE_H    0.03f     /* active extrusion height */
#define RAISE_BASE   0.03f    /* active floats this much above grey */
#define Z_BIAS_STEP  0.00001f  /* depth bias per draw (layer ordering) */

/* Camera — perspective mode */
#define CAM_EYE_X    0.0f
#define CAM_EYE_Y    0.95f
#define CAM_EYE_Z   -1.40f
#define CAM_CTR_X    0.0f
#define CAM_CTR_Y   -0.50f
#define CAM_CTR_Z    0.25f
#define CAM_FOV_DEG  40.0f

/* Composite ground plane extents (world space) */
#define ROUTE_Y   0.04f   /* raised height for route layer */

typedef struct {
    float base_color[4];
    float surface[4];  /* ambient floor, diffuse strength, spec strength, spec power */
    float fx[4];       /* fresnel strength, fresnel power, clearcoat strength, clearcoat power */
    float grain[2];    /* grain strength, grain scale */
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
 * Shader program — 3D with directional lighting
 *
 * tex_mode values:
 *   0.0 = normal 3D geometry (MVP transform + lighting)
 *   1.0 = fullscreen blit (clip-space passthrough, sample texture)
 *   2.0 = lit 3D blit (MVP transform + lighting, sample mask texture)
 *   3.0 = flat mask (MVP transform, flat color, no lighting)
 * ================================================================ */

static GLuint g_program = 0;
static GLint  g_attr_pos   = -1;
static GLint  g_attr_norm  = -1;
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
static GLint  g_uni_grain  = -1;
static GLint  g_uni_tex_mode = -1;
static GLint  g_uni_tex      = -1;
static GLint  g_uni_resolution = -1;
static GLint  g_uni_mask_scale = -1;  /* vec2: 1/(2*hw), 1/(2*hh) for mask UV */

static int   g_perspective = 1;   /* target: 0=ortho, 1=perspective */
static float g_persp_t = 1.0f;   /* animated blend: 0.0=ortho, 1.0=perspective */
#define PERSP_ANIM_SPEED 0.033f   /* per-frame step (~1s at 30fps) */
static int g_raised = 1;
static int g_fb_w = 640, g_fb_h = 400;
static float g_z_bias = 0.0f;

/* Mask cache dirty flag */
static int g_masks_dirty = 1;
static int g_mask_append = 0;  /* 1 = don't clear FBO on begin_mask (additive) */

/* Camera pan offset (maneuver space) — shifts entire scene to follow arrow */
static float g_cam_pan_x = 0.0f;
static float g_cam_pan_z = 0.0f;  /* z in 3D = y in maneuver 2D */
static float g_cam_rot = 0.0f;
static render_material_t g_active_material = RENDER_MAT_GENERIC_SOLID;

/* Stored MVPs for composite pass */
static float g_mvp_current[16];    /* perspective-blended MVP (for 3D composite) */
static float g_mvp_ortho_2d[16];   /* pure orthographic for mask rendering */

/* ================================================================
 * Multi-FBO system
 * ================================================================ */

enum { FBO_OUTLINE = 0, FBO_FILL = 1, FBO_ROUTE = 2, FBO_COMPOSITE = 3, FBO_COUNT = 4 };

static GLuint g_fbos[FBO_COUNT];
static GLuint g_fbo_texs[FBO_COUNT];
static GLuint g_fbo_depths[FBO_COUNT];
static int g_fbo_w = 0, g_fbo_h = 0;
static GLint g_default_fbo = 0;  /* saved at init — may not be 0 on macOS */
static float g_mask_half_w = 1.6f;
static float g_mask_half_h = 1.0f;

static const material_preset_t k_material_presets[RENDER_MAT_COUNT] = {
    [RENDER_MAT_GENERIC_SOLID] = {
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.20f, 0.56f, 0.06f, 10.0f },
        { 0.02f, 3.5f, 0.0f, 1.0f },
        { 0.0f, 0.0f }
    },
    [RENDER_MAT_ROAD_ASPHALT] = {
        { 0.24f, 0.26f, 0.29f, 1.0f },
        { 0.22f, 0.46f, 0.10f, 9.0f },
        { 0.03f, 4.5f, 0.0f, 1.0f },
        { 0.55f, 68.0f }
    },
    [RENDER_MAT_ROAD_BORDER_PAINT] = {
        { 0.83f, 0.84f, 0.86f, 1.0f },
        { 0.28f, 0.40f, 0.14f, 18.0f },
        { 0.04f, 4.0f, 0.02f, 42.0f },
        { 0.08f, 92.0f }
    },
    [RENDER_MAT_ROUTE_ACTIVE] = {
        { 0.10f, 0.49f, 0.84f, 1.0f },
        { 0.12f, 0.68f, 0.24f, 26.0f },
        { 0.10f, 5.2f, 0.12f, 72.0f },
        { 0.02f, 96.0f }
    }
};

static const lighting_state_t k_lighting_state = {
    { 0.18f, 0.97f, -0.14f },
    { 0.78f, 0.74f, 0.68f },
    { 0.06f, 0.09f, 0.12f },
    { 0.08f, 0.11f, 0.17f },
    { 0.08f, 0.06f, 0.04f },
    { 0.78f, 0.76f, 0.74f }
};

static const char *k_vert_src =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform float u_z_bias;\n"
    "uniform float u_tex_mode;\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_world_pos;\n"
    "void main() {\n"
    "  if (u_tex_mode > 0.5 && u_tex_mode < 1.5) {\n"
    "    gl_Position = vec4(a_pos.xy, 0.0, 1.0);\n"
    "  } else {\n"
    "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "    gl_Position.z -= u_z_bias;\n"
    "  }\n"
    "  v_normal = a_normal;\n"
    "  v_world_pos = a_pos;\n"
    "}\n";

static const char *k_frag_src_body =
    "uniform vec4 u_color;\n"
    "uniform vec3 u_light_dir;\n"
    "uniform vec3 u_light_key_color;\n"
    "uniform vec3 u_light_fill_color;\n"
    "uniform vec3 u_light_sky_color;\n"
    "uniform vec3 u_light_ground_color;\n"
    "uniform vec3 u_light_spec_color;\n"
    "uniform vec3 u_eye;\n"
    "uniform vec4 u_mat_surface;\n"
    "uniform vec4 u_mat_fx;\n"
    "uniform vec2 u_grain;\n"
    "uniform float u_tex_mode;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec2 u_mask_scale;\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_world_pos;\n"
    "float hash21(vec2 p) {\n"
    "  p = fract(p * vec2(233.34, 851.73));\n"
    "  p += dot(p, p + 23.45);\n"
    "  return fract(p.x * p.y);\n"
    "}\n"
    "float vnoise(vec2 p) {\n"
    "  vec2 i = floor(p);\n"
    "  vec2 f = fract(p);\n"
    "  f = f * f * (3.0 - 2.0 * f);\n"
    "  float a = hash21(i);\n"
    "  float b = hash21(i + vec2(1.0, 0.0));\n"
    "  float c = hash21(i + vec2(0.0, 1.0));\n"
    "  float d = hash21(i + vec2(1.0, 1.0));\n"
    "  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
    "}\n"
    "vec3 tone_map(vec3 x) {\n"
    "  x *= 0.82;\n"
    "  x = max(x - 0.004, 0.0);\n"
    "  return clamp((x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06), 0.0, 1.0);\n"
    "}\n"
    "vec4 shade_surface(vec4 base, float alpha) {\n"
    "  vec3 N = normalize(v_normal);\n"
    "  vec3 L = normalize(u_light_dir);\n"
    "  vec3 fill_dir = normalize(vec3(-L.x * 0.55, 0.45, -L.z * 0.55));\n"
    "  vec3 V = normalize(u_eye - v_world_pos);\n"
    "  float grain = 0.0;\n"
    "  if (u_grain.x > 0.0) {\n"
    "    vec2 guv = v_world_pos.xz * u_grain.y;\n"
    "    float n1 = vnoise(guv);\n"
    "    float n2 = vnoise(guv * 3.7 + 17.0);\n"
    "    grain = (n1 * 0.6 + n2 * 0.4) - 0.5;\n"
    "    float bump = grain * u_grain.x * 0.24;\n"
    "    N = normalize(N + vec3(bump, 0.0, bump));\n"
    "  }\n"
    "  float key_n = max(dot(N, L), 0.0);\n"
    "  float fill_n = max(dot(N, fill_dir), 0.0);\n"
    "  float hemi_t = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);\n"
    "  vec3 hemi = mix(u_light_ground_color, u_light_sky_color, hemi_t);\n"
    "  vec3 diffuse_light = hemi;\n"
    "  diffuse_light += u_light_key_color * (0.45 * u_mat_surface.x + u_mat_surface.y * key_n);\n"
    "  diffuse_light += u_light_fill_color * fill_n * (0.08 + 0.18 * u_mat_surface.y);\n"
    "  vec3 H = normalize(L + V);\n"
    "  float ndotv = max(dot(N, V), 0.0);\n"
    "  float fres = pow(1.0 - ndotv, u_mat_fx.y);\n"
    "  float spec_lobe = pow(max(dot(N, H), 0.0), u_mat_surface.w);\n"
    "  float coat_lobe = pow(max(dot(N, H), 0.0), u_mat_fx.w);\n"
    "  float spec = u_mat_surface.z * spec_lobe * (0.18 + 0.42 * fres);\n"
    "  float clearcoat = u_mat_fx.z * coat_lobe * (0.18 + 0.52 * key_n);\n"
    "  vec3 color = base.rgb * diffuse_light;\n"
    "  color += u_light_spec_color * (spec + clearcoat);\n"
    "  color += base.rgb * (u_mat_fx.x * fres);\n"
    "  if (u_grain.x > 0.0) {\n"
    "    color += color * grain * u_grain.x * 0.10;\n"
    "  }\n"
    "  color = tone_map(color);\n"
    "  return vec4(color, alpha * base.a);\n"
    "}\n"
    "void main() {\n"
    /* tex_mode 1: fullscreen blit — passthrough texture sample */
    "  if (u_tex_mode > 0.5 && u_tex_mode < 1.5) {\n"
    "    vec2 uv = v_world_pos.xy * 0.5 + 0.5;\n"
    "    gl_FragColor = texture2D(u_tex, uv);\n"
    "    return;\n"
    "  }\n"
    /* tex_mode 4: sprite blit — UVs piggybacked on normal attribute */
    "  if (u_tex_mode > 3.5 && u_tex_mode < 4.5) {\n"
    "    vec2 uv = v_normal.xy;\n"
    "    gl_FragColor = texture2D(u_tex, uv);\n"
    "    return;\n"
    "  }\n"
    /* tex_mode 3: flat mask — flat color, no lighting */
    "  if (u_tex_mode > 2.5 && u_tex_mode < 3.5) {\n"
    "    gl_FragColor = u_color;\n"
    "    return;\n"
    "  }\n"
    /* tex_mode 2: lit 3D blit — sample mask texture, apply lighting.
     * UV from world position: maps (x,z) through ortho projection to mask texture coords. */
    "  if (u_tex_mode > 1.5 && u_tex_mode < 2.5) {\n"
    "    vec2 uv = vec2(v_world_pos.x * u_mask_scale.x + 0.5,\n"
    "                    v_world_pos.z * u_mask_scale.y + 0.5);\n"
    "    vec4 mask = texture2D(u_tex, uv);\n"
    "    if (mask.a < 0.01) discard;\n"
    "    gl_FragColor = shade_surface(u_color, mask.a);\n"
    "    return;\n"
    "  }\n"
    /* tex_mode 0: normal 3D geometry with lighting */
    "  gl_FragColor = shade_surface(u_color, 1.0);\n"
    "}\n";

/* ================================================================
 * Vertex buffer — interleaved pos(3) + normal(3), 6 floats/vert
 * ================================================================ */

#define MAX_VERTS 1200
static float g_vbuf[MAX_VERTS * 6];
static int g_vcount;

void vb_reset(void) { g_vcount = 0; }

void vb_v(float x, float y, float z, float nx, float ny, float nz) {
    if (g_vcount >= MAX_VERTS) return;
    int i = g_vcount * 6;
    g_vbuf[i] = x; g_vbuf[i+1] = y; g_vbuf[i+2] = z;
    g_vbuf[i+3] = nx; g_vbuf[i+4] = ny; g_vbuf[i+5] = nz;
    g_vcount++;
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
    glUniform2f(g_uni_grain, preset->grain[0], preset->grain[1]);
}

void vb_flush(float r, float g, float b, float a) {
    const material_preset_t *preset;

    if (g_vcount == 0) return;
    preset = material_preset(g_active_material);
    apply_material(preset, r, g, b, a);
    glUniform1f(g_uni_zbias, g_z_bias);
    g_z_bias += Z_BIAS_STEP;

    glVertexAttribPointer(g_attr_pos,  3, GL_FLOAT, GL_FALSE, 24, g_vbuf);
    glVertexAttribPointer(g_attr_norm, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf + 3);
    glEnableVertexAttribArray(g_attr_pos);
    glEnableVertexAttribArray(g_attr_norm);
    glDrawArrays(GL_TRIANGLES, 0, g_vcount);
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

static int build_program(void) {
    char frag_src[16384];
    snprintf(frag_src, sizeof(frag_src), "%s%s%s",
             SHADER_HEADER, SHADER_PRECISION, k_frag_src_body);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, k_vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) return -1;

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glLinkProgram(g_program);

    GLint ok = 0;
    glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(g_program, sizeof(log), NULL, log);
        fprintf(stderr, "render: link error: %s\n", log);
        return -1;
    }

    g_attr_pos  = glGetAttribLocation(g_program, "a_pos");
    g_attr_norm = glGetAttribLocation(g_program, "a_normal");
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
    g_uni_grain = glGetUniformLocation(g_program, "u_grain");
    g_uni_tex_mode = glGetUniformLocation(g_program, "u_tex_mode");
    g_uni_tex      = glGetUniformLocation(g_program, "u_tex");
    g_uni_resolution = glGetUniformLocation(g_program, "u_resolution");
    g_uni_mask_scale = glGetUniformLocation(g_program, "u_mask_scale");

    glDeleteShader(vs);
    glDeleteShader(fs);
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
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fprintf(stderr, "render: %d FBOs init %dx%d (mask half extents %.2f x %.2f)\n",
            FBO_COUNT, g_fbo_w, g_fbo_h, g_mask_half_w, g_mask_half_h);
}

static void fbos_shutdown(void) {
    if (g_fbos[0]) { glDeleteFramebuffers(FBO_COUNT, g_fbos); memset(g_fbos, 0, sizeof(g_fbos)); }
    if (g_fbo_texs[0]) { glDeleteTextures(FBO_COUNT, g_fbo_texs); memset(g_fbo_texs, 0, sizeof(g_fbo_texs)); }
    if (g_fbo_depths[0]) { glDeleteRenderbuffers(FBO_COUNT, g_fbo_depths); memset(g_fbo_depths, 0, sizeof(g_fbo_depths)); }
    g_fbo_w = g_fbo_h = 0;
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

static void fbo_bind_noclear(int idx) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbos[idx]);
    glViewport(0, 0, g_fbo_w, g_fbo_h);
}

/* Restore default framebuffer */
static void fbo_unbind(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_default_fbo);
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

    g_fb_w = fb_width;
    g_fb_h = fb_height;
    update_mask_config(fb_width, fb_height);

    glViewport(0, 0, fb_width, fb_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_DITHER);
#ifdef GL_MULTISAMPLE
    glEnable(GL_MULTISAMPLE);   /* 4x MSAA — requested in platform init */
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Save default framebuffer — may not be 0 on macOS with MSAA */
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &g_default_fbo);
    fbos_init(fb_width, fb_height);

    /* Init tex_mode off */
    glUseProgram(g_program);
    glUniform1f(g_uni_tex_mode, 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fbo_texs[0]);
    glUniform1i(g_uni_tex, 0);
    glUniform2f(g_uni_resolution, (float)fb_width, (float)fb_height);

    fprintf(stderr, "render: init multi-layer %dx%d (default fbo=%d)\n",
            fb_width, fb_height, (int)g_default_fbo);
    return 0;
}

void render_set_viewport(int fb_width, int fb_height) {
    g_fb_w = fb_width;
    g_fb_h = fb_height;
    update_mask_config(fb_width, fb_height);
    glViewport(0, 0, fb_width, fb_height);
    glUniform2f(g_uni_resolution, (float)fb_width, (float)fb_height);
    fbos_resize(fb_width, fb_height);
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

    float aspect = (float)g_fb_w / (float)g_fb_h;
    float cam_cos = cosf(g_cam_rot);
    float cam_sin = sinf(g_cam_rot);
    float eye_x = g_cam_pan_x + cam_cos * CAM_EYE_X - cam_sin * CAM_EYE_Z;
    float eye_z = g_cam_pan_z + cam_sin * CAM_EYE_X + cam_cos * CAM_EYE_Z;
    float ctr_x = g_cam_pan_x + cam_cos * CAM_CTR_X - cam_sin * CAM_CTR_Z;
    float ctr_z = g_cam_pan_z + cam_sin * CAM_CTR_X + cam_cos * CAM_CTR_Z;

    /* Compute both MVPs and lerp */
    float mvp_persp[16], mvp_ortho[16];

    {
        float proj[16], view[16];
        float fov_rad = CAM_FOV_DEG * (float)M_PI / 180.0f;
        mat4_perspective(proj, fov_rad, aspect, 0.1f, 20.0f);
        mat4_lookAt(view,
                    eye_x, CAM_EYE_Y, eye_z,
                    ctr_x, CAM_CTR_Y, ctr_z,
                    0.0f, 1.0f, 0.0f);
        mat4_mul(mvp_persp, proj, view);
    }
    {
        float proj[16], view[16];
        float hh = 1.0f, hw = hh * aspect;
        mat4_ortho(proj, -hw, hw, -hh, hh, 0.1f, 20.0f);
        mat4_zero(view);
        /* Ortho view uses the inverse camera yaw so top-down motion matches
         * the perspective camera heading instead of orbiting sideways. */
        view[0]  =  cam_cos;
        view[1]  = -cam_sin;
        view[8]  =  cam_sin;
        view[9]  =  cam_cos;
        view[12] = -cam_cos * g_cam_pan_x - cam_sin * g_cam_pan_z;
        view[13] =  cam_sin * g_cam_pan_x - cam_cos * g_cam_pan_z;
        view[6]  =  1.0f;
        view[14] = -5.0f;
        view[15] =  1.0f;
        mat4_mul(mvp_ortho, proj, view);
    }

    /* Blended MVP for 3D composite */
    {
        int i;
        for (i = 0; i < 16; i++)
            g_mvp_current[i] = mvp_ortho[i] + ts * (mvp_persp[i] - mvp_ortho[i]);
    }

    /* Pure 2D orthographic MVP for mask rendering.
     * Same top-down view as ortho mode but with no perspective blend. */
    {
        float proj[16], view[16];
        mat4_ortho(proj, -g_mask_half_w, g_mask_half_w,
                   -g_mask_half_h, g_mask_half_h, 0.1f, 20.0f);
        mat4_zero(view);
        view[0]  =  1.0f;   /* x → x */
        view[9]  =  1.0f;   /* z → y_screen */
        view[13] =  0.0f;
        view[6]  =  1.0f;   /* y → z_depth */
        view[14] = -5.0f;   /* push into view */
        view[15] =  1.0f;
        mat4_mul(g_mvp_ortho_2d, proj, view);

        /* mask_scale: maps world (x,z) → mask UV (0..1).
         * UV = world_coord * scale + 0.5 */
        glUniform2f(g_uni_mask_scale, 0.5f / g_mask_half_w, 0.5f / g_mask_half_h);
    }

    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    /* World-stable showroom lighting tuned to stay readable during camera motion. */
    float lx = k_lighting_state.key_dir[0];
    float ly = k_lighting_state.key_dir[1];
    float lz = k_lighting_state.key_dir[2];
    float ll = sqrtf(lx*lx + ly*ly + lz*lz);
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

    /* Camera eye position for specular/rim — lerp between modes */
    glUniform3f(g_uni_eye,
                eye_x * ts + 0.0f * (1.0f - ts),
                CAM_EYE_Y * ts + 5.0f * (1.0f - ts),
                eye_z * ts + 0.0f * (1.0f - ts));

    g_z_bias = 0.0f;
}

void render_begin_frame(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_program);
    glUniform1f(g_uni_tex_mode, 0.0f);
    sync_camera_uniforms();
}

void render_sync_camera(void) {
    glUseProgram(g_program);
    sync_camera_uniforms();
}

void render_end_frame(void) { /* no-op */ }

void render_set_perspective(int enabled) {
    g_perspective = enabled;
}

int render_is_animating(void) {
    return (g_persp_t != (float)g_perspective);
}

void render_set_raised(int raised) {
    g_raised = raised;
}

void render_set_mask_append(int append) {
    g_mask_append = append;
}

void render_set_camera_pan(float x, float y) {
    g_cam_pan_x = x;
    g_cam_pan_z = y;  /* maneuver y → 3D z */
}

void render_set_camera_rotation(float angle_rad) {
    g_cam_rot = angle_rad;
}

void render_set_material(render_material_t material) {
    g_active_material = material;
}

void render_invalidate_masks(void) {
    g_masks_dirty = 1;
}

int render_masks_dirty(void) {
    return g_masks_dirty;
}

/* ================================================================
 * Flag sprite atlas
 * ================================================================ */

static GLuint g_flag_tex = 0;
static int g_flag_frame_w = 0, g_flag_frame_h = 0, g_flag_frame_count = 0;

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
    g_flag_frame_w = frame_w;
    g_flag_frame_h = frame_h;
    g_flag_frame_count = frame_count;

    GLenum err = glGetError();
    fprintf(stderr, "render: flag atlas loaded %dx%d (%d frames) tex=%u gl_err=0x%x\n",
            atlas_w, atlas_h, frame_count, g_flag_tex, err);
    return 0;
}

void render_sprite_flag(float x, float y, float size, int frame) {
    /* Vertical billboard quad anchored at pole base.
     * Pole base in sprite UV: u=0.22, v=0.88 (from top).
     * Quad = full sprite (size*2 x size*2), offset so pole base = (x, 0, y). */
    float sprite_w = size * 2.0f;
    float sprite_h = size * 2.0f;
    float pole_u = 0.22f;     /* pole base X fraction in sprite */
    float pole_v_top = 0.88f; /* pole base Y fraction from top */

    float fx0 = x - pole_u * sprite_w;
    float fx1 = fx0 + sprite_w;
    float z = y;              /* maneuver y → world z (depth) */

    /* 3D (perspective): vertical — rises in world Y at fixed z.
     * 2D (ortho):       horizontal — lies flat, extends in +z at ground level.
     * g_persp_t: 1.0 = perspective (vertical), 0.0 = ortho (horizontal).
     * Apply same quintic ease-in-out (smootherstep) as camera. */
    float t = g_persp_t;
    t = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    float flat_h = 0.07f;     /* small height above ground in 2D mode */

    /* Bottom edge: stays at pole base position, lerp y only */
    float by_3d = -(1.0f - pole_v_top) * sprite_h;
    float by_2d = flat_h;
    float bot_y = by_3d * t + by_2d * (1.0f - t);
    float bot_z = z;

    /* Top edge: in 3D rises up in Y, in 2D extends back in Z */
    float ty_3d = by_3d + sprite_h;
    float ty_2d = flat_h;
    float top_y = ty_3d * t + ty_2d * (1.0f - t);
    float tz_3d = z;
    float tz_2d = z + sprite_h;
    float top_z = tz_3d * t + tz_2d * (1.0f - t);

    glDisable(GL_DEPTH_TEST);
    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    if (g_flag_tex && g_flag_frame_count > 0) {
        if (frame < 0) frame = 0;
        if (frame >= g_flag_frame_count) frame = g_flag_frame_count - 1;

        float u0 = (float)frame / (float)g_flag_frame_count;
        float u1 = (float)(frame + 1) / (float)g_flag_frame_count;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_flag_tex);
        glUniform1i(g_uni_tex, 0);
        glUniform1f(g_uni_tex_mode, 4.0f);

        vb_reset();
        vb_v(fx0, bot_y, bot_z,  u0, 1.0f, 0);
        vb_v(fx1, bot_y, bot_z,  u1, 1.0f, 0);
        vb_v(fx1, top_y, top_z,  u1, 0.0f, 0);
        vb_v(fx0, bot_y, bot_z,  u0, 1.0f, 0);
        vb_v(fx1, top_y, top_z,  u1, 0.0f, 0);
        vb_v(fx0, top_y, top_z,  u0, 0.0f, 0);

        glUniform4f(g_uni_color, 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform1f(g_uni_zbias, 0.0f);
        glVertexAttribPointer(g_attr_pos,  3, GL_FLOAT, GL_FALSE, 24, g_vbuf);
        glVertexAttribPointer(g_attr_norm, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf + 3);
        glEnableVertexAttribArray(g_attr_pos);
        glEnableVertexAttribArray(g_attr_norm);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        /* Fallback: magenta quad when atlas not loaded */
        glUniform1f(g_uni_tex_mode, 0.0f);
        vb_reset();
        vb_v(fx0, bot_y, bot_z,  0,0,1);
        vb_v(fx1, bot_y, bot_z,  0,0,1);
        vb_v(fx1, top_y, top_z,  0,0,1);
        vb_v(fx0, bot_y, bot_z,  0,0,1);
        vb_v(fx1, top_y, top_z,  0,0,1);
        vb_v(fx0, top_y, top_z,  0,0,1);
        render_set_material(RENDER_MAT_GENERIC_SOLID);
        vb_flush(1.0f, 0.0f, 1.0f, 1.0f);
    }

    glEnable(GL_DEPTH_TEST);
    glUniform1f(g_uni_tex_mode, 0.0f);
}

void render_shutdown(void) {
    fbos_shutdown();
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
 * Mask rendering — begin/end for each mask layer
 *
 * Sets up: FBO bound, flat 2D ortho MVP, tex_mode=3 (flat color),
 * no depth test, no blending, overwrite mode.
 * ================================================================ */

static void begin_mask(int fbo_idx) {
    if (g_mask_append)
        fbo_bind_noclear(fbo_idx);
    else
        fbo_bind(fbo_idx);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUniform1f(g_uni_tex_mode, 3.0f);
    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_ortho_2d);
    g_z_bias = 0.0f;
}

static void end_mask(void) {
    fbo_unbind();
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUniform1f(g_uni_tex_mode, 0.0f);
    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);
}

/* Apply a 2D rigid transform to the mask MVP (for rendering a second maneuver).
 * Modifies g_mvp_ortho_2d so that begin_mask picks up the transform.
 * Maneuver (x,y) → rotated by (cos_r,sin_r) then translated by (tx,ty).
 * In 3D: maneuver x→world x, maneuver y→world z. */
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

/* Resume mask — bind without clearing (append to existing mask content) */
static void resume_mask(int fbo_idx) {
    fbo_bind_noclear(fbo_idx);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUniform1f(g_uni_tex_mode, 3.0f);
    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_ortho_2d);
    g_z_bias = 0.0f;
}

void render_resume_outline_mask(void) { resume_mask(FBO_OUTLINE); }
void render_resume_fill_mask(void)    { resume_mask(FBO_FILL); }

void render_begin_outline_mask(void) { begin_mask(FBO_OUTLINE); }
void render_end_outline_mask(void)   { end_mask(); }

void render_begin_fill_mask(void) { begin_mask(FBO_FILL); }
void render_end_fill_mask(void)   { end_mask(); }

void render_begin_route_mask(void) { begin_mask(FBO_ROUTE); }
void render_end_route_mask(void)   { end_mask(); }

/* ================================================================
 * Composite pipeline
 *
 * Step 1: Subtract FBO_FILL alpha from FBO_OUTLINE → FBO_COMPOSITE
 *         (only the thin border ring remains)
 * Step 2: Render 3D ground-plane quads textured with each mask:
 *         - Fill → asphalt material
 *         - Outline border → outline material
 *         - Route → route material (raised)
 * ================================================================ */

/* Blit a fullscreen quad with the given FBO texture (clip-space passthrough) */
static void blit_fbo_fullscreen(int fbo_idx) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fbo_texs[fbo_idx]);
    glUniform1i(g_uni_tex, 0);
    glUniform1f(g_uni_tex_mode, 1.0f);

    vb_reset();
    vb_v(-1,-1,0, 0,0,0);  vb_v( 1,-1,0, 0,0,0);  vb_v( 1, 1,0, 0,0,0);
    vb_v(-1,-1,0, 0,0,0);  vb_v( 1, 1,0, 0,0,0);  vb_v(-1, 1,0, 0,0,0);

    glUniform4f(g_uni_color, 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1f(g_uni_zbias, 0.0f);
    glVertexAttribPointer(g_attr_pos,  3, GL_FLOAT, GL_FALSE, 24, g_vbuf);
    glVertexAttribPointer(g_attr_norm, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf + 3);
    glEnableVertexAttribArray(g_attr_pos);
    glEnableVertexAttribArray(g_attr_norm);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* Render a 3D ground-plane quad textured with a mask FBO.
 * The quad goes through MVP (perspective), shader does lighting (tex_mode=2).
 * y_height: world Y of the quad (0 for ground, ROUTE_Y for route). */
static void composite_layer(int fbo_tex_idx, float y_height,
                             render_material_t material) {
    const material_preset_t *preset = material_preset(material);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fbo_texs[fbo_tex_idx]);
    glUniform1i(g_uni_tex, 0);
    glUniform1f(g_uni_tex_mode, 2.0f);
    glUniform2f(g_uni_resolution, (float)g_fb_w, (float)g_fb_h);

    apply_material(preset,
                   preset->base_color[0], preset->base_color[1],
                   preset->base_color[2], preset->base_color[3]);
    glUniform1f(g_uni_zbias, g_z_bias);
    g_z_bias += Z_BIAS_STEP;

    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    /* 6 verts = 2 triangles covering the render area */
    float gx = g_mask_half_w, gz = g_mask_half_h;
    float y = y_height;

    vb_reset();
    vb_v(-gx, y, -gz,  0, 1, 0);
    vb_v( gx, y, -gz,  0, 1, 0);
    vb_v( gx, y,  gz,  0, 1, 0);
    vb_v(-gx, y, -gz,  0, 1, 0);
    vb_v( gx, y,  gz,  0, 1, 0);
    vb_v(-gx, y,  gz,  0, 1, 0);

    glVertexAttribPointer(g_attr_pos,  3, GL_FLOAT, GL_FALSE, 24, g_vbuf);
    glVertexAttribPointer(g_attr_norm, 3, GL_FLOAT, GL_FALSE, 24, g_vbuf + 3);
    glEnableVertexAttribArray(g_attr_pos);
    glEnableVertexAttribArray(g_attr_norm);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_composite(void) {
    /* Step 1: Subtract — render outline to composite, then erase where fill has alpha */
    fbo_bind(FBO_COMPOSITE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    /* First pass: copy outline to composite */
    glDisable(GL_BLEND);
    blit_fbo_fullscreen(FBO_OUTLINE);

    /* Second pass: subtract fill alpha from composite.
     * Blend: RGB unchanged (ZERO,ONE), Alpha: dest *= (1 - src_alpha) */
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    blit_fbo_fullscreen(FBO_FILL);

    /* Restore default blend */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    fbo_unbind();

    /* Step 2: Composite layers to screen as 3D quads with materials + perspective */
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Layer 1: Fill (grey asphalt) — ground level */
    composite_layer(FBO_FILL, 0.0f, RENDER_MAT_ROAD_ASPHALT);

    /* Layer 2: Outline border (white, after subtraction) */
    composite_layer(FBO_COMPOSITE, 0.001f, RENDER_MAT_ROAD_BORDER_PAINT);

    /* Route layer removed — now rendered as direct 3D mesh after composite */

    /* Restore tex_mode for any subsequent draws */
    glUniform1f(g_uni_tex_mode, 0.0f);

    g_masks_dirty = 0;
}

/* ================================================================
 * 3D Primitives — extrude 2D shapes into boxes/prisms
 *
 * All functions take 2D coordinates (x, y_2d).
 * In mask mode (tex_mode=3): flat 2D, y_2d maps to 3D z, y=0.
 * In normal mode (tex_mode=0): extruded 3D.
 * ================================================================ */

static float cur_base(void) { return g_raised ? RAISE_BASE : 0.0f; }
static float cur_top(void)  { return g_raised ? RAISE_BASE + EXTRUDE_H : 0.0f; }

void render_thick_line(float x0, float y0, float x1, float y1, float thickness,
                       float r, float g, float b, float a) {
    float z0 = y0, z1 = y1;          /* 2D y → 3D z */
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
    render_set_material(RENDER_MAT_GENERIC_SOLID);
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
    render_set_material(RENDER_MAT_GENERIC_SOLID);
    vb_flush(r, g, b, a);
}

void render_arrowhead(float bx, float by, float angle_rad, float size,
                      float r, float g, float b, float a) {
    float dx = cosf(angle_rad), dy = sinf(angle_rad);
    float px = -dy, py = dx;
    float hw = size * 0.885f;

    float tip_x = bx + dx * size, tip_y = by + dy * size;
    float l_x = bx + px * hw,     l_y = by + py * hw;
    float r_x = bx - px * hw,     r_y = by - py * hw;

    render_triangle(tip_x, tip_y, l_x, l_y, r_x, r_y, r, g, b, a);
}

void render_disc(float cx, float cy, float radius, int segments,
                 float r, float g, float b, float a) {
    if (segments < 3) segments = 3;
    if (segments > 64) segments = 64;

    float bt = cur_base(), tp = cur_top();
    float cz = cy;   /* 2D y → 3D z */
    int i;

    vb_reset();

    /* Top face — individual triangles */
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

    render_set_material(RENDER_MAT_GENERIC_SOLID);
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
    float cz = cy;   /* 2D y → 3D z */
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

    render_set_material(RENDER_MAT_GENERIC_SOLID);
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
    render_set_material(RENDER_MAT_GENERIC_SOLID);
    vb_flush(r, g, b, a);
}

/* Legacy stub pass API — kept for compatibility but now uses mask pipeline */
void render_begin_stubs(void) {
    begin_mask(FBO_OUTLINE);
}

void render_end_stubs(void) {
    end_mask();
}
