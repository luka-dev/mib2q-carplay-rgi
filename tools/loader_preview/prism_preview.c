/* PRISM: standalone macOS preview of the embedded vehicle scene. */
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/prism.h"
#include "../../common/prism_baked.h"
#include "prism_demo_baked.h"

#define W PRISM_WIDTH
#define H PRISM_HEIGHT
static GLuint shader(GLenum type,const char *src) {
    GLuint s=glCreateShader(type); GLint ok=0;
    glShaderSource(s,1,&src,NULL); glCompileShader(s);
    glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        char log[4096]; glGetShaderInfoLog(s,sizeof(log),NULL,log);
        fprintf(stderr,"Shader error: %s\n",log); glDeleteShader(s); return 0;
    }
    return s;
}
typedef struct {
    GLuint id;
    GLint res,time,content,view,guide,sig_rect,stream_mix;
} scene_t;
static int load_scene(scene_t *scene) {
    GLuint vs=shader(GL_VERTEX_SHADER,"void main(){gl_Position=gl_Vertex;}");
    GLuint fs=shader(GL_FRAGMENT_SHADER,PRISM_FRAGMENT_DESKTOP);
    if(!vs || !fs) return 0;
    GLuint prog=glCreateProgram(); glAttachShader(prog,vs); glAttachShader(prog,fs);
    glLinkProgram(prog); GLint ok=0; glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    glDeleteShader(vs); glDeleteShader(fs);
    if(!ok) {
        char log[4096]; glGetProgramInfoLog(prog,sizeof(log),NULL,log);
        fprintf(stderr,"Link error: %s\n",log); glDeleteProgram(prog); return 0;
    }
    scene->id=prog;
    scene->res=glGetUniformLocation(prog,"iRes");
    scene->time=glGetUniformLocation(prog,"iTime");
    scene->content=glGetUniformLocation(prog,"uContent");
    scene->view=glGetUniformLocation(prog,"uView");
    scene->guide=glGetUniformLocation(prog,"uGuides");
    scene->sig_rect=glGetUniformLocation(prog,"uSigRect");
    scene->stream_mix=glGetUniformLocation(prog,"uStreamMix");
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog,"uSig"),1);
    glUniform1i(glGetUniformLocation(prog,"uMaterial"),2);
    glUniform1i(glGetUniformLocation(prog,"uStream"),3);
    glUniform2f(glGetUniformLocation(prog,"uPixelMM"),PRISM_PHYSICAL_WIDTH_MM/W,PRISM_PHYSICAL_HEIGHT_MM/H);
    return 1;
}
static GLuint texture(const unsigned char *data,size_t length,int *w,int *h) {
    unsigned char *p=prism_decode_png(data,length,w,h);
    if(!p) { fprintf(stderr,"Embedded image decode failed\n"); return 0; }
    GLuint tex; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,*w,*h,0,GL_RGBA,GL_UNSIGNED_BYTE,p);
    prism_free_image(p); return tex;
}
enum { ACT_VIEW=1, ACT_STYLE=2, ACT_GUIDES=4, ACT_REPLAY=16, ACT_PAUSE=32, ACT_STREAM=128 };
static const char *stage_names[]={"LOADER","ZOOM IN","DEMO STREAM","ZOOM OUT"};
static unsigned pending_actions;
static void key_event(GLFWwindow *win,int key,int scancode,int action,int mods) {
    (void)scancode; (void)mods;
    if(action!=GLFW_PRESS) return;
    /* Polling final key state misses a press+release delivered in one frame. */
    switch(key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(win,GLFW_TRUE); break;
        case GLFW_KEY_V: pending_actions^=ACT_VIEW; break;
        case GLFW_KEY_S: pending_actions^=ACT_STYLE; break;
        case GLFW_KEY_G: pending_actions^=ACT_GUIDES; break;
        case GLFW_KEY_T: pending_actions^=ACT_STREAM; break;
        case GLFW_KEY_R: pending_actions|=ACT_REPLAY; break;
        case GLFW_KEY_SPACE: pending_actions^=ACT_PAUSE; break;
        default: break;
    }
}
static int usage(const char *name) {
    fprintf(stderr,"Usage: %s [--capture file.rgba --frames N] [--time ms] "
        "[--view full|compact] [--style sport|classic] "
        "[--handoff|--loader-only] [--guides]\n",name);
    return 2;
}
int main(int argc,char **argv) {
    const char *capture=NULL; int frames=180,view=0,style=0,guides=0;
    double frozen=-1; int demo=-1;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--guides")) guides=1;
        else if(!strcmp(argv[i],"--handoff")) demo=1;
        else if(!strcmp(argv[i],"--loader-only")) demo=0;
        else if(!strcmp(argv[i],"--capture") && i+1<argc) capture=argv[++i];
        else if(!strcmp(argv[i],"--frames") && i+1<argc) frames=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--time") && i+1<argc) frozen=atof(argv[++i])/1000.0;
        else if(!strcmp(argv[i],"--view") && i+1<argc) {
            const char *v=argv[++i];
            if(strcmp(v,"compact") && strcmp(v,"full")) return usage(argv[0]);
            view=!strcmp(v,"compact");
        } else if(!strcmp(argv[i],"--style") && i+1<argc) {
            const char *s=argv[++i];
            if(strcmp(s,"classic") && strcmp(s,"sport")) return usage(argv[0]);
            style=!strcmp(s,"classic");
        } else return usage(argv[0]);
    }
    if(frames<1 || frames>18000) return usage(argv[0]);
    if(demo<0) demo=capture?0:1;
    const int initial_demo=demo;
    if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    glfwWindowHint(GLFW_RESIZABLE,GLFW_FALSE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER,GLFW_FALSE);
    if(capture) glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    GLFWwindow *win=glfwCreateWindow(W,H,"PRISM | T:stream R:replay V:view S:style G:guides Space:pause Esc:quit",NULL,NULL);
    if(!win) { glfwTerminate(); return 1; }
    if(!capture) glfwSetKeyCallback(win,key_event);
    glfwMakeContextCurrent(win); glfwSwapInterval(capture?0:1);
    int sw,sh,mw,mh,tw,th;
    GLuint sig=texture(PRISM_SIGNATURE_PNG,sizeof(PRISM_SIGNATURE_PNG),&sw,&sh);
    GLuint material=texture(PRISM_MATERIAL_PNG,sizeof(PRISM_MATERIAL_PNG),&mw,&mh);
    GLuint stream=texture(PRISM_DEMO_PNG,sizeof(PRISM_DEMO_PNG),&tw,&th);
    if(!sig || !material || !stream || tw!=W || th!=H) return 1;
    scene_t scene_storage; const scene_t *scene=&scene_storage;
    if(!load_scene(&scene_storage)) return 1;
    FILE *out=NULL; unsigned char *pixels=NULL;
    if(capture) {
        out=fopen(capture,"wb"); if(!out) { perror(capture); return 1; }
        pixels=malloc((size_t)W*H*4); if(!pixels) return 1;
    }
    double start=glfwGetTime(),pauseAt=0; int paused=0;
    double previous_t=0; handoff_t handoff={0,0,0};
    int frame=0,failed=0,shown=-1;
    while(!glfwWindowShouldClose(win)) {
        glfwPollEvents(); if(glfwWindowShouldClose(win)) break;
        unsigned actions=pending_actions; pending_actions=0;
        if(actions&ACT_VIEW) view=1-view;
        if(actions&ACT_STYLE) style=1-style;
        if(actions&ACT_GUIDES) guides=1-guides;
        if(actions&ACT_PAUSE) {
            if(paused) start=glfwGetTime()-pauseAt; else pauseAt=glfwGetTime()-start;
            paused=!paused;
        }
        /* Replay always resumes playback, including --time still previews. */
        if(actions&ACT_REPLAY) {
            start=glfwGetTime(); pauseAt=0; paused=0; frozen=-1;
            handoff=(handoff_t){0,0,0}; previous_t=0; demo=initial_demo;
        }
        double t=capture?(double)frame/30.0:(paused?pauseAt:glfwGetTime()-start);
        if(frozen>=0) t=frozen;
        if(demo) handoff=handoff_demo(t);
        else handoff_step(&handoff,t-previous_t);
        previous_t=t;
        if(actions&ACT_STREAM) {
            demo=0; handoff.target=!handoff.target;
        }
        int stage=handoff.position==0?0:handoff.position==1?2:handoff.target?1:3;
        int state=(paused<<1)|((frozen>=0)<<2)|(stage<<5);
        if(shown!=state) {
            char title[256];
            snprintf(title,sizeof(title),"PRISM / %s%s | T:stream R:replay V:view S:style G:guides Space:pause Esc:quit",
                     stage_names[stage],(paused || frozen>=0)?" [PAUSED]":"");
            glfwSetWindowTitle(win,title); shown=state;
        }
        prism_layout_t layout=prism_layout(view,style,(float)sw,(float)sh);
        float *c=layout.content,*v=layout.view,*sigRect=layout.signature;
        if(capture && frame==0) fprintf(stderr,"Content: %.0f %.0f %.0f %.0f\n",c[0],c[1],c[2],c[3]);
        glUseProgram(scene->id);
        int fw,fh; glfwGetFramebufferSize(win,&fw,&fh); glViewport(0,0,fw,fh);
        glUniform2f(scene->res,(float)fw,(float)fh); glUniform1f(scene->time,(float)t);
        glUniform4f(scene->content,c[0],c[1],c[2],c[3]);
        glUniform4f(scene->view,v[0],v[1],v[2],v[3]);
        glUniform1f(scene->guide,(float)guides);
        glUniform1f(scene->stream_mix,(float)handoff.position);
        glUniform4f(scene->sig_rect,sigRect[0],sigRect[1],sigRect[2],sigRect[3]);
        if(capture && frame==0) fprintf(stderr,"Signature: %.3f %.3f %.3f %.3f\n",sigRect[0],sigRect[1],sigRect[2],sigRect[3]);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,sig);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,material);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,stream);
        glBegin(GL_QUADS); glVertex2f(-1,-1); glVertex2f(1,-1); glVertex2f(1,1); glVertex2f(-1,1); glEnd();
        GLenum err=glGetError(); if(err) { fprintf(stderr,"GL error 0x%x\n",err); failed=1; break; }
        if(capture) {
            if(fw!=W || fh!=H) { fprintf(stderr,"Unexpected capture dimensions\n"); failed=1; break; }
            glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
            for(int row=H-1;row>=0;row--) {
                if(fwrite(pixels+(size_t)row*W*4,1,W*4,out)!=W*4) { failed=1; break; }
            }
            if(failed || ++frame>=frames) break;
        }
        glfwSwapBuffers(win);
    }
    if(out && fclose(out)) failed=1;
    free(pixels); glDeleteTextures(1,&sig);
    glDeleteTextures(1,&stream);
    glDeleteTextures(1,&material); glDeleteProgram(scene->id);
    glfwDestroyWindow(win); glfwTerminate();
    if(capture && !failed) fprintf(stderr,"Captured %d frames, 1440x542 RGBA at 30 fps\n",frame);
    return failed?1:0;
}
