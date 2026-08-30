#ifndef HOSTCHECK_EGL_H
#define HOSTCHECK_EGL_H
/* Minimal EGL stub — offline syntax-check of platform_qnx.c ONLY (real EGL is
 * BSP-only).  Types are opaque; constants are placeholders. */

typedef void        *EGLDisplay;
typedef void        *EGLSurface;
typedef void        *EGLContext;
typedef void        *EGLConfig;
typedef void        *EGLNativeWindowType;
typedef void        *EGLNativeDisplayType;
typedef int          EGLint;
typedef unsigned int EGLBoolean;

#define EGL_NO_DISPLAY             ((EGLDisplay)0)
#define EGL_NO_SURFACE             ((EGLSurface)0)
#define EGL_NO_CONTEXT             ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY        ((EGLNativeDisplayType)0)
#define EGL_FALSE                  0
#define EGL_TRUE                   1
#define EGL_NONE                   0x3038
#define EGL_SURFACE_TYPE           0x3033
#define EGL_WINDOW_BIT             0x0004
#define EGL_RENDERABLE_TYPE        0x3040
#define EGL_OPENGL_ES2_BIT         0x0004
#define EGL_RED_SIZE               0x3024
#define EGL_GREEN_SIZE             0x3023
#define EGL_BLUE_SIZE              0x3022
#define EGL_ALPHA_SIZE             0x3021
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_OPENGL_ES_API          0x30A0
#define EGL_VENDOR                 0x3053
#define EGL_VERSION                0x3054
#define EGL_EXTENSIONS             0x3055

EGLDisplay eglGetDisplay(EGLNativeDisplayType);
EGLBoolean eglInitialize(EGLDisplay, EGLint *, EGLint *);
EGLBoolean eglChooseConfig(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
EGLSurface eglCreateWindowSurface(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
EGLBoolean eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
EGLBoolean eglSwapBuffers(EGLDisplay, EGLSurface);
EGLBoolean eglSwapInterval(EGLDisplay, EGLint);
EGLBoolean eglDestroySurface(EGLDisplay, EGLSurface);
EGLBoolean eglDestroyContext(EGLDisplay, EGLContext);
EGLint     eglGetError(void);
EGLBoolean eglBindAPI(EGLint);
const char *eglQueryString(EGLDisplay, EGLint);

#endif
