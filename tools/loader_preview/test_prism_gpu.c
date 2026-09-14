/* Verify the owned-frame cache convention used by the QNX handoff. */
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/prism.h"
#include "../../common/prism_baked.h"
#include "prism_demo_baked.h"

static GLuint program(const char *vertex,const char *fragment) {
    GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
    const char *sources[2]={vertex,fragment};
    for(int i=0;i<2;i++) {
        GLint ok=0;glShaderSource(shaders[i],1,&sources[i],NULL);glCompileShader(shaders[i]);
        glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&ok);
        if(!ok) { char log[1024];glGetShaderInfoLog(shaders[i],sizeof(log),NULL,log);fprintf(stderr,"%s\n",log); }
        assert(ok);
    }
    GLuint p=glCreateProgram();glAttachShader(p,shaders[0]);glAttachShader(p,shaders[1]);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);
    glDeleteShader(shaders[0]);glDeleteShader(shaders[1]);return p;
}
static GLuint texture(const unsigned char *data) {
    GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,PRISM_WIDTH,PRISM_HEIGHT,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    return t;
}
static void quad(void) {
    glBegin(GL_QUADS);glVertex2f(-1,-1);glVertex2f(1,-1);glVertex2f(1,1);glVertex2f(-1,1);glEnd();
}
int main(void) {
    assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GL_FALSE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER,GL_FALSE);
    GLFWwindow *window=glfwCreateWindow(PRISM_WIDTH,PRISM_HEIGHT,"PRISM cache check",NULL,NULL);assert(window);
    glfwMakeContextCurrent(window);
    int w,h;unsigned char *expected=prism_decode_png(PRISM_DEMO_PNG,sizeof(PRISM_DEMO_PNG),&w,&h);
    assert(expected && w==PRISM_WIDTH && h==PRISM_HEIGHT);
    GLuint source=texture(expected),cache=texture(NULL),fbo;
    glGenFramebuffersEXT(1,&fbo);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,cache,0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
    GLuint copy=program("#version 120\nvarying vec2 uv;void main(){gl_Position=gl_Vertex;uv=gl_Vertex.xy*.5+.5;}",
                        "#version 120\nvarying vec2 uv;uniform sampler2D src;void main(){gl_FragColor=texture2D(src,uv);}");
    glUseProgram(copy);glUniform1i(glGetUniformLocation(copy,"src"),0);
    glBindTexture(GL_TEXTURE_2D,source);glViewport(0,0,w,h);quad();glFinish();
    /* A return transition must remain correct after its input resource dies. */
    glDeleteTextures(1,&source);glDeleteProgram(copy);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    GLuint prism=program("#version 120\nvoid main(){gl_Position=gl_Vertex;}",PRISM_FRAGMENT_DESKTOP);
    glUseProgram(prism);glUniform2f(glGetUniformLocation(prism,"iRes"),w,h);
    glUniform1f(glGetUniformLocation(prism,"uStreamMix"),1);
    glUniform1i(glGetUniformLocation(prism,"uStream"),0);glBindTexture(GL_TEXTURE_2D,cache);quad();glFinish();
    assert(glGetError()==GL_NO_ERROR);
    unsigned char *pixels=malloc((size_t)w*h*4);assert(pixels);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for(int y=0;y<h;y++) assert(!memcmp(expected+(size_t)y*w*4,pixels+(size_t)(h-1-y)*w*4,(size_t)w*4));
    free(pixels);prism_free_image(expected);glDeleteProgram(prism);
    glDeleteFramebuffersEXT(1,&fbo);glDeleteTextures(1,&cache);glfwDestroyWindow(window);glfwTerminate();
    puts("Owned cache: full-panel pixels and orientation preserved after source deletion.");
}
