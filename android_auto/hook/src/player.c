/* SPDX-License-Identifier: GPL-3.0-or-later
 * Secondary AA packets -> software H.264 decoder -> managed window 99.
 * This process never reads the main-screen frame ring or changes DM contexts. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include "encoded_ring.h"
#include "menu.h"
#include "cardpanel.h"
#ifndef CL_HOST
#include "omxdec.h"
#endif
#ifndef CL_HOST
#include "cluster_surface.h"
#endif
#define DW 1440
#define DH 455
#define READY "/tmp/sq5_cluster_ready"
static volatile sig_atomic_t quit;
static unsigned decoded, posted, counter;
static uint64_t last_frame, last_ready;
static int owner;
static char prefix[256];
static FILE *recording;
static size_t recorded;
static int record_error;
#ifndef CL_HOST
static cluster_surface_t *surface;
static int adopted;
#endif
static uint64_t ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
static void stop(int sig) { (void)sig; quit=1; }
static void clear_ready(void) {
#ifndef CL_HOST
    /* /dev/shmem does not support atomic rename; empty means immediately absent. */
    FILE *f=fopen(READY,"w"); if(f) fclose(f);
#endif
}
static unsigned char clip(int n) { return n<0?0:n>255?255:(unsigned char)n; }
/* Wide-band mode: the hook asks for 1280x720 with height_margin 315, so the phone draws its UI only
 * in the centred 1280x405 band (same shape as the 1440x455 cockpit area). Keep the two in sync. */
#define BAND_W 1280
#define BAND_H 720
#define BAND_CONTENT_H 405
/* Aspect-preserving fit. crop=1 and a 1280x720 frame: use only the centred content band, which then
 * fills DWxDH (x1.125). Any other size: the whole picture, letterboxed. Strides are honored. */
/* Picture shift in cockpit pixels (SD file sq5_cluster_offset: "<left> <up>", re-read every 5 s).
 * Google draws the guidance car ~88% down the band, under the Audi footer (owner photos run 10); the
 * insets only move Google's cards. Shifting up/left hides the top strip under the Audi top bar and
 * leaves a black strip under the footer. Host tests: shift 0 unless CL_TEST_SHIFT="<left> <up>". */
#ifdef CL_HOST
static volatile int shift_left=0, shift_up=0;
#else
static volatile int shift_left=30, shift_up=60;
#endif
/* Roller zoom (owner 2026-09-29; the phone ignores rotary input on the cluster display, runs 81-85): the
 * player magnifies its own picture around a focus point. The 1080p source is shown at x0.75, so up to x1.33
 * uses real source pixels; capped at x1.6. zoom256 = 256 is 1.0 (identical to no zoom). The focus is where
 * Google puts the car: middle-left of the full view, middle of the Sport window (window x 476..956). */
static volatile int zoom256=256, zoom_small;
static int band_mode;          /* 1080p: 1 = old 1920x607 band scaled x0.75 (SD sq5_cluster_band), 0 = 1:1 viewport */
static volatile int view_sport;  /* small view with the sport skin (/tmp/sq5_cluster_skin) */
/* Card cut-out (owner 2026-09-30: "no matter what zoom, the card stays the same size, same place"). While the
 * roller zoom is active in the 1:1 viewport, Google's green turn card is found in the decoded frame (its header
 * is one exact colour, RGB 9,120,110 = YUV ~90/140/80, plus the near-black ETA strip under it), the zoomed map
 * fills the card's zoomed footprint with the surrounding map colour, and the card itself is pasted 1:1 at its
 * unzoomed place. SD flag sq5_card_cutout_off. */
static int cut_on=1, cut_valid, cut_miss, cut_x0, cut_y0, cut_x1, cut_y1, cut_fy=60, cut_fu=140, cut_fv=118;
/* Map zoom (hook src/uiconfig.c, /tmp/sq5_cluster_mapzoom): mode 1 = the roller resizes Google's layout; the
 * video stays 1:1 and the card is pinned where it sat at step 0 (its top-right corner). */
/* Run 126 (Sport photo): one shared reference pasted the Sport card at its Large-view place -> one per view (L/C/S). */
static volatile int mz_mode, mz_step; static int ref_ok[3], ref_x1[3], ref_y0[3];
static int yuv_at(const AVFrame *f,int x,int y,int *u,int *v) {
    *u=f->data[1][(y>>1)*f->linesize[1]+(x>>1)]; *v=f->data[2][(y>>1)*f->linesize[2]+(x>>1)];
    return f->data[0][y*f->linesize[0]+x];
}
/* Longest run of indices with cnt >= need, allowing gaps of up to 3 samples (white card text). */
static int best_run(const unsigned short *cnt,int n,int need,int *a0,int *a1) {
    int i,s=-1,last=-1,best=0;
    for(i=0;i<=n;i++) {
        int hit=i<n&&cnt[i]>=need;
        if(hit) { if(s<0) s=i; last=i; }
        if(s>=0 && (i==n || (!hit && i-last>3))) { if(last-s+1>best) { best=last-s+1; *a0=s; *a1=last; } s=-1; }
    }
    return best;
}
#define CUT_X0 200
#define CUT_X1 1720
#define CUT_Y0 200
#define CUT_Y1 880
static int card_green(const AVFrame *f,int x,int y) {
    int u,v,yy=yuv_at(f,x,y,&u,&v);
    return yy>=80&&yy<=102&&u>=130&&u<=150&&v>=70&&v<=90;
}
static void detect_card(const AVFrame *f) {
    /* Run 120 (real stream): stray pixels of the same teal elsewhere stretch a plain bounding box, so take the
     * densest run of rows, then the densest run of columns inside those rows. Samples every 4 px. */
    static unsigned short rows[(CUT_Y1-CUT_Y0)/4+1], cols[(CUT_X1-CUT_X0)/4+1];
    int x,y,u,v,ok=0,r0=0,r1=-1,c0=0,c1=-1,bx0,bx1,by0,by1;
    memset(rows,0,sizeof(rows)); memset(cols,0,sizeof(cols));
    for(y=CUT_Y0;y<CUT_Y1;y+=4) for(x=CUT_X0;x<CUT_X1;x+=4) if(card_green(f,x,y)) rows[(y-CUT_Y0)/4]++;
    if(best_run(rows,(CUT_Y1-CUT_Y0)/4,25,&r0,&r1)>=10) {            /* >= 100 px wide per row, >= 40 px tall */
        for(y=CUT_Y0+r0*4;y<=CUT_Y0+r1*4;y+=4) for(x=CUT_X0;x<CUT_X1;x+=4) if(card_green(f,x,y)) cols[(x-CUT_X0)/4]++;
        if(best_run(cols,(CUT_X1-CUT_X0)/4,(r1-r0+1)/3,&c0,&c1)>=30) ok=1;
    }
    bx0=CUT_X0+c0*4; bx1=CUT_X0+c1*4; by0=CUT_Y0+r0*4; by1=CUT_Y0+r1*4;
    if(ok) {
        int yy2=by1+4, s, dark, tot, miss=0, last=by1;
        /* rest of the header, then the near-black ETA strip. Run 125 (photo): the ETA text row was cut off ->
         * a row counts at >= 40 % dark/green, and up to 3 weaker rows (text) may sit inside the strip. */
        for(yy2=by1+1;yy2<by1+110 && yy2<1080;yy2+=2) {
            for(dark=tot=0,s=bx0+6;s<bx1-6;s+=8,tot++) if(yuv_at(f,s,yy2,&u,&v)<48||card_green(f,s,yy2)) dark++;
            if(dark*10>=tot*4) { last=yy2; miss=0; } else if(++miss>3) break;
        }
        yy2=last+2;
        cut_x0=bx0-4; cut_x1=bx1+6; cut_y0=by0-4; cut_y1=yy2+2;
        {   long sy=0,su=0,sv=0; int c=0;                  /* fill = average map colour on a ring outside */
            for(s=cut_x0;s<cut_x1;s+=8) { int yv0=cut_y0-6, yv1=cut_y1+6;
                if(yv0>=0) { sy+=yuv_at(f,s,yv0,&u,&v); su+=u; sv+=v; c++; }
                if(yv1<1080) { sy+=yuv_at(f,s,yv1,&u,&v); su+=u; sv+=v; c++; } }
            if(c) { cut_fy=(int)(sy/c); cut_fu=(int)(su/c); cut_fv=(int)(sv/c); } }
        ok=1;
    }
    if(ok) { cut_valid=1; cut_miss=0; } else if(++cut_miss>3) cut_valid=0;
}
static uint64_t us_now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000u+(uint64_t)t.tv_nsec/1000u; }
/* Run 133 (hardware decode: 30 fps decoded, ~12 shown): the 1:1 conversion is split across both cores (the second
 * one no longer decodes). fast_rows = the fast path of the row loop below for rows [ya, yb). */
