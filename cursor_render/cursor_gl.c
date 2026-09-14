#include "cursor_gl.h"
#include "glass_shader.h"
#include <stdio.h>
#include <string.h>
#if defined(__APPLE__) && !defined(__QNX__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GLES2/gl2.h>
#endif

static GLuint compile(GLenum type,const char *source) {
    GLuint shader=glCreateShader(type); GLint ok=0;
    if (!shader) return 0;
    glShaderSource(shader,1,&source,NULL); glCompileShader(shader);
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[1024]={0}; glGetShaderInfoLog(shader,sizeof(log)-1,NULL,log);
        fprintf(stderr,"HU cursor: shader failed: %s\n",log); glDeleteShader(shader); return 0;
    }
    return shader;
}
void cursor_gl_destroy(cursor_gl_t *s) {
    prism_gl_destroy(&s->prism);
    if (s->program) glDeleteProgram(s->program);
    memset(s,0,sizeof(*s));
}
int cursor_gl_init(cursor_gl_t *s) {
    const char *vertex="attribute vec2 a_pos;attribute vec2 a_uv;varying vec2 uv;"
        "void main(){gl_Position=vec4(a_pos,0.0,1.0);uv=a_uv;}";
    GLuint vs=0,fs=0; GLint ok=0;
    memset(s,0,sizeof(*s));
    if (prism_gl_init(&s->prism)) return -1;
    vs=compile(GL_VERTEX_SHADER,vertex); fs=compile(GL_FRAGMENT_SHADER,cursor_glass_fragment);
    if (!vs || !fs) goto fail;
    s->program=glCreateProgram();
    if (!s->program) goto fail;
    glAttachShader(s->program,vs); glAttachShader(s->program,fs); glLinkProgram(s->program);
    glGetProgramiv(s->program,GL_LINK_STATUS,&ok);
    if (!ok) goto fail;
    s->position=glGetAttribLocation(s->program,"a_pos");
    s->uv=glGetAttribLocation(s->program,"a_uv");
    s->alpha=glGetUniformLocation(s->program,"alpha");
    s->motion=glGetUniformLocation(s->program,"motion");
    s->light_phase=glGetUniformLocation(s->program,"lightPhase");
    if (s->position<0 || s->uv<0 || s->alpha<0 || s->motion<0 || s->light_phase<0) goto fail;
    glDeleteShader(vs); glDeleteShader(fs); return 0;
fail:
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    cursor_gl_destroy(s); return -1;
}
int cursor_gl_draw(cursor_gl_t *s,const cursor_frame *f) {
    glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST);
    glViewport(0,0,f->width,f->height); glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
    if (f->mode==CURSOR_LOADER) {
        prism_layout_t layout=prism_layout_fullscreen(f->width,f->height,
            s->prism.signature_width,s->prism.signature_height);
        float elapsed=(f->x | ((uint32_t)f->y<<16))/1000.0f;
        prism_gl_draw(&s->prism,&layout,f->width,f->height,elapsed,1-f->alpha/255.0f,0,1);
    } else if (f->mode==CURSOR_SHOW && f->alpha) {
        float left=(float)(f->vx+f->x)-CURSOR_GLASS_EXTENT/2;
        float top=(float)(f->vy+f->y)-CURSOR_GLASS_EXTENT/2;
        float x0=left/f->width*2-1,x1=(left+CURSOR_GLASS_EXTENT)/f->width*2-1;
        float y0=1-top/f->height*2,y1=1-(top+CURSOR_GLASS_EXTENT)/f->height*2;
        GLfloat quad[]={x0,y1,x1,y1,x0,y0,x1,y0};
        static const GLfloat uv[]={0,1,1,1,0,0,1,0};
        glEnable(GL_SCISSOR_TEST); glScissor(f->vx,f->height-f->vy-f->vh,f->vw,f->vh);
        glUseProgram(s->program); glUniform1f(s->alpha,f->alpha/255.0f);
        glUniform2f(s->motion,f->motion_x,f->motion_y);
        glUniform1f(s->light_phase,f->light_phase);
        glEnableVertexAttribArray((GLuint)s->position); glEnableVertexAttribArray((GLuint)s->uv);
        glVertexAttribPointer((GLuint)s->position,2,GL_FLOAT,GL_FALSE,0,quad);
        glVertexAttribPointer((GLuint)s->uv,2,GL_FLOAT,GL_FALSE,0,uv);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        glDisableVertexAttribArray((GLuint)s->position); glDisableVertexAttribArray((GLuint)s->uv);
        glDisable(GL_SCISSOR_TEST);
    }
    return glGetError()==GL_NO_ERROR ? 0:-1;
}
