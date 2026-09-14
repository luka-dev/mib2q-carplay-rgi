#include "cursor_plane.h"
#include "cursor_gl.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <time.h>
#include <screen/screen.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* The shipped MU1316 BSP uses 17 here; the public compatibility header's newer
 * enum is 46. This is the same BSP value as common/cluster_surface.c. */
#define HU_TRANSPARENCY 17
#define HU_MANAGER_STRING 152
#define HU_SOURCE_OVER 2

static struct {
    int (*create_context)(screen_context_t *, int);
    int (*destroy_context)(screen_context_t);
    int (*create_window)(screen_window_t *, screen_context_t);
    int (*destroy_window)(screen_window_t);
    int (*set_iv)(screen_window_t, int, const int *);
    int (*set_cv)(screen_window_t, int, int, const char *);
    int (*get_cv)(screen_window_t, int, int, char *);
    int (*get_pv)(screen_window_t, int, void **);
    int (*buffer_iv)(screen_buffer_t, int, int *);
    int (*buffer_pv)(screen_buffer_t, int, void **);
    int (*create_buffers)(screen_window_t, int);
    int (*post)(screen_window_t, screen_buffer_t, int, const int *, int);
    int (*manage)(screen_window_t, const char *);
} api;
static void *screen_library;
static int api_ready;
static screen_context_t ctx;
static screen_window_t win;
static screen_buffer_t buffers[2];
static unsigned canvas_w, canvas_h;
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLContext gl_context = EGL_NO_CONTEXT;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLConfig config;
static cursor_gl_t scene;
static int scene_ready, egl_initialized;