typedef struct {
    const unsigned char *Y,*U,*V; int ys,us,vs,fh,fx0,ry0,ow,top,left,stride,rv,gu,gv,bu,sh_r,sh_b;
    const int *ytab; unsigned char *dst;
} fast_job;
static void fast_rows(const fast_job *j,int ya,int yb) {
    int y,x;
    for(y=ya;y<yb;++y) {
        int y0=j->ry0+y; if(y0<0) y0=0; if(y0>j->fh-1) y0=j->fh-1;
        const unsigned char *ys0=j->Y+y0*j->ys+j->fx0, *us0=j->U+(y0>>1)*j->us+(j->fx0>>1), *vs0=j->V+(y0>>1)*j->vs+(j->fx0>>1);
        uint32_t *o=(uint32_t *)(j->dst+(y+j->top)*j->stride+j->left*4);
        for(x=0;x+1<j->ow;x+=2) {             /* fx0 is even (240): one chroma pair per 2 pixels */
            const int cu=us0[x>>1]-128, cv=vs0[x>>1]-128;
            const int r=j->rv*cv+128, g=-j->gu*cu-j->gv*cv+128, b=j->bu*cu+128;
            int c=j->ytab[ys0[x]];
            o[x]=(uint32_t)clip((c+r)>>8)<<j->sh_r|(uint32_t)clip((c+g)>>8)<<8|(uint32_t)clip((c+b)>>8)<<j->sh_b|0xff000000u;
            c=j->ytab[ys0[x+1]];
            o[x+1]=(uint32_t)clip((c+r)>>8)<<j->sh_r|(uint32_t)clip((c+g)>>8)<<8|(uint32_t)clip((c+b)>>8)<<j->sh_b|0xff000000u;
        }
        if(x<j->ow) {
            const int cu=us0[x>>1]-128, cv=vs0[x>>1]-128, c=j->ytab[ys0[x]];
            o[x]=(uint32_t)clip((c+j->rv*cv+128)>>8)<<j->sh_r|(uint32_t)clip((c-j->gu*cu-j->gv*cv+128)>>8)<<8|
                 (uint32_t)clip((c+j->bu*cu+128)>>8)<<j->sh_b|0xff000000u;
        }
    }
}
/* Second-core worker (run 137: also used for the hardware frame fetch). par_rows(fn, arg, ya, yb, align): rows
 * [ya, mid) here and [mid, yb) on the worker, mid a multiple of align; returns when both halves are done. */
