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
#include "cluster_surface.h"

#include <screen/screen.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EP_DISPLAYABLE_ID          99
#define EP_QCOM_TILED_NV12          0x1000c
/* Exact native decoder allocation class. Do not add READ/WRITE/GLES bits to
 * the WINDOW itself: on this Screen BSP that can leave OMX and Screen referring
 * to different chroma storage (detailed luma but a solid-green picture). EGL
 * imports the same allocation through a non-owning pixmap alias below. */
#define EP_QCOM_VIDEO_USAGE         0x484
#define EP_RGBA_BUFFER_COUNT        2

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
    pthread_mutex_t gl_lock;
    int gl_lock_initialized;

    unsigned long frames;
    unsigned long failures;
    int first_success_logged;
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
    if (ep_make_program(p) != 0) return -1;
    eglSwapInterval(p->egl_display, 1);
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

screen_egl_present_t *screen_egl_present_create(int width, int height,
                                                 unsigned int source_count) {
    screen_egl_present_t *p;
    cluster_surface_cfg cfg;
    if (width <= 0 || height <= 0 || source_count == 0) return NULL;
    p = (screen_egl_present_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->width = width;
    p->height = height;
    p->source_count = source_count;
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
    if (ep_create_source_buffers(p) != 0) goto fail;
    if (ep_alias_source_pixmaps(p) != 0) goto fail;
    if (ep_init_egl(p) != 0) goto fail;
    if (ep_import_sources(p) != 0) goto fail;

    /* The OMX callback owns the context while drawing.  Release it from the
     * setup thread now; every frame binds and unbinds under gl_lock. */
    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    fprintf(stderr,
            "altscreen_render[egl]: ready %dx%d, %u native-32m WINDOW buffers -> attached pixmaps -> external EGLImage -> RGBA plane 99\n",
            width, height, source_count);
    return p;

fail:
    screen_egl_present_destroy(p);
    return NULL;
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

    if (!p || source_index >= p->source_count || filled_len == 0) return -1;
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
    if (!eglSwapBuffers(p->egl_display, p->egl_surface)) goto fail;
    p->frames++;
    if (!p->first_success_logged) {
        p->first_success_logged = 1;
        fprintf(stderr,
                "altscreen_render[egl]: VISIBLE PATH OK: Qcom 32m -> EGLImage external sampler -> RGBA8888 swap (%dx%d src=%u)\n",
                p->width, p->height, source_index);
    } else if ((p->frames % 600) == 0) {
        fprintf(stderr, "altscreen_render[egl]: %lu GPU RGBA frames swapped\n",
                p->frames);
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
    /* Permit the next callback thread to acquire this context cleanly. */
    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
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

    /* A callback that was already presenting finishes before this point.  The
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
    eglSwapInterval(p->egl_display, 1);
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
    unsigned int i;
    if (!p) return;
    if (p->gl_lock_initialized) pthread_mutex_lock(&p->gl_lock);
    if (p->egl_display != EGL_NO_DISPLAY &&
        p->egl_surface != EGL_NO_SURFACE &&
        p->egl_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                       p->egl_context);
    }
    if (p->textures) glDeleteTextures((GLsizei)p->source_count, p->textures);
    if (p->program) glDeleteProgram(p->program);
    if (p->destroy_image && p->images) {
        for (i = 0; i < p->source_count; ++i) {
            if (p->images[i] != EGL_NO_IMAGE_KHR)
                p->destroy_image(p->egl_display, p->images[i]);
        }
    }
    if (p->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (p->egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(p->egl_display, p->egl_surface);
        if (p->egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(p->egl_display, p->egl_context);
        eglTerminate(p->egl_display);
    }
    if (p->gl_lock_initialized) pthread_mutex_unlock(&p->gl_lock);

    /* EGL released its screen_ref_pixmap references above.  Remove the
     * non-owning aliases before freeing the WINDOW allocations they point at. */
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
    if (p->rgba_surface) cluster_surface_destroy(p->rgba_surface);
    if (p->gl_lock_initialized) pthread_mutex_destroy(&p->gl_lock);
    free(p);
}
