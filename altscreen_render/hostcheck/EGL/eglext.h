#ifndef ALTSCREEN_HOSTCHECK_EGLEXT_H
#define ALTSCREEN_HOSTCHECK_EGLEXT_H

#include "egl.h"

typedef void *EGLImageKHR;
#define EGL_NO_IMAGE_KHR ((EGLImageKHR)0)
typedef EGLImageKHR (*PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay, EGLContext,
                                                EGLenum, EGLClientBuffer,
                                                const EGLint *);
typedef EGLBoolean (*PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay, EGLImageKHR);

#endif
