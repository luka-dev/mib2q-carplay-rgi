/*
 * APQ8064 native video presentation:
 *
 *   OMX.qcom.video.decoder.avc
 *       -> Qualcomm 32m tiled Screen WINDOW buffer (format 0x1000c)
 *       -> attached Screen pixmap alias
 *       -> EGL_NATIVE_PIXMAP_KHR
 *       -> GL_OES_EGL_image_external texture
 *       -> GLES2 fullscreen draw (driver de-tiles and converts YUV to RGB)
 *       -> managed RGBA8888 cluster EGL window, displayable 99
 *
 * The MU1316 Qualcomm EGL subdriver is older than the public
 * EGL_QNX_image_native_buffer ABI: it rejects target 0x3551 with
 * EGL_BAD_PARAMETER.  Its native-image entry point accepts Screen pixmaps as
 * EGL_NATIVE_PIXMAP_KHR (0x30b0).  screen_attach_pixmap_buffer() makes a
 * non-owning pixmap alias of each decoder-compatible WINDOW buffer, so OMX still
 * receives exactly the native allocation it accepts while EGL gets the handle
 * type this driver understands.
 *
 * screen_read_window is deliberately not used: on this BSP it copied native
 * tiled bytes without YUV->RGBA conversion, producing green/black output.
 */
#include "screen_egl_present.h"
#include "pipeline_timing.h"
#include "loader/prism_baked.h"
#include "loader/prism_waiting.h"
#include "cluster_surface.h"
#include "loader/loader_layout.h"

#include <screen/screen.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define EP_DISPLAYABLE_ID          99
#define EP_QCOM_TILED_NV12          0x1000c
/* Exact native decoder allocation class. Do not add READ/WRITE/GLES bits to
 * the WINDOW itself: on this Screen BSP that can leave OMX and Screen referring
 * to different chroma storage (detailed luma but a solid-green picture). EGL
 * imports the same allocation through a non-owning pixmap alias below. */
#define EP_QCOM_VIDEO_USAGE         0x484
#define EP_RGBA_BUFFER_COUNT        2

/* Retain the interval-2 setting shared with maneuver_render for nominal 30 Hz
 * presentation on a 60 Hz surface. This does not lock the independent VC
 * capture clock: MU1316 videoencoderservice requests an integer 1000/fps ms
 * timer (33 ms for 30), and actual capture/post cadence needs measurement.
 * See docs/ALTSCREEN_STOCK_30FPS_PATH.md. */
#define EP_SWAP_INTERVAL            2

/* Present in KHR_image headers, but keep the value explicit for the QNX 6.5
 * build shim.  The live eglsub-screen.so compares this exact value. */
#ifndef EGL_NATIVE_PIXMAP_KHR
#define EGL_NATIVE_PIXMAP_KHR       0x30b0
#endif
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES     0x8d65
#endif

struct screen_egl_present {
    int width;
    int height;
    unsigned int source_count;

    cluster_surface_t *rgba_surface;
    screen_context_t screen_ctx;
    screen_window_t source_window;
    screen_buffer_t *source_buffers;
    screen_pixmap_t *source_pixmaps;
    void **source_pointers;
    unsigned int *source_capacities;

    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLConfig egl_config;
    PFNEGLCREATEIMAGEKHRPROC create_image;
    PFNEGLDESTROYIMAGEKHRPROC destroy_image;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture;
    EGLImageKHR *images;
    GLuint *textures;

    GLuint program;
    GLint attr_pos;
    GLint attr_uv;
    GLint uniform_tex;

    GLuint loader_program;
    GLint loader_attr_pos;
    GLint loader_u_ires, loader_u_itime;
    GLint loader_u_content, loader_u_sigrect;
    GLint loader_u_logo, loader_u_sig;
    GLuint logo_texture, sig_texture;
    pthread_t loader_thread;
    volatile int loader_thread_started;
    volatile int loader_stop;
    volatile int loader_exited;
    unsigned long loader_frames;
    unsigned long loader_failures;
    int loader_first_logged;
    /* Stable process-lifetime word owned by sink_omx_qnx.  The control thread
     * updates it atomically; EGL remains exclusively on the loader thread. */
    volatile int *loader_view_area_mode;
    volatile int *loader_layout_style;

    pthread_mutex_t gl_lock;
    int gl_lock_initialized;

    unsigned long frames;
    unsigned long failures;
    int first_success_logged;

    /* Cadence probe: arrival = decoded callback entry spacing, not phone FPS.
     * Synchronous presentation can delay the next callback. swap measures only
     * eglSwapBuffers; it excludes loader/lock/context waits, image refresh,
     * draw and glFinish, so a cheap swap does not exclude a GPU bottleneck. */
    uint64_t pace_last_entry_ns;
    uint64_t pace_arrival_sum_ns;
    uint64_t pace_arrival_max_ns;
    uint64_t pace_swap_sum_ns;
    uint64_t pace_swap_max_ns;
    unsigned long pace_samples, pace_swap_samples;
    uint64_t pace_work_sum_ns, pace_work_max_ns;
    unsigned long pace_work_samples;
};

static int ep_has_extension(const char *extensions, const char *wanted) {
    size_t n;
    const char *p;
    if (!extensions || !wanted || !wanted[0] || strchr(wanted, ' ')) return 0;
    n = strlen(wanted);
    p = extensions;
    while ((p = strstr(p, wanted)) != NULL) {
        if ((p == extensions || p[-1] == ' ') &&
            (p[n] == '\0' || p[n] == ' ')) return 1;
        p += n;
    }
    return 0;
}

/* eglsub-screen.so keeps its own swap state while the native window carries a
 * Screen property of the same name, and this firmware accepts eglSwapInterval()
 * without reliably updating the latter (proven in Tools/retroarch-qnx,
 * src/gfx/drivers_context/qnx_ctx.c:425-432).  Set both sides explicitly, and
 * the Screen side before eglCreateWindowSurface() so the subdriver cannot
 * snapshot libdisplayinit's default of 1. */
