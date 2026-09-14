#include "prism_gl.h"
#include "prism_baked.h"
#include <stdio.h>
#include <string.h>
#if defined(__APPLE__) && !defined(__QNX__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GLES2/gl2.h>
#endif

static GLuint compile(GLenum type,const char *source) {
    GLuint shader=glCreateShader(type);
    GLint ok=0;
    if (!shader) return 0;
    glShaderSource(shader,1,&source,NULL); glCompileShader(shader);
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[1024]={0}; glGetShaderInfoLog(shader,sizeof(log)-1,NULL,log);
        fprintf(stderr,"PRISM: shader failed: %s\n",log);
        glDeleteShader(shader); return 0;
    }
    return shader;
}
static GLuint texture(const unsigned char *pixels,int w,int h) {
    GLuint tex=0;
    glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    return tex;
}
static GLuint baked(const unsigned char *png,size_t length,int expected_w,int expected_h) {
    int w=0,h=0; GLuint tex=0;
    unsigned char *pixels=prism_decode_png(png,length,&w,&h);
    if (pixels && w==expected_w && h==expected_h) tex=texture(pixels,w,h);
    prism_free_image(pixels); return tex;
}
void prism_gl_destroy(prism_gl_t *s) {
    if (s->program) glDeleteProgram(s->program);
    if (s->material) glDeleteTextures(1,&s->material);
    if (s->signature) glDeleteTextures(1,&s->signature);
    if (s->empty) glDeleteTextures(1,&s->empty);
    memset(s,0,sizeof(*s));
}
int prism_gl_init(prism_gl_t *s) {
#if defined(__APPLE__) && !defined(__QNX__)
    const char *vertex="#version 120\nattribute vec2 a_pos;void main(){gl_Position=vec4(a_pos,0.0,1.0);}";
    const char *fragment=PRISM_FRAGMENT_DESKTOP;
#else
    const char *vertex="attribute vec2 a_pos;void main(){gl_Position=vec4(a_pos,0.0,1.0);}";
    const char *fragment=PRISM_FRAGMENT_GLES;
#endif
    GLuint vs=0,fs=0; GLint ok=0;
    static const unsigned char black[]={0,0,0,255};
    memset(s,0,sizeof(*s));
    vs=compile(GL_VERTEX_SHADER,vertex); fs=compile(GL_FRAGMENT_SHADER,fragment);
    if (!vs || !fs) goto fail;
    s->program=glCreateProgram();
    if (!s->program) goto fail;
    glAttachShader(s->program,vs); glAttachShader(s->program,fs); glLinkProgram(s->program);
    glGetProgramiv(s->program,GL_LINK_STATUS,&ok);
    if (!ok) {
        char log[1024]={0}; glGetProgramInfoLog(s->program,sizeof(log)-1,NULL,log);
        fprintf(stderr,"PRISM: link failed: %s\n",log); goto fail;
    }
    s->position=glGetAttribLocation(s->program,"a_pos");
#define UNIFORM(field,name) s->field=glGetUniformLocation(s->program,name)
    UNIFORM(resolution,"iRes"); UNIFORM(time,"iTime"); UNIFORM(content,"uContent");
    UNIFORM(view,"uView"); UNIFORM(sig_rect,"uSigRect"); UNIFORM(mix,"uStreamMix");
    UNIFORM(panel,"uPanel"); UNIFORM(overlay,"uOverlay"); UNIFORM(pixel_mm,"uPixelMM");
#undef UNIFORM
    if (s->position<0 || s->resolution<0 || s->time<0 || s->content<0 || s->sig_rect<0 ||
        s->mix<0 || s->panel<0 || s->overlay<0) goto fail;
    glUseProgram(s->program);
    glUniform1i(glGetUniformLocation(s->program,"uSig"),1);
    glUniform1i(glGetUniformLocation(s->program,"uMaterial"),2);
    glUniform1i(glGetUniformLocation(s->program,"uStream"),3);
    glUniform1f(glGetUniformLocation(s->program,"uGuides"),0);
    glActiveTexture(GL_TEXTURE0);
    s->material=baked(PRISM_MATERIAL_PNG,sizeof(PRISM_MATERIAL_PNG),PRISM_MATERIAL_W,PRISM_MATERIAL_H);
    s->signature=baked(PRISM_SIGNATURE_PNG,sizeof(PRISM_SIGNATURE_PNG),PRISM_SIGNATURE_W,PRISM_SIGNATURE_H);
    s->empty=texture(black,1,1);
    s->signature_width=PRISM_SIGNATURE_W; s->signature_height=PRISM_SIGNATURE_H;
    if (!s->material || !s->signature || !s->empty || glGetError()!=GL_NO_ERROR) goto fail;
    glDeleteShader(vs); glDeleteShader(fs); return 0;
fail:
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    prism_gl_destroy(s); return -1;
}
void prism_gl_draw(prism_gl_t *s,const prism_layout_t *l,int w,int h,
                   float elapsed,float reveal,unsigned stream,int overlay) {
    static const GLfloat quad[]={-1,-1,1,-1,-1,1,1,1};
    glViewport(0,0,w,h); glDisable(GL_BLEND); glUseProgram(s->program);
    glUniform2f(s->resolution,(float)w,(float)h); glUniform2f(s->panel,(float)w,(float)h);
    glUniform1f(s->time,elapsed); glUniform1f(s->mix,reveal); glUniform1f(s->overlay,overlay?1:0);
    glUniform2f(s->pixel_mm,overlay?1:PRISM_PHYSICAL_WIDTH_MM/PRISM_WIDTH,
                overlay?1:PRISM_PHYSICAL_HEIGHT_MM/PRISM_HEIGHT);
    glUniform4f(s->content,l->content[0],l->content[1],l->content[2],l->content[3]);
    glUniform4f(s->view,l->view[0],l->view[1],l->view[2],l->view[3]);
    glUniform4f(s->sig_rect,l->signature[0],l->signature[1],l->signature[2],l->signature[3]);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,s->signature);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,s->material);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,stream?stream:s->empty);
    glEnableVertexAttribArray((GLuint)s->position);
    glVertexAttribPointer((GLuint)s->position,2,GL_FLOAT,GL_FALSE,0,quad);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4); glDisableVertexAttribArray((GLuint)s->position);
}
