#ifndef ALTSCREEN_HOSTCHECK_GL2_H
#define ALTSCREEN_HOSTCHECK_GL2_H

#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef char GLchar;

#define GL_FALSE 0
#define GL_NO_ERROR 0
#define GL_VENDOR 0x1f00
#define GL_RENDERER 0x1f01
#define GL_VERSION 0x1f02
#define GL_EXTENSIONS 0x1f03
#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_COMPILE_STATUS 0x8b81
#define GL_LINK_STATUS 0x8b82
#define GL_TEXTURE0 0x84c0
#define GL_TEXTURE_2D 0x0de1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812f
#define GL_BLEND 0x0be2
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_RGBA 0x1908
#define GL_UNPACK_ALIGNMENT 0x0cf5
#define GL_TRIANGLE_STRIP 0x0005

const unsigned char *glGetString(GLenum name);
GLuint glCreateShader(GLenum type);
void glShaderSource(GLuint shader, GLsizei count, const GLchar **strings,
                    const GLint *lengths);
void glCompileShader(GLuint shader);
void glGetShaderiv(GLuint shader, GLenum pname, GLint *value);
void glGetShaderInfoLog(GLuint shader, GLsizei cap, GLsizei *used, GLchar *log);
void glDeleteShader(GLuint shader);
GLuint glCreateProgram(void);
void glAttachShader(GLuint program, GLuint shader);
void glLinkProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint *value);
void glGetProgramInfoLog(GLuint program, GLsizei cap, GLsizei *used, GLchar *log);
void glDeleteProgram(GLuint program);
GLint glGetAttribLocation(GLuint program, const GLchar *name);
GLint glGetUniformLocation(GLuint program, const GLchar *name);
void glGenTextures(GLsizei count, GLuint *textures);
void glDeleteTextures(GLsizei count, const GLuint *textures);
void glBindTexture(GLenum target, GLuint texture);
void glTexParameteri(GLenum target, GLenum pname, GLint value);
void glPixelStorei(GLenum pname, GLint value);
void glTexImage2D(GLenum target, GLint level, GLint internal_format,
                  GLsizei width, GLsizei height, GLint border, GLenum format,
                  GLenum type, const void *pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const void *pixels);
GLenum glGetError(void);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glDisable(GLenum cap);
void glUseProgram(GLuint program);
void glActiveTexture(GLenum texture);
void glUniform1i(GLint location, GLint value);
void glEnableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride,
                           const void *pointer);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glFinish(void);
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, void *pixels);

#endif