static void ep_set_native_swap_interval(screen_egl_present_t *p) {
    int interval = EP_SWAP_INTERVAL;
    int readback = -1;
    screen_window_t win;
    if (!p || !p->rgba_surface) return;
    win = cluster_surface_window(p->rgba_surface);
    if (!win) return;
    if (screen_set_window_property_iv(win, SCREEN_PROPERTY_SWAP_INTERVAL,
                                      &interval) != 0) {
        fprintf(stderr,
                "altscreen_render[egl]: Screen swap interval=%d failed errno=%d "
                "(EGL side still applied)\n", interval, errno);
        return;
    }
    screen_get_window_property_iv(win, SCREEN_PROPERTY_SWAP_INTERVAL, &readback);
    fprintf(stderr, "altscreen_render[egl]: Screen swap interval requested=%d readback=%d\n",
            interval, readback);
}

static GLuint ep_compile_shader(GLenum type, const char *source,
                                const char *label) {
    GLuint shader = glCreateShader(type);
    GLint ok = 0;
    if (!shader) return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[768];
        GLsizei used = 0;
        log[0] = '\0';
        glGetShaderInfoLog(shader, (GLsizei)sizeof(log) - 1, &used, log);
        log[(used >= 0 && used < (GLsizei)sizeof(log)) ? used : (GLsizei)sizeof(log) - 1] = '\0';
        fprintf(stderr, "altscreen_render[egl]: %s shader compile FAILED: %s\n",
                label, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int ep_make_program(screen_egl_present_t *p) {
    static const char *vertex_source =
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() { gl_Position=vec4(a_pos,0.0,1.0); v_uv=a_uv; }\n";
    static const char *fragment_source =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "uniform samplerExternalOES u_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() { gl_FragColor=texture2D(u_tex,v_uv); }\n";
    GLuint vs = 0, fs = 0;
    GLint ok = 0;

    vs = ep_compile_shader(GL_VERTEX_SHADER, vertex_source, "vertex");
    fs = ep_compile_shader(GL_FRAGMENT_SHADER, fragment_source, "external-YUV");
    if (!vs || !fs) goto fail;
    p->program = glCreateProgram();
    if (!p->program) goto fail;
    glAttachShader(p->program, vs);
    glAttachShader(p->program, fs);
    glLinkProgram(p->program);
    glGetProgramiv(p->program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[768];
        GLsizei used = 0;
        log[0] = '\0';
        glGetProgramInfoLog(p->program, (GLsizei)sizeof(log) - 1, &used, log);
        log[(used >= 0 && used < (GLsizei)sizeof(log)) ? used : (GLsizei)sizeof(log) - 1] = '\0';
        fprintf(stderr, "altscreen_render[egl]: program link FAILED: %s\n", log);
        goto fail;
    }
    p->attr_pos = glGetAttribLocation(p->program, "a_pos");
    p->attr_uv = glGetAttribLocation(p->program, "a_uv");
    p->uniform_tex = glGetUniformLocation(p->program, "u_tex");
    glDeleteShader(vs);
    glDeleteShader(fs);
    return (p->attr_pos >= 0 && p->attr_uv >= 0 && p->uniform_tex >= 0) ? 0 : -1;

fail:
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return -1;
}

/* PRISM waiting artwork only. Video never enters this shader or its thread. */
static int ep_make_loader_program(screen_egl_present_t *p) {
    static const char *vertex_source =
        "attribute vec2 a_pos;\n"
        "void main() { gl_Position=vec4(a_pos,0.0,1.0); }\n";
    GLuint vs = 0, fs = 0;
    GLint ok = 0;

    vs = ep_compile_shader(GL_VERTEX_SHADER, vertex_source, "loader vertex");
    fs = ep_compile_shader(GL_FRAGMENT_SHADER, PRISM_WAITING_GLES, "PRISM waiting");
    if (!vs || !fs) goto fail;
    p->loader_program = glCreateProgram();
    if (!p->loader_program) goto fail;
    glAttachShader(p->loader_program, vs);
    glAttachShader(p->loader_program, fs);
    glLinkProgram(p->loader_program);
    glGetProgramiv(p->loader_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[768];
        GLsizei used = 0;
        log[0] = '\0';
        glGetProgramInfoLog(p->loader_program, (GLsizei)sizeof(log) - 1,
                            &used, log);
        log[(used >= 0 && used < (GLsizei)sizeof(log))
                ? used : (GLsizei)sizeof(log) - 1] = '\0';
        fprintf(stderr,
                "altscreen_render[egl]: loader program link FAILED: %s\n",
                log);
        goto fail;
    }
    p->loader_attr_pos = glGetAttribLocation(p->loader_program, "a_pos");
    p->loader_u_ires   = glGetUniformLocation(p->loader_program, "iRes");
    p->loader_u_itime  = glGetUniformLocation(p->loader_program, "iTime");
    p->loader_u_content = glGetUniformLocation(p->loader_program, "uContent");
    p->loader_u_sigrect = glGetUniformLocation(p->loader_program, "uSigRect");
    p->loader_u_logo = glGetUniformLocation(p->loader_program, "uMaterial");
    p->loader_u_sig = glGetUniformLocation(p->loader_program, "uSig");
    glDeleteShader(vs);
    glDeleteShader(fs);
    return (p->loader_attr_pos >= 0 && p->loader_u_ires >= 0 &&
            p->loader_u_itime >= 0 && p->loader_u_content >= 0 &&
            p->loader_u_sigrect >= 0 && p->loader_u_logo >= 0 &&
            p->loader_u_sig >= 0) ? 0 : -1;

fail:
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return -1;
}

static GLuint ep_upload_rgba(const unsigned char *data, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static GLuint ep_upload_png(const unsigned char *bytes, size_t size) {
    int w, h;
    unsigned char *rgba = prism_decode_png(bytes, size, &w, &h);
    GLuint tex;
    if (!rgba) return 0;
    tex = ep_upload_rgba(rgba, w, h);
    prism_free_image(rgba);
    return tex;
}

static int ep_make_loader_textures(screen_egl_present_t *p) {
    p->logo_texture = ep_upload_png(PRISM_MATERIAL_PNG, sizeof(PRISM_MATERIAL_PNG));
    p->sig_texture = ep_upload_png(PRISM_SIGNATURE_PNG, sizeof(PRISM_SIGNATURE_PNG));
    if (!p->logo_texture || !p->sig_texture || glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "altscreen_render[egl]: PRISM art upload FAILED\n");
        return -1;
    }
    return 0;
}

static int ep_create_source_buffers(screen_egl_present_t *p) {
    int format = EP_QCOM_TILED_NV12;
    int usage = EP_QCOM_VIDEO_USAGE;
    int visible = 0;
    int size[2] = { p->width, p->height };
    unsigned int i;

    if (screen_create_window(&p->source_window, p->screen_ctx) != 0) goto fail;
#define EP_SCREEN(call, label) do {                                             \
        errno = 0;                                                              \
        if ((call) != 0) {                                                      \
            fprintf(stderr, "altscreen_render[egl]: %s failed errno=%d\n",    \
                    label, errno);                                              \
            goto fail;                                                         \
        }                                                                       \
    } while (0)
    EP_SCREEN(screen_set_window_property_iv(p->source_window,
                                             SCREEN_PROPERTY_VISIBLE,
                                             &visible), "source VISIBLE=0");
    EP_SCREEN(screen_set_window_property_iv(p->source_window,
                                             SCREEN_PROPERTY_FORMAT,
                                             &format), "source FORMAT=32m");
    EP_SCREEN(screen_set_window_property_iv(p->source_window,
                                             SCREEN_PROPERTY_USAGE,
                                             &usage), "source USAGE=native decoder 0x484");
    EP_SCREEN(screen_set_window_property_iv(p->source_window,
                                             SCREEN_PROPERTY_SIZE,
                                             size), "source SIZE");
    EP_SCREEN(screen_set_window_property_iv(p->source_window,
                                             SCREEN_PROPERTY_BUFFER_SIZE,
                                             size), "source BUFFER_SIZE");
    EP_SCREEN(screen_create_window_buffers(p->source_window,
                                            (int)p->source_count),
              "create source buffers");
#undef EP_SCREEN

    p->source_buffers = (screen_buffer_t *)calloc(p->source_count,
                                                   sizeof(*p->source_buffers));
    p->source_pointers = (void **)calloc(p->source_count,
                                         sizeof(*p->source_pointers));
    p->source_capacities = (unsigned int *)calloc(p->source_count,
                                                   sizeof(*p->source_capacities));
    if (!p->source_buffers || !p->source_pointers || !p->source_capacities) goto fail;
    if (screen_get_window_property_pv(p->source_window,
                                      SCREEN_PROPERTY_RENDER_BUFFERS,
                                      (void **)p->source_buffers) != 0) goto fail;
    for (i = 0; i < p->source_count; ++i) {
        int actual_format = 0, actual_usage = 0;
        int actual_size[2] = { 0, 0 };
        int offsets[3] = { 0, 0, 0 };
        int stride = 0;
        if (screen_get_buffer_property_pv(p->source_buffers[i],
                                          SCREEN_PROPERTY_POINTER,
                                          &p->source_pointers[i]) != 0 ||
            !p->source_pointers[i]) {
            fprintf(stderr,
                    "altscreen_render[egl]: source buffer %u pointer failed errno=%d\n",
                    i, errno);
            goto fail;
        }
        if (screen_get_buffer_property_iv(p->source_buffers[i],
                                          SCREEN_PROPERTY_STRIDE,
                                          &stride) != 0)
            stride = 0;
        screen_get_buffer_property_iv(p->source_buffers[i],
                                      SCREEN_PROPERTY_PLANAR_OFFSETS, offsets);
        /* Qualcomm 32m allocates luma/chroma scanlines in 32-row groups.
         * Screen exposes no byte-size property on this BSP, but it does expose
         * the exact stride and UV-plane offset.  Derive a conservative byte
         * capacity and let the OMX binder reject any allocation smaller than
         * the component's nBufferSize instead of risking an overrun. */
        if (stride > 0) {
            unsigned int chroma_rows = (unsigned int)((p->height + 1) / 2);
            chroma_rows = (chroma_rows + 31u) & ~31u;
            if (offsets[1] > 0)
                p->source_capacities[i] = (unsigned int)offsets[1]
                    + (unsigned int)stride * chroma_rows;
            else {
                unsigned int luma_rows = ((unsigned int)p->height + 31u) & ~31u;
                p->source_capacities[i] = (unsigned int)stride
                    * (luma_rows + chroma_rows);
            }
        }
        if (i == 0) {
            screen_get_buffer_property_iv(p->source_buffers[i],
                                          SCREEN_PROPERTY_FORMAT,
                                          &actual_format);
            screen_get_buffer_property_iv(p->source_buffers[i],
                                          SCREEN_PROPERTY_USAGE,
                                          &actual_usage);
            screen_get_buffer_property_iv(p->source_buffers[i],
                                          SCREEN_PROPERTY_BUFFER_SIZE,
                                          actual_size);
            fprintf(stderr,
                    "altscreen_render[egl]: source ABI fmt=0x%x usage=0x%x size=%dx%d stride=%d planar=%d,%d,%d capacity=%u ptr=%p\n",
                    actual_format, actual_usage, actual_size[0], actual_size[1],
                    stride, offsets[0], offsets[1], offsets[2],
                    p->source_capacities[i],
                    p->source_pointers[i]);
        }
    }
    return 0;

fail:
    return -1;
}

static int ep_alias_source_pixmaps(screen_egl_present_t *p) {
    int format = EP_QCOM_TILED_NV12;
    int usage = EP_QCOM_VIDEO_USAGE;
    int size[2] = { p->width, p->height };
    unsigned int i;

    p->source_pixmaps = (screen_pixmap_t *)calloc(p->source_count,
                                                   sizeof(*p->source_pixmaps));
    if (!p->source_pixmaps) return -1;
    for (i = 0; i < p->source_count; ++i) {
        errno = 0;
        if (screen_create_pixmap(&p->source_pixmaps[i], p->screen_ctx) != 0) {
            fprintf(stderr,
                    "altscreen_render[egl]: source pixmap %u create FAILED errno=%d\n",
                    i, errno);
            return -1;
        }
        if (screen_set_pixmap_property_iv(p->source_pixmaps[i],
                                           SCREEN_PROPERTY_FORMAT,
                                           &format) != 0 ||
            screen_set_pixmap_property_iv(p->source_pixmaps[i],
                                           SCREEN_PROPERTY_USAGE,
                                           &usage) != 0 ||
            screen_set_pixmap_property_iv(p->source_pixmaps[i],
                                           SCREEN_PROPERTY_BUFFER_SIZE,
                                           size) != 0) {
            fprintf(stderr,
                    "altscreen_render[egl]: source pixmap %u properties FAILED errno=%d\n",
                    i, errno);
            return -1;
        }
        errno = 0;
        if (screen_attach_pixmap_buffer(p->source_pixmaps[i],
                                        p->source_buffers[i]) != 0) {
            fprintf(stderr,
                    "altscreen_render[egl]: source pixmap %u attach FAILED errno=%d\n",
                    i, errno);
            return -1;
        }
        fprintf(stderr,
                "altscreen_render[egl]: source %u WINDOW buffer aliased as Screen pixmap\n",
                i);
    }
    return 0;
}

static int ep_init_egl(screen_egl_present_t *p) {
    const char *egl_ext;
    const char *gl_ext;
    EGLint major = 0, minor = 0, count = 0;
    EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLNativeWindowType native_window;

    p->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (p->egl_display == EGL_NO_DISPLAY) goto egl_fail;
    if (!eglInitialize(p->egl_display, &major, &minor)) goto egl_fail;
    egl_ext = eglQueryString(p->egl_display, EGL_EXTENSIONS);
    if (!ep_has_extension(egl_ext, "EGL_KHR_image")) {
        fprintf(stderr,
                "altscreen_render[egl]: required EGL image extension missing: %s\n",
                egl_ext ? egl_ext : "(null)");
        return -1;
    }
    if (!eglChooseConfig(p->egl_display, config_attributes, &p->egl_config, 1,
                         &count) || count < 1) goto egl_fail;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) goto egl_fail;
    p->egl_context = eglCreateContext(p->egl_display, p->egl_config,
                                      EGL_NO_CONTEXT, context_attributes);
    if (p->egl_context == EGL_NO_CONTEXT) goto egl_fail;
    native_window = (EGLNativeWindowType)cluster_surface_window(p->rgba_surface);
    p->egl_surface = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                             native_window, NULL);
    if (p->egl_surface == EGL_NO_SURFACE) goto egl_fail;
    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context)) goto egl_fail;

    gl_ext = (const char *)glGetString(GL_EXTENSIONS);
    if (!ep_has_extension(gl_ext, "GL_OES_EGL_image") ||
        !ep_has_extension(gl_ext, "GL_OES_EGL_image_external")) {
        fprintf(stderr,
                "altscreen_render[egl]: required GL external-image extensions missing: %s\n",
                gl_ext ? gl_ext : "(null)");
        return -1;
    }
    p->create_image = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    p->destroy_image = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    p->image_target_texture =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
        eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (!p->create_image || !p->destroy_image || !p->image_target_texture) {
        fprintf(stderr,
                "altscreen_render[egl]: image entry points unresolved create=%p destroy=%p target=%p\n",
                (void *)p->create_image, (void *)p->destroy_image,
                (void *)p->image_target_texture);
        return -1;
    }
    if (ep_make_program(p) != 0 || ep_make_loader_program(p) != 0 ||
        ep_make_loader_textures(p) != 0) return -1;
    if (!eglSwapInterval(p->egl_display, EP_SWAP_INTERVAL)) goto egl_fail;
    fprintf(stderr,
            "altscreen_render[egl]: live GPU=%s GL=%s EGL=%d.%d pixmap-image+external-image OK\n",
            (const char *)glGetString(GL_RENDERER),
            (const char *)glGetString(GL_VERSION), major, minor);
    return 0;

