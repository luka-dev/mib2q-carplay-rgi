#include "now_playing.h"
#include <assert.h>
#include <stdio.h>
/* Independent iap2-csm golden vector (tests/vectors.txt). */
static const unsigned char vector[]=
    "\x40\x40\x00\x52\x50\x01\x00\x31\x00\x00\x00\x0c\x00\x00\x00\x00"
    "\x00\x00\xde\xad\xbe\xef\x00\x09\x00\x01\x53\x6f\x6e\x67\x00\x00"
    "\x08\x00\x04\x00\x02\xbf\x20\x00\x0b\x00\x0c\x41\x72\x74\x69\x73"
    "\x74\x00\x00\x05\x00\x1a\x07\x00\x1b\x00\x01\x00\x05\x00\x00\x01"
    "\x00\x08\x00\x01\x00\x00\xa4\x10\x00\x0a\x00\x07\x4d\x75\x73\x69\x63\x00";
int main(void) {
    struct np_update out,untouched;
    memset(&untouched,0xa5,sizeof(untouched));
    assert(np_decode(vector,sizeof(vector)-1,&out)==0);
    assert(!strcmp(out.text[NP_TITLE],"Song")&&!strcmp(out.text[NP_ARTIST],"Artist"));
    assert(!strcmp(out.text[NP_APP],"Music")&&out.number[NP_STATUS]==1);
    assert(out.number[NP_DURATION]==180000&&out.number[NP_ELAPSED]==42000);
    assert(!(out.mask&(1u<<NP_ALBUM)));
    for(size_t i=0;i<sizeof(vector)-1;++i) {
        out=untouched;assert(np_decode(vector,i,&out)==-1);
        assert(!memcmp(&out,&untouched,sizeof(out)));
    }
    char text[NP_TEXT_MAX+1];
    assert(!np_text(text,(const uint8_t *)"QNX Песня",strlen("QNX Песня")));
    assert(!strcmp(text,"QNX Песня"));
    const unsigned char *invalid[]={
        (const unsigned char *)"\xc0\x80",(const unsigned char *)"\xed\xa0\x80",
        (const unsigned char *)"\xf4\x90\x80\x80",(const unsigned char *)"\xe2\x82",
        (const unsigned char *)"a\nb"
    };
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)
        assert(np_text(text,invalid[i],strlen((const char *)invalid[i]))==-1);
    const uint8_t nul[]={ 'a',0,'b' };
    assert(np_text(text,nul,sizeof(nul))==-1);
    /* Header-adjusted malformed nested TLVs cannot be silently truncated. */
    uint8_t bad[NP_FRAME_MAX]={0};memcpy(bad,vector,sizeof(vector)-1);
    bad[7]=0xff;assert(np_decode(bad,sizeof(vector)-1,&out)==-1);
    memcpy(bad,vector,sizeof(vector)-1);bad[11]=3;
    assert(np_decode(bad,sizeof(vector)-1,&out)==-1);
    const uint8_t duplicate[]={0x40,0x40,0,14,0x50,1,0,4,0,0,0,4,0,0};
    assert(np_decode(duplicate,sizeof(duplicate),&out)==-1);
    uint32_t random=0x841842;
    for(unsigned trial=0;trial<20000;++trial) {
        memcpy(bad,vector,sizeof(vector)-1);
        random=random*1664525u+1013904223u;
        size_t at=random%(sizeof(vector)-1);bad[at]^=(uint8_t)(random>>24);
        (void)np_decode(bad,(random&1)?sizeof(vector)-1:(random%sizeof(bad)),&out);
    }
    puts("Now Playing codec: golden vector, UTF-8, partial update, truncation, duplicate, 20000 mutations PASS");
}
