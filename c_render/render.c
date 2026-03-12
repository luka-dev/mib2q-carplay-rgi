/*
 * OpenGL rendering — shared between macOS (GL 2.1) and QNX (GLES2).
 *
 * Uses programmable pipeline (shaders) in the GLES2-compatible subset.
 * All coordinates in NDC (-1..+1).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gl_compat.h"
#include "render.h"

/* ================================================================
 * Shader program — flat color
 * ================================================================ */

static GLuint g_program = 0;
static GLint  g_attr_pos = -1;
static GLint  g_uni_color = -1;
static GLint  g_uni_img_size = -1;
static GLint  g_uni_vp = -1;
static GLint  g_uni_depth = -1;
static GLint  g_uni_ref_y = -1;

/* Perspective parameters (pixel space) */
#define PERSP_VP_X    142.0
#define PERSP_VP_Y    -118.0
#define PERSP_DEPTH   370.0
#define PERSP_REF_Y   252.0
#define PERSP_IMG_W   284.0
#define PERSP_IMG_H   276.0

static const char *k_vert_src =
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_img_size;\n"
    "uniform vec2 u_vp;\n"
    "uniform float u_depth;\n"
    "uniform float u_ref_y;\n"
    "void main() {\n"
    "  float y_off = -0.25;\n"
    "  float px_x = (a_pos.x + 1.0) * 0.5 * u_img_size.x;\n"
    "  float px_y = (1.0 - (a_pos.y + y_off)) * 0.5 * u_img_size.y;\n"
    "  float s = u_depth / (u_depth + (u_ref_y - px_y));\n"
    "  float px_x2 = u_vp.x + (px_x - u_vp.x) * s;\n"
    "  float px_y2 = u_vp.y + (px_y - u_vp.y) * s;\n"
    "  float ndc_x = px_x2 / u_img_size.x * 2.0 - 1.0;\n"
    "  float ndc_y = 1.0 - px_y2 / u_img_size.y * 2.0;\n"
    "  gl_Position = vec4(ndc_x, ndc_y, 0.0, 1.0);\n"
    "}\n";

static const char *k_frag_src_body =
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "  gl_FragColor = u_color;\n"
    "}\n";

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
    /* Build fragment source with precision prefix for GLES2 */
    char frag_src[1024];
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

    g_attr_pos   = glGetAttribLocation(g_program, "a_pos");
    g_uni_color  = glGetUniformLocation(g_program, "u_color");
    g_uni_img_size = glGetUniformLocation(g_program, "u_img_size");
    g_uni_vp     = glGetUniformLocation(g_program, "u_vp");
    g_uni_depth  = glGetUniformLocation(g_program, "u_depth");
    g_uni_ref_y  = glGetUniformLocation(g_program, "u_ref_y");

    glDeleteShader(vs);
    glDeleteShader(fs);
    return 0;
}

/* ================================================================
 * Public API
 * ================================================================ */

int render_init(int fb_width, int fb_height) {
    if (build_program() < 0) return -1;

    glViewport(0, 0, fb_width, fb_height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    fprintf(stderr, "render: init %dx%d\n", fb_width, fb_height);
    return 0;
}

void render_set_viewport(int fb_width, int fb_height) {
    glViewport(0, 0, fb_width, fb_height);
}

void render_begin_frame(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_program);

    /* Set perspective uniforms */
    glUniform2f(g_uni_img_size, (float)PERSP_IMG_W, (float)PERSP_IMG_H);
    glUniform2f(g_uni_vp, (float)PERSP_VP_X, (float)PERSP_VP_Y);
    glUniform1f(g_uni_depth, (float)PERSP_DEPTH);
    glUniform1f(g_uni_ref_y, (float)PERSP_REF_Y);
}

