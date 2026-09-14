/* Exercise the actual desktop event routing and scene driver, not a second demo model. */
#include <SDL.h>
#define main cursor_preview_app_main
#include "../tools/cursor-preview/preview.c"
#undef main
#include <assert.h>

static void begin_demo(void) {
    automatic=1; start=0; loader_start=-100; demo_cycle=-1;
    cursor_engine_reset(&engine); input_activity=input_revision=0;
    input_flags=CURSOR_INPUT_DISMISS;
    render(0); render(3.9);
    assert(frame.mode==CURSOR_LOADER && frame.alpha==255);
}
static void check_handoff(unsigned type) {
    begin_demo();
    SDL_Event event; memset(&event,0,sizeof(event)); event.type=type;
    if (type==SDL_MOUSEMOTION) { event.motion.xrel=1; event.motion.x=500; event.motion.y=250; }
    if (type==SDL_MOUSEWHEEL) event.wheel.y=1;
    assert(handle_event(event,3.95));
    int intermediate=0;
    for (int i=0;i<=110;i++) {
        double now=4+i*.02; render(now);
        if (now<4.4) assert(frame.mode==CURSOR_LOADER); // Input must never cut to the background.
        if (frame.mode==CURSOR_LOADER && frame.alpha>0 && frame.alpha<255) intermediate++;
    }
    assert(intermediate>20);
    assert(frame.mode==CURSOR_SHOW);
}
static void check_presented_reveal(void) {
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_Window *w=SDL_CreateWindow("Reveal regression",0,0,W,H,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);
    assert(w); SDL_GLContext context=SDL_GL_CreateContext(w); assert(context);
    assert(!cursor_gl_init(&scene));
    GLuint target=texture(NULL),fbo=0;
    glGenFramebuffersEXT(1,&fbo); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,target,0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
    begin_demo();
    SDL_Event event; memset(&event,0,sizeof(event)); event.type=SDL_MOUSEMOTION;
    event.motion.xrel=1; event.motion.x=500; event.motion.y=250;
    assert(handle_event(event,3.95));
    double previous=1,max_step=0; int partial_frames=0;
    for (int i=0;i<=80;i++) {
        double now=4+i*.025;
        if (i==8) { event.type=SDL_MOUSEWHEEL; event.wheel.y=1; assert(handle_event(event,now)); }
        render(now); assert(!cursor_gl_draw(&scene,&frame));
        glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,output);
        assert(glGetError()==GL_NO_ERROR);
        uint64_t sum=0;
        for (unsigned p=3;p<sizeof(output);p+=4) sum+=output[p];
        double coverage=(double)sum/(W*H*255.0),step=previous-coverage;
        assert(coverage<=previous+.002); // The aperture opens continuously.
        if (step>max_step) max_step=step;
        if (coverage>.01 && coverage<.99) partial_frames++;
        previous=coverage;
    }
    assert(partial_frames>=8 && max_step<.20 && previous==0);
    printf("Presented reveal: %d partial frames, largest 25ms coverage step %.3f\n",partial_frames,max_step);
    cursor_gl_destroy(&scene); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    glDeleteFramebuffersEXT(1,&fbo); glDeleteTextures(1,&target);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(w); SDL_Quit();
}
int main(void) {
    check_handoff(SDL_MOUSEMOTION);
    check_handoff(SDL_MOUSEBUTTONDOWN);
    check_handoff(SDL_MOUSEWHEEL);
    // Manual L uses the same protected lifetime as the automatic startup.
    SDL_Event event; memset(&event,0,sizeof(event)); event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_l;
    assert(handle_event(event,100)); render(100);
    event.type=SDL_MOUSEBUTTONDOWN; assert(handle_event(event,101)); render(101);
    assert(frame.mode==CURSOR_LOADER && frame.alpha==255);
    // If frame delivery is delayed, an arbitrary 5.8s demo cutoff cannot finish reveal.
    render(105.7); render(105.8);
    assert(loader_active && frame.mode==CURSOR_LOADER && frame.alpha>100);
    for (int i=0;i<110;i++) render(105.8+i*.02);
    assert(!loader_active && frame.mode==CURSOR_SHOW);
    check_presented_reveal();
    puts("cursor_preview_handoff_test: pointer input cannot bypass startup/reveal PASS");
}
