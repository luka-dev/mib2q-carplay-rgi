#include "../hook/wireless/paired_screen_crypto.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned calls;
static int reject;
static uint64_t expected_nonce;
static int open_fixture(const uint8_t *key,const uint8_t *nonce,const void *aad,size_t aad_length,
                        const void *cipher,size_t length,const uint8_t *tag,void *plain)
{
    const uint8_t *header=aad;
    uint64_t value=0;
    for(unsigned i=0;i<8;++i)value|=(uint64_t)nonce[i]<<(8*i);
    assert(key && aad_length==128 && header[4]==0 && length>0);
    assert(tag==(const uint8_t *)cipher+length && plain==cipher);
    assert(value==expected_nonce);
    ++calls;
    if(reject)return -6754;
    ++expected_nonce;
    return 0;
}
static void header_length(uint8_t *header,uint32_t length)
{for(unsigned i=0;i<4;++i)header[i]=(uint8_t)(length>>(8*i));}
int main(void)
{
    uint8_t key[32]={0},header[128]={0},body[1024]={0};
    uint32_t length=32;header_length(header,length);
    cp_screen_crypto state={0,0};
    assert(!cp_screen_open(&state,key,header,body,&length,open_fixture));
    assert(state.nonce==1 && !state.failed && length==16 && calls==1);
    length=32;reject=1;
    assert(cp_screen_open(&state,key,header,body,&length,open_fixture)==-6754);
    assert(state.nonce==1 && state.failed && length==32);
    unsigned previous=calls;
    reject=0;
    assert(cp_screen_open(&state,key,header,body,&length,open_fixture)==-6754);
    assert(calls==previous); /* Authentication failure never permits retry. */
    state=(cp_screen_crypto){UINT64_MAX,0};length=32;
    assert(cp_screen_open(&state,key,header,body,&length,open_fixture));
    assert(calls==previous && state.failed);
    for(unsigned i=0;i<2;++i){
        length=i?UINT32_MAX:16777217u;header_length(header,length);state=(cp_screen_crypto){0,0};
        assert(cp_screen_open(&state,key,header,body,&length,open_fixture));
        assert(calls==previous && state.failed);
    }
    uint32_t rng=111;
    for(unsigned n=0;n<10000;++n){
        rng=rng*1664525+1013904223;
        length=rng%1024;header_length(header,length);
        header[4]=n%3==0?5:0;
        state=(cp_screen_crypto){0,0};expected_nonce=0;previous=calls;
        int valid=length>16 && header[4]==0;
        assert((cp_screen_open(&state,key,header,body,&length,open_fixture)==0)==valid);
        assert(calls==previous+(unsigned)valid);
        assert(state.failed==!valid && state.nonce==(uint64_t)valid);
    }
    header[4]=0;header_length(header,32);length=31;state=(cp_screen_crypto){0,0};previous=calls;
    assert(cp_screen_open(&state,key,header,body,&length,open_fixture));
    assert(calls==previous && state.failed);
    state=(cp_screen_crypto){0,0};length=32;
    assert(cp_screen_open(&state,NULL,header,body,&length,open_fixture));
    assert(calls==previous && state.failed);
    puts("paired screen framing/AAD, nonce saturation, fail-closed retry and 10000 bounds cases passed");
    return 0;
}