typedef void (*rows_fn)(void *arg,int ya,int yb);
static pthread_mutex_t fw_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fw_cv=PTHREAD_COND_INITIALIZER;
static rows_fn fw_fn; static void *fw_arg; static int fw_ya,fw_yb,fw_busy,fw_started,fw_seq;
static void *fast_worker(void *unused) {
    int seen=0; (void)unused;
    pthread_mutex_lock(&fw_lock);
    for(;;) {
        while(fw_seq==seen) pthread_cond_wait(&fw_cv,&fw_lock);
        seen=fw_seq;
        { rows_fn fn=fw_fn; void *arg=fw_arg; int a=fw_ya,b=fw_yb;
          pthread_mutex_unlock(&fw_lock); fn(arg,a,b); pthread_mutex_lock(&fw_lock); }
        fw_busy=0; pthread_cond_broadcast(&fw_cv);
    }
    return NULL;
}
static void par_rows(rows_fn fn,void *arg,int ya,int yb,int align) {
    int mid=ya+(yb-ya)/2;
    if(align>1) mid=(mid/align)*align;
    if(!fw_started) { pthread_t t; fw_started=pthread_create(&t,NULL,fast_worker,NULL)==0?1:-1; if(fw_started>0) pthread_detach(t); }
    if(fw_started<0||mid<=ya||mid>=yb) { fn(arg,ya,yb); return; }
    pthread_mutex_lock(&fw_lock); fw_fn=fn; fw_arg=arg; fw_ya=mid; fw_yb=yb; fw_busy=1; fw_seq++; pthread_cond_broadcast(&fw_cv); pthread_mutex_unlock(&fw_lock);
    fn(arg,ya,mid);
    pthread_mutex_lock(&fw_lock); while(fw_busy) pthread_cond_wait(&fw_cv,&fw_lock); pthread_mutex_unlock(&fw_lock);
}
static void fast_rows_fn(void *arg,int ya,int yb) { fast_rows((const fast_job *)arg,ya,yb); }
/* card-detection area in this frame: 1 = FFmpeg (always, detect every 15th), 2 = wide hardware frame (detect), 0 = no */
static volatile int detect_ok=1;
static unsigned rgba_calls;
static int rgba(const AVFrame *f,unsigned char *dst,int stride,int w,int h,int crop,int bgra) {
    int x,y,ow,oh,left,top,full,bt709,sy0=0,sx0=0,sw=f->width,sh=f->height;
    if(f->width<1||f->height<1||f->width>1920||f->height>1088||stride<w*4||
       (f->format!=AV_PIX_FMT_YUV420P&&f->format!=AV_PIX_FMT_YUVJ420P)) return -1;
    if(crop && f->width==BAND_W && f->height==BAND_H) { sh=BAND_CONTENT_H; sy0=(BAND_H-BAND_CONTENT_H)/2; }
    /* 1080p band (flag sq5_cluster_1080): height_margin 473 -> centred 1920x607, scaled down x0.75 */
    if(crop && f->width==1920 && f->height==1080 && band_mode) { sh=607; sy0=(1080-607)/2; }
    /* 2026-09-30 viewport mode: the phone lays out the centred 1440x540 (margins 480x540) = the cockpit terminal;
     * window 99 is terminal rows 26..480 -> frame x 240.., y 270+26.., copied 1:1 (no scaling). */
    else if(crop && f->width==1920 && f->height==1080) { sx0=240; sw=1440; sy0=270+26; sh=455; }
    ow=w; oh=(int)((int64_t)w*sh/sw);
    if(oh>h) { oh=h; ow=(int)((int64_t)h*sw/sh); }
    if(ow<1||oh<1) return -1;
    left=(w-ow)/2; top=(h-oh)/2;
    full=f->color_range==AVCOL_RANGE_JPEG||f->format==AV_PIX_FMT_YUVJ420P;
    bt709=f->colorspace==AVCOL_SPC_BT709;
    {
        /* Bilinear, luma and chroma (owner 2026-09-29: nearest-neighbour x1.125 looked jagged/pixelated).
         * Fixed point: positions in 1/256 source pixels, pixel-centre aligned; x tables per call. */
        static int lx0[1920],lw[1920],cx0[1920],cw[1920];
        const int rv=full?(bt709?403:359):(bt709?459:409), gu=full?(bt709?48:88):(bt709?55:100);
        const int gv=full?(bt709?120:183):(bt709?136:208), bu=full?(bt709?475:454):(bt709?541:516);
        const int ri=bgra?2:0, bi=bgra?0:2, cwid=(f->width+1)/2, chei=(f->height+1)/2;
        const unsigned char *Y=f->data[0],*U=f->data[1],*V=f->data[2];
        const int ys=f->linesize[0],us=f->linesize[1],vs=f->linesize[2];
        if(ow>1920) return -1;
        for(y=0;y<h;++y) {                      /* only the letterbox bars need clearing */
            unsigned char *d=dst+y*stride;
            if(y<top||y>=top+oh) { for(x=0;x<w;++x) { d[4*x]=d[4*x+1]=d[4*x+2]=0; d[4*x+3]=255; } continue; }
            for(x=0;x<left;++x) { d[4*x]=d[4*x+1]=d[4*x+2]=0; d[4*x+3]=255; }
            for(x=left+ow;x<w;++x) { d[4*x]=d[4*x+1]=d[4*x+2]=0; d[4*x+3]=255; }
        }
        const int shx=crop?shift_left:0, shy=crop?shift_up:0;   /* owner run 10: move the car up/left */
        const int z=crop&&zoom256!=256?zoom256:256;   /* run 108: below 256 = zoom out (Sport only) */
        const int vp=crop&&cut_on&&sx0==240&&f->width==1920&&f->height==1080;
        int cut=vp&&(z!=256||(mz_mode&&mz_step));   /* card cut-out, viewport only */
        int pdx=0,pdy=0;
        const int rv3=view_sport&&!band_mode?2:zoom_small?1:0;
        /* reference for the zoom pin: every 15th frame is plenty (run 133: it ran on every frame) */
        if(vp&&mz_mode&&!mz_step&&detect_ok&&(detect_ok==2||rgba_calls++%15==0||!ref_ok[rv3])) { detect_card(f); if(cut_valid) { ref_x1[rv3]=cut_x1; ref_y0[rv3]=cut_y0; ref_ok[rv3]=1; } }
        int fr=0,fg=0,fb=0;
        if(cut) {
            detect_card(f);
            if(mz_mode&&mz_step&&ref_ok[rv3]&&cut_valid) { pdx=ref_x1[rv3]-cut_x1; pdy=ref_y0[rv3]-cut_y0; }
            /* map zoom is anchored at the card (hook mz_apply): if it stayed within 8 px, show Google's frame
             * untouched - no filler smear. The pin is only a fallback when the card did move. */
            if(z==256&&(!ref_ok[rv3]||!cut_valid||(pdx<=8&&pdx>=-8&&pdy<=8&&pdy>=-8))) { cut=0; pdx=pdy=0; }
            {   int c=full?256*cut_fy:298*(cut_fy-16), u=cut_fu-128, v=cut_fv-128;
                fr=clip((c+rv*v+128)>>8); fg=clip((c-gu*u-gv*v+128)>>8); fb=clip((c+bu*u+128)>>8); }
        }
        /* Focus = where the car is ON SCREEN (run 86 photo: full view ~648,335; Sport window ~716,300), so the car
         * stays fixed while zooming; converted to source-view coords by adding the picture shift. */
        const int zax=(view_sport&&!band_mode?765:zoom_small?716:(ow*50)/100)+shx,   /* sport: map panel 104..474 */ zay=(zoom_small?(oh*66)/100:(oh*68)/100)+shy;   /* run 108: no shift, car at ~50%/68% of the window */
        for(x=0;x<ow;++x) {
            int xv=x+shx, p, q;
            int64_t xv256=z==256?(int64_t)xv*256:((int64_t)(xv-zax)*256*256)/z+(int64_t)zax*256;
            /* run 108: the picture shift right (full_left -40) would leave a black strip -> repeat the frame edge */
            p=(int)((((2*xv256+256)*(int64_t)sw))/(2*ow)-128)+sx0*256;   /* viewport: real frame pixels around it */
            if(p<0) p=0;
            if(p>(f->width-1)*256) p=(f->width-1)*256;
            lx0[x]=p>>8; lw[x]=p&255; if(lx0[x]>=f->width-1) { lx0[x]=f->width-2>0?f->width-2:0; lw[x]=f->width>1?256:0; }
            q=p/2-64; if(q<0) q=0;
            if(q>(cwid-1)*256) q=(cwid-1)*256;
            cx0[x]=q>>8; cw[x]=q&255; if(cx0[x]>=cwid-1) { cx0[x]=cwid-2>0?cwid-2:0; cw[x]=cwid>1?256:0; }
        }
        /* 1:1 fast path (see the row loop): whole rows map onto real frame pixels, nothing zoomed or cut */
        const int fx0=sx0+shx, fast=z==256&&ow==sw&&oh==sh&&fx0>=0&&fx0+ow<=f->width
#ifdef CL_HOST
            &&!getenv("CL_TEST_NOFAST")                   /* host test: compare against the bilinear path */
#endif
            ;
        int ytab[256];
        for(x=0;x<256;++x) ytab[x]=full?256*x:298*(x-16);
        y=0;
        if(fast && !cut && !(fx0&1)) {          /* the common case: whole window 1:1, both cores */
            fast_job j;
            j.Y=Y; j.U=U; j.V=V; j.ys=ys; j.us=us; j.vs=vs; j.fh=f->height; j.fx0=fx0; j.ry0=sy0+shy; j.ow=ow;
            j.top=top; j.left=left; j.stride=stride; j.rv=rv; j.gu=gu; j.gv=gv; j.bu=bu; j.sh_r=ri*8; j.sh_b=bi*8;
            j.ytab=ytab; j.dst=dst;
            par_rows(fast_rows_fn,&j,0,oh,1);
            y=oh;
        }
        for(;y<oh;++y) {
            int yv=y+shy, p, q, y0,wy,c0,wc,y1,c1;
            unsigned char *d=dst+(y+top)*stride+left*4;
            const unsigned char *Y0,*Y1,*U0,*U1,*V0,*V1;
            int64_t yv256=z==256?(int64_t)yv*256:((int64_t)(yv-zay)*256*256)/z+(int64_t)zay*256;
            /* Run 108 (recording, GAL 4.3): the phone draws map over the WHOLE frame, not only the band, so rows
             * uncovered by the picture shift / zoom come from the real frame above/below the band (the frame
             * clamp below still repeats the frame edge). Run 91's band-edge repeat is no longer needed. */
            p=(int)((((2*yv256+256)*(int64_t)sh))/(2*oh)-128)+sy0*256;
            if(p<0) p=0;
            if(p>(f->height-1)*256) p=(f->height-1)*256;
            y0=p>>8; wy=p&255; y1=y0+1<f->height?y0+1:y0;
            q=p/2-64; if(q<0) q=0;
            if(q>(chei-1)*256) q=(chei-1)*256;
            c0=q>>8; wc=q&255; c1=c0+1<chei?c0+1:c0;
            Y0=Y+y0*ys; Y1=Y+y1*ys; U0=U+c0*us; U1=U+c1*us; V0=V+c0*vs; V1=V+c1*vs;
            const int rowcut=cut&&cut_valid&&y0>=cut_y0&&y0<cut_y1;
            /* Run 125 (photo: grey box): the filler is blended per row between the real map pixels just left and
             * right of the card, so it takes the surrounding map colours instead of one averaged grey. */
            int lr=fr,lg=fg,lb=fb,rr=fr,rg=fg,rb=fb;
            if(rowcut) {
                int xl=cut_x0-4<0?0:cut_x0-4, xr=cut_x1+4>f->width-1?f->width-1:cut_x1+4, k;
                for(k=0;k<2;k++) {
                    int xx=k?xr:xl, yy=Y0[xx], u=U0[xx>>1]-128, v=V0[xx>>1]-128, c=full?256*yy:298*(yy-16);
                    int R=clip((c+rv*v+128)>>8), G=clip((c-gu*u-gv*v+128)>>8), B=clip((c+bu*u+128)>>8);
                    if(k) { rr=R; rg=G; rb=B; } else { lr=R; lg=G; lb=B; }
                }
            }
            /* Run 127 (owner: "laggy, ~5 fps"; log: ~25 fps decoded, ~12 posted): at 1:1 every bilinear weight is
             * 0, yet the loop above paid 12 multiplies a pixel. Straight copy-convert when the row maps 1:1 onto
             * real frame pixels: one chroma pair per 2 pixels, one 32-bit store per pixel. */
            if(fast && !rowcut && wy==0) {
                const unsigned char *ys0=Y0+fx0, *us0=U+(y0>>1)*us, *vs0=V+(y0>>1)*vs;
                uint32_t *o=(uint32_t *)d;
                const int sh_r=ri*8, sh_b=bi*8;
                for(x=0;x<ow;++x) {
                    const int sx=fx0+x, cu=us0[sx>>1]-128, cv=vs0[sx>>1]-128;
                    const int c=ytab[ys0[x]], R=clip((c+rv*cv+128)>>8), G=clip((c-gu*cu-gv*cv+128)>>8), B=clip((c+bu*cu+128)>>8);
                    o[x]=(uint32_t)R<<sh_r|(uint32_t)G<<8|(uint32_t)B<<sh_b|0xff000000u;
                }
                continue;
            }
            for(x=0;x<ow;++x) {
                int a=lx0[x],wa=lw[x],b=cx0[x],wb=cw[x];
                if(a<0) { d[4*x]=d[4*x+1]=d[4*x+2]=0; d[4*x+3]=255; continue; }
                if(rowcut&&a>=cut_x0&&a<cut_x1) {
                    int w8=(a-cut_x0)*256/(cut_x1-cut_x0>0?cut_x1-cut_x0:1);
                    d[4*x+ri]=(unsigned char)((lr*(256-w8)+rr*w8)>>8); d[4*x+1]=(unsigned char)((lg*(256-w8)+rg*w8)>>8);
                    d[4*x+bi]=(unsigned char)((lb*(256-w8)+rb*w8)>>8); d[4*x+3]=255; continue;
                }
                int t0=Y0[a]*(256-wa)+Y0[a+1]*wa, t1=Y1[a]*(256-wa)+Y1[a+1]*wa;
                int yy=(t0*(256-wy)+t1*wy+32768)>>16;
                int u0=U0[b]*(256-wb)+U0[b+1]*wb, u1=U1[b]*(256-wb)+U1[b+1]*wb;
                int v0=V0[b]*(256-wb)+V0[b+1]*wb, v1=V1[b]*(256-wb)+V1[b+1]*wb;
                int u=((u0*(256-wc)+u1*wc+32768)>>16)-128, v=((v0*(256-wc)+v1*wc+32768)>>16)-128;
                int c=full?256*yy:298*(yy-16);
                d[4*x+ri]=clip((c+rv*v+128)>>8);
                d[4*x+1]=clip((c-gu*u-gv*v+128)>>8);
                d[4*x+bi]=clip((c+bu*u+128)>>8); d[4*x+3]=255;
            }
        }
        if(cut&&cut_valid) {                          /* paste the card 1:1 where it sits unzoomed */
            int sy,sx;
            for(sy=cut_y0;sy<cut_y1;++sy) {
                int oy=sy-sy0-shy+pdy; unsigned char *d;
                if(oy<0||oy>=oh||sy<0||sy>=f->height) continue;
                d=dst+(oy+top)*stride+left*4;
                for(sx=cut_x0;sx<cut_x1;++sx) {
                    int ox=sx-sx0-shx+pdx, yy, u, v, c;
                    if(ox<0||ox>=ow||sx<0||sx>=f->width) continue;
                    yy=Y[sy*ys+sx]; u=U[(sy>>1)*us+(sx>>1)]-128; v=V[(sy>>1)*vs+(sx>>1)]-128;
                    c=full?256*yy:298*(yy-16);
                    d[4*ox+ri]=clip((c+rv*v+128)>>8); d[4*ox+1]=clip((c-gu*u-gv*v+128)>>8);
                    d[4*ox+bi]=clip((c+bu*u+128)>>8); d[4*ox+3]=255;
                }
            }
        }
    }
    return 0;
}
static void snapshot(const AVFrame *f) {
    char path[300]; unsigned char *pixels,*row; FILE *out; int x,y;
    pixels=malloc((size_t)f->width*f->height*4); if(!pixels) return;
    if(rgba(f,pixels,f->width*4,f->width,f->height,0,0)) { free(pixels); return; }
    row=malloc(f->width*3); if(!row) { free(pixels); return; }
    snprintf(path,sizeof(path),"%s.ppm",prefix); out=fopen(path,"wb");
    if(out) {
        fprintf(out,"P6\n%d %d\n255\n",f->width,f->height);
        for(y=0;y<f->height;++y) {
            for(x=0;x<f->width;++x) memcpy(row+x*3,pixels+((size_t)y*f->width+x)*4,3);
            fwrite(row,1,f->width*3,out);
        }
        fclose(out);
    }
    free(row); free(pixels);
}
/* Run 105: the recording holds only the first ~45 s (full view), so the phone's per-view layout (0x8009) was
 * never seen. A half-size frame is saved ~2.5 s after each view change (the phone has re-laid out by then),
 * at most 6 per session (~1.5 MB each in /tmp): <prefix>-small1.ppm, -full2.ppm, ... */
