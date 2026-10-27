/* Included only by the offline capture fixture. Real stock receiver/crypto,
 * fake audio hardware edge. Never part of libcarplay_hook or deployment. */
int AudioStreamPrepare(void *stream) { (void)stream;return 0; }
int AudioStreamStart(void *stream) { (void)stream;return 0; }

int RTPJitterBufferPutBusyNode(void *context,void *node)
{
    typedef int (*fn)(void *,void *);
    fn real=(fn)dlsym(RTLD_NEXT,"RTPJitterBufferPutBusyNode");
    const uint8_t *p=(const uint8_t *)node+20;
    size_t i;int correct=1;
    for(i=0;i<1408;i+=2)if(p[i]!=0 || p[i+1]!=0x20){correct=0;break;}
    if(!correct){
        static const char bad[]="[stock210-media] unexpected PCM admitted\n";
        (void)write(2,bad,sizeof(bad)-1);
    }else{
        static const char good[]="[stock210-media] authenticated PCM reached stock jitter buffer\n";
        (void)write(2,good,sizeof(good)-1);
    }
    return real?real(context,node):-1;
}
static void media_event_done(void *message)
{
    int status;memcpy(&status,(uint8_t *)message+40400,4);
    char line[96];int n=snprintf(line,sizeof(line),"[stock210-media] events completion status=%d\n",status);
    if(n>0)(void)write(2,line,(size_t)n);
}
int AirPlayReceiverSessionStart(void *session,void *info)
{
    typedef int (*start_fn)(void *,void *);
    typedef int (*create_fn)(void **);
    typedef int (*header_fn)(void *,const char *,const char *,const char *);
    typedef int (*body_fn)(void *,const char *,const void *,size_t);
    typedef int (*send_fn)(void *,void *);
    typedef void (*release_fn)(void *);
    typedef int (*audio_fn)(void *,int,uint32_t,uint64_t,const void *,size_t);
    start_fn real=(start_fn)dlsym(RTLD_NEXT,"AirPlayReceiverSessionStart");
    create_fn create=(create_fn)dlsym(RTLD_NEXT,"HTTPMessageCreate");
    header_fn header=(header_fn)dlsym(RTLD_NEXT,"HTTPHeader_InitRequest");
    body_fn body=(body_fn)dlsym(RTLD_NEXT,"HTTPMessageSetBody");
    send_fn send=(send_fn)dlsym(RTLD_NEXT,"HTTPClientSendMessage");
    release_fn release=(release_fn)dlsym(RTLD_NEXT,"CFRelease");
    audio_fn audio=(audio_fn)dlsym(RTLD_NEXT,"AirPlayReceiverSessionWriteAudio");
    void *client;int err=real?real(session,info):-1,i;
    if(err)return err;
    memcpy(&client,(uint8_t *)session+704,4);
    for(i=0;i<2 && create && header && body && send && release;++i){
        void *message=NULL;
        void (*done)(void *)=media_event_done;
        if(create(&message))return -1;
        err=header((uint8_t *)message+16,"POST","/hook-media-test","HTTP/1.1");
        if(!err)err=body(message,"application/octet-stream","event-check",11);
        memcpy((uint8_t *)message+40428,&done,4);
        if(!err)err=send(client,message);
        release(message);
        if(err)return err;
    }
    if(audio){
        uint16_t pcm[720];for(i=0;i<720;++i)pcm[i]=0x1234;
        err=audio(session,100,0,0,pcm,sizeof(pcm));
        char line[96];int n=snprintf(line,sizeof(line),"[stock210-media] stock microphone write status=%d\n",err);
        if(n>0)(void)write(2,line,(size_t)n);
    }
    return err;
}

/* The base IFS fake OMX incorrectly frees caller-owned UseBuffer storage.
 * Stock210 supplies screen buffers here; only AllocateBuffer memory belongs
 * to OMX. Correct this fixture ownership bug without replacing stock teardown. */
#include "../../../toolchain/qnx65-abi/include/OMX/OMX_Component.h"
static OMX_ERRORTYPE (*media_real_use)(OMX_HANDLETYPE,OMX_BUFFERHEADERTYPE **,OMX_U32,OMX_PTR,OMX_U32,OMX_U8 *);
static OMX_ERRORTYPE (*media_real_free)(OMX_HANDLETYPE,OMX_U32,OMX_BUFFERHEADERTYPE *);
static OMX_BUFFERHEADERTYPE *media_borrowed[64];
static pthread_mutex_t media_omx_lock=PTHREAD_MUTEX_INITIALIZER;
static OMX_ERRORTYPE media_omx_use(OMX_HANDLETYPE h,OMX_BUFFERHEADERTYPE **b,OMX_U32 port,OMX_PTR app,OMX_U32 size,OMX_U8 *data)
{
    OMX_ERRORTYPE err=media_real_use(h,b,port,app,size,data);
    if(!err){unsigned i;pthread_mutex_lock(&media_omx_lock);
        for(i=0;i<64;++i)if(!media_borrowed[i]){media_borrowed[i]=*b;break;}
        pthread_mutex_unlock(&media_omx_lock);
        if(i==64)return OMX_ErrorInsufficientResources;
    }
    return err;
}
static OMX_ERRORTYPE media_omx_free(OMX_HANDLETYPE h,OMX_U32 port,OMX_BUFFERHEADERTYPE *b)
{
    unsigned i;pthread_mutex_lock(&media_omx_lock);
    for(i=0;i<64;++i)if(media_borrowed[i]==b){media_borrowed[i]=NULL;b->pBuffer=NULL;break;}
    pthread_mutex_unlock(&media_omx_lock);
    return media_real_free(h,port,b);
}
/* Input-only decoder mock: no decoded NV12 output is fabricated. The base
 * fake emitted FillBufferDone even after announcing OMX_StateLoaded, leaving
 * stale output headers queued in the stock renderer during FreeBuffer. */
static OMX_ERRORTYPE media_omx_no_output(OMX_HANDLETYPE h,OMX_BUFFERHEADERTYPE *b)
{ (void)h;(void)b;return OMX_ErrorNone; }
OMX_ERRORTYPE OMX_GetHandle(OMX_HANDLETYPE *out,OMX_STRING name,OMX_PTR app,OMX_CALLBACKTYPE *cb)
{
    typedef OMX_ERRORTYPE (*fn)(OMX_HANDLETYPE *,OMX_STRING,OMX_PTR,OMX_CALLBACKTYPE *);
    fn real=(fn)dlsym(RTLD_NEXT,"OMX_GetHandle");
    OMX_ERRORTYPE err=real?real(out,name,app,cb):OMX_ErrorComponentNotFound;
    if(!err){OMX_COMPONENTTYPE *c=(OMX_COMPONENTTYPE *)*out;
        media_real_use=c->UseBuffer;media_real_free=c->FreeBuffer;
        c->UseBuffer=media_omx_use;c->FreeBuffer=media_omx_free;
        c->FillThisBuffer=media_omx_no_output;
    }
    return err;
}
