/* Real stock210 jitter allocation/PLT fallthrough with the production AU
 * adapter; only the media admission lookup is a fixture. No HMI/audio mock. */
#include "../../../hook/wireless/audio_stream.h"
#include "../../../hook/wireless/audio_queue.h"
#include "../../../hook/wireless/advertisement.h"
#include "wireless_audio_aac_vectors.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <process.h>
#include <pthread.h>
static void timeout_stack(int signal,siginfo_t *info,void *context)
{
    const ucontext_t *state=context;const uint32_t *r=state->uc_mcontext.cpu.gpr;
    (void)signal;(void)info;
    printf("AUDIO_STREAM_FAIL timeout pc=%08x lr=%08x sp=%08x r0=%08x r1=%08x r2=%08x\n",r[15],r[14],r[13],r[0],r[1],r[2]);
    const uint32_t *stack=(const uint32_t *)(uintptr_t)r[13];
    for(unsigned i=0;i<24;++i)printf(" %08x",stack[i]);
    printf("\n");fflush(stdout);_exit(8);
}

int RTPJitterBufferInit(void *,int,int,int);
int RTPJitterBufferRead(void *,void *,size_t);
int RTPJitterBufferReset(void *);
void RTPJitterBufferFree(void *);
int RTPJitterBufferPutBusyNode(void *,void *);
static void *owned;
static void *owned_aac;
static uint64_t format;
static int advertisement_failed;
static int buffered_marker;
int spa_buffered_wireless_offer(void *session){return session==&buffered_marker;}
int cp210_callsite(void *caller,uintptr_t offset) { (void)caller;(void)offset;return 0; }
int carplay_wireless_receiver_ready(void) { return 1; }
int carplay_wireless_copy_pairing_identity(char *id,size_t size,uint8_t key[32])
{ snprintf(id,size,"QNX-AUDIO-UNIT");if(key)memset(key,0,32);return 0; }
void cp_wireless_advertisement_failed(void) { ++advertisement_failed; }
uint64_t cp_media_audio_format(void *j) { return j==owned?format:j==owned_aac?(UINT64_C(1)<<23):0; }
int cp_media_enabled(void) { return 1; }
static unsigned emissions;
static cp_audio_codec *mic_decoder;
static int emit(void *context,const uint8_t *au,size_t length,uint32_t timestamp)
{
    int16_t out[960];unsigned frames=0;
    (void)context;
    assert(timestamp==12345+emissions*960);
    assert(!cp_audio_codec_decode(mic_decoder,au,length,out,960,&frames));
    assert(frames==960);++emissions;return 0;
}
static pthread_mutex_t mic_gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mic_changed=PTHREAD_COND_INITIALIZER;
static int mic_entered,mic_release;
static int held_emit(void *context,const uint8_t *au,size_t length,uint32_t timestamp)
{
    pthread_mutex_lock(&mic_gate);mic_entered=1;pthread_cond_broadcast(&mic_changed);
    while(!mic_release)pthread_cond_wait(&mic_changed,&mic_gate);
    pthread_mutex_unlock(&mic_gate);return emit(context,au,length,timestamp);
}
static void *held_microphone(void *pcm)
{ assert(!cp_wireless_audio_encode(owned,12345+960,pcm,1920,held_emit,NULL));return NULL; }
int main(void)
{
    static char stdout_buffer[4096];setvbuf(stdout,stdout_buffer,_IOLBF,sizeof(stdout_buffer));
    puts("AUDIO_STREAM loading stock210");
    void *library=dlopen("libairplay.so",RTLD_NOW|RTLD_GLOBAL);
    if(!library){printf("AUDIO_STREAM_FAIL dlopen: %s\n",dlerror());return 1;}
    puts("AUDIO_STREAM loading Cinemo");
    assert(dlopen("libNmeSDK.so",RTLD_NOW|RTLD_GLOBAL));
    puts("AUDIO_STREAM probing codecs");
    printf("CINEMO_SETDIR=%p\n",dlsym(RTLD_DEFAULT,"ICinemoConfig_SetPluginDirectory"));
    struct sigaction action;memset(&action,0,sizeof(action));action.sa_sigaction=timeout_stack;action.sa_flags=SA_SIGINFO;
    sigemptyset(&action.sa_mask);sigaction(SIGALRM,&action,NULL);alarm(25);
    uint64_t codecs=cp_audio_codec_formats();
    printf("AUDIO_CODEC_FORMATS=%llx\n",(unsigned long long)codecs);fflush(stdout);
    assert((codecs&CP_AUDIO_AAC)==CP_AUDIO_AAC);
    cp_audio_codec *aac=cp_audio_codec_create(UINT64_C(1)<<23,0);assert(aac);
    unsigned nonzero=0;
    int16_t *aac_reference=calloc(AAC_VECTOR_COUNT*2048,sizeof(int16_t));
    int16_t *aac_queued=calloc(AAC_VECTOR_COUNT*2048,sizeof(int16_t));
    size_t aac_samples=AAC_VECTOR_COUNT*2048;assert(aac_reference&&aac_queued);
    for(unsigned i=0;i<AAC_VECTOR_COUNT;++i) {
        int16_t pcm[2048];unsigned frames=0;
        assert(!cp_audio_codec_decode(aac,aac_vectors[i],aac_lengths[i],pcm,2048,&frames));
        printf("AAC_VECTOR frame=%u decoded=%u\n",i,frames);fflush(stdout);
        assert(frames==0 || frames==1024);
        if(frames) {
            static const char digits[]="0123456789abcdef";char line[1025];
            const uint8_t *bytes=(const uint8_t *)pcm;
            for(unsigned j=0;j<512;++j){line[2*j]=digits[bytes[j]>>4];line[2*j+1]=digits[bytes[j]&15];}
            line[1024]=0;char record[1100];int size=snprintf(record,sizeof(record),"AAC_PCM frame=%u hex=%s\n",i,line);
            assert(size>0 && size<(int)sizeof(record));assert(write(1,record,size)==size);
            memcpy(aac_reference+i*2048,pcm,frames*2*sizeof(*pcm));
        }
        for(unsigned j=0;j<frames*2;++j)nonzero|=(uint16_t)pcm[j];
    }
    assert(nonzero);cp_audio_codec_destroy(aac);
    cp_audio_queue *aq=cp_audio_queue_create(UINT64_C(1)<<23,0);assert(aq);
    for(unsigned i=0;i<AAC_VECTOR_COUNT;++i) {
        unsigned p=i^1;if(p>=AAC_VECTOR_COUNT)p=i;
        assert(!cp_audio_queue_put(aq,1000+p*1024,aac_vectors[p],aac_lengths[p],1000000));
    }
    assert(!cp_audio_queue_read(aq,aac_queued,aac_samples,100000000));
    assert(!memcmp(aac_reference,aac_queued,aac_samples*sizeof(int16_t)));
    puts("AAC_QUEUE_PASS suppressed priming AU, current-AU RTP timestamps, reorder and silence");
    cp_audio_queue_destroy(aq);free(aac_queued);
    int (*get_free)(void *,void **)=dlsym(library,"RTPJitterBufferGetFreeNode");
    assert(get_free);
    uint32_t jitter[512]={0},wired[512]={0},asbd[9],bits=0;
    owned=jitter;format=UINT64_C(1)<<30;
    setenv("MHI2Q_DIO_OWNER_MODE","wireless",1);
    setenv("MHI2Q_CP_WIRELESS_AUDIO","1",1);
    assert(cp_advertisement_available());
    /* Construct /info using the target CFLite, including real retain/release
     * callbacks. Validate the new capability type102 before booting iOS. */
    void *(*dict)(void *,long,const void *,const void *)=dlsym(library,"CFDictionaryCreateMutable");
    void *(*array)(void *,long,const void *)=dlsym(library,"CFArrayCreateMutable");
    void *(*str)(void *,const char *,unsigned)=dlsym(library,"CFStringCreateWithCString");
    void (*setnum)(void *,const void *,int64_t)=dlsym(library,"CFDictionarySetInt64");
    void (*setvalue)(void *,const void *,const void *)=dlsym(library,"CFDictionarySetValue");
    void (*append)(void *,const void *)=dlsym(library,"CFArrayAppendValue");
    void (*release)(const void *)=dlsym(library,"CFRelease");
    long (*count)(const void *)=dlsym(library,"CFArrayGetCount");
    void *(*at)(const void *,long)=dlsym(library,"CFArrayGetValueAtIndex");
    int64_t (*getnum)(void *,const void *,int *)=dlsym(library,"CFDictionaryGetInt64");
    void *kc=dlsym(library,"kCFLDictionaryKeyCallBacksCFLTypes"),*vc=dlsym(library,"kCFLDictionaryValueCallBacksCFLTypes");
    void *ac=dlsym(library,"kCFLArrayCallBacksCFLTypes");
    assert(dict&&array&&str&&setnum&&setvalue&&append&&release&&count&&at&&getnum&&kc&&vc&&ac);
    void *info=dict(0,0,kc,vc),*formats=array(0,0,ac);
    void *typekey=str(0,"type",134217984),*inkey=str(0,"audioInputFormats",134217984),
         *outkey=str(0,"audioOutputFormats",134217984),*formatskey=str(0,"audioFormats",134217984);
    for(unsigned i=100;i<=101;++i) {
        void *entry=dict(0,0,kc,vc);setnum(entry,typekey,i);setnum(entry,inkey,16);setnum(entry,outkey,0x8010);
        append(formats,entry);release(entry);
    }
    setvalue(info,formatskey,formats);cp_advertisement_info(0,info);
    assert(!advertisement_failed && count(formats)==3);
    for(unsigned i=0;i<3;++i) {
        void *entry=at(formats,i);int64_t type=getnum(entry,typekey,0);
        assert(type==(int64_t)(100+i));
        assert((uint64_t)getnum(entry,outkey,0)==(i==2?CP_AUDIO_AAC:CP_AUDIO_OPUS));
    }
    void *(*num)(int64_t)=dlsym(library,"CFNumberCreateInt64");assert(num);
    void *extendedkey=str(0,"audioOutputFormatsExtended",134217984);
    void *spatial=dict(0,0,kc,vc),*extended=array(0,0,ac);
    void *apac=num(65),*aac_format=num(23);assert(spatial&&extended&&apac&&aac_format);
    append(extended,apac);append(extended,aac_format);release(apac);release(aac_format);
    setnum(spatial,typekey,103);setnum(spatial,outkey,0x400000);setvalue(spatial,extendedkey,extended);
    append(formats,spatial);release(spatial);
    cp_advertisement_info(&buffered_marker,info);
    assert(!advertisement_failed&&count(formats)==4);
    void *(*getvalue)(const void *,const void *)=dlsym(library,"CFDictionaryGetValue");assert(getvalue);
    unsigned seen103=0;
    for(unsigned i=0;i<4;++i){void *entry=at(formats,i);
        if(getnum(entry,typekey,0)==103){++seen103;assert(getvalue(entry,extendedkey)==extended && count(extended)==2);}
    }
    assert(seen103==1);release(extended);release(extendedkey);
    cp_advertisement_info(NULL,info);assert(count(formats)==3); /* no borrowed contract */
    release(typekey);release(inkey);release(outkey);release(formatskey);release(formats);release(info);
    assert(cp_wireless_audio_asbd(format,asbd,&bits) && bits==16 && asbd[0]==48000 && asbd[5]==2);
    int init=RTPJitterBufferInit(jitter,48000,2,0);
    printf("AUDIO_STREAM init=%d native=%p\n",init,dlsym(library,"RTPJitterBufferInit"));fflush(stdout);
    assert(!init);
    assert(RTPJitterBufferInit(jitter,48000,2,0)); /* Must preserve the live native pool. */
    cp_audio_codec *encoder=cp_audio_codec_create(format,1);
    cp_audio_codec *decoder=cp_audio_codec_create(format,0);
    mic_decoder=cp_audio_codec_create(format,0);
    assert(encoder && decoder && mic_decoder);
    int16_t input[960],expected[960],out[960];uint8_t au[1416],be[1920];unsigned frames=0;
    for(unsigned i=0;i<960;++i){input[i]=(int16_t)((i%80)*300-12000);be[2*i]=(uint16_t)input[i]>>8;be[2*i+1]=(uint8_t)input[i];}
    int n=cp_audio_codec_encode(encoder,input,960,au,sizeof(au));assert(n>0);
    assert(!cp_audio_codec_decode(decoder,au,n,expected,960,&frames));assert(frames==960);
    assert(!cp_wireless_audio_receive(jitter,9000,au,n));
    void *node=NULL;assert(!get_free(jitter,&node) && node);
    assert(!RTPJitterBufferPutBusyNode(jitter,node));
    usleep(1000);
    assert(!RTPJitterBufferRead(jitter,out,sizeof(out)));
    assert(!memcmp(out,expected,sizeof(out)));
    assert(!cp_wireless_audio_encode(jitter,12345,be,274,emit,NULL));assert(!emissions);
    assert(!cp_wireless_audio_encode(jitter,12345+137,be+274,sizeof(be)-274,emit,NULL));assert(emissions==1);
    /* Real native AAC decode must proceed while Opus microphone emit is held.
     * Create both native resources before the stall: init/free intentionally
     * quiesce both directions, while packet receive/read do not. */
    uint32_t aac_jitter[512]={0};owned_aac=aac_jitter;
    assert(!RTPJitterBufferInit(aac_jitter,48000,4,0));
    pthread_t mic_thread;assert(!pthread_create(&mic_thread,NULL,held_microphone,be));
    pthread_mutex_lock(&mic_gate);
    while(!mic_entered)pthread_cond_wait(&mic_changed,&mic_gate);
    pthread_mutex_unlock(&mic_gate);
    for(unsigned i=0;i<AAC_VECTOR_COUNT;++i)
        assert(!cp_wireless_audio_receive(aac_jitter,1000+i*1024,aac_vectors[i],aac_lengths[i]));
    usleep(2000);
    int16_t *simultaneous=calloc(aac_samples,sizeof(int16_t));assert(simultaneous);
    assert(!RTPJitterBufferRead(aac_jitter,simultaneous,aac_samples*sizeof(int16_t)));
    assert(!memcmp(simultaneous,aac_reference,aac_samples*sizeof(int16_t)));free(simultaneous);
    pthread_mutex_lock(&mic_gate);mic_release=1;pthread_cond_broadcast(&mic_changed);
    pthread_mutex_unlock(&mic_gate);assert(!pthread_join(mic_thread,NULL));assert(emissions==2);
    RTPJitterBufferFree(aac_jitter);owned_aac=NULL;free(aac_reference);
    puts("AAC_RX_OPUS_TX_PASS real native AAC PCM while Opus microphone emit is blocked");
    assert(!RTPJitterBufferReset(jitter));
    assert(!RTPJitterBufferRead(jitter,out,sizeof(out)));
    for(unsigned i=0;i<960;++i)assert(!out[i]);
    RTPJitterBufferFree(jitter);
    assert(cp_wireless_audio_receive(jitter,9000,au,n));
    /* A native init followed by adapter-geometry rejection must release the
     * native allocation under the lifecycle gates, then permit clean reuse. */
    assert(RTPJitterBufferInit(jitter,48000,4,0));
    assert(cp_wireless_audio_receive(jitter,9000,au,n));
    assert(!RTPJitterBufferInit(jitter,48000,2,0));
    assert(!RTPJitterBufferReset(jitter));
    RTPJitterBufferFree(jitter);
    setenv("MHI2Q_DIO_OWNER_MODE","wired",1);
    assert(!cp_wireless_audio_asbd(format,asbd,&bits));
    assert(!RTPJitterBufferInit(wired,48000,4,50));
    assert(!RTPJitterBufferRead(wired,out,sizeof(out)));
    for(unsigned i=0;i<960;++i)assert(!out[i]);
    RTPJitterBufferFree(wired);
    cp_audio_codec_destroy(encoder);cp_audio_codec_destroy(decoder);cp_audio_codec_destroy(mic_decoder);
    alarm(0);
    puts("AUDIO_STREAM_PASS native stock210 init/free, compressed read, Opus mic, reset, wired fallthrough");
    return 0;
}
