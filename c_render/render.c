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

/* Light direction (world space, normalized in shader) */
#define LIGHT_X  0.1f
#define LIGHT_Y  0.9f
#define LIGHT_Z -0.4f

/* Composite ground plane extents (world space) */
#define GROUND_X  1.2f
#define GROUND_Z  1.2f
#define ROUTE_Y   0.04f   /* raised height for route layer */

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
static GLint  g_uni_zbias  = -1;
static GLint  g_uni_eye    = -1;
static GLint  g_uni_mat    = -1;
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
    "uniform vec3 u_eye;\n"
    "uniform vec4 u_material;\n"
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
    "void main() {\n"
    /* tex_mode 1: fullscreen blit — passthrough texture sample */
    "  if (u_tex_mode > 0.5 && u_tex_mode < 1.5) {\n"
    "    vec2 uv = v_world_pos.xy * 0.5 + 0.5;\n"
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
    "    vec3 N = normalize(v_normal);\n"
    "    vec3 L = u_light_dir;\n"
    "    vec3 V = normalize(u_eye - v_world_pos);\n"
    "    float grain = 0.0;\n"
    "    if (u_grain.x > 0.0) {\n"
    "      vec2 guv = v_world_pos.xz * u_grain.y;\n"
    "      float n1 = vnoise(guv);\n"
    "      float n2 = vnoise(guv * 3.7 + 17.0);\n"
    "      grain = (n1 * 0.6 + n2 * 0.4) - 0.5;\n"
    "      float bump = grain * u_grain.x * 0.3;\n"
    "      N = normalize(N + vec3(bump, 0.0, bump));\n"
    "    }\n"
    "    float NdotL = max(dot(N, L), 0.0);\n"
    "    float diffuse = 0.42 + 0.58 * NdotL;\n"
    "    vec3 H = normalize(L + V);\n"
    "    float NdotH = max(dot(N, H), 0.0);\n"
    "    float spec = u_material.x * pow(NdotH, u_material.y);\n"
    "    float rim = 1.0 - max(dot(N, V), 0.0);\n"
    "    rim = u_material.z * pow(rim, u_material.w);\n"
    "    vec3 color = u_color.rgb * diffuse + vec3(spec) + u_color.rgb * rim;\n"
    "    if (u_grain.x > 0.0) {\n"
    "      color += color * grain * u_grain.x * 0.15;\n"
    "    }\n"
    "    gl_FragColor = vec4(color, mask.a);\n"
    "    return;\n"
    "  }\n"
    /* tex_mode 0: normal 3D geometry with lighting */
    "  vec3 N = normalize(v_normal);\n"
    "  vec3 L = u_light_dir;\n"
    "  vec3 V = normalize(u_eye - v_world_pos);\n"
    "  float grain = 0.0;\n"
    "  if (u_grain.x > 0.0) {\n"
    "    vec2 uv = v_world_pos.xz * u_grain.y;\n"
    "    float n1 = vnoise(uv);\n"
    "    float n2 = vnoise(uv * 3.7 + 17.0);\n"
    "    grain = (n1 * 0.6 + n2 * 0.4) - 0.5;\n"
    "    float bump = grain * u_grain.x * 0.3;\n"
    "    N = normalize(N + vec3(bump, 0.0, bump));\n"
    "  }\n"
    "  float NdotL = max(dot(N, L), 0.0);\n"
    "  float diffuse = 0.42 + 0.58 * NdotL;\n"
    "  vec3 H = normalize(L + V);\n"
    "  float NdotH = max(dot(N, H), 0.0);\n"
    "  float spec = u_material.x * pow(NdotH, u_material.y);\n"
    "  float rim = 1.0 - max(dot(N, V), 0.0);\n"
    "  rim = u_material.z * pow(rim, u_material.w);\n"
    "  vec3 color = u_color.rgb * diffuse + vec3(spec) + u_color.rgb * rim;\n"
    "  if (u_grain.x > 0.0) {\n"
    "    color += color * grain * u_grain.x * 0.15;\n"
    "  }\n"
    "  gl_FragColor = vec4(color, u_color.a);\n"
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

