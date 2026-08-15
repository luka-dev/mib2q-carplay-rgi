/*
 * loader_preview — macOS host preview of the CarPlay cluster startup loader.
 *
 * Apple-boot look, rendered with a fragment shader (per-pixel, AA): pure-black
 * field, white Apple logo (rasterised SVG texture) centered, and the HIG activity
 * indicator below it, smaller.  Ports to the on-unit altscreen_render GLES2 path.
 *
 * DEBUG overlays (drawn on top, not part of the shippable loader):
 *   G = calibration grid (16 cols A..P x 6 rows 1..6, matching the on-unit checkerboard)
 *   V = advertised view-area safe regions (off -> fullscreen -> smallscreen)
 *   ESC = quit,  CR_T=<ms> freeze a moment.
 *
 * NOTE: the Apple logo is Apple's trademark — personal cluster mod only.
 */
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../hook/coverart/stb_image.h"   /* prototypes; impl in stbi.c */
#include "../../common/loader_layout.h"

#define PANEL_W 1440
#define PANEL_H 542

/* ── loader shader (the real, shippable art) ─────────────────────────────────── */
static const char *k_vert = "void main(){ gl_Position = gl_Vertex; }\n";
static const char *k_frag =
    "#version 120\n"
    "uniform vec2  iRes;\n"
    "uniform float iTime;\n"
    "uniform sampler2D uLogo;\n"
    "uniform vec2  uCenter;\n"                             /* active view-area center (p-space) */
    "uniform float uScale;\n"                              /* active view-area scale */
    "const float TAU = 6.28318531;\n"
    "float sdSeg(vec2 p, vec2 a, vec2 b, float rad){\n"
    "  vec2 pa=p-a, ba=b-a;\n"
    "  float h=clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0);\n"
    "  return length(pa-ba*h)-rad;\n"
    "}\n"
    "float spinner(vec2 q, float t, float R1, float R2, float wth){\n"
    "  float aa = 1.4/iRes.y;\n"
    "  float phase = t/1.1;\n"
    "  float lum = 0.0;\n"
    "  for(int k=0;k<12;k++){\n"
    "    float a = TAU*float(k)/12.0;\n"
    "    vec2 dir = vec2(sin(a), cos(a));\n"
    "    float d = sdSeg(q, dir*R1, dir*R2, wth);\n"
    "    float seg = smoothstep(aa, -aa, d);\n"
    "    float age = fract(phase - float(k)/12.0);\n"
    "    lum = max(lum, seg*(0.16 + 0.84*age));\n"
    "  }\n"
    "  return lum;\n"
    "}\n"
    "void main(){\n"
    "  float t = iTime;\n"
    "  vec2 p = (gl_FragCoord.xy - 0.5*iRes)/iRes.y;\n"
    "  vec2 q = (p - uCenter)/uScale;\n"                   /* recenter+scale into active view area */
    "  vec3 col = vec3(0.0);\n"
    "  vec2 lc = vec2(0.0, 0.1125); float lh = 0.115;\n" /* group bbox vertically centered in view */
    "  vec2 lp = (q - lc)/lh;\n"
    "  if(abs(lp.x) < 1.0 && abs(lp.y) < 1.0){\n"
    "    vec2 uv = vec2(lp.x*0.5 + 0.5, 0.5 - lp.y*0.5);\n"
    "    col = mix(col, vec3(0.97,0.98,1.0), texture2D(uLogo, uv).a);\n"
    "  }\n"
    "  vec2 sq = q - vec2(0.0,-0.1475);\n"
    "  float lum = 0.0;\n"
    "  if(abs(sq.x) < 0.050 && abs(sq.y) < 0.050)\n"
    "    lum = spinner(sq, t, 0.024, 0.040, 0.00375);\n"
    "  col += vec3(0.93,0.95,1.0) * lum;\n"
    "  col *= smoothstep(0.0, 0.45, t);\n"
    "  gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

