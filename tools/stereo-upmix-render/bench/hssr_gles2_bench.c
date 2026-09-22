/*
 * Offscreen GLES2 fragment-shader microbenchmark for the MHI2Q QNX unit.
 *
 * The Qualcomm EGL loader on this BSP requires a QNX Screen context to exist
 * before eglGetDisplay().  We create only that context, then use a 16x16 EGL
 * pbuffer: no Screen window, displayable, compositor plane, or buffer swap is
 * involved.  The measured workload maps three dense HSSR-160 GEMVs to fragment
 * shaders backed by RGBA8 textures.  Timing includes GLES submission and a
 * final glFinish(), which is the relevant cost for a synchronous audio worker.
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <screen/screen.h>

#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define HIDDEN 160
#define INPUT_FEATURES 368
#define OUTPUT_FEATURES 264
#define BENCH_ITERATIONS 256

typedef struct {
    GLuint program;
    GLuint weights;
    GLuint input;
    GLint weights_loc;
    GLint input_loc;
    GLint jitter_loc;
    int rows;
    int taps;
    uint64_t macs;
} GemvPass;

static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;
static screen_context_t g_screen_context;
static GLuint g_fbo;
static GLuint g_output_texture;
static GLuint g_vbo;

static uint64_t now_ns(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

static void alarm_handler(int sig)
{
    static const char message[] = "GPU benchmark watchdog timeout\n";
    (void)sig;
    (void)write(STDERR_FILENO, message, sizeof(message) - 1u);
    _exit(124);
}

static void print_shader_log(GLuint object, int program)
{
    GLint length = 0;
    char *log;

    if (program) glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
    else glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) return;
    log = (char *)malloc((size_t)length);
    if (!log) return;
    if (program) glGetProgramInfoLog(object, length, NULL, log);
    else glGetShaderInfoLog(object, length, NULL, log);
    fprintf(stderr, "%s\n", log);
    free(log);
}

static GLuint compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint ok = GL_FALSE;
    if (!shader) return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        fprintf(stderr, "shader compile failed type=0x%x\n", (unsigned)type);
        print_shader_log(shader, 0);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char *fragment_source)
{
    static const char vertex_source[] =
        "attribute vec2 a_position;\n"
        "varying float v_row;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "  v_row = a_position.x * 0.5 + 0.5;\n"
        "}\n";
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fs;
    GLuint program;
    GLint ok = GL_FALSE;

    if (!vs) return 0;
    fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }
    program = glCreateProgram();
    if (!program) {
        glDeleteShader(fs);
        glDeleteShader(vs);
        return 0;
    }
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "a_position");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glDeleteShader(fs);
    glDeleteShader(vs);
    if (ok != GL_TRUE) {
        fprintf(stderr, "program link failed\n");
        print_shader_log(program, 1);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static GLuint create_rgba8_texture(int width, int height, unsigned seed)
{
    const size_t bytes = (size_t)width * (size_t)height * 4u;
    unsigned char *data = (unsigned char *)malloc(bytes);
    GLuint texture = 0;
    size_t i;

    if (!data) return 0;
    for (i = 0; i < bytes; ++i) {
        data[i] = (unsigned char)((i * 29u + seed * 17u + 1u) & 0xffu);
    }
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    free(data);
    return texture;
}

static int create_gemv_pass(GemvPass *pass, int rows, int cols, unsigned seed)
{
    char fragment_source[1024];
    int taps = (cols + 3) / 4;
    int length;

    memset(pass, 0, sizeof(*pass));
    length = snprintf(fragment_source, sizeof(fragment_source),
        "precision highp float;\n"
        "uniform sampler2D u_weights;\n"
        "uniform sampler2D u_input;\n"
        "uniform float u_jitter;\n"
        "varying float v_row;\n"
        "void main() {\n"
        "  float sum = 0.0;\n"
        "  for (int i = 0; i < %d; ++i) {\n"
        "    float u = (float(i) + 0.5) * %.12f;\n"
        "    vec4 w = texture2D(u_weights, vec2(u, v_row));\n"
        "    vec4 x = texture2D(u_input, vec2(u, 0.5));\n"
        "    sum += dot(w, x);\n"
        "  }\n"
        "  gl_FragColor = vec4(fract(sum * 0.0009765625 + u_jitter),"
        " 0.0, 0.0, 1.0);\n"
        "}\n", taps, 1.0 / (double)taps);
    if (length < 0 || (size_t)length >= sizeof(fragment_source)) return -1;

    pass->program = create_program(fragment_source);
    if (!pass->program) return -1;
    pass->weights = create_rgba8_texture(taps, rows, seed);
    pass->input = create_rgba8_texture(taps, 1, seed + 1u);
    if (!pass->weights || !pass->input) return -1;
    pass->weights_loc = glGetUniformLocation(pass->program, "u_weights");
    pass->input_loc = glGetUniformLocation(pass->program, "u_input");
    pass->jitter_loc = glGetUniformLocation(pass->program, "u_jitter");
    pass->rows = rows;
    pass->taps = taps;
    pass->macs = (uint64_t)rows * (uint64_t)taps * UINT64_C(4);
    return 0;
}

static void destroy_gemv_pass(GemvPass *pass)
{
    if (pass->weights) glDeleteTextures(1, &pass->weights);
    if (pass->input) glDeleteTextures(1, &pass->input);
    if (pass->program) glDeleteProgram(pass->program);
    memset(pass, 0, sizeof(*pass));
}

static void draw_pass(const GemvPass *pass, float jitter)
{
    glViewport(0, 0, pass->rows, 1);
    glUseProgram(pass->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pass->weights);
    glUniform1i(pass->weights_loc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pass->input);
    glUniform1i(pass->input_loc, 1);
    glUniform1f(pass->jitter_loc, jitter);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static int init_egl(void)
{
    EGLConfig config = 0;
    EGLint num_configs = 0;
    EGLint major = 0, minor = 0;
    const EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint pbuffer_attrs[] = {
        EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE
    };
    const EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
    };

    if (screen_create_context(&g_screen_context, SCREEN_APPLICATION_CONTEXT) != 0) {
        fprintf(stderr, "screen_create_context failed\n");
        return -1;
    }
    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetDisplay failed err=0x%x\n", eglGetError());
        return -1;
    }
    if (!eglInitialize(g_display, &major, &minor)) {
        fprintf(stderr, "eglInitialize failed err=0x%x\n", eglGetError());
        return -1;
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "eglBindAPI failed err=0x%x\n", eglGetError());
        return -1;
    }
    if (!eglChooseConfig(g_display, config_attrs, &config, 1, &num_configs) ||
        num_configs < 1) {
        fprintf(stderr, "no GLES2 pbuffer config err=0x%x\n", eglGetError());
        return -1;
    }
    g_surface = eglCreatePbufferSurface(g_display, config, pbuffer_attrs);
    if (g_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreatePbufferSurface failed err=0x%x\n", eglGetError());
        return -1;
    }
    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, context_attrs);
    if (g_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "eglCreateContext failed err=0x%x\n", eglGetError());
        return -1;
    }
    if (!eglMakeCurrent(g_display, g_surface, g_surface, g_context)) {
        fprintf(stderr, "eglMakeCurrent failed err=0x%x\n", eglGetError());
        return -1;
    }

    printf("EGL=%d.%d vendor=%s\n", major, minor,
           eglQueryString(g_display, EGL_VENDOR));
    printf("GL renderer=%s version=%s\n",
           (const char *)glGetString(GL_RENDERER),
           (const char *)glGetString(GL_VERSION));
    return 0;
}

static int init_draw_target(void)
{
    static const GLfloat vertices[] = {
        -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,  1.0f
    };
    GLenum status;

    g_output_texture = create_rgba8_texture(3 * HIDDEN, 1, 91u);
    if (!g_output_texture) return -1;
    glGenFramebuffers(1, &g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g_output_texture, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete status=0x%x\n", (unsigned)status);
        return -1;
    }

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);
    return 0;
}

static void cleanup(void)
{
    if (g_context != EGL_NO_CONTEXT) {
        if (g_vbo) glDeleteBuffers(1, &g_vbo);
        if (g_fbo) glDeleteFramebuffers(1, &g_fbo);
        if (g_output_texture) glDeleteTextures(1, &g_output_texture);
        glFinish();
    }
    if (g_display != EGL_NO_DISPLAY) {
        (void)eglMakeCurrent(g_display, EGL_NO_SURFACE,
                             EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_context != EGL_NO_CONTEXT)
            (void)eglDestroyContext(g_display, g_context);
        if (g_surface != EGL_NO_SURFACE)
            (void)eglDestroySurface(g_display, g_surface);
        /* Intentionally do not call eglTerminate on this old shared BSP. */
    }
    if (g_screen_context) (void)screen_destroy_context(g_screen_context);
}

