/* Bounded iAP2 metadata decode shared by host sanitizers and QNX adapter. */
#ifndef MHI2Q_NOW_PLAYING_H
#define MHI2Q_NOW_PLAYING_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define NP_FRAME_MAX 4096u
#define NP_TEXT_MAX 255u
enum { NP_TITLE,NP_DURATION,NP_ALBUM,NP_ARTIST,NP_GENRE,NP_COMPOSER,
       NP_STATUS,NP_ELAPSED,NP_SHUFFLE,NP_REPEAT,NP_APP,NP_FIELDS };
struct np_update {
    uint32_t mask,number[NP_FIELDS];
    char text[NP_FIELDS][NP_TEXT_MAX+1];
};
static inline unsigned np_be16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static inline int np_text(char *out,const uint8_t *p,size_t n) {
    if(n && p[n-1]==0)--n;
    if(n>NP_TEXT_MAX)return -1;
    for(size_t i=0;i<n;) {
        uint32_t c=p[i++];unsigned k;uint32_t min;
        if(c<0x80){if(c<32||c==127)return -1;continue;}
        if(c>=0xc2&&c<=0xdf){k=1;min=0x80;c&=31;}
        else if(c>=0xe0&&c<=0xef){k=2;min=0x800;c&=15;}
        else if(c>=0xf0&&c<=0xf4){k=3;min=0x10000;c&=7;}
        else return -1;
        if(k>n-i)return -1;
        while(k--){if((p[i]&0xc0)!=0x80)return -1;c=(c<<6)|(p[i++]&63);}
        if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return -1;
    }
    memcpy(out,p,n);out[n]=0;return 0;
}
static inline int np_decode(const uint8_t *frame,size_t length,struct np_update *out) {
    struct np_update next;unsigned groups=0;memset(&next,0,sizeof(next));
    if(!frame||!out||length<6||length>NP_FRAME_MAX||memcmp(frame,"@@",2)||
       np_be16(frame+2)!=length||np_be16(frame+4)!=0x5001)return -1;
    for(size_t pos=6;pos<length;) {
        if(length-pos<4)return -1;
        size_t n=np_be16(frame+pos);unsigned group=np_be16(frame+pos+2);
        if(n<4||n>length-pos)return -1;
        if(group<=1) {
            if(groups&(1u<<group))return -1;
            groups|=1u<<group;
            for(size_t at=pos+4;at<pos+n;) {
                if(pos+n-at<4)return -1;
                size_t size=np_be16(frame+at);unsigned id=np_be16(frame+at+2);
                if(size<4||size>pos+n-at)return -1;
                int field=-1,is_text=0;unsigned width=0;
                if(group==0)switch(id) {
                    case 1:field=NP_TITLE;is_text=1;break;
                    case 4:field=NP_DURATION;width=4;break;
                    case 6:field=NP_ALBUM;is_text=1;break;
                    case 12:field=NP_ARTIST;is_text=1;break;
                    case 16:field=NP_GENRE;is_text=1;break;
                    case 18:field=NP_COMPOSER;is_text=1;break;
                }
                else switch(id) {
                    case 0:field=NP_STATUS;width=1;break;
                    case 1:field=NP_ELAPSED;width=4;break;
                    case 2:field=NP_SHUFFLE;width=1;break;
                    case 3:field=NP_REPEAT;width=1;break;
                    case 7:field=NP_APP;is_text=1;break;
                }
                if(field>=0) {
                    if(next.mask&(1u<<field))return -1;
                    next.mask|=1u<<field;
                    if(is_text){if(np_text(next.text[field],frame+at+4,size-4))return -1;}
                    else {
                        if(size!=4+width)return -1;
                        for(unsigned i=0;i<width;++i)next.number[field]=(next.number[field]<<8)|frame[at+4+i];
                        if((field==NP_STATUS&&next.number[field]>4)||
                           ((field==NP_SHUFFLE||field==NP_REPEAT)&&next.number[field]>2))return -1;
                    }
                }
                at+=size;
            }
        }
        pos+=n;
    }
    *out=next;return 0;
}
int mhi2q_now_playing_capture(void *manager);
int mhi2q_now_playing_submit(const uint8_t *frame,size_t length);
#endif
