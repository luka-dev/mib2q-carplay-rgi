/*
 * OpenGL rendering — 3D extruded geometry with lighting.
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

#define EXTRUDE_H    0.065f    /* active extrusion height */
#define RAISE_BASE   0.015f   /* active floats this much above grey */
#define Z_BIAS_STEP  0.00001f  /* depth bias per draw (layer ordering) */

/* Camera — perspective mode */
#define CAM_EYE_X    0.0f
#define CAM_EYE_Y    1.0f
#define CAM_EYE_Z   -1.5f
#define CAM_CTR_X    0.0f
#define CAM_CTR_Y    0.04f
#define CAM_CTR_Z    0.15f
#define CAM_FOV_DEG  45.0f

/* Light direction (world space, normalized in shader) */
#define LIGHT_X  0.1f
#define LIGHT_Y  0.9f
#define LIGHT_Z -0.4f

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

static int g_perspective = 1;
static int g_raised = 1;
static int g_fb_w = 284, g_fb_h = 276;
static float g_z_bias = 0.0f;

static const char *k_vert_src =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform float u_z_bias;\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_world_pos;\n"
    "void main() {\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "  gl_Position.z -= u_z_bias;\n"
    "  v_normal = a_normal;\n"
    "  v_world_pos = a_pos;\n"
    "}\n";

static const char *k_frag_src_body =
    "uniform vec4 u_color;\n"
    "uniform vec3 u_light_dir;\n"
    "uniform vec3 u_eye;\n"
    "uniform vec4 u_material;\n"
    "uniform vec2 u_grain;\n"
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

static void vb_reset(void) { g_vcount = 0; }

static void vb_v(float x, float y, float z, float nx, float ny, float nz) {
    if (g_vcount >= MAX_VERTS) return;
    int i = g_vcount * 6;
    g_vbuf[i] = x; g_vbuf[i+1] = y; g_vbuf[i+2] = z;
    g_vbuf[i+3] = nx; g_vbuf[i+4] = ny; g_vbuf[i+5] = nz;
    g_vcount++;
}

/* Push a quad as 2 triangles with flat normal */
static void vb_quad(float x0, float y0, float z0,
                    float x1, float y1, float z1,
                    float x2, float y2, float z2,
                    float x3, float y3, float z3,
                    float nx, float ny, float nz) {
    vb_v(x0,y0,z0, nx,ny,nz);  vb_v(x1,y1,z1, nx,ny,nz);  vb_v(x2,y2,z2, nx,ny,nz);
    vb_v(x0,y0,z0, nx,ny,nz);  vb_v(x2,y2,z2, nx,ny,nz);  vb_v(x3,y3,z3, nx,ny,nz);
}

static void vb_flush(float r, float g, float b, float a) {
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
    char frag_src[4096];
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

    glDeleteShader(vs);
    glDeleteShader(fs);
    return 0;
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

    fprintf(stderr, "render: init 3D %dx%d\n", fb_width, fb_height);
    return 0;
}

void render_set_viewport(int fb_width, int fb_height) {
    g_fb_w = fb_width;
    g_fb_h = fb_height;
    glViewport(0, 0, fb_width, fb_height);
}

void render_begin_frame(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_program);

    float proj[16], view[16], mvp[16];
    float aspect = (float)g_fb_w / (float)g_fb_h;

    if (g_perspective) {
        float fov_rad = CAM_FOV_DEG * (float)M_PI / 180.0f;
        mat4_perspective(proj, fov_rad, aspect, 0.1f, 20.0f);
        mat4_lookAt(view,
                    CAM_EYE_X, CAM_EYE_Y, CAM_EYE_Z,
                    CAM_CTR_X, CAM_CTR_Y, CAM_CTR_Z,
                    0.0f, 1.0f, 0.0f);
    } else {
        /* Top-down orthographic — matches original 2D layout */
        float hw = 1.05f, hh = hw / aspect;
        mat4_ortho(proj, -hw, hw, -hh, hh, 0.1f, 20.0f);
        /* Custom view: x→x, z_world→screen_y, y_world→depth */
        mat4_zero(view);
        view[0]  =  1.0f;           /* x_eye = x_world */
        view[9]  =  1.0f;           /* y_eye = z_world */
        view[13] =  0.1f;           /* recenter y */
        view[6]  =  1.0f;           /* z_eye = y_world (height→depth) */
        view[14] = -5.0f;           /* push into view volume */
        view[15] =  1.0f;
    }

    mat4_mul(mvp, proj, view);
    glUniformMatrix4fv(g_uni_mvp, 1, GL_FALSE, mvp);

    /* Normalized light direction */
    float lx = LIGHT_X, ly = LIGHT_Y, lz = LIGHT_Z;
    float ll = sqrtf(lx*lx + ly*ly + lz*lz);
    glUniform3f(g_uni_light, lx/ll, ly/ll, lz/ll);

    /* Camera eye position for specular/rim */
    if (g_perspective) {
        glUniform3f(g_uni_eye, CAM_EYE_X, CAM_EYE_Y, CAM_EYE_Z);
    } else {
        glUniform3f(g_uni_eye, 0.0f, 5.0f, 0.0f);
    }

    g_z_bias = 0.0f;
}

void render_end_frame(void) { /* no-op */ }

void render_set_perspective(int enabled) {
    g_perspective = enabled;
}

void render_set_raised(int raised) {
    g_raised = raised;
}

void render_shutdown(void) {
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
}

/* ================================================================
 * 3D Primitives — extrude 2D shapes into boxes/prisms
 *
 * All functions take 2D coordinates (x, y_2d).
 * Internally: (x, h, y_2d) for top faces, (x, 0, y_2d) for bottom.
 * h = EXTRUDE_H when raised, ~0 when flat.
 * ================================================================ */

#define FLAT_H 0.002f  /* near-zero height for flat elements */
static float cur_base(void) { return g_raised ? RAISE_BASE : 0.0f; }
static float cur_top(void)  { return g_raised ? RAISE_BASE + EXTRUDE_H : FLAT_H; }

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
