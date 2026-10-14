#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bgra_to_nv12.h"

static int rounded(double value)
{ return value < 0 ? 0 : value > 255 ? 255 : (int)(value+.5); }
static int distance(int lhs, int rhs) { return lhs > rhs ? lhs-rhs : rhs-lhs; }

int main(void)
{
    unsigned char src[4096], luma[512], uv[512];
    const unsigned char colors[][6] = {
        {0,0,0,0,128,128}, {255,255,255,255,128,128},
        {0,0,255,76,85,255}, {0,255,0,150,44,21}, {255,0,0,29,255,107}};
    for (unsigned c = 0; c < sizeof(colors)/sizeof(colors[0]); ++c) {
        memset(src, 0, sizeof(src));
        for (unsigned pixel = 0; pixel < 4; ++pixel) {
            memcpy(src+pixel*4, colors[c], 3); src[pixel*4+3] = pixel*64;
        }
        assert(!bgra_to_nv12(src, 8, 16, luma, 2, 4, uv, 2, 2, 2, 2));
        for (unsigned pixel = 0; pixel < 4; ++pixel) assert(luma[pixel] == colors[c][3]);
        assert(uv[0] == colors[c][4] && uv[1] == colors[c][5]);
    }
    assert(bgra_to_nv12(src, 8, 15, luma, 2, 4, uv, 2, 2, 2, 2));
    assert(bgra_to_nv12(src, 8, 16, luma, 2, 3, uv, 2, 2, 2, 2));
    assert(bgra_to_nv12(src, 8, 16, luma, 2, 4, uv, 2, 1, 2, 2));
    assert(bgra_to_nv12(src, 8, 16, luma, 2, 4, uv, 2, 2, 1, 2));
    assert(bgra_to_nv12(src, 8, 16, luma, 2, 4, uv, 2, 2, 2, 1));
    assert(bgra_to_nv12(src, 8, 16, luma, 2, 4, uv, 2, 2, 802, 2));
    assert(bgra_to_nv12(src, 8, 16, luma, 2, 4, uv, 2, 2, 2, 482));
    assert(bgra_to_nv12(src, 0, 16, luma, 2, 4, uv, 2, 2, 2, 2));
    uint32_t rng = 8030;
    for (unsigned trial = 0; trial < 10000; ++trial) {
        size_t width = (trial%4+1)*2, height = (trial%3+1)*2;
        size_t srow = width*4+(trial%5)*4, yrow = width+trial%5, uvrow = width+trial%7;
        for (size_t i = 0; i < srow*height; ++i) { rng = rng*1664525+1013904223; src[i] = rng >> 24; }
        memset(luma, 0xa5, sizeof(luma)); memset(uv, 0xa5, sizeof(uv));
        assert(!bgra_to_nv12(src, srow, srow*height, luma, yrow, yrow*height,
                            uv, uvrow, uvrow*height/2, width, height));
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                const unsigned char *p = src+y*srow+x*4;
                assert(distance(luma[y*yrow+x], rounded(.299*p[2]+.587*p[1]+.114*p[0])) <= 1);
            }
            for (size_t x = width; x < yrow; ++x) assert(luma[y*yrow+x] == 0xa5);
        }
        for (size_t y = 0; y < height; y += 2) {
            for (size_t x = 0; x < width; x += 2) {
                double r = 0, g = 0, b = 0;
                for (size_t dy = 0; dy < 2; ++dy)
                    for (size_t dx = 0; dx < 2; ++dx) {
                        const unsigned char *p = src+(y+dy)*srow+(x+dx)*4;
                        b += p[0]/4.0; g += p[1]/4.0; r += p[2]/4.0;
                    }
                assert(distance(uv[(y/2)*uvrow+x], rounded(128-.168736*r-.331264*g+.5*b)) <= 1);
                assert(distance(uv[(y/2)*uvrow+x+1], rounded(128+.5*r-.418688*g-.081312*b)) <= 1);
            }
            for (size_t x = width; x < uvrow; ++x) assert(uv[(y/2)*uvrow+x] == 0xa5);
        }
        for (size_t i = yrow*height; i < sizeof(luma); ++i) assert(luma[i] == 0xa5);
        for (size_t i = uvrow*(height/2); i < sizeof(uv); ++i) assert(uv[i] == 0xa5);
    }
    /* Full cluster geometry, with independent color checks at both edges of
     * every row and padding guards in all three planes. */
    size_t w=1440,h=542,sr=w*4+16,yr=w+32;
    unsigned char *wide=malloc(sr*h),*wy=malloc(yr*h+64),*wuv=malloc(yr*h/2+64);
    assert(wide && wy && wuv);
    memset(wide,0xa5,sr*h);memset(wy,0xa5,yr*h+64);memset(wuv,0xa5,yr*h/2+64);
    for(size_t y=0;y<h;++y)for(size_t x=0;x<w;++x){
        unsigned char *p=wide+y*sr+x*4;
        p[0]=(x*13+y*5)&255;p[1]=(x*7+y*11)&255;p[2]=(x*17+y*31)&255;p[3]=x&255;
    }
    assert(!bgra_to_nv12(wide,sr,sr*h,wy,yr,yr*h,wuv,yr,yr*h/2,w,h));
    for(size_t y=0;y<h;++y){
        for(size_t edge=0;edge<2;++edge){size_t x=edge?w-1:0;const unsigned char *p=wide+y*sr+x*4;
            assert(distance(wy[y*yr+x],rounded(.299*p[2]+.587*p[1]+.114*p[0]))<=1);}
        for(size_t x=w;x<yr;++x)assert(wy[y*yr+x]==0xa5);
    }
    for(size_t y=0;y<h;y+=2){
        for(size_t edge=0;edge<2;++edge){size_t x=edge?w-2:0;double r=0,g=0,b=0;
            for(size_t dy=0;dy<2;++dy)for(size_t dx=0;dx<2;++dx){const unsigned char *p=wide+(y+dy)*sr+(x+dx)*4;
                b+=p[0]/4.0;g+=p[1]/4.0;r+=p[2]/4.0;}
            assert(distance(wuv[y/2*yr+x],rounded(128-.168736*r-.331264*g+.5*b))<=1);
            assert(distance(wuv[y/2*yr+x+1],rounded(128+.5*r-.418688*g-.081312*b))<=1);}
        for(size_t x=w;x<yr;++x)assert(wuv[y/2*yr+x]==0xa5);
    }
    for(size_t i=0;i<64;++i){assert(wy[yr*h+i]==0xa5);assert(wuv[yr*h/2+i]==0xa5);}
    free(wide);free(wy);free(wuv);
    puts("NV12 colors, independent float reference, padding and 10000 cases passed");
    return 0;
}