egl_fail:
    fprintf(stderr, "altscreen_render[egl]: EGL setup FAILED err=0x%x\n",
            (unsigned)eglGetError());
    return -1;
}

static int ep_import_sources(screen_egl_present_t *p) {
    unsigned int i;
    GLenum gl_error;
    p->images = (EGLImageKHR *)calloc(p->source_count, sizeof(*p->images));
    p->textures = (GLuint *)calloc(p->source_count, sizeof(*p->textures));
    if (!p->images || !p->textures) return -1;

    glGenTextures((GLsizei)p->source_count, p->textures);
    for (i = 0; i < p->source_count; ++i) {
        p->images[i] = p->create_image(p->egl_display, EGL_NO_CONTEXT,
                                       EGL_NATIVE_PIXMAP_KHR,
                                       (EGLClientBuffer)p->source_pixmaps[i],
                                       NULL);
        if (p->images[i] == EGL_NO_IMAGE_KHR) {
            fprintf(stderr,
                    "altscreen_render[egl]: eglCreateImageKHR source=%u FAILED err=0x%x\n",
                    i, (unsigned)eglGetError());
            return -1;
        }
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, p->textures[i]);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        p->image_target_texture(GL_TEXTURE_EXTERNAL_OES,
                                (GLeglImageOES)p->images[i]);
        gl_error = glGetError();
        if (gl_error != GL_NO_ERROR) {
            fprintf(stderr,
                    "altscreen_render[egl]: external texture bind source=%u FAILED gl=0x%x\n",
                    i, (unsigned)gl_error);
            return -1;
        }
    }
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    return 0;
}