static volatile uint64_t snap_due;
static volatile int snap_small, snap_count;
void view_snapshot_arm(int small) { snap_small=small; snap_due=ms()+2500; }
/* Run 108: rgba()'s x tables are static and shared with the presenter thread (the first snapshots came out
 * with the presenter's 1440-wide mapping on the right) -> own nearest-neighbour YUV->RGB here, no tables. */
static void view_snapshot(const AVFrame *f) {
    char path[300]; unsigned char *row; FILE *out; int x,y,w=f->width/2,h=f->height/2;
    if(snap_count>=6 || (f->format!=AV_PIX_FMT_YUV420P&&f->format!=AV_PIX_FMT_YUVJ420P)) return;
    row=malloc((size_t)w*3); if(!row) return;
    ++snap_count;
    snprintf(path,sizeof(path),"%s-%s%d.ppm",prefix,snap_small?"small":"full",snap_count); out=fopen(path,"wb");
    if(out) {
        fprintf(out,"P6\n%d %d\n255\n",w,h);
        for(y=0;y<h;++y) {
            const unsigned char *Yr=f->data[0]+(size_t)(y*2)*f->linesize[0], *Ur=f->data[1]+(size_t)y*f->linesize[1],
                                *Vr=f->data[2]+(size_t)y*f->linesize[2];
            for(x=0;x<w;++x) {
                int c=298*(Yr[x*2]-16), u=Ur[x]-128, v=Vr[x]-128;
                row[x*3]=clip((c+409*v+128)>>8); row[x*3+1]=clip((c-100*u-208*v+128)>>8); row[x*3+2]=clip((c+516*u+128)>>8);
            }
            fwrite(row,1,(size_t)w*3,out);
        }
        fclose(out);
        fprintf(stderr,"snapshot %s\n",path);
    }
    free(row);
}
#ifndef CL_HOST
static int ensure_surface(void) {
    if(!surface) {
        cluster_surface_cfg cfg={99,DW,DH,SCREEN_FORMAT_RGBA8888,
                                  SCREEN_USAGE_WRITE|SCREEN_USAGE_NATIVE,2,0};
        surface=cluster_surface_create(&cfg); if(!surface) return -1;
    }
    return 0;
}
#endif
/* Cockpit options menu (src/menu.h): state from Luka, drawn over the converted frame. */
static menu_state g_menu;
static card_state g_card;       /* turn-card panel + bars (src/cardpanel.h) */
static int card_off;            /* SD flag sq5_cardpanel_off (read at start): no panel, no "tc1" */
static volatile int menu_dirty;
static int view_small;
/* Run 133 timing (owner: "push it to 30 fps"): microseconds per presented frame, logged every 10 s */
static uint64_t tm_take,tm_conv,tm_draw,tm_post; static unsigned tm_n,tm_max;
static int present(const AVFrame *f) {
#ifndef CL_HOST
    screen_buffer_t buffers[2]; void *ptr=NULL; int stride=0,rect[4]={0,0,DW,DH};
    char manager[80]={0}; uint64_t now=ms();
    if(ensure_surface()) return -1;
    if(cluster_surface_lost(surface)) {
        clear_ready(); adopted=0; if(cluster_surface_recreate(surface)) return -1;
    }
    if(screen_get_window_property_pv(cluster_surface_window(surface),SCREEN_PROPERTY_RENDER_BUFFERS,(void **)buffers)||
       screen_get_buffer_property_pv(buffers[0],SCREEN_PROPERTY_POINTER,&ptr)||!ptr||
       screen_get_buffer_property_iv(buffers[0],SCREEN_PROPERTY_STRIDE,&stride)) return -1;
    { uint64_t t0=us_now(),t1,t2,t3;
    if(rgba(f,ptr,stride,DW,DH,1,1)) return -1;
    t1=us_now();
    if(!view_small && !card_off) card_draw((unsigned char *)ptr,stride,DW,DH,1,&g_card);
    if(g_menu.valid && g_menu.open) menu_draw((unsigned char *)ptr,stride,DW,DH,1,&g_menu,view_small);
    t2=us_now();
    if(screen_post_window(cluster_surface_window(surface),buffers[0],1,rect,0)) return -1;
    t3=us_now();
    tm_conv+=t1-t0; tm_draw+=t2-t1; tm_post+=t3-t2; tm_n++; if(t3-t0>tm_max) tm_max=(unsigned)(t3-t0);
    }
    ++posted;
    /* Live run 8: the cockpit flashed the Audi map whenever one manager-property read failed (marker not
     * rewritten) - Java then saw it stale. Check adoption once, then refresh the marker every 250 ms. */
    if(!adopted && screen_get_window_property_cv(cluster_surface_window(surface),152,sizeof(manager)-1,manager)==0 &&
       !strcmp(manager,"All your base are belong to us!")) adopted=1;
    if(adopted && now-last_ready>=250) {
        /* Live run 9: fopen("w") truncates first; MirrorGate sometimes read the empty file in between ->
         * "stale/absent" -> the Audi map flashed for ~40 ms (8x per drive). Overwrite in place with a
         * fixed-length record (zero-padded counter/pid), never truncating. */
        char rec[64]; int fd=open(READY,O_WRONLY|O_CREAT,0644), len;
        /* " tc1" (run 106): this player draws the turn-card panel behind plane 98 (MirrorGate.drawsCardPanel), so
         * Luka leaves its opaque backing off; "---" instead keeps the record length when SD sq5_cardpanel_off */
        len=snprintf(rec,sizeof(rec),"%010u %010ld 1440x455 cluster1 %s\n",++counter,(long)getpid(),card_off?"---":"tc1");
        if(fd>=0) { if(pwrite(fd,rec,(size_t)len,0)==len) last_ready=now; close(fd); }
    }
#else
    unsigned char *out=malloc(DW*DH*4); int rc;
    const char *dump=getenv("CL_TEST_PRESENT_PPM");   /* host test only: first presented DWxDH frame */
    const char *stall=getenv("CL_TEST_STALL_MS");     /* host test only: car-like window-adoption stall */
    if(!out) return -1;
    if(stall && !posted) usleep((useconds_t)atoi(stall)*1000u);
    rc=rgba(f,out,DW*4,DW,DH,1,0);
    if(!g_menu.valid && getenv("CL_TEST_MENU")) menu_poll(getenv("CL_TEST_MENU"),&g_menu);   /* host test only */
    if(!rc && g_menu.valid && g_menu.open) menu_draw(out,DW*4,DW,DH,0,&g_menu,view_small);
    if(!rc && dump && !posted) {
        FILE *p=fopen(dump,"wb"); int i;
        if(p) { fprintf(p,"P6\n%d %d\n255\n",DW,DH); for(i=0;i<DW*DH;++i) fwrite(out+4*i,1,3,p); fclose(p); }
    }
    free(out); if(rc) return rc;
    ++posted;
#endif
    return 0;
}
static pthread_mutex_t frame_lock=PTHREAD_MUTEX_INITIALIZER;
static AVFrame *latest;          /* newest decoded frame, owned under frame_lock */
static int fresh;                /* latest not yet presented */
static volatile int present_quit;
/* Run 127 (lag): hardware decode (src/omxdec.c) unless SD sq5_omx_off or it failed earlier this power cycle
 * (/tmp/sq5_omx_failed); FFmpeg takes over if the decoder fails to start or shows nothing in 90 packets. */