static int init_egl(void) {
    EGLint count=0;
    const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
        EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    const EGLint context_attributes[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    if (gl_context==EGL_NO_CONTEXT) {
        if (!egl_initialized) {
            display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (display==EGL_NO_DISPLAY || !eglInitialize(display,NULL,NULL)) goto fail;
            egl_initialized=1;
        }
        if (!eglChooseConfig(display,attributes,&config,1,&count) || count<1 ||
            !eglBindAPI(EGL_OPENGL_ES_API)) goto fail;
        gl_context=eglCreateContext(display,config,EGL_NO_CONTEXT,context_attributes);
        if (gl_context==EGL_NO_CONTEXT) goto fail;
    }
    surface=eglCreateWindowSurface(display,config,(EGLNativeWindowType)win,NULL);
    if (surface==EGL_NO_SURFACE || !eglMakeCurrent(display,surface,surface,gl_context)) goto fail;
    // The native loop paces frames; an invisible window must not wait for scanout.
    if (!eglSwapInterval(display,0)) goto fail;
    if (!scene_ready) {
        if (cursor_gl_init(&scene)) goto fail;
        scene_ready=1;
    }
    return 0;
fail:
    fprintf(stderr,"cursor-plane: EGL/PRISM initialization failed 0x%x\n",(unsigned)eglGetError());
    return -1;
}

static int load_api(void) {
    if (api_ready) return 0;
    if (!screen_library) screen_library = dlopen("libscreen.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!screen_library) return -1;
#define BIND(field, name) do { *(void **)(&api.field) = dlsym(screen_library, #name); if (!api.field) return -1; } while (0)
    BIND(create_context, screen_create_context); BIND(destroy_context, screen_destroy_context);
    BIND(create_window, screen_create_window); BIND(destroy_window, screen_destroy_window);
    BIND(set_iv, screen_set_window_property_iv); BIND(set_cv, screen_set_window_property_cv);
    BIND(get_cv, screen_get_window_property_cv); BIND(get_pv, screen_get_window_property_pv);
    BIND(buffer_iv, screen_get_buffer_property_iv); BIND(buffer_pv, screen_get_buffer_property_pv);
    BIND(create_buffers, screen_create_window_buffers); BIND(post, screen_post_window);
    BIND(manage, screen_manage_window);
#undef BIND
    api_ready = 1;
    return 0;
}

void cursor_plane_close(void) {
    if (win) {
        int hidden=0;
        api.set_iv(win,SCREEN_PROPERTY_VISIBLE,&hidden);
    }
    if (surface!=EGL_NO_SURFACE) {
        // Finish our own draws before releasing storage read by the compositor.
        if (eglGetCurrentContext()==gl_context) glFinish();
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        eglDestroySurface(display,surface); surface=EGL_NO_SURFACE;
        struct timespec release_wait={0,50000000};
        while (nanosleep(&release_wait,&release_wait)!=0 && errno==EINTR) {}
    }
    if (win) api.destroy_window(win);
    if (ctx) api.destroy_context(ctx);
    win=NULL; ctx=NULL;
    memset(buffers,0,sizeof(buffers)); canvas_w=canvas_h=0;
    // Keep context-local shaders/assets for the next cursor gesture. No window remains.
    // Do not eglTerminate: display-wide teardown is unsafe on this Qualcomm stack.
}

int cursor_plane_open(unsigned width, unsigned height) {
    int i, stride, size[2], hidden = 0, format = SCREEN_FORMAT_RGBA8888;
    int usage = SCREEN_USAGE_READ | SCREEN_USAGE_WRITE | SCREEN_USAGE_OPENGL_ES2, transparency = HU_SOURCE_OVER;
    void *pixels;
    if (win && canvas_w == width && canvas_h == height) return 0;
    cursor_plane_close();
    if (load_api() != 0) return -1;
    size[0] = width; size[1] = height;
#define CHECK(call) do { if ((call) != 0) { fprintf(stderr, "cursor-plane: %s errno=%d\n", #call, errno); goto fail; } } while (0)
    CHECK(api.create_context(&ctx, SCREEN_APPLICATION_CONTEXT));
    CHECK(api.create_window(&win, ctx));
    // Never expose an unmanaged cursor over stock HMI. DM owns visibility.
    CHECK(api.set_iv(win, SCREEN_PROPERTY_VISIBLE, &hidden));
    CHECK(api.set_cv(win, SCREEN_PROPERTY_ID_STRING, 3, "103"));
    CHECK(api.set_iv(win, SCREEN_PROPERTY_FORMAT, &format));
    CHECK(api.set_iv(win, SCREEN_PROPERTY_USAGE, &usage));
    CHECK(api.set_iv(win, SCREEN_PROPERTY_SIZE, size));
    CHECK(api.set_iv(win, SCREEN_PROPERTY_BUFFER_SIZE, size));
    CHECK(api.set_iv(win, HU_TRANSPARENCY, &transparency));
    {
        int interval=0;
        CHECK(api.set_iv(win,SCREEN_PROPERTY_SWAP_INTERVAL,&interval));
    }
    CHECK(api.create_buffers(win, 2));
    CHECK(api.get_pv(win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)buffers));
    for (i = 0; i < 2; i++) {
        CHECK(api.buffer_iv(buffers[i], SCREEN_PROPERTY_STRIDE, &stride));
        CHECK(api.buffer_pv(buffers[i], SCREEN_PROPERTY_POINTER, &pixels));
        if (!pixels || stride < (int)width * 4 || stride > 16384) goto fail;
        memset(pixels, 0, (size_t)stride * height);
    }
    // No wait for scanout: this window is deliberately invisible until adoption.
    CHECK(api.post(win, buffers[0], 0, NULL, 0));
    if (init_egl()!=0) goto fail;
    // Adoption happens only after both buffers and the first posted frame are transparent.
    CHECK(api.manage(win, "How are you gentlemen?"));
    canvas_w = width; canvas_h = height;
    return 0;
fail:
    cursor_plane_close();
    return -1;
#undef CHECK
}

int cursor_plane_managed(void) {
    char manager[64];
    if (!win) return 0;
    memset(manager, 0, sizeof(manager));
    if (api.get_cv(win, HU_MANAGER_STRING, sizeof(manager) - 1, manager) != 0) return 0;
    return strcmp(manager, "All your base are belong to us!") == 0;
}

int cursor_plane_draw(const cursor_frame *f) {
    if (!win || f->width!=canvas_w || f->height!=canvas_h || !cursor_plane_managed()) return -1;
    if (cursor_gl_draw(&scene,f)!=0) return -1;
    if (!eglSwapBuffers(display,surface)) {
        fprintf(stderr,"cursor-plane: EGL post failed 0x%x\n",(unsigned)eglGetError());
        return -1;
    }
    return 0;
}