/* Caller holds gl_lock with this presenter's EGL context current. */
static void ep_release_sources(screen_egl_present_t *p) {
    unsigned int i;
    if (!p) return;

    if (p->textures) glDeleteTextures((GLsizei)p->source_count, p->textures);
    if (p->destroy_image && p->images) {
        for (i = 0; i < p->source_count; ++i) {
            if (p->images[i] != EGL_NO_IMAGE_KHR)
                p->destroy_image(p->egl_display, p->images[i]);
        }
    }
    if (p->source_pixmaps) {
        for (i = 0; i < p->source_count; ++i) {
            if (p->source_pixmaps[i])
                screen_destroy_pixmap(p->source_pixmaps[i]);
        }
    }
    if (p->source_window) screen_destroy_window(p->source_window);

    free(p->source_buffers);
    free(p->source_pixmaps);
    free(p->source_pointers);
    free(p->source_capacities);
    free(p->images);
    free(p->textures);
    p->source_window = NULL;
    p->source_buffers = NULL;
    p->source_pixmaps = NULL;
    p->source_pointers = NULL;
    p->source_capacities = NULL;
    p->images = NULL;
    p->textures = NULL;
    p->source_count = 0;
}

screen_egl_present_t *screen_egl_present_create_output(int width, int height,
                                                       volatile int *view_area_mode,
                                                       volatile int *layout_style) {
    screen_egl_present_t *p;
    cluster_surface_cfg cfg;
    if (width <= 0 || height <= 0) return NULL;
    p = (screen_egl_present_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->width = width;
    p->height = height;
    p->loader_view_area_mode = view_area_mode;
    p->loader_layout_style = layout_style;
    p->egl_display = EGL_NO_DISPLAY;
    p->egl_surface = EGL_NO_SURFACE;
    p->egl_context = EGL_NO_CONTEXT;
    if (pthread_mutex_init(&p->gl_lock, NULL) != 0) goto fail;
    p->gl_lock_initialized = 1;

    memset(&cfg, 0, sizeof(cfg));
    cfg.id = EP_DISPLAYABLE_ID;
    cfg.width = width;
    cfg.height = height;
    cfg.format = SCREEN_FORMAT_RGBA8888;
    cfg.usage = SCREEN_USAGE_OPENGL_ES2;
    cfg.nbuffers = EP_RGBA_BUFFER_COUNT;
    cfg.transparent = 0;
    p->rgba_surface = cluster_surface_create(&cfg);
    if (!p->rgba_surface) goto fail;
    p->screen_ctx = cluster_surface_context(p->rgba_surface);
    ep_set_native_swap_interval(p);   /* must precede eglCreateWindowSurface */
    if (ep_init_egl(p) != 0) goto fail;

    /* Loader/OMX threads bind the context only while drawing. */
    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    fprintf(stderr,
            "altscreen_render[egl]: early RGBA output ready %dx%d plane=99 (sources deferred)\n",
            width, height);
    return p;

fail:
    screen_egl_present_destroy(p);
    return NULL;
}

int screen_egl_present_attach_sources(screen_egl_present_t *p,
                                      unsigned int source_count) {
    int result = -1;
    if (!p || source_count == 0 || !p->gl_lock_initialized) return -1;

    pthread_mutex_lock(&p->gl_lock);
    if (p->source_count == source_count && p->source_pointers && p->images) {
        result = 0;
        goto out;
    }
    if (p->source_count != 0) {
        fprintf(stderr,
                "altscreen_render[egl]: refusing live source-count change old=%u new=%u\n",
                p->source_count, source_count);
        goto out;
    }
    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context)) {
        fprintf(stderr,
                "altscreen_render[egl]: attach sources eglMakeCurrent FAILED err=0x%x\n",
                (unsigned)eglGetError());
        goto out;
    }

    p->source_count = source_count;
    if (ep_create_source_buffers(p) != 0 || ep_alias_source_pixmaps(p) != 0 ||
        ep_import_sources(p) != 0) {
        fprintf(stderr,
                "altscreen_render[egl]: attach %u native sources FAILED\n",
                source_count);
        ep_release_sources(p);
        goto release_context;
    }
    fprintf(stderr,
            "altscreen_render[egl]: attached %u native-32m sources to existing RGBA plane 99\n",
            source_count);
    result = 0;