int main(void)
{
    GemvPass layer1, layer2, head;
    uint64_t total_macs;
    uint64_t begin, elapsed;
    double us_per_control;
    double throughput;
    double core_block_fraction;
    int i;

    memset(&layer1, 0, sizeof(layer1));
    memset(&layer2, 0, sizeof(layer2));
    memset(&head, 0, sizeof(head));
    signal(SIGALRM, alarm_handler);
    alarm(20);

    puts("HSSR GLES2 offscreen fragment-GEMV benchmark");
    puts("Safety: Screen context + EGL pbuffer only; no visible window or swap.");
    if (init_egl() != 0 || init_draw_target() != 0) goto fail;
    if (create_gemv_pass(&layer1, 3 * HIDDEN,
                         INPUT_FEATURES + HIDDEN, 1u) != 0) goto fail;
    if (create_gemv_pass(&layer2, 3 * HIDDEN,
                         2 * HIDDEN, 2u) != 0) goto fail;
    if (create_gemv_pass(&head, OUTPUT_FEATURES,
                         HIDDEN, 3u) != 0) goto fail;

    total_macs = layer1.macs + layer2.macs + head.macs;
    printf("mapped shapes: %dx%d + %dx%d + %dx%d = %" PRIu64
           " float MAC/control\n",
           layer1.rows, layer1.taps * 4,
           layer2.rows, layer2.taps * 4,
           head.rows, head.taps * 4,
           total_macs);

    for (i = 0; i < 8; ++i) {
        const float jitter = (float)i * 0.000001f;
        draw_pass(&layer1, jitter);
        draw_pass(&layer2, jitter);
        draw_pass(&head, jitter);
    }
    glFinish();

    begin = now_ns();
    for (i = 0; i < BENCH_ITERATIONS; ++i) {
        const float jitter = (float)(i & 255) * 0.000001f;
        draw_pass(&layer1, jitter);
        draw_pass(&layer2, jitter);
        draw_pass(&head, jitter);
    }
    glFinish();
    elapsed = now_ns() - begin;
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "GL error after benchmark\n");
        goto fail;
    }

    us_per_control = (double)elapsed / (double)BENCH_ITERATIONS / 1000.0;
    throughput = ((double)total_macs * (double)BENCH_ITERATIONS) /
                 (double)elapsed * 1000.0;
    core_block_fraction = us_per_control * 93.75 / 10000.0;
    printf("iterations=%d elapsed=%.3fms time=%.3fus/control\n",
           BENCH_ITERATIONS, (double)elapsed / 1000000.0, us_per_control);
    printf("throughput=%.1fMMAC/s synchronous_worker@93.75Hz=%.2f%%\n",
           throughput, core_block_fraction);

    destroy_gemv_pass(&head);
    destroy_gemv_pass(&layer2);
    destroy_gemv_pass(&layer1);
    cleanup();
    alarm(0);
    return 0;

fail:
    destroy_gemv_pass(&head);
    destroy_gemv_pass(&layer2);
    destroy_gemv_pass(&layer1);
    cleanup();
    alarm(0);
    return 1;
}
