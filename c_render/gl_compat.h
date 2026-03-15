/*
 * OpenGL compatibility header.
 *
 * Includes the correct GL headers for each platform and provides
 * compatibility shims so rendering code works on both OpenGL 2.1
 * (macOS) and GLES2 (QNX).
 */

#ifndef CR_GL_COMPAT_H
#define CR_GL_COMPAT_H

#ifdef PLATFORM_MACOS

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>

#elif defined(PLATFORM_QNX)

#include <GLES2/gl2.h>

#else
#error "Define PLATFORM_MACOS or PLATFORM_QNX"
#endif

/*
 * Shader source prefix -- prepended to all shader sources.
 * macOS OpenGL 2.1 compatibility profile accepts GLSL 1.20
 * which is close enough to GLES2 GLSL 1.00.
 * Main difference: no "precision" qualifiers in desktop GL.
 */
#ifdef PLATFORM_MACOS
#define SHADER_HEADER ""
#define SHADER_PRECISION ""
#else
#define SHADER_HEADER ""
#define SHADER_PRECISION "precision mediump float;\n"
#endif

/*
 * FBO compatibility -- core in GLES2, available via GL_EXT_framebuffer_object
 * on macOS OpenGL 2.1.
 */
#ifdef PLATFORM_MACOS
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER              GL_FRAMEBUFFER_EXT
#define GL_COLOR_ATTACHMENT0        GL_COLOR_ATTACHMENT0_EXT
#define GL_DEPTH_ATTACHMENT         GL_DEPTH_ATTACHMENT_EXT
#define GL_FRAMEBUFFER_COMPLETE     GL_FRAMEBUFFER_COMPLETE_EXT
#define GL_RENDERBUFFER             GL_RENDERBUFFER_EXT
#define glGenFramebuffers           glGenFramebuffersEXT
#define glDeleteFramebuffers        glDeleteFramebuffersEXT
#define glBindFramebuffer           glBindFramebufferEXT
#define glFramebufferTexture2D      glFramebufferTexture2DEXT
#define glCheckFramebufferStatus    glCheckFramebufferStatusEXT
#define glGenRenderbuffers          glGenRenderbuffersEXT
#define glDeleteRenderbuffers       glDeleteRenderbuffersEXT
#define glBindRenderbuffer          glBindRenderbufferEXT
#define glRenderbufferStorage       glRenderbufferStorageEXT
#define glFramebufferRenderbuffer   glFramebufferRenderbufferEXT
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING      GL_FRAMEBUFFER_BINDING_EXT
#endif
#define GL_DEPTH_FBO GL_DEPTH_COMPONENT
#else
#define GL_DEPTH_FBO GL_DEPTH_COMPONENT16
#endif

#endif /* CR_GL_COMPAT_H */