/* ── debug overlay: 5x7 font + grid + view-area regions into an RGBA buffer ───── */
#define GRID_COLS 16
#define GRID_ROWS 6
static const int k_row_y[GRID_ROWS + 1] = { 0, 90, 181, 271, 361, 452, 542 };
static const uint8_t k_font[36][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
};
static const uint8_t *glyph(char c){
    if (c>='0'&&c<='9') return k_font[c-'0'];
    if (c>='A'&&c<='Z') return k_font[10+(c-'A')];
    return NULL;
}
static void set_px(uint8_t *b, int x, int y, uint8_t r, uint8_t g, uint8_t bl, uint8_t a){
    if (x<0||y<0||x>=PANEL_W||y>=PANEL_H) return;
    uint8_t *p = b + ((size_t)y*PANEL_W + x)*4;
    p[0]=r; p[1]=g; p[2]=bl; p[3]=a;
}
static void set_rect(uint8_t *b, int x0,int y0,int w,int h, uint8_t r,uint8_t g,uint8_t bl,uint8_t a){
    int x1=x0+w, y1=y0+h, x, y;
    if (x0<0)x0=0; if(y0<0)y0=0; if(x1>PANEL_W)x1=PANEL_W; if(y1>PANEL_H)y1=PANEL_H;
    for (y=y0;y<y1;++y) for (x=x0;x<x1;++x) set_px(b,x,y,r,g,bl,a);
}
static void draw_label(uint8_t *b, const char *s, int x, int y, int sc,
                       uint8_t r, uint8_t g, uint8_t bl){
    for (; *s; ++s){ const uint8_t *gl=glyph(*s);
        if (gl){ int row,col,sx,sy;
            for (row=0;row<7;++row) for (col=0;col<5;++col)
                if (gl[row]&(1<<(4-col))) for (sy=0;sy<sc;++sy) for (sx=0;sx<sc;++sx)
                    set_px(b, x+col*sc+sx, y+row*sc+sy, r,g,bl,255); }
        x += (5+1)*sc; }
}
static void outline(uint8_t *b, int x,int y,int w,int h,int t, uint8_t r,uint8_t g,uint8_t bl,uint8_t a){
    set_rect(b,x,y,w,t,r,g,bl,a); set_rect(b,x,y+h-t,w,t,r,g,bl,a);
    set_rect(b,x,y,t,h,r,g,bl,a); set_rect(b,x+w-t,y,t,h,r,g,bl,a);
}
static void draw_grid(uint8_t *b){
    int c,r;
    for (c=0;c<GRID_COLS;++c){ int cx0=c*(PANEL_W/GRID_COLS), cw=PANEL_W/GRID_COLS;
        for (r=0;r<GRID_ROWS;++r){ int cy0=k_row_y[r], ch=k_row_y[r+1]-k_row_y[r];
            char lab[3]; int dark=((c+r)&1);
            if (dark) set_rect(b,cx0,cy0,cw,ch, 0x1e,0x2a,0x66, 130);
            else      set_rect(b,cx0,cy0,cw,ch, 0xb8,0xc0,0xe0, 90);
            lab[0]=(char)('A'+c); lab[1]=(char)('1'+r); lab[2]=0;
            if (dark) draw_label(b,lab,cx0+6,cy0+6,3, 0xff,0xff,0xff);
            else      draw_label(b,lab,cx0+6,cy0+6,3, 0x10,0x18,0x38); } }
    for (c=0;c<=GRID_COLS;++c) set_rect(b, c*(PANEL_W/GRID_COLS)-(c==GRID_COLS?2:0),0,2,PANEL_H, 0xff,0xff,0xff,80);
    for (r=0;r<=GRID_ROWS;++r) set_rect(b, 0,k_row_y[r]-(r==GRID_ROWS?2:0),PANEL_W,2, 0xff,0xff,0xff,80);
}
static void draw_view(uint8_t *b, int mode, int style){
    const loader_layout_t *active = carplay_loader_layout(mode==2 ? 1 : 0, style);
    const loader_layout_t *other = carplay_loader_layout(mode==2 ? 0 : 1, style);
    int ax=(int)active->view_x, ay=(int)active->view_y;
    int aw=(int)active->view_w, ah=(int)active->view_h;
    const char *name = mode==2 ? "SMALLSCREEN" : "FULLSCREEN";
    set_rect(b,0,0,PANEL_W,ay, 0,0,0,130);
    set_rect(b,0,ay+ah,PANEL_W,PANEL_H-(ay+ah), 0,0,0,130);
    set_rect(b,0,ay,ax,ah, 0,0,0,130);
    set_rect(b,ax+aw,ay,PANEL_W-(ax+aw),ah, 0,0,0,130);
    outline(b,(int)other->view_x,(int)other->view_y,
            (int)other->view_w,(int)other->view_h,2, 0xff,0xb4,0x3c,150);
    outline(b,ax,ay,aw,ah,4, 0x28,0xff,0x8c,235);
    draw_label(b,name,ax+10,ay+10,3, 0x28,0xff,0x8c);
    draw_label(b, mode==2?"VIEW 1 SMALLSCREEN":"VIEW 0 FULLSCREEN", 12,8,3, 0x28,0xff,0x8c);
    draw_label(b, style==LOADER_STYLE_CLASSIC?"CLASSIC":"SPORT", 12,34,3, 0x28,0xff,0x8c);
}