static volatile int use_omx;
static unsigned omx_shown, omx_takes;
static int receive_frames(AVCodecContext *ctx,AVFrame *f) {
    int rc;
    while((rc=avcodec_receive_frame(ctx,f))==0) {
        ++decoded; last_frame=ms();
        if(decoded==1) {
            fprintf(stderr,"decoded.first width=%d height=%d format=%d range=%d space=%d\n",
                    f->width,f->height,f->format,f->color_range,f->colorspace);
            snapshot(f);
        }
        if(snap_due && last_frame>=snap_due) { snap_due=0; view_snapshot(f); }
        /* Live run 8: presenting on this thread cost decode time (20-28 fps decoded for 30 sent, backlog
         * up to 32 MB = the cockpit ran seconds late). Hand the newest frame to the presenter thread;
         * older undisplayed frames are simply replaced, never shown late. */
        pthread_mutex_lock(&frame_lock);
        av_frame_unref(latest);
        if(av_frame_ref(latest,f)==0) fresh=1;
        pthread_mutex_unlock(&frame_lock);
        if(decoded%300==0) fprintf(stderr,"decoded.frames=%u posted=%u\n",decoded,posted);
        av_frame_unref(f);
    }
    return rc==AVERROR(EAGAIN)||rc==AVERROR_EOF?0:rc;
}
/* Full view vs the Sport layout's small view area (run 11: only the left ~480 px of window 99 is visible
 * there, so the car and card were behind the centre dial). luka's small-stage offset is 476 px, so the
 * small view default is 30+476 left. The patched MirrorGate writes /tmp/sq5_cluster_view on change. */
/* Run 54 (owner-approved): no shift in either view; the phone lays out per view (uiconfig.c relayout). */
/* Small view: Sport window = window x 476..956; the full-view car sits at ~519, so -200 centres it there
 * (the picture moves right; the black strip at window x < 200 is outside the Sport window). */
/* Run 80 photos (owner): map up 50 in both views; Sport slice -120 puts the car nearer the Sport window centre.
 * base_* = defaults or SD sq5_cluster_offset; the menu's Map up/down (Luka record) is added on top. */
/* Run 88: driving camera puts the car near the frame bottom (ignores the bottom inset) -> full shift 100;
 * overview is centred in the safe area -> bottom inset back to 40 keeps it from riding high. */
/* Run 91: margins back to 60/160 (Sport was right there), right 170 moves the card left -> Sport slice -80
 * keeps the car where it was; the full view gets its own shift (edge rows repeat, no black bars). */