void render_rect(float x, float y, float w, float h,
                 float r, float g, float b, float a) {
    GLfloat verts[] = {
        x,     y,
        x + w, y,
        x + w, y + h,
        x,     y,
        x + w, y + h,
        x,     y + h
    };
    glUniform4f(g_uni_color, r, g, b, a);
    glVertexAttribPointer(g_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(g_attr_pos);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_triangle(float x0, float y0, float x1, float y1, float x2, float y2,
                     float r, float g, float b, float a) {
    GLfloat verts[] = { x0, y0, x1, y1, x2, y2 };
    glUniform4f(g_uni_color, r, g, b, a);
    glVertexAttribPointer(g_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(g_attr_pos);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void render_end_frame(void) {
    /* No-op — swap is done by platform layer */
}

void render_shutdown(void) {
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
}

/* ================================================================
 * Extended primitives for maneuver icons
 * ================================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void render_thick_line(float x0, float y0, float x1, float y1, float thickness,
                       float r, float g, float b, float a) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    float nx = (-dy / len) * thickness * 0.5f;
    float ny = ( dx / len) * thickness * 0.5f;

    GLfloat verts[] = {
        x0 + nx, y0 + ny,
        x0 - nx, y0 - ny,
        x1 - nx, y1 - ny,
        x0 + nx, y0 + ny,
        x1 - nx, y1 - ny,
        x1 + nx, y1 + ny,
    };
    glUniform4f(g_uni_color, r, g, b, a);
    glVertexAttribPointer(g_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(g_attr_pos);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_arrowhead(float bx, float by, float angle_rad, float size,
                      float r, float g, float b, float a) {
    float dx = cosf(angle_rad), dy = sinf(angle_rad);
    float px = -dy, py = dx;   /* perpendicular */
    float hw = size * 0.885f;  /* half-width: 2.3/1.3/2 of size → 2.3× shaft width */

    float tip_x = bx + dx * size;
    float tip_y = by + dy * size;
    float l_x = bx + px * hw;
    float l_y = by + py * hw;
    float r_x = bx - px * hw;
    float r_y = by - py * hw;

    render_triangle(tip_x, tip_y, l_x, l_y, r_x, r_y, r, g, b, a);
}

void render_disc(float cx, float cy, float radius, int segments,
                 float r, float g, float b, float a) {
    if (segments < 3) segments = 3;
    if (segments > 64) segments = 64;

    GLfloat verts[(64 + 2) * 2];
    verts[0] = cx;
    verts[1] = cy;
    for (int i = 0; i <= segments; i++) {
        float ang = 2.0f * (float)M_PI * i / segments;
        verts[2 + i * 2]     = cx + radius * cosf(ang);
        verts[2 + i * 2 + 1] = cy + radius * sinf(ang);
    }

    glUniform4f(g_uni_color, r, g, b, a);
    glVertexAttribPointer(g_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(g_attr_pos);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
}

void render_arc(float cx, float cy, float radius, float thickness,
                float start_rad, float end_rad, int segments,
                float r, float g, float b, float a) {
    if (segments < 1) segments = 1;
    if (segments > 64) segments = 64;

    float ri = radius - thickness * 0.5f;
    float ro = radius + thickness * 0.5f;

    GLfloat buf[64 * 12];  /* 64 segs * 2 tris * 6 floats */
    int idx = 0;

    for (int i = 0; i < segments; i++) {
        float a0 = start_rad + (end_rad - start_rad) * i / segments;
        float a1 = start_rad + (end_rad - start_rad) * (i + 1) / segments;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        float ix0 = cx + ri * c0, iy0 = cy + ri * s0;
        float ox0 = cx + ro * c0, oy0 = cy + ro * s0;
        float ix1 = cx + ri * c1, iy1 = cy + ri * s1;
        float ox1 = cx + ro * c1, oy1 = cy + ro * s1;

        buf[idx++] = ix0; buf[idx++] = iy0;
        buf[idx++] = ox0; buf[idx++] = oy0;
        buf[idx++] = ox1; buf[idx++] = oy1;

        buf[idx++] = ix0; buf[idx++] = iy0;
        buf[idx++] = ox1; buf[idx++] = oy1;
        buf[idx++] = ix1; buf[idx++] = iy1;
    }

    glUniform4f(g_uni_color, r, g, b, a);
    glVertexAttribPointer(g_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, buf);
    glEnableVertexAttribArray(g_attr_pos);
    glDrawArrays(GL_TRIANGLES, 0, segments * 6);
}

void render_circle(float cx, float cy, float radius, float thickness, int segments,
                   float r, float g, float b, float a) {
    render_arc(cx, cy, radius, thickness, 0, 2.0f * (float)M_PI, segments, r, g, b, a);
}
