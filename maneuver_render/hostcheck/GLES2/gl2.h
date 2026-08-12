#ifndef HOSTCHECK_GLES2_H
#define HOSTCHECK_GLES2_H
typedef unsigned char GLubyte;

#define GL_VENDOR     0x1F00
#define GL_RENDERER   0x1F01
#define GL_VERSION    0x1F02
#define GL_EXTENSIONS 0x1F03

const GLubyte *glGetString(unsigned int name);
void glFinish(void);
#endif