release_context:
    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
out:
    pthread_mutex_unlock(&p->gl_lock);
    return result;
}

static uint64_t ep_monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int ep_loader_should_stop(screen_egl_present_t *p) {
    int stop;
    pthread_mutex_lock(&p->gl_lock);
    stop = p->loader_stop;
    pthread_mutex_unlock(&p->gl_lock);
    return stop;
}

static int ep_loader_frame(screen_egl_present_t *p, uint64_t elapsed_ms) {
    GLfloat positions[24];
    GLenum gl_error = GL_NO_ERROR;
    int ok = -1;
    int viewMode, layoutStyle;
    prism_layout_t layout;
    if (!p || !p->logo_texture || !p->sig_texture) return -1;
    viewMode = (p->loader_view_area_mode && *p->loader_view_area_mode == 1) ? 1 : 0;
    layoutStyle = (p->loader_layout_style &&
                   *p->loader_layout_style == LOADER_STYLE_CLASSIC)
        ? LOADER_STYLE_CLASSIC : LOADER_STYLE_SPORT;
    layout = prism_layout(viewMode, layoutStyle, PRISM_SIGNATURE_W, PRISM_SIGNATURE_H);
    prism_waiting_vertices(&layout, positions);

    pthread_mutex_lock(&p->gl_lock);
    if (p->loader_stop) {
        pthread_mutex_unlock(&p->gl_lock);
        return 1;
    }
    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context)) goto out;

    glViewport(0, 0, p->width, p->height);
    glDisable(GL_BLEND);
    glUseProgram(p->loader_program);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform2f(p->loader_u_ires, (float)p->width, (float)p->height);
    glUniform1f(p->loader_u_itime, (float)elapsed_ms / 1000.0f);
    glUniform4fv(p->loader_u_content, 1, layout.content);
    glUniform4fv(p->loader_u_sigrect, 1, layout.signature);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, p->logo_texture);
    glUniform1i(p->loader_u_logo, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, p->sig_texture);
    glUniform1i(p->loader_u_sig, 1);
    glEnableVertexAttribArray((GLuint)p->loader_attr_pos);
    glVertexAttribPointer((GLuint)p->loader_attr_pos, 2, GL_FLOAT, GL_FALSE, 0,
                          positions);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    glDisableVertexAttribArray((GLuint)p->loader_attr_pos);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR ||
        !eglSwapBuffers(p->egl_display, p->egl_surface)) goto out;

    p->loader_frames++;
    if (!p->loader_first_logged) {
        p->loader_first_logged = 1;
        fprintf(stderr,
                "altscreen_render[loader]: PRISM waiting window 99 visible %dx%d @30fps\n",
                p->width, p->height);
    }
    ok = 0;

