/* Actual shared GPU scene: alpha, native pixels, viewport and stale-buffer regressions. */
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../cursor_render/cursor_gl.h"

enum { W=1024,H=480 };
static uint8_t gpu[W*H*4],reference[W*H*4],full[W*H*4];
static void pixels(void) {
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,gpu);
    assert(glGetError()==GL_NO_ERROR);
}
static void premultiplied(void) {
    for (unsigned i=0;i<sizeof(gpu);i+=4)
        for (unsigned c=0;c<3;c++) assert(gpu[i+c]<=gpu[i+3]);
}
int main(void) {
    assert(glfwInit()); glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    GLFWwindow *window=glfwCreateWindow(W,H,"HU PRISM GPU checks",NULL,NULL); assert(window);
    glfwMakeContextCurrent(window); cursor_gl_t scene; assert(cursor_gl_init(&scene)==0);
    GLuint textures[2],fbos[2]; glGenTextures(2,textures); glGenFramebuffersEXT(2,fbos);
    for (int i=0;i<2;i++) {
        glBindTexture(GL_TEXTURE_2D,textures[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbos[i]);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,textures[i],0);
        assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
        glClearColor(1,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    }
    cursor_frame f={0}; f.width=W; f.height=H; f.vx=100; f.vy=20; f.vw=800; f.vh=440;
    f.mode=CURSOR_LOADER; f.x=3400; f.alpha=255;
    prism_layout_t layout=prism_layout_fullscreen(W,H,scene.prism.signature_width,scene.prism.signature_height);
    assert(layout.view[0]==0 && layout.view[1]==0 && layout.view[2]==W && layout.view[3]==H);
    assert(layout.content[0]==0 && layout.content[1]==0 && layout.content[2]==W && layout.content[3]==H);
    for (int i=0;i<2;i++) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbos[i]); assert(!cursor_gl_draw(&scene,&f)); pixels();
        int lit=0;
        for (unsigned p=0;p<sizeof(gpu);p+=4) { assert(gpu[p+3]==255); lit+=gpu[p]>0; }
        assert(lit>1000); // Actual PRISM art, not only an opaque placeholder.
    }
    for (int opacity=255;opacity>=0;opacity-=51) {
        f.alpha=opacity; assert(!cursor_gl_draw(&scene,&f)); pixels(); premultiplied();
        if (!opacity) for (unsigned p=0;p<sizeof(gpu);p++) assert(gpu[p]==0);
    }
    f.alpha=128; assert(!cursor_gl_draw(&scene,&f)); pixels();
    int transparent=0,opaque=0;
    for (unsigned p=0;p<sizeof(gpu);p+=4) { transparent+=gpu[p+3]<128; opaque+=gpu[p+3]>240; }
    assert(transparent>1000 && opaque>1000); // A growing aperture, not a whole-screen fade.
    // Procedural glass on two independently dirty buffers: clipping and no trails.
    f.mode=CURSOR_SHOW;
    for (int i=0;i<12;i++) {
        f.x=i%3==0?0:i%3==1?f.vw-1:f.vw/2;
        f.y=i%3==0?0:i%3==1?f.vh-1:f.vh/2;
        f.motion_x=i%2?.8f:-.8f; f.motion_y=i%3?.3f:-.5f;
        f.light_phase=i*.47f;
        f.alpha=i==11?0:i%2?128:255;
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbos[i%2]); assert(!cursor_gl_draw(&scene,&f)); pixels();
        memcpy(reference,gpu,sizeof(gpu)); premultiplied();
        // Drawing the same state over another stale image must produce identical pixels.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbos[1-i%2]); assert(!cursor_gl_draw(&scene,&f)); pixels();
        assert(!memcmp(reference,gpu,sizeof(gpu)));
        for (int y=0;y<H;y++) for (int x=0;x<W;x++) {
            unsigned a=gpu[((H-1-y)*W+x)*4+3];
            int dx=x-(int)(f.vx+f.x),dy=y-(int)(f.vy+f.y);
            if (!f.alpha || x<(int)f.vx || x>=(int)(f.vx+f.vw) || y<(int)f.vy || y>=(int)(f.vy+f.vh)
                    || abs(dx)>=CURSOR_GLASS_EXTENT/2 || abs(dy)>=CURSOR_GLASS_EXTENT/2
                    || (abs(dx)<15 && abs(dy)<15)) assert(!a);
        }
    }
    f.x=f.vw/2; f.y=f.vh/2; f.alpha=255; f.motion_x=.9f; f.motion_y=0;
    assert(!cursor_gl_draw(&scene,&f)); pixels(); memcpy(full,gpu,sizeof(gpu));
    int lit=0,coloured=0;
    for(unsigned p=0;p<sizeof(gpu);p+=4) {
        lit+=gpu[p+3]>16;
        coloured+=abs((int)gpu[p]-(int)gpu[p+2])>12;
    }
    assert(lit>1500 && coloured>40);
    f.alpha=128; assert(!cursor_gl_draw(&scene,&f)); pixels();
    for (unsigned p=0;p<sizeof(gpu);p++) assert(abs((int)gpu[p]-(full[p]*128+127)/255)<=1);
    f.alpha=255; f.motion_x=-.9f; assert(!cursor_gl_draw(&scene,&f)); pixels();
    unsigned difference=0;
    for (unsigned p=0;p<sizeof(gpu);p++) difference+=abs((int)gpu[p]-full[p]);
    assert(difference>10000); // Direction changes highlights/dispersion, not only position.
    premultiplied();
    // Even a stationary cursor has a travelling optical highlight, with no Java input.
    f.motion_x=f.motion_y=0; f.light_phase=0;
    assert(!cursor_gl_draw(&scene,&f)); pixels(); memcpy(full,gpu,sizeof(gpu));
    f.light_phase=1.1f; assert(!cursor_gl_draw(&scene,&f)); pixels();
    difference=0;
    for (unsigned p=0;p<sizeof(gpu);p++) difference+=abs((int)gpu[p]-full[p]);
    assert(difference>10000); premultiplied();
    // The native phase wraps every orbit without a flash at the boundary.
    f.light_phase=6.28318530718f; assert(!cursor_gl_draw(&scene,&f)); pixels();
    for (unsigned p=0;p<sizeof(gpu);p++) assert(abs((int)gpu[p]-full[p])<=1);
    cursor_gl_destroy(&scene); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    glDeleteFramebuffersEXT(2,fbos); glDeleteTextures(2,textures);
    glfwDestroyWindow(window); glfwTerminate();
    puts("HU GPU: full-screen PRISM, aperture alpha, procedural directional glass ring, edge clipping, alternating-buffer cleanup PASS");
    return 0;
}