static int base_full_up=0, base_small_up=160; /* run 108: no shift - the full band is shown and the phone places car + card via the insets */
static int full_left=0, /* run 108: symmetric insets centre the car, no shift */ full_up=0, small_left=-80, small_up=160;
static void apply_view(void) {
    int l=view_small?small_left:full_left, u=view_small?small_up:full_up;
    zoom_small=view_small;
    if(!view_small && zoom256<256) zoom256=256;   /* zoom-out is Sport only: full view already shows the full width */
    if(l!=shift_left||u!=shift_up) { shift_left=l; shift_up=u; fprintf(stderr,"shift left=%d up=%d view=%s\n",l,u,view_small?"small":"full"); }
}
static void load_shift(void) {
    int a,b,c,d,n; const char *s;
#ifdef CL_HOST
    s=getenv("CL_TEST_SHIFT");
    if(s && sscanf(s,"%d %d",&a,&b)==2) { shift_left=a; shift_up=b; }
    s=getenv("CL_TEST_ZOOM");                         /* host test only: zoom in 1/256 */
    if(s && sscanf(s,"%d",&c)==1 && c>=256 && c<=410) zoom256=c;
    (void)d; (void)n;
#else
    FILE *f;
    (void)s;
    /* Relayout mode: the phone lays the map out for the Sport window
     * itself (0x8009), so Sport needs no picture shift; an offset file below still overrides. */
    { int rl=access("/fs/sda0/sq5_cluster_relayout_off",F_OK)!=0; static int last_rl=-1;
      small_left=rl?0:-80; base_small_up=rl?0:160;    /* flag removed -> the normal Sport defaults again */
      if(rl!=last_rl) { last_rl=rl; fprintf(stderr,"relayout flag=%d small base %d/%d menu up=%d upSmall=%d\n",rl,small_left,base_small_up,g_menu.valid?g_menu.up:0,g_menu.valid?g_menu.up_small:0); } }
    small_up=base_small_up+(g_menu.valid?g_menu.up_small:0);
    f=fopen("/fs/sda0/sq5_cluster_offset","r");
    if(!f) f=fopen("/mnt/app/root/sq5_android_auto/sq5_cluster_offset","r");
    if(!f) { apply_view(); return; }
    /* "<left> <up> [<small_left> <small_up>]" cockpit pixels */
    n=fscanf(f,"%d %d %d %d",&a,&b,&c,&d);
    if(n>=2 && a>=-600 && a<=600 && b>=-200 && b<=200) { full_left=a; base_full_up=b; }
    if(n==4 && c>=-600 && c<=900 && d>=-200 && d<=200) { small_left=c; base_small_up=d; }
    fclose(f);
    full_up=base_full_up+(g_menu.valid?g_menu.up:0); small_up=base_small_up+(g_menu.valid?g_menu.up_small:0);
    apply_view();
#endif
}
static void load_view(void) {
#ifndef CL_HOST
    char v[16]={0}; FILE *f=fopen("/tmp/sq5_cluster_view","r");
    if(!f) return;
    if(fgets(v,sizeof(v),f)) {                        /* empty (mid-write) -> keep the previous view */
        int was=view_small;
        if(!strncmp(v,"small",5)) view_small=1;
        else if(!strncmp(v,"full",4)) view_small=0;
        if(view_small!=was) { void view_snapshot_arm(int); view_snapshot_arm(view_small); }
    }
    fclose(f);
    {   char k[8]={0}; FILE *kf=fopen("/tmp/sq5_cluster_skin","r");
        if(kf) { if(!fgets(k,sizeof(k),kf)) k[0]=0; fclose(kf); }
        view_sport=view_small && k[0]=='s';
    }
    apply_view();
#endif
}
/* Presenter: newest decoded frame only, at most 15 fps (66 ms), on its own thread and core. */
static void *presenter(void *unused) {
    AVFrame *mine=av_frame_alloc(); uint64_t lastp=0,lastshift=0,lastview=0,lastmenu=0; (void)unused;
    if(!mine) return NULL;
    while(!present_quit) {
        int got=0; uint64_t now=ms();
        if(now-lastshift>=5000) { load_shift(); lastshift=now; }
        if(now-lastview>=250) { int was=view_small; load_view(); lastview=now; if(was!=view_small && g_menu.open) menu_dirty=1; }
#ifndef CL_HOST
        if(now-lastmenu>=100) {
            lastmenu=now;
            {   static uint64_t card_changed;
                if(card_poll(CP_PATH,&g_card,now,&card_changed)) {
                    menu_dirty=1;
                    fprintf(stderr,"card shown=%d rect=%d,%d %dx%d bar=%d pm=%d dist='%s'\n",g_card.card,g_card.x,g_card.y,
                            g_card.w,g_card.h,g_card.bar,g_card.pm,g_card.dist);
                }
            }
            if(menu_poll("/tmp/sq5_cluster_menu",&g_menu)) {
                full_up=base_full_up+g_menu.up; small_up=base_small_up+g_menu.up_small; apply_view(); menu_dirty=1;
                fprintf(stderr,"menu seq=%d open=%d sel=%d edit=%d up=%d\n",g_menu.seq,g_menu.open,g_menu.sel,g_menu.edit,g_menu.up);
            }
            {   /* map zoom state from the hook: "SQ5Z <mode> <step>" */
                int m,s; char zb[24]={0}; FILE *zf=fopen("/tmp/sq5_cluster_mapzoom","r");
                if(zf) {
                    if(fgets(zb,sizeof(zb),zf) && sscanf(zb,"SQ5Z %d %d",&m,&s)==2 && (m!=mz_mode||s!=mz_step)) {
                        mz_mode=m; mz_step=s; menu_dirty=1;
                        if(m && zoom256!=256) zoom256=256;              /* map mode: the video stays 1:1 */
                        fprintf(stderr,"mapzoom mode=%d step=%d\n",m,s);
                    }
                    fclose(zf);
                }
            }
            {   /* roller zoom: Luka's cumulative counter (AaRollerInput), ~6 % per step, x1.0 .. x1.6 (Sport x0.6 .. x1.6) */
                static int have, last;
                /* Run 137 (owner): Digital zoom removed (blurry, ~7 fps) - the hook zooms the map; the roller counter is
                 * no longer read here. SD sq5_digital_zoom_on brings it back for testing only. */
                static int dz=-1; if(dz<0) dz=access("/fs/sda0/sq5_digital_zoom_on",F_OK)==0;
                int tot; char rb[40]={0}; FILE *rf=(mz_mode||!dz)?NULL:fopen("/tmp/sq5_cluster_rotary","r");
                if(rf) {
                    if(fgets(rb,sizeof(rb),rf) && sscanf(rb,"SQ5R %d",&tot)==1) {
                        if(!have) { last=tot; have=1; }
                        else if(tot!=last) {
                            int z=zoom256+(tot-last)*16; last=tot;
                            if(z<(view_small?154:256)) z=view_small?154:256;   /* run 108: Sport can zoom out to x0.6 (the phone draws the whole frame) */
                            if(z>410) z=410;
                            if(z!=zoom256) { zoom256=z; menu_dirty=1; fprintf(stderr,"zoom %d/256\n",z); }
                        }
                    }
                    fclose(rf);
                }
            }
        }
#else
        (void)lastmenu;
#endif
        /* 1:1 viewport: 20 ms gate (run 142: ~22 ms of work a frame, the decoder delivers in pairs sometimes -> at 30 ms
         * the older of two close frames was replaced: 24-27 of 30 shown). 15 fps on the old band. */
        if(now-lastp>=(band_mode?66:20)) {
#ifndef CL_HOST
            if(use_omx) {                        /* hardware decoder: de-tile the newest frame into mine */
                /* Run 135 (take ~26 ms = slow reads of the decoder buffer): only the shown window (rows 296..751,
                 * columns 240..1679); every 15th frame also the card-detection area (200..990 x 200..1720);
                 * the whole frame for snapshots / zoom / shift / cut-out. detect_ok tells rgba() it may detect. */
                /* run 149: a map-zoom step only needs the card area (the wide window), not the whole frame - the
                 * owner left step -1 on and every frame was de-tiled whole (take ~20 ms, 22.6 fps shown) */
                const int whole=!omx_shown||snap_due||zoom256!=256||shift_up||shift_left;
                const int wide=whole||mz_step||omx_takes%15==0;
                uint64_t t0=us_now(); int took;
                detect_ok=wide?2:0;
                took=omxdec_take(mine,whole?0:wide?192:296,whole?1088:wide?1000:752,whole?0:wide?192:240,whole?1920:wide?1728:1680,par_rows);
                if(took) omx_takes++;
                tm_take+=us_now()-t0;
                if(took) {
                    got=1; last_frame=now;
                    if(!omx_shown++) { fprintf(stderr,"decoded.first width=%d height=%d source=omx\n",mine->width,mine->height); snapshot(mine); }
                    if(snap_due && now>=snap_due) { snap_due=0; view_snapshot(mine); }
                }
            } else
#endif
            {
            pthread_mutex_lock(&frame_lock);
            if(fresh) { av_frame_unref(mine); av_frame_move_ref(mine,latest); fresh=0; got=1; }
            pthread_mutex_unlock(&frame_lock);
            }
        }
        /* The last frame is kept, so a menu change is shown at once even while the map picture is still. */
        if(got || (menu_dirty && mine->data[0])) { menu_dirty=0; if(!present(mine)) lastp=now; }
        else usleep(5000);
    }
    av_frame_free(&mine);
    return NULL;
}
static int decode(AVCodecContext *ctx,AVFrame *f,AVPacket *pkt,const void *data,unsigned size) {
    int rc;
    av_packet_unref(pkt);
    if(av_new_packet(pkt,(int)size)<0) return -1;
    memcpy(pkt->data,data,size); /* av_new_packet provides decoder input padding */
    rc=avcodec_send_packet(ctx,pkt);
    if(rc==AVERROR(EAGAIN)) {
        if(receive_frames(ctx,f)) return -1;
        rc=avcodec_send_packet(ctx,pkt);
    }
    if(rc<0) return rc;
    return receive_frames(ctx,f);
}
/* Reader thread (car logs 2026-09-29): the ring has CL_SLOTS slots and the hook waits for a new IDR
 * after any overflow, but AA sends an IDR only at stream start. One stall here (window adoption at the
 * first frame) filled the ring, and the stream stopped for good after 8 packets while the phone kept
 * sending. The reader copies packets out and frees each slot at once; decode/present run off this
 * in-process queue. QUEUE_MAX bounds memory if the decoder cannot keep up (logged: NEON build needed). */
