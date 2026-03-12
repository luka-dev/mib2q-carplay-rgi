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
 * Shader source prefix — prepended to all shader sources.
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

#endif /* CR_GL_COMPAT_H */