out:
    if (ok != 0) {
        p->loader_failures++;
        if (p->loader_failures <= 3)
            fprintf(stderr,
                    "altscreen_render[loader]: frame FAILED #%lu egl=0x%x gl=0x%x\n",
                    p->loader_failures, (unsigned)eglGetError(),
                    (unsigned)gl_error);
    }
    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    pthread_mutex_unlock(&p->gl_lock);
    return ok;
}

/* The waiting loader stops completely before the first video draw. Preserve
 * swap-interval pacing; only wait for unused frame time if an invisible QNX
 * window returns early. This wait is outside gl_lock and never applies to video. */
static void *ep_loader_thread_main(void *context) {
    screen_egl_present_t *p = (screen_egl_present_t *)context;
    uint64_t started = ep_monotonic_ns();

    while (!ep_loader_should_stop(p)) {
        uint64_t now = ep_monotonic_ns();
        uint64_t elapsed_ms = (started != 0 && now >= started)
            ? (now - started) / 1000000ULL : 0;

        if (ep_loader_frame(p, elapsed_ms) == 1) break;
        uint64_t remaining = alt_frame_remaining_ns(now, ep_monotonic_ns());
        if (remaining) {
            struct timespec pause = {0, (long)remaining};
            while (nanosleep(&pause, &pause) != 0 && errno == EINTR && !p->loader_stop) {}
        }
    }
    __sync_synchronize();
    p->loader_exited = 1;
    __sync_synchronize();
    return NULL;
}

int screen_egl_present_start_loader(screen_egl_present_t *p) {
    int result;
    if (!p || !p->gl_lock_initialized) return -1;
    pthread_mutex_lock(&p->gl_lock);
    if (p->loader_thread_started) {
        pthread_mutex_unlock(&p->gl_lock);
        return 0;
    }
    p->loader_stop = 0;
    p->loader_exited = 0;
    p->loader_frames = 0;
    p->loader_failures = 0;
    p->loader_first_logged = 0;
    result = pthread_create(&p->loader_thread, NULL, ep_loader_thread_main, p);
    if (result == 0) p->loader_thread_started = 1;
    else p->loader_exited = 1;
    pthread_mutex_unlock(&p->gl_lock);
    if (result != 0)
        fprintf(stderr,
                "altscreen_render[loader]: thread create FAILED rc=%d\n",
                result);
    return result == 0 ? 0 : -1;
}

void screen_egl_present_stop_loader(screen_egl_present_t *p) {
    pthread_t thread;
    int started;
    uint64_t deadline;
    if (!p || !p->gl_lock_initialized) return;

    /* Do not take gl_lock to request stop: the loader deliberately holds that
     * lock across eglSwapBuffers(), and an Adreno/WFD wedge there would block
     * this caller forever before a timeout could even start. */
    started = p->loader_thread_started;
    __sync_lock_test_and_set(&p->loader_stop, 1);
    thread = p->loader_thread;
    if (!started) return;

    deadline = ep_monotonic_ns() + ALT_LOADER_STOP_WAIT_MS * 1000000ULL;
    while (!p->loader_exited) {
        uint64_t now = ep_monotonic_ns();
        struct timespec pause = { 0, 10L * 1000000L };
        if (now == 0 || now >= deadline) {
            fprintf(stderr,
                    "altscreen_render[loader]: stop TIMEOUT in EGL/Adreno handoff; exiting isolated renderer safely\n");
            /* The loader may still own EGL/native images. Never free storage
             * under it and never let OMX's FillDone callback remain wedged.
             * The external supervisor will restart this isolated service. */
            _exit(87);
        }
        nanosleep(&pause, NULL);
    }
    __sync_synchronize();
    /* The terminal flag makes this a collection-only join. */
    pthread_join(thread, NULL);
    pthread_mutex_lock(&p->gl_lock);
    p->loader_thread_started = 0;
    pthread_mutex_unlock(&p->gl_lock);
    fprintf(stderr,
            "altscreen_render[loader]: stopped for decoded-video takeover\n");
}

void *screen_egl_present_source_pointer(screen_egl_present_t *p,
                                        unsigned int index) {
    if (!p || index >= p->source_count) return NULL;
    return p->source_pointers[index];
}