void vb_flush(float r, float g, float b, float a) {
    if (g_vcount == 0) return;
    glUniform4f(g_uni_color, r, g, b, a);
    glUniform1f(g_uni_zbias, g_z_bias);
    g_z_bias += Z_BIAS_STEP;

    /* Auto-detect material from color */
    if (r > 0.95f && g > 0.95f && b > 0.95f) {
        /* White outline — flat, fine grain */
        glUniform4f(g_uni_mat, 0.0f, 1.0f, 0.0f, 1.0f);
        glUniform2f(g_uni_grain, 0.35f, 80.0f);
    } else if (b > 0.8f && r < 0.5f) {
        /* Active blue — glossy, no grain */
        glUniform4f(g_uni_mat, 0.45f, 32.0f, 0.25f, 2.5f);
        glUniform2f(g_uni_grain, 0.0f, 0.0f);
    } else {
        /* Grey (default) — matte, asphalt grain */
        glUniform4f(g_uni_mat, 0.08f, 8.0f, 0.10f, 3.0f);
        glUniform2f(g_uni_grain, 0.5f, 60.0f);
    }

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
    char frag_src[8192];
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
    g_uni_zbias = glGetUniformLocation(g_program, "u_z_bias");
    g_uni_eye   = glGetUniformLocation(g_program, "u_eye");
    g_uni_mat   = glGetUniformLocation(g_program, "u_material");
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
    g_fbo_w = w; g_fbo_h = h;

    glGenTextures(FBO_COUNT, g_fbo_texs);
    glGenRenderbuffers(FBO_COUNT, g_fbo_depths);
    glGenFramebuffers(FBO_COUNT, g_fbos);

    for (i = 0; i < FBO_COUNT; i++) {
        glBindTexture(GL_TEXTURE_2D, g_fbo_texs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindRenderbuffer(GL_RENDERBUFFER, g_fbo_depths[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_FBO, w, h);

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
    fprintf(stderr, "render: %d FBOs init %dx%d\n", FBO_COUNT, w, h);
}

static void fbos_shutdown(void) {
    if (g_fbos[0]) { glDeleteFramebuffers(FBO_COUNT, g_fbos); memset(g_fbos, 0, sizeof(g_fbos)); }
    if (g_fbo_texs[0]) { glDeleteTextures(FBO_COUNT, g_fbo_texs); memset(g_fbo_texs, 0, sizeof(g_fbo_texs)); }
    if (g_fbo_depths[0]) { glDeleteRenderbuffers(FBO_COUNT, g_fbo_depths); memset(g_fbo_depths, 0, sizeof(g_fbo_depths)); }
    g_fbo_w = g_fbo_h = 0;
}

static void fbos_resize(int w, int h) {
    if (w == g_fbo_w && h == g_fbo_h) return;
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

/* Restore default framebuffer */
static void fbo_unbind(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_default_fbo);
    glViewport(0, 0, g_fb_w, g_fb_h);
}

/* ================================================================
 * Public API
 * ================================================================ */

int render_init(int fb_width, int fb_height) {
    if (build_program() < 0) return -1;

    g_fb_w = fb_width;
    g_fb_h = fb_height;

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
    glViewport(0, 0, fb_width, fb_height);
    glUniform2f(g_uni_resolution, (float)fb_width, (float)fb_height);
    fbos_resize(fb_width, fb_height);
}

void render_begin_frame(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_program);
    glUniform1f(g_uni_tex_mode, 0.0f);

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

    /* Compute both MVPs and lerp */
    float mvp_persp[16], mvp_ortho[16];

    {
        float proj[16], view[16];
        float fov_rad = CAM_FOV_DEG * (float)M_PI / 180.0f;
        mat4_perspective(proj, fov_rad, aspect, 0.1f, 20.0f);
        mat4_lookAt(view,
                    CAM_EYE_X, CAM_EYE_Y, CAM_EYE_Z,
                    CAM_CTR_X, CAM_CTR_Y, CAM_CTR_Z,
                    0.0f, 1.0f, 0.0f);
        mat4_mul(mvp_persp, proj, view);
    }
    {
        float proj[16], view[16];
        float hh = 1.0f, hw = hh * aspect;
        mat4_ortho(proj, -hw, hw, -hh, hh, 0.1f, 20.0f);
        mat4_zero(view);
        view[0]  =  1.0f;
        view[9]  =  1.0f;
        view[13] =  0.0f;
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
        float hh = 1.0f, hw = hh * aspect;
        mat4_ortho(proj, -hw, hw, -hh, hh, 0.1f, 20.0f);
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
        glUniform2f(g_uni_mask_scale, 0.5f / hw, 0.5f / hh);
    }

    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    /* Normalized light direction */
    float lx = LIGHT_X, ly = LIGHT_Y, lz = LIGHT_Z;
    float ll = sqrtf(lx*lx + ly*ly + lz*lz);
    glUniform3f(g_uni_light, lx/ll, ly/ll, lz/ll);

    /* Camera eye position for specular/rim — lerp between modes */
    glUniform3f(g_uni_eye,
                CAM_EYE_X * ts + 0.0f * (1.0f - ts),
                CAM_EYE_Y * ts + 5.0f * (1.0f - ts),
                CAM_EYE_Z * ts + 0.0f * (1.0f - ts));

    g_z_bias = 0.0f;
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

void render_invalidate_masks(void) {
    g_masks_dirty = 1;
}

int render_masks_dirty(void) {
    return g_masks_dirty;
}

void render_shutdown(void) {
    fbos_shutdown();
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
                             float cr, float cg, float cb, float ca,
                             float mat_spec, float mat_shininess,
                             float mat_rim, float mat_rim_pow,
                             float grain_str, float grain_scale) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fbo_texs[fbo_tex_idx]);
    glUniform1i(g_uni_tex, 0);
    glUniform1f(g_uni_tex_mode, 2.0f);
    glUniform2f(g_uni_resolution, (float)g_fb_w, (float)g_fb_h);

    glUniform4f(g_uni_color, cr, cg, cb, ca);
    glUniform4f(g_uni_mat, mat_spec, mat_shininess, mat_rim, mat_rim_pow);
    glUniform2f(g_uni_grain, grain_str, grain_scale);
    glUniform1f(g_uni_zbias, g_z_bias);
    g_z_bias += Z_BIAS_STEP;

    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, g_mvp_current);

    /* 6 verts = 2 triangles covering the render area */
    float gx = GROUND_X, gz = GROUND_Z;
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
    composite_layer(FBO_FILL, 0.0f,
                    0.392f, 0.392f, 0.392f, 1.0f,   /* grey color */
                    0.08f, 8.0f, 0.10f, 3.0f,        /* matte material */
                    0.5f, 60.0f);                     /* asphalt grain */

    /* Layer 2: Outline border (white, after subtraction) */
    composite_layer(FBO_COMPOSITE, 0.001f,
                    1.0f, 1.0f, 1.0f, 1.0f,          /* white color */
                    0.0f, 1.0f, 0.0f, 1.0f,          /* flat material */
                    0.35f, 80.0f);                    /* fine grain */

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

/* Legacy stub pass API — kept for compatibility but now uses mask pipeline */
void render_begin_stubs(void) {
    begin_mask(FBO_OUTLINE);
}

void render_end_stubs(void) {
    end_mask();
}