/* ── GL helpers ──────────────────────────────────────────────────────────────── */
static GLuint compile(GLenum type, const char *src){
    GLuint s=glCreateShader(type); glShaderSource(s,1,&src,NULL); glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[2048]; glGetShaderInfoLog(s,sizeof(log),NULL,log);
             fprintf(stderr,"shader compile failed:\n%s\n",log); }
    return s;
}
static GLuint load_texture(const char *path, int *ow, int *oh){
    int w,h,n; unsigned char *d=stbi_load(path,&w,&h,&n,4);
    if(!d){ fprintf(stderr,"could not load %s\n",path); if(ow)*ow=0; if(oh)*oh=0; return 0; }
    if(ow)*ow=w; if(oh)*oh=h;
    GLuint tex=0; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,d);
    stbi_image_free(d); return tex;
}

int main(void)
{
    if (!glfwInit()){ fprintf(stderr,"glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *win = glfwCreateWindow(PANEL_W, PANEL_H,
        "CarPlay cluster loader (1440x542) — G:grid V:view S:style ESC:quit", NULL, NULL);
    if (!win){ fprintf(stderr,"window failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, k_vert));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, k_frag));
    glLinkProgram(prog);
    GLint uRes=glGetUniformLocation(prog,"iRes");
    GLint uTime=glGetUniformLocation(prog,"iTime");
    GLint uLogo=glGetUniformLocation(prog,"uLogo");
    GLint uCenter=glGetUniformLocation(prog,"uCenter");
    GLint uScale=glGetUniformLocation(prog,"uScale");
    GLuint logo = load_texture("apple_logo.png", NULL, NULL);
    int sigW=0, sigH=0;
    GLuint sig = load_texture("sig.png", &sigW, &sigH);
    float sigAspect = sigH ? (float)sigW/(float)sigH : 6.0f;

    /* debug-overlay texture (regenerated only when G/V state changes) */
    static uint8_t ov[PANEL_W*PANEL_H*4];
    GLuint ovTex=0; glGenTextures(1,&ovTex);
    glBindTexture(GL_TEXTURE_2D,ovTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

    int grid = getenv("CR_GRID")?1:0;
    int view = getenv("CR_VIEW")?atoi(getenv("CR_VIEW")):0;   /* 0 off,1 fullscreen,2 smallscreen */
    int style = getenv("CR_STYLE") && atoi(getenv("CR_STYLE"))==1
        ? LOADER_STYLE_CLASSIC : LOADER_STYLE_SPORT;
    int dirty = 1, gp=GLFW_RELEASE, vp=GLFW_RELEASE, sp=GLFW_RELEASE;
    double start = glfwGetTime();

    while (!glfwWindowShouldClose(win)){
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE)==GLFW_PRESS) break;
        int gn=glfwGetKey(win,GLFW_KEY_G);
        if (gn==GLFW_PRESS && gp==GLFW_RELEASE){ grid=!grid; dirty=1; fprintf(stderr,"grid=%d\n",grid); }
        gp=gn;
        int vn=glfwGetKey(win,GLFW_KEY_V);
        if (vn==GLFW_PRESS && vp==GLFW_RELEASE){ view=(view+1)%3; dirty=1; fprintf(stderr,"view=%d\n",view); }
        vp=vn;
        int sn=glfwGetKey(win,GLFW_KEY_S);
        if (sn==GLFW_PRESS && sp==GLFW_RELEASE){
            style = style==LOADER_STYLE_CLASSIC ? LOADER_STYLE_SPORT : LOADER_STYLE_CLASSIC;
            dirty=1;
            fprintf(stderr,"style=%s\n",style==LOADER_STYLE_CLASSIC?"classic":"sport");
        }
        sp=sn;

        if (dirty){
            memset(ov,0,sizeof(ov));
            if (grid) draw_grid(ov);
            if (view) draw_view(ov,view,style);
            glBindTexture(GL_TEXTURE_2D,ovTex);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,PANEL_W,PANEL_H,0,GL_RGBA,GL_UNSIGNED_BYTE,ov);
            dirty=0;
        }

        float t=(float)(glfwGetTime()-start);
        if (getenv("CR_T")) t=(float)atol(getenv("CR_T"))/1000.0f;

        int fbw,fbh; glfwGetFramebufferSize(win,&fbw,&fbh);
        glViewport(0,0,fbw,fbh);
        /* loader (shader) */
        /* Exact shared production geometry: [style][fullscreen/smallscreen]. */
        const loader_layout_t *layout = carplay_loader_layout(view==2 ? 1 : 0, style);
        float sx=layout->view_x, sy=layout->view_y;
        float sw=layout->view_w, sh=layout->view_h;
        float content_left=sx+layout->pad_left;
        float content_right=sx+sw-layout->pad_right;
        float content_top=sy+layout->pad_top;
        float content_bottom=sy+sh-layout->pad_bottom;
        float aspect=(float)PANEL_W/(float)PANEL_H;
        float cx_p=(((content_left+content_right)*0.5f)/(float)PANEL_W-0.5f)*aspect;
        float cy_p=0.5f-((content_top+content_bottom)*0.5f)/(float)PANEL_H;
        glUseProgram(prog);
        glUniform2f(uRes,(float)fbw,(float)fbh);
        glUniform1f(uTime,t);
        glUniform2f(uCenter,cx_p,cy_p);
        glUniform1f(uScale,1.0f);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,logo); glUniform1i(uLogo,0);
        glBegin(GL_QUADS);
        glVertex2f(-1,-1); glVertex2f(1,-1); glVertex2f(1,1); glVertex2f(-1,1);
        glEnd();
        /* @LuKa_dev signature — bottom-left of the active view, padded (panel px -> NDC) */
        if (sig){
            float ph=sh*0.055f, pw=ph*sigAspect, pad=sh*0.045f;
            float x0=content_left+pad+layout->signature_x_offset;
            float y1=content_bottom-pad, y0=y1-ph;
            float nx0=(x0/PANEL_W)*2-1,      nx1=((x0+pw)/PANEL_W)*2-1;
            float ny0=1-(y0/PANEL_H)*2,      ny1=1-(y1/PANEL_H)*2;
            float sf=(t-0.3f)/0.6f; sf=sf<0?0:(sf>1?1:sf);
            glUseProgram(0); glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,sig);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1,1,1,0.55f*sf);
            glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2f(nx0,ny1);
            glTexCoord2f(1,1); glVertex2f(nx1,ny1);
            glTexCoord2f(1,0); glVertex2f(nx1,ny0);
            glTexCoord2f(0,0); glVertex2f(nx0,ny0);
            glEnd();
            glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D);
        }
        /* debug overlay (fixed-function textured quad, alpha-blended over the loader) */
        if (grid || view){
            glUseProgram(0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D,ovTex);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1,1,1,1);
            glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2f(-1,-1);
            glTexCoord2f(1,1); glVertex2f(1,-1);
            glTexCoord2f(1,0); glVertex2f(1,1);
            glTexCoord2f(0,0); glVertex2f(-1,1);
            glEnd();
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
        }
        glfwSwapBuffers(win);
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
