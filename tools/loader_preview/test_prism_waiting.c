/* Compare the production bounded waiting draw with a full-panel reference draw
 * on a real GL driver, across every VC layout and several animation phases. */
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../altscreen_render/loader/prism_waiting.h"
#include "../../altscreen_render/loader/prism_baked.h"

static GLuint program(const char *fragment) {
    const char *vertex="#version 120\nvoid main(){gl_Position=gl_Vertex;}";
    const char *sources[2]={vertex,fragment};
    GLuint stages[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
    for(int i=0;i<2;i++) {
        GLint ok=0;
        glShaderSource(stages[i],1,&sources[i],NULL);glCompileShader(stages[i]);
        glGetShaderiv(stages[i],GL_COMPILE_STATUS,&ok);
        if(!ok) { char log[2048];glGetShaderInfoLog(stages[i],sizeof(log),NULL,log);fprintf(stderr,"%s\n",log); }
        assert(ok);
    }
    GLuint p=glCreateProgram();
    glAttachShader(p,stages[0]);glAttachShader(p,stages[1]);glLinkProgram(p);
    GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);
    glDeleteShader(stages[0]);glDeleteShader(stages[1]);
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p,"uMaterial"),0);
    glUniform1i(glGetUniformLocation(p,"uSig"),1);
    glUniform2f(glGetUniformLocation(p,"iRes"),PRISM_WIDTH,PRISM_HEIGHT);
    return p;
}
static GLuint upload(GLenum unit,const unsigned char *bytes,size_t size) {
    int w,h;unsigned char *rgba=prism_decode_png(bytes,size,&w,&h);assert(rgba);
    GLuint id;glActiveTexture(unit);glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    prism_free_image(rgba);return id;
}
static void draw(GLuint p,const prism_layout_t *layout,float time,int bounded,unsigned char *pixels) {
    glUseProgram(p);
    glUniform4fv(glGetUniformLocation(p,"uContent"),1,layout->content);
    glUniform4fv(glGetUniformLocation(p,"uSigRect"),1,layout->signature);
    glUniform1f(glGetUniformLocation(p,"iTime"),time);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    if(bounded) {
        float vertices[24];prism_waiting_vertices(layout,vertices);
        glBegin(GL_TRIANGLES);
        for(int i=0;i<12;i++) glVertex2fv(vertices+i*2);
        glEnd();
    } else {
        glBegin(GL_QUADS);glVertex2f(-1,-1);glVertex2f(1,-1);glVertex2f(1,1);glVertex2f(-1,1);glEnd();
    }
    glReadPixels(0,0,PRISM_WIDTH,PRISM_HEIGHT,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    assert(glGetError()==GL_NO_ERROR);
}
int main(void) {
    assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GL_FALSE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER,GL_FALSE);
    GLFWwindow *win=glfwCreateWindow(PRISM_WIDTH,PRISM_HEIGHT,"PRISM waiting check",NULL,NULL);assert(win);
    glfwMakeContextCurrent(win);glViewport(0,0,PRISM_WIDTH,PRISM_HEIGHT);
    glDisable(GL_BLEND);glDisable(GL_DITHER);
    GLuint waiting=program(PRISM_WAITING_DESKTOP);
    GLint uniforms;glGetProgramiv(waiting,GL_ACTIVE_UNIFORMS,&uniforms);
    assert(uniforms==6); /* resolution, time, two rectangles and two art samplers */
    GLuint material=upload(GL_TEXTURE0,PRISM_MATERIAL_PNG,sizeof(PRISM_MATERIAL_PNG));
    GLuint signature=upload(GL_TEXTURE1,PRISM_SIGNATURE_PNG,sizeof(PRISM_SIGNATURE_PNG));
    const size_t bytes=(size_t)PRISM_WIDTH*PRISM_HEIGHT*4;
    unsigned char *a=malloc(bytes),*b=malloc(bytes);assert(a && b);
    const float times[]={0.0f,0.35f,1.2f,2.6f,5.0f,23.0f};
    for(int style=0;style<2;style++) for(int view=0;view<2;view++) {
        prism_layout_t layout=prism_layout(view,style,PRISM_SIGNATURE_W,PRISM_SIGNATURE_H);
        float vertices[24],area=0;prism_waiting_vertices(&layout,vertices);
        for(int rect=0;rect<2;rect++) {
            float *v=vertices+12*rect;
            area+=(v[2]-v[0])*(v[5]-v[1])*0.25f;
        }
        assert(area>0 && area<0.15f);
        for(size_t t=0;t<sizeof(times)/sizeof(times[0]);t++) {
            draw(waiting,&layout,times[t],0,a);
            draw(waiting,&layout,times[t],1,b);
            unsigned largest=0,lit=0;
            for(size_t i=0;i<bytes;i++) {
                unsigned delta=(unsigned)abs((int)a[i]-(int)b[i]);
                if(delta>largest) largest=delta;
                if(i%4==3) assert(b[i]==255);
                else {
                    if(b[i]>8) ++lit;
                    if(times[t]==0.0f) assert(b[i]==0);
                    int x=(int)(i/4%PRISM_WIDTH), y=PRISM_HEIGHT-1-(int)(i/4/PRISM_WIDTH);
                    if(x<layout.content[0] || x>=layout.content[0]+layout.content[2] ||
                       y<layout.content[1] || y>=layout.content[1]+layout.content[3]) assert(b[i]==0);
                }
            }
            if(largest>1) fprintf(stderr,"Mismatch style=%d view=%d t=%.2f max=%u\n",style,view,times[t],largest);
            assert(largest<=1);
            if(times[t]>=1.2f) assert(lit>100);
        }
        printf("PRISM waiting style=%d view=%d: %.2f%% shaded panel, 6 phases match full scene\n",style,view,area*100.0f);
    }
    free(a);free(b);glDeleteProgram(waiting);
    glDeleteTextures(1,&material);glDeleteTextures(1,&signature);
    glfwDestroyWindow(win);glfwTerminate();
    puts("PRISM waiting: same artwork, native resolution, six artwork uniforms, black margins and opaque clear PASS");
    return 0;
}