unsigned int screen_egl_present_source_capacity(screen_egl_present_t *p,
                                                 unsigned int index) {
    if (!p || index >= p->source_count || !p->source_capacities) return 0;
    return p->source_capacities[index];
}

int screen_egl_present_frame(screen_egl_present_t *p,
                             unsigned int source_index,
                             unsigned int filled_len) {
    static const GLfloat positions[] = {
        -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,  1.0f
    };
    /* Screen native buffers are top-left-origin; flip V for GLES coordinates.
     * Do not flip S: the first on-unit colour run proved that doing so mirrors
     * road labels horizontally on the physical cluster. */
    static const GLfloat texcoords[] = {
         0.0f, 1.0f,  1.0f, 1.0f,
         0.0f, 0.0f,  1.0f, 0.0f
    };
    GLenum gl_error = GL_NO_ERROR;
    const char *stage = "validate";
    int ok = -1;
    uint64_t entry_ns, swap_begin_ns;

    if (!p || source_index >= p->source_count || filled_len == 0) return -1;

    /* Worker entry spacing includes queue starvation and previous presentation
     * time. It is not the phone receive rate or the OMX callback interval. */
    entry_ns = ep_monotonic_ns();
    if (entry_ns && p->pace_last_entry_ns && entry_ns > p->pace_last_entry_ns) {
        uint64_t d = entry_ns - p->pace_last_entry_ns;
        p->pace_arrival_sum_ns += d;
        if (d > p->pace_arrival_max_ns) p->pace_arrival_max_ns = d;
        p->pace_samples++;
    }
    if (entry_ns) p->pace_last_entry_ns = entry_ns;
    /* The join is the ownership barrier: after it returns no loader draw can
     * race this first decoded frame or any subsequent video swap. */
    screen_egl_present_stop_loader(p);
    pthread_mutex_lock(&p->gl_lock);
    stage = "eglMakeCurrent";
    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context)) goto fail;

    /* The EGLImage objects are created before OMX ever writes these buffers.
     * Adreno 320 updates luma through the persistent binding, but on this old
     * driver the UV plane/cache state can remain from image creation (solid
     * green output). Re-target the same image after FillBufferDone so both
     * planes are acquired for this frame. No allocation is created here. */
    stage = "refresh external image";
    (void)eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, p->textures[source_index]);
    p->image_target_texture(GL_TEXTURE_EXTERNAL_OES,
                            (GLeglImageOES)p->images[source_index]);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) goto fail;

    stage = "draw external image";
    glViewport(0, 0, p->width, p->height);
    glDisable(GL_BLEND);
    glUseProgram(p->program);
    glUniform1i(p->uniform_tex, 0);
    glEnableVertexAttribArray((GLuint)p->attr_pos);
    glEnableVertexAttribArray((GLuint)p->attr_uv);
    glVertexAttribPointer((GLuint)p->attr_pos, 2, GL_FLOAT, GL_FALSE, 0,
                          positions);
    glVertexAttribPointer((GLuint)p->attr_uv, 2, GL_FLOAT, GL_FALSE, 0,
                          texcoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFinish();
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) goto fail;

    stage = "eglSwapBuffers";
    swap_begin_ns = ep_monotonic_ns();
    if (!eglSwapBuffers(p->egl_display, p->egl_surface)) goto fail;
    {
        uint64_t swap_end_ns = ep_monotonic_ns();
        if (swap_begin_ns && swap_end_ns > swap_begin_ns) {
            uint64_t d = swap_end_ns - swap_begin_ns;
            p->pace_swap_samples++;
            p->pace_swap_sum_ns += d;
            if (d > p->pace_swap_max_ns) p->pace_swap_max_ns = d;
        }
    }
    p->frames++;
    if (!p->first_success_logged) {
        p->first_success_logged = 1;
        fprintf(stderr,
                "altscreen_render[egl]: VISIBLE PATH OK: Qcom 32m -> EGLImage external sampler -> RGBA8888 swap (%dx%d src=%u)\n",
                p->width, p->height, source_index);
    } else if ((p->frames % 600) == 0) {
        unsigned long n = p->pace_samples, swaps = p->pace_swap_samples;
        fprintf(stderr,
                "altscreen_render[egl]: %lu GPU RGBA frames swapped; "
                "worker-entry avg=%lu.%02lu max=%lu.%02lu ms, swap avg=%lu.%02lu max=%lu.%02lu ms (interval=%d)\n",
                p->frames,
                n ? (unsigned long)(p->pace_arrival_sum_ns / n / 1000000ULL) : 0UL,
                n ? (unsigned long)(p->pace_arrival_sum_ns / n / 10000ULL % 100ULL) : 0UL,
                (unsigned long)(p->pace_arrival_max_ns / 1000000ULL),
                (unsigned long)(p->pace_arrival_max_ns / 10000ULL % 100ULL),
                swaps ? (unsigned long)(p->pace_swap_sum_ns / swaps / 1000000ULL) : 0UL,
                swaps ? (unsigned long)(p->pace_swap_sum_ns / swaps / 10000ULL % 100ULL) : 0UL,
                (unsigned long)(p->pace_swap_max_ns / 1000000ULL),
                (unsigned long)(p->pace_swap_max_ns / 10000ULL % 100ULL),
                EP_SWAP_INTERVAL);
        p->pace_arrival_sum_ns = 0;
        p->pace_arrival_max_ns = 0;
        p->pace_swap_sum_ns = 0;
        p->pace_swap_max_ns = 0;
        p->pace_samples = p->pace_swap_samples = 0;
    }
    ok = 0;
    goto out;

fail:
    p->failures++;
    if (p->failures <= 3 || (p->failures % 120) == 0) {
        fprintf(stderr,
                "altscreen_render[egl]: presentation FAILED #%lu stage='%s' egl=0x%x gl=0x%x src=%u\n",
                p->failures, stage, (unsigned)eglGetError(),
                (unsigned)gl_error, source_index);
    }
