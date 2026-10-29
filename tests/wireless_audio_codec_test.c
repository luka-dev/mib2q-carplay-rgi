#include "../hook/wireless/audio_queue.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PACKETS 12u
/* Exercise every storage slot, then reuse it without resetting decoder history.
 * Comparing with an independent sequential decoder detects payload aliasing,
 * ordering mistakes and stale slots across wrap/rebuffer/reset. */
static void exercise_full_queue(unsigned bit,uint32_t base)
{
    enum { FULL=64 };
    uint64_t format=UINT64_C(1)<<bit;
    cp_audio_geometry g;
    assert(!cp_audio_geometry_for(format,&g));
    cp_audio_codec *encoder=cp_audio_codec_create(format,1);
    cp_audio_codec *decoder=cp_audio_codec_create(format,0);
    cp_audio_queue *q=cp_audio_queue_create(format,0);
    assert(encoder && decoder && q);
    uint8_t packets[FULL][CP_AUDIO_AU_MAX];
    unsigned lengths[FULL],order[FULL],frames;
    int16_t input[960],reference[FULL*960],output[FULL*960];
    uint32_t random=UINT32_C(0xa492ef37);
    for(unsigned round=0;round<5;++round) {
        if(round==3) {
            /* Reset a nonempty queue after its permutation has changed. */
            assert(!cp_audio_queue_put(q,base,packets[0],lengths[0],1000));
            cp_audio_queue_reset(q);
            cp_audio_codec_reset(decoder);
        }
        for(unsigned i=0;i<FULL;++i) {
            for(unsigned j=0;j<g.frames;++j)
                input[j]=(int16_t)(9000*sin((round*FULL*g.frames+i*g.frames+j)*0.043));
            int n=cp_audio_codec_encode(encoder,input,g.frames,packets[i],CP_AUDIO_AU_MAX);
            assert(n>0);lengths[i]=(unsigned)n;order[i]=i;
            assert(!cp_audio_codec_decode(decoder,packets[i],n,reference+i*g.frames,g.frames,&frames));
            assert(frames==g.frames);
        }
        for(unsigned i=FULL-1;i;--i) {
            random^=random<<13;random^=random>>17;random^=random<<5;
            unsigned j=random%(i+1),swap=order[i];order[i]=order[j];order[j]=swap;
        }
        for(unsigned i=0;i<FULL;++i) {
            unsigned p=order[i];
            assert(!cp_audio_queue_put(q,base+p*g.frames,packets[p],lengths[p],1000));
            /* Duplicates must not consume a free slot, including when full. */
            assert(!cp_audio_queue_put(q,base+p*g.frames,packets[p],lengths[p],1000));
        }
        unsigned done=0,total=FULL*g.frames;
        while(done<total) {
            unsigned n=137;if(n>total-done)n=total-done;
            assert(!cp_audio_queue_read(q,output+done,n,2000));done+=n;
        }
        assert(!memcmp(output,reference,total*sizeof(*output)));
        assert(!cp_audio_queue_read(q,output,1,2000));assert(output[0]==0);
        base+=FULL*g.frames;
    }
    cp_audio_queue_destroy(q);cp_audio_codec_destroy(encoder);cp_audio_codec_destroy(decoder);
}
static void exercise(unsigned bit,uint32_t base)
{
    cp_audio_geometry g;
    uint64_t format=UINT64_C(1)<<bit;
    int16_t input[960],reference[PACKETS*960],output[PACKETS*960];
    uint8_t packets[PACKETS][CP_AUDIO_AU_MAX];
    unsigned lengths[PACKETS],frames;
    cp_audio_codec *encoder=cp_audio_codec_create(format,1);
    cp_audio_codec *decoder=cp_audio_codec_create(format,0);
    assert(!cp_audio_geometry_for(format,&g));
    assert(encoder && decoder);
    for(unsigned i=0;i<PACKETS;++i) {
        for(unsigned j=0;j<g.frames;++j)
            input[j]=(int16_t)(12000*sin(2*3.141592653589793*440*(i*g.frames+j)/g.rate));
        int n=cp_audio_codec_encode(encoder,input,g.frames,packets[i],CP_AUDIO_AU_MAX);
        assert(n>0); lengths[i]=(unsigned)n;
        assert(!cp_audio_codec_decode(decoder,packets[i],n,reference+i*g.frames,g.frames,&frames));
        assert(frames==g.frames);
    }
    /* Compressed data must not require PCM frame-byte alignment. */
    unsigned odd=0;for(unsigned i=0;i<PACKETS;++i)odd|=lengths[i]&1;
    assert(odd);
    cp_audio_queue *q=cp_audio_queue_create(format,60);
    assert(q);
    const unsigned order[PACKETS]={2,0,1,5,4,3,6,8,7,11,10,9};
    for(unsigned i=0;i<PACKETS;++i) {
        unsigned p=order[i];
        assert(!cp_audio_queue_put(q,base+p*g.frames,packets[p],lengths[p],1000000));
        assert(!cp_audio_queue_put(q,base+p*g.frames,packets[p],lengths[p],1000000));
    }
    memset(output,0x55,sizeof(output));
    assert(!cp_audio_queue_read(q,output,g.frames,2000000));
    for(unsigned i=0;i<g.frames;++i)assert(!output[i]);
    /* Deliberately split output through AU boundaries, like QSA callbacks. */
    unsigned done=0,total=PACKETS*g.frames;
    while(done<total) {
        unsigned n=137;if(n>total-done)n=total-done;
        assert(!cp_audio_queue_read(q,output+done,n,100000000));done+=n;
    }
    assert(!memcmp(output,reference,total*sizeof(*output)));
    /* Retransmitted/late AUs cannot alter predictive decoder history. */
    assert(!cp_audio_queue_put(q,base,packets[0],lengths[0],110000000));
    cp_audio_queue_reset(q);
    assert(!cp_audio_queue_put(q,base,packets[0],lengths[0],200000000));
    assert(!cp_audio_queue_read(q,output,g.frames,300000000));
    cp_audio_codec_reset(decoder);
    assert(!cp_audio_codec_decode(decoder,packets[0],lengths[0],input,g.frames,&frames));
    assert(!memcmp(input,output,g.frames*sizeof(*input)));
    assert(cp_audio_queue_put(q,base,packets[0],CP_AUDIO_AU_MAX+1,300000000));
    /* Force bounded overflow and a forward timestamp discontinuity. */
    for(unsigned i=1;i<200;++i)
        assert(!cp_audio_queue_put(q,base+i*g.frames,packets[i%PACKETS],lengths[i%PACKETS],300000000));
    assert(!cp_audio_queue_read(q,output,g.frames,400000000));
    cp_audio_queue_destroy(q);cp_audio_codec_destroy(encoder);cp_audio_codec_destroy(decoder);
}
int main(void)
{
    cp_audio_geometry g;
    assert(cp_audio_geometry_for(0,&g));assert(cp_audio_geometry_for(3,&g));
    assert(!cp_audio_geometry_for(UINT64_C(1)<<23,&g) && g.channels==2 && g.frames==1024);
    assert(!cp_audio_codec_create(UINT64_C(1)<<23,1));
    for(unsigned bit=28;bit<=30;++bit) {
        exercise(bit,1000);exercise(bit,UINT32_MAX-111);
        exercise_full_queue(bit,UINT32_MAX-12345);
    }
    cp_audio_queue *q=cp_audio_queue_create(UINT64_C(1)<<30,20);assert(q);
    uint32_t random=UINT32_C(0x1268abcd);uint8_t packet[1416];int16_t out[2048];
    for(unsigned i=0;i<5000;++i) {
        random^=random<<13;random^=random>>17;random^=random<<5;
        size_t length=random%sizeof(packet)+1;
        for(size_t j=0;j<length;++j)packet[j]=(uint8_t)(random+j*73);
        assert(!cp_audio_queue_put(q,(uint32_t)(i*960+(random%5)*960),packet,length,(uint64_t)i*20000000));
        assert(!cp_audio_queue_read(q,out,random%2049,(uint64_t)i*20000000+100000000));
        if(i%97==0)cp_audio_queue_reset(q);
    }
    assert(cp_audio_queue_read(q,out,SIZE_MAX,0));
    cp_audio_queue_destroy(q);
    puts("wireless audio: real Opus encode/decode, reorder, duplicate, wrap, split reads, reset, bounds PASS");
    return 0;
}