#define QUEUE_MAX (32u*1024u*1024u)
typedef struct qpkt { struct qpkt *next; unsigned size,epoch,flags; unsigned char data[1]; } qpkt;
static pthread_mutex_t qlock=PTHREAD_MUTEX_INITIALIZER;
static qpkt *qhead,*qtail;
static size_t qbytes;
static unsigned qdrops,qpeak_kb;
static volatile int reader_done,reader_error;
static cl_ring *ring_r;
static void *reader(void *unused) {
    int skip=0; (void)unused;
    while(!quit && !ring_r->stopped && kill(owner,0)==0) {
        cl_packet *p=cl_read_slot(ring_r); qpkt *n; size_t queued;
        if(!p) { usleep(2000); continue; }
        if(!p->size||p->size>CL_PACKET_MAX) { cl_release(ring_r); reader_error=3; break; }
        /* After a queue drop the P-frame chain is broken: skip to the next keyframe. */
        if(skip && !(p->flags&CL_KEYFRAME)) { cl_release(ring_r); ++qdrops; continue; }
        pthread_mutex_lock(&qlock); queued=qbytes; pthread_mutex_unlock(&qlock);
        n=queued+p->size<=QUEUE_MAX?malloc(sizeof(qpkt)+p->size):NULL;
        if(!n) { cl_release(ring_r); ++qdrops; skip=1; continue; }
        n->next=NULL; n->size=p->size; n->epoch=p->epoch; n->flags=p->flags;
        memcpy(n->data,p->data,p->size);
        cl_release(ring_r); skip=0;
        pthread_mutex_lock(&qlock);
        if(qtail) qtail->next=n; else qhead=n;
        qtail=n; qbytes+=n->size;
        if(qbytes/1024>qpeak_kb) qpeak_kb=(unsigned)(qbytes/1024);
        pthread_mutex_unlock(&qlock);
    }
    reader_done=1;
    return NULL;
}
static qpkt *qpop(void) {
    qpkt *n;
    pthread_mutex_lock(&qlock);
    n=qhead;
    if(n) { qhead=n->next; if(!qhead) qtail=NULL; qbytes-=n->size; }
    pthread_mutex_unlock(&qlock);
    return n;
}
int main(int argc,char **argv) {
    const AVCodec *codec; AVCodecContext *ctx=NULL; AVFrame *frame=NULL; AVPacket *pkt=NULL;
    cl_ring *r=NULL; int fd=-1,rc=1; unsigned epoch=0; char object[96],path[300];
#ifndef CL_HOST
    static qpkt *stash[400]; unsigned stash_n=0, omx_fed_blind=0; size_t stash_bytes=0;
#endif
    uint64_t start=ms(),last_stat=0; struct stat st; pthread_t reader_thread,present_thread; int reader_started=0,present_started=0;
    if(argc!=2 || (owner=atoi(argv[1]))<2) { fprintf(stderr,"usage: cluster-player GAL_PID\n"); return 2; }
    /* Live run 5 ended with PING_TIMEOUT_OCCURRED while the decoder ran flat out at gal's priority.
     * Run below gal and the HMI; decoder threads inherit this. */
    errno=0; if(nice(4)==-1 && errno) fprintf(stderr,"nice failed errno=%d\n",errno);
    signal(SIGTERM,stop); signal(SIGINT,stop);
    snprintf(prefix,sizeof(prefix),"/tmp/sq5_cluster_live-%d",owner);
    snprintf(object,sizeof(object),"/sq5_cluster_encoded_%d",owner);
    clear_ready();
    while(!quit && kill(owner,0)==0 && ms()-start<15000) {
        fd=shm_open(object,O_RDWR,0600);
        if(fd>=0) {
            if(!fstat(fd,&st) && st.st_size==(off_t)sizeof(cl_ring)) {
                r=mmap(NULL,sizeof(cl_ring),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
                if(r==MAP_FAILED) r=NULL;
            }
            close(fd);
            if(r) {
                while(!r->magic && !quit && kill(owner,0)==0 && ms()-start<15000) usleep(5000);
                break;
            }
        }
        usleep(100000);
    }
    if(!r) { fprintf(stderr,"transport unavailable\n"); goto done; }
    __sync_synchronize();
    if(r->magic!=CL_MAGIC||r->version!=CL_VERSION||r->owner!=(unsigned)owner||r->bytes!=sizeof(cl_ring)) goto done;
    codec=avcodec_find_decoder(AV_CODEC_ID_H264); if(!codec) goto done;
    ctx=avcodec_alloc_context3(codec); frame=av_frame_alloc(); pkt=av_packet_alloc();
    if(!ctx||!frame||!pkt) goto done;
    /* Live run 5: slice threads alone gave 12-18 fps on a moving 720p map (phone encodes one slice).
     * Frame threads decode 2 frames in parallel (+1 frame latency); NEON build does the rest. */
    ctx->thread_count=3; ctx->thread_type=FF_THREAD_FRAME|FF_THREAD_SLICE;  /* run 8: 2 threads = 20-28 fps */
    ctx->flags2|=AV_CODEC_FLAG2_FAST;
    ctx->max_pixels=1920*1088;
    if(avcodec_open2(ctx,codec,NULL)<0) goto done;
    snprintf(path,sizeof(path),"%s.h264",prefix); recording=fopen(path,"wb");
    card_off=access("/fs/sda0/sq5_cardpanel_off",F_OK)==0;
    band_mode=access("/fs/sda0/sq5_cluster_band",F_OK)==0;
    cut_on=access("/fs/sda0/sq5_card_cutout_off",F_OK)!=0;
#ifdef CL_HOST
    band_mode=getenv("CL_TEST_BAND")!=NULL;   /* host tests: legacy band geometry */
#endif
#ifndef CL_HOST
    if(access("/fs/sda0/sq5_omx_off",F_OK)==0) fprintf(stderr,"decoder=ffmpeg (sq5_omx_off)\n");
    else if(access("/tmp/sq5_omx_failed",F_OK)==0) fprintf(stderr,"decoder=ffmpeg (hardware failed earlier this power cycle)\n");
    else use_omx=omxdec_open(1920,1080)==0;
#endif
    fprintf(stderr,"player.start owner=%d window=99 geometry=%s fps_limit=%d reader=thread decoder=%s\n",owner,
            band_mode?"band":"viewport1440x540-1to1",band_mode?15:30,use_omx?"omx":"ffmpeg");
#ifndef CL_HOST
    /* Adopt the window before the stream starts, not at the first frame. */
    if(ensure_surface()) fprintf(stderr,"surface.early failed; retried at the first frame\n");
    else fprintf(stderr,"surface.early ok\n");
#endif
    ring_r=r;
    if(pthread_create(&reader_thread,NULL,reader,NULL)) { fprintf(stderr,"reader thread failed\n"); goto done; }
    reader_started=1;
    latest=av_frame_alloc();
    if(latest && !pthread_create(&present_thread,NULL,presenter,NULL)) present_started=1;
    else fprintf(stderr,"presenter thread failed\n");
    while(!quit) {
        qpkt *p=qpop();
        if(!p) {
            if(reader_done) break;
            if(last_frame && ms()-last_frame>5000) clear_ready();   /* run 8: 2 s caused Audi-map flashes */
            usleep(3000); continue;
        }
        if(epoch!=p->epoch) {
            avcodec_flush_buffers(ctx); epoch=p->epoch; clear_ready();
#ifndef CL_HOST
            if(use_omx) {                           /* new stream: fresh hardware decoder, fresh stash */
                while(stash_n) free(stash[--stash_n]);
                stash_bytes=0; omx_fed_blind=0; omxdec_close();
                if(omxdec_open(1920,1080)) { use_omx=0; fprintf(stderr,"decoder.fallback ffmpeg reopen failed\n"); }
            }
#endif
            fprintf(stderr,"decoder.epoch=%u drops=%u\n",epoch,r->drops);
            if(!(p->flags&CL_KEYFRAME)) { free(p); continue; }
        }
        if(recording && recorded+p->size<=16u*1024u*1024u) {
            size_t n=fwrite(p->data,1,p->size,recording); recorded+=n;
            if(n!=p->size || fflush(recording)) record_error=1;
        }
#ifndef CL_HOST
        if(use_omx) {
            int bad;
            const unsigned char *fd_data=p->data; unsigned fd_size=p->size;
            if(!omxdec_frames()) {                  /* keep the stream start until the hardware shows a frame */
                if(stash_n<400 && stash_bytes+p->size<=16u*1024u*1024u) { stash[stash_n++]=p; stash_bytes+=p->size; p=NULL; }
            } else if(stash_n) { while(stash_n) free(stash[--stash_n]); stash_bytes=0; }
            bad=omxdec_feed(fd_data,fd_size)<0;
            if(!bad && !omxdec_frames() && ++omx_fed_blind>90) { fprintf(stderr,"omx: no frame after 90 packets\n"); bad=1; }
            if(bad) {
                unsigned i; FILE *mf;
                fprintf(stderr,"decoder.fallback ffmpeg frames=%d replay=%u\n",omxdec_frames(),stash_n);
                use_omx=0; omxdec_close();
                mf=fopen("/tmp/sq5_omx_failed","w"); if(mf) fclose(mf);
                for(i=0;i<stash_n;i++) { decode(ctx,frame,pkt,stash[i]->data,stash[i]->size); free(stash[i]); }
                stash_n=0; stash_bytes=0;
                if(p) decode(ctx,frame,pkt,p->data,p->size);
            }
            if(use_omx) decoded=(unsigned)omxdec_frames();
        } else
#endif
        if(decode(ctx,frame,pkt,p->data,p->size)<0) {
            fprintf(stderr,"decoder.error epoch=%u size=%u\n",epoch,p->size); clear_ready();
        }
        free(p);
        if(ms()-last_stat>=10000) {
            size_t q; pthread_mutex_lock(&qlock); q=qbytes; pthread_mutex_unlock(&qlock);
            fprintf(stderr,"stat decoded=%u posted=%u queue_kb=%u peak_kb=%u queue_drops=%u ring_drops=%u\n",
                    decoded,posted,(unsigned)(q/1024),qpeak_kb,qdrops,r->drops);
            if(tm_n) {                              /* averages in 0.1 ms; the presenter races these counters, fine for a log */
                unsigned n=tm_n;
                fprintf(stderr,"present.timing n=%u take=%.1f conv=%.1f draw=%.1f post=%.1f max=%.1f ms\n",n,
                        tm_take/1000.0/n,tm_conv/1000.0/n,tm_draw/1000.0/n,tm_post/1000.0/n,tm_max/1000.0);
                tm_take=tm_conv=tm_draw=tm_post=0; tm_n=0; tm_max=0;
            }
            last_stat=ms();
        }
    }
    rc=reader_error;
    /* End of stream: drain frames still held by the decoder (frame threads keep one per thread). */
    if(ctx && avcodec_send_packet(ctx,NULL)>=0) receive_frames(ctx,frame);
done:
    quit=1;
    present_quit=1;
    if(present_started) pthread_join(present_thread,NULL);
#ifndef CL_HOST
    if(use_omx) { decoded=(unsigned)omxdec_frames(); omxdec_close(); }
    while(stash_n) free(stash[--stash_n]);
#endif
    av_frame_free(&latest);
    if(reader_started) {
        qpkt *p;
        pthread_join(reader_thread,NULL);
        while((p=qpop())) free(p);
        fprintf(stderr,"reader.exit queue_peak_kb=%u queue_drops=%u ring_drops=%u\n",qpeak_kb,qdrops,r?r->drops:0);
    }
    clear_ready();
    if(recording) { if(fclose(recording)) record_error=1; recording=NULL;
        snprintf(path,sizeof(path),"%s.h264.done",prefix); recording=fopen(path,"w");
        if(recording) { fprintf(recording,"bytes=%lu\ndecoded=%u\nio_error=%d\nexit=%d\n",(unsigned long)recorded,decoded,record_error,rc); fclose(recording); }
    }
    fprintf(stderr,"player.exit rc=%d decoded=%u posted=%u recorded=%lu\n",rc,decoded,posted,(unsigned long)recorded);
    if(r) munmap(r,sizeof(cl_ring));
    /* Only own producer object after its producer has exited. */
    if(kill(owner,0)!=0 && errno==ESRCH) shm_unlink(object);
    av_packet_free(&pkt); av_frame_free(&frame); avcodec_free_context(&ctx);
#ifndef CL_HOST
    cluster_surface_destroy(surface);
#endif
    return rc;
}