out:
    /* Release ownership for output recovery or the reconnect loader. Only the
     * presenter worker draws video; the OMX callback never binds EGL. */
    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    {
        uint64_t end_ns = ep_monotonic_ns();
        if (entry_ns && end_ns >= entry_ns) {
            uint64_t work = end_ns - entry_ns;
            p->pace_work_samples++;
            p->pace_work_sum_ns += work;
            if (work > p->pace_work_max_ns) p->pace_work_max_ns = work;
        }
        if (p->pace_work_samples == 600) {
            fprintf(stderr, "altscreen_render[egl]: full present incl loader/lock/UV/GPU/swap/unbind avg=%llu us max=%llu us\n",
                    (unsigned long long)(p->pace_work_sum_ns / p->pace_work_samples / 1000ULL),
                    (unsigned long long)(p->pace_work_max_ns / 1000ULL));
            p->pace_work_sum_ns = p->pace_work_max_ns = p->pace_work_samples = 0;
        }
    }
    pthread_mutex_unlock(&p->gl_lock);
    return ok;
}

int screen_egl_present_recover_output(screen_egl_present_t *p) {
    EGLNativeWindowType native_window;
    EGLSurface new_surface = EGL_NO_SURFACE;
    int was_lost;
    int ok = -1;

    if (!p || !p->rgba_surface || p->egl_display == EGL_NO_DISPLAY ||
        p->egl_context == EGL_NO_CONTEXT) return -1;

    /* A worker that was already presenting finishes before this point.  The
     * source buffers are deliberately outside this recovery operation. */
    pthread_mutex_lock(&p->gl_lock);
    was_lost = cluster_surface_lost(p->rgba_surface);
    fprintf(stderr,
            "altscreen_render[egl]: output recovery begin (plane99 lost=%d, source buffers preserved)\n",
            was_lost);

    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    if (p->egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(p->egl_display, p->egl_surface);
        p->egl_surface = EGL_NO_SURFACE;
    }

    if (cluster_surface_recreate(p->rgba_surface) != 0) {
        fprintf(stderr,
                "altscreen_render[egl]: output recovery FAILED recreating managed plane99\n");
        goto out;
    }
    ep_set_native_swap_interval(p); /* before EGL snapshots the recreated window */
    native_window =
        (EGLNativeWindowType)cluster_surface_window(p->rgba_surface);
    new_surface = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                          native_window, NULL);
    if (new_surface == EGL_NO_SURFACE) {
        fprintf(stderr,
                "altscreen_render[egl]: output recovery FAILED creating EGLSurface err=0x%x\n",
                (unsigned)eglGetError());
        goto out;
    }
    p->egl_surface = new_surface;
    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context)) {
        fprintf(stderr,
                "altscreen_render[egl]: output recovery FAILED binding EGLSurface err=0x%x\n",
                (unsigned)eglGetError());
        eglDestroySurface(p->egl_display, p->egl_surface);
        p->egl_surface = EGL_NO_SURFACE;
        goto out;
    }
    if (!eglSwapInterval(p->egl_display, EP_SWAP_INTERVAL)) {
        fprintf(stderr, "altscreen_render[egl]: recovery swap interval rejected err=0x%x\n", (unsigned)eglGetError());
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(p->egl_display, p->egl_surface);
        p->egl_surface = EGL_NO_SURFACE;
        goto out;
    }
    ep_set_native_swap_interval(p);
    p->first_success_logged = 0;
    fprintf(stderr,
            "altscreen_render[egl]: output surface recreated; OMX/EGLImage source ABI unchanged\n");
    ok = 0;

out:
    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    pthread_mutex_unlock(&p->gl_lock);
    return ok;
}

void screen_egl_present_destroy(screen_egl_present_t *p) {
    int hidden = 0;
    struct timespec release_wait;
    if (!p) return;
    screen_egl_present_stop_loader(p);
    if (p->gl_lock_initialized) pthread_mutex_lock(&p->gl_lock);
    if (p->egl_display != EGL_NO_DISPLAY &&
        p->egl_surface != EGL_NO_SURFACE &&
        p->egl_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                       p->egl_context);
    }
    /* Stop scanout and flush every external-image read before destroying the
     * 32m EGLImages.  Without this grace the MU1316 WFD driver reports twelve
     * DEFER DESTROY nativeImage entries with uUsageCnt=1 and the next EGL client
     * may crash during eglInitialize. */
    if (p->rgba_surface && cluster_surface_window(p->rgba_surface))
        screen_set_window_property_iv(cluster_surface_window(p->rgba_surface),
                                      SCREEN_PROPERTY_VISIBLE, &hidden);
    if (p->egl_context != EGL_NO_CONTEXT) glFinish();
    release_wait.tv_sec = 0;
    release_wait.tv_nsec = 50L * 1000L * 1000L; /* >=3 frames at 60 Hz */
    while (nanosleep(&release_wait, &release_wait) != 0 && errno == EINTR) {}
    ep_release_sources(p);
    if (p->logo_texture) glDeleteTextures(1, &p->logo_texture);
    if (p->sig_texture) glDeleteTextures(1, &p->sig_texture);
    if (p->loader_program) glDeleteProgram(p->loader_program);
    if (p->program) glDeleteProgram(p->program);
    if (p->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (p->egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(p->egl_display, p->egl_surface);
        if (p->egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(p->egl_display, p->egl_context);
        /* Do NOT eglTerminate on this old Qualcomm stack. EGLDisplay-wide
         * teardown collides with the HMI/maneuver clients even though they are
         * separate processes; process exit reclaims our remaining display ref. */
    }
    if (p->gl_lock_initialized) pthread_mutex_unlock(&p->gl_lock);

    if (p->rgba_surface) cluster_surface_destroy(p->rgba_surface);
    if (p->gl_lock_initialized) pthread_mutex_destroy(&p->gl_lock);
    free(p);
}
