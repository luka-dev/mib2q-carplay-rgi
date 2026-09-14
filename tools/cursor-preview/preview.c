/* Desktop viewer of the production native scene and animation engine.
 * Host input/readiness are fixtures; no HU, DSI or network access.
 */
#define GL_SILENCE_DEPRECATION
#include <SDL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../cursor_render/cursor_gl.h"
#include "../../cursor_render/engine.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_THREAD_LOCALS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "../../hook/coverart/stb_image.h"
#pragma clang diagnostic pop

enum { W = 1024, H = 480 };
static uint8_t output[W * H * 4], background[W * H * 4];
static cursor_gl_t scene;
static cursor_frame frame;
static cursor_engine engine;
static uint32_t input_activity, input_revision;
static int input_flags=CURSOR_INPUT_DISMISS, demo_cycle=-1;
static int background_dirty=1;
static uint8_t *map;
static int map_w, map_h, backdrop;
static int automatic = 1, pointer_x = W / 2, pointer_y = H / 2;
static double start, loader_start, pointer_start;
static int loader_active=1;

static void move(void) { ++input_activity; ++input_revision; input_flags=CURSOR_INPUT_TOUCH; }
static void dismiss(void) { ++input_revision; input_flags=CURSOR_INPUT_DISMISS; }
static void force_hide(void) { ++input_revision; input_flags=CURSOR_INPUT_FORCE_HIDE; }
static void begin_loader(double now) {
    loader_active=1; loader_start=now;
    cursor_engine_reset(&engine); dismiss();
}

static void make_background(void) {
    background_dirty=1;
    double scale = map ? fmax((double)W/map_w, (double)H/map_h) : 1;
    for (int y=0; y<H; ++y) for (int x=0; x<W; ++x) {
        uint8_t *p=background+(y*W+x)*4;
        if (backdrop == 0 && map) {
            int sx=(int)((x-W*.5)/scale+map_w*.5), sy=(int)((y-H*.5)/scale+map_h*.5);
            const uint8_t *s=map+(sy*map_w+sx)*4;
            p[0]=s[0]; p[1]=s[1]; p[2]=s[2];
        } else {
            static const uint8_t grey[] = {24, 114, 242};
            int value = grey[backdrop == 0 ? (x*3/W) : backdrop-1];
            p[0]=p[1]=p[2]=(uint8_t)value;
        }
        p[3]=255;
    }
}

static void render(double now) {
    if (automatic) {
        int cycle=(int)((now-start)/14.0);
        if (cycle!=demo_cycle) { begin_loader(now); demo_cycle=cycle; }
        if (!loader_active) {
            double movement=now-pointer_start, path=fmax(0,fmin(4,movement));
            pointer_x=(int)(W*.5+W*.28*sin(path*1.25));
            pointer_y=(int)(H*.5+H*.19*sin(path*2.0));
            if (movement<4) move();
            else if (input_flags==CURSOR_INPUT_TOUCH) {
                ++input_revision; input_flags=CURSOR_INPUT_RELEASED;
            }
        }
    }
    cursor_frame command={0};
    command.width=command.vw=W; command.height=command.vh=H;
    if (loader_active) {
        // The fixture supplies video readiness. Only the native engine ends reveal.
        command.mode=CURSOR_LOADER; command.flags=now-loader_start>=4; command.revision=1;
    } else {
        command.mode=CURSOR_SHOW; command.x=pointer_x; command.y=pointer_y;
        command.flags=input_flags; command.activity=input_activity; command.revision=input_revision;
    }
    uint64_t ms=(uint64_t)(now*1000);
    cursor_engine_apply(&engine,&command,ms);
    cursor_engine_frame(&engine,ms,&frame);
    if (loader_active && engine.done) { loader_active=0; pointer_start=now; }
}
static void capture_stage(const char *stage) {
    cursor_frame command={0};
    command.width=command.vw=W; command.height=command.vh=H;
    cursor_engine_reset(&engine);
    if (stage && strncmp(stage,"cursor",6)) {
        command.mode=CURSOR_LOADER; command.revision=1;
        cursor_engine_apply(&engine,&command,0);
        if (!strcmp(stage,"reveal")) { command.flags=1; cursor_engine_apply(&engine,&command,3000); }
    } else {
        command.mode=CURSOR_SHOW; command.x=W/2; command.y=H/2;
        command.flags=CURSOR_INPUT_TOUCH; command.activity=command.revision=1;
        if (stage && !strcmp(stage,"cursor-motion")) {
            command.x=W/2-90;
            cursor_engine_apply(&engine,&command,3200);
            for (int i=1;i<=5;i++) {
                command.x+=18; command.activity++; command.revision++;
                cursor_engine_apply(&engine,&command,3200+i*25);
            }
        } else cursor_engine_apply(&engine,&command,3400);
    }
    cursor_engine_frame(&engine,3400,&frame);
}

static GLuint texture(const void *pixels) {
    GLuint t=0; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    return t;
}
static void quad(GLuint image,int top_down) {
    glBindTexture(GL_TEXTURE_2D,image);
    glBegin(GL_QUADS);
    glTexCoord2f(0,top_down?1:0); glVertex2f(-1,-1);
    glTexCoord2f(1,top_down?1:0); glVertex2f(1,-1);
    glTexCoord2f(1,top_down?0:1); glVertex2f(1,1);
    glTexCoord2f(0,top_down?0:1); glVertex2f(-1,1);
    glEnd();
}

static int handle_event(SDL_Event event,double now) {
    if (event.type==SDL_QUIT) return 0;
    else if (event.type==SDL_WINDOWEVENT) {
        if (event.window.event==SDL_WINDOWEVENT_ENTER) SDL_ShowCursor(SDL_DISABLE);
        if (event.window.event==SDL_WINDOWEVENT_LEAVE) SDL_ShowCursor(SDL_ENABLE);
    } else if (event.type==SDL_MOUSEMOTION && (event.motion.xrel || event.motion.yrel)) {
        automatic=0;
        // Like the HU, pointer input must not replace an unfinished loader scene.
        if (loader_active) return 1;
        move();
        pointer_x=event.motion.x<0 ? 0 : event.motion.x>=W ? W-1 : event.motion.x;
        pointer_y=event.motion.y<0 ? 0 : event.motion.y>=H ? H-1 : event.motion.y;
    } else if (event.type==SDL_MOUSEBUTTONDOWN || event.type==SDL_MOUSEWHEEL) {
        if (event.type==SDL_MOUSEWHEEL) force_hide(); else dismiss();
        automatic=0;
    } else if (event.type==SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE: return 0;
            case SDLK_l: automatic=0; begin_loader(now); break;
            case SDLK_b: backdrop=(backdrop+1)%4; make_background(); break;
            case SDLK_SPACE: automatic=1; start=now; demo_cycle=-1; dismiss(); break;
            default: break;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        int components;
        map=stbi_load(argv[1],&map_w,&map_h,&components,4);
        if (!map) fprintf(stderr,"Background unavailable: %s\n",stbi_failure_reason());
    }
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) { fprintf(stderr,"%s\n",SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window *window=SDL_CreateWindow(
        "HU cursor + loader | Move: cursor | L: loader | B: background | Space: demo | Click: fade | Wheel: force hide | Esc: close",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_OPENGL);
    if (!window) { fprintf(stderr,"%s\n",SDL_GetError()); SDL_Quit(); return 1; }
    SDL_GLContext context=SDL_GL_CreateContext(window);
    if (!context || SDL_GL_MakeCurrent(window,context)<0 || cursor_gl_init(&scene)) {
        fprintf(stderr,"GL init failed: %s\n",SDL_GetError()); SDL_Quit(); return 1;
    }
    SDL_GL_SetSwapInterval(1);
    glActiveTexture(GL_TEXTURE0);
    GLuint overlay=texture(NULL),bg=texture(NULL),fbo=0;
    glGenFramebuffersEXT(1,&fbo); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,overlay,0);
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)!=GL_FRAMEBUFFER_COMPLETE_EXT) return 1;
    const char *bg_choice=getenv("CURSOR_PREVIEW_BACKGROUND");
    if (bg_choice) backdrop=atoi(bg_choice)%4;
    make_background(); start=SDL_GetTicks()/1000.0; begin_loader(start);
    int running=1, frames=0;
    const char *capture=getenv("CURSOR_PREVIEW_CAPTURE");
    while (running) {
        double now=SDL_GetTicks()/1000.0;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (!handle_event(event,now)) running=0;
        }
        if (capture) capture_stage(getenv("CURSOR_PREVIEW_STAGE"));
        else render(now);
        if (background_dirty) {
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,bg);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,background);
            background_dirty=0;
        }
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
        if (cursor_gl_draw(&scene,&frame)) { fprintf(stderr,"cursor/PRISM draw failed\n"); return 1; }
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
        int dw,dh; SDL_GL_GetDrawableSize(window,&dw,&dh); glViewport(0,0,dw,dh);
        glUseProgram(0); glActiveTexture(GL_TEXTURE0); glEnable(GL_TEXTURE_2D);
        glDisable(GL_BLEND); glColor4f(1,1,1,1); quad(bg,1);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA); quad(overlay,0);
        glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D);
        if (glGetError()!=GL_NO_ERROR) { fprintf(stderr,"preview composite failed\n"); return 1; }
        if (capture && ++frames==3) {
            // Capture the native canvas (not Retina magnification) including the background.
            glViewport(0,0,W,H); glEnable(GL_TEXTURE_2D); glDisable(GL_BLEND);
            // Draw into a separate capture texture to avoid sampling an attached image.
            GLuint capture_tex=texture(NULL),capture_fbo=0;
            glGenFramebuffersEXT(1,&capture_fbo); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,capture_fbo);
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,capture_tex,0);
            if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)!=GL_FRAMEBUFFER_COMPLETE_EXT) return 1;
            quad(bg,1);
            glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA); quad(overlay,0);
            glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,output);
            unsigned char row[W*4];
            for (int y=0;y<H/2;y++) {
                memcpy(row,output+y*W*4,W*4); memcpy(output+y*W*4,output+(H-1-y)*W*4,W*4);
                memcpy(output+(H-1-y)*W*4,row,W*4);
            }
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(output,W,H,32,W*4,SDL_PIXELFORMAT_RGBA32);
            if (!surface || SDL_SaveBMP(surface,capture)<0) { fprintf(stderr,"capture failed: %s\n",SDL_GetError()); return 1; }
            SDL_FreeSurface(surface); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
            glDeleteFramebuffersEXT(1,&capture_fbo); glDeleteTextures(1,&capture_tex); running=0;
        }
        SDL_GL_SwapWindow(window);
        SDL_Delay(10);
    }
    SDL_ShowCursor(SDL_ENABLE); cursor_gl_destroy(&scene);
    glDeleteFramebuffersEXT(1,&fbo); glDeleteTextures(1,&overlay); glDeleteTextures(1,&bg);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window); SDL_Quit(); stbi_image_free(map); return 0;
}
