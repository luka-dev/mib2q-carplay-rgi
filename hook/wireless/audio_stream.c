/* stock210 PCM worker boundary. Only authenticated compressed wireless streams
 * use this AU queue. Native AudioStream/QSA, focus and clock callbacks remain
 * in libairplay; ordinary wired/PCM jitter operations fall through unchanged. */
#include "audio_stream.h"
#include "audio_queue.h"
#include "media.h"
#include "stock210_abi.h"
#include <time.h>
#ifndef HOOK_AUDIO
#define HOOK_AUDIO 1
#endif

DEFINE_LOG_MODULE(WAUDIO);
#define AUDIO_STREAMS 16u
struct stream {
    void *jitter;
    cp_audio_queue *queue;
    cp_audio_codec *encoder;
    uint64_t format;
    uint64_t generation;
    cp_audio_geometry geometry;
    unsigned input_frames;
    uint32_t input_timestamp;
    int16_t input_pcm[960];
};
static struct stream streams[AUDIO_STREAMS];
/* Registry only. RX owns queue/decoder, TX owns encoder/PCM accumulation.
 * Init/free/reset take TX -> RX before changing either side. Media direction
 * gates precede these gates; never call media lifecycle from an emit callback. */
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tx_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rx_lock=PTHREAD_MUTEX_INITIALIZER;
static uint64_t stream_generation;
static void stream_mutation_lock(void)
{ pthread_mutex_lock(&tx_lock);pthread_mutex_lock(&rx_lock); }
static void stream_mutation_unlock(void)
{ pthread_mutex_unlock(&rx_lock);pthread_mutex_unlock(&tx_lock); }
static uint64_t now_ns(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec; }
int cp_wireless_audio_enabled(void)
{
    const char *flag=getenv("MHI2Q_CP_WIRELESS_AUDIO");
    return cp_dio_owner_is_wireless() && cp_media_enabled() && flag && !strcmp(flag,"1");
}
int cp_wireless_audio_asbd(uint64_t format,uint32_t *asbd,uint32_t *bits)
{
    cp_audio_geometry g;
    if (!cp_wireless_audio_enabled() || cp_audio_geometry_for(format,&g)) return 0;
    if (!asbd) return 0;
    memset(asbd,0,9*sizeof(*asbd));
    asbd[0]=g.rate; asbd[1]=UINT32_C(0x6c70636d); asbd[2]=12;
    asbd[3]=asbd[5]=g.channels*2; asbd[4]=1; asbd[6]=g.channels; asbd[7]=16;
    if (bits) *bits=16;
    return 1;
}
static struct stream *find(void *jitter)
{ unsigned i; if(!jitter)return NULL;for(i=0;i<AUDIO_STREAMS;++i) if(streams[i].jitter==jitter) return &streams[i]; return NULL; }
struct stream_ticket { struct stream *stream; uint64_t generation; };
static struct stream_ticket stream_capture(void *jitter)
{
    struct stream_ticket t={0};
    pthread_mutex_lock(&lock);
    t.stream=find(jitter);
    if(t.stream)t.generation=t.stream->generation;
    pthread_mutex_unlock(&lock);
    return t;
}
static struct stream *stream_begin(struct stream_ticket t,pthread_mutex_t *direction)
{
    int valid;
    if(!t.stream)return NULL;
    pthread_mutex_lock(direction);
    pthread_mutex_lock(&lock);
    valid=t.stream->jitter && t.stream->generation==t.generation;
    pthread_mutex_unlock(&lock);
    if(!valid){pthread_mutex_unlock(direction);return NULL;}
    return t.stream;
}
/* Caller holds both direction gates and the registry lock. A delayed native
 * free/reset must not operate on a replacement at the same jitter address. */
static int stream_mutation_matches(void *jitter,struct stream_ticket t)
{
    struct stream *current=find(jitter);
    return t.stream ? current==t.stream && current->generation==t.generation : !current;
}
HOOK_EXPORT int RTPJitterBufferInit(void *jitter,int rate,int bpf,int latency)
{
    typedef int (*fn)(void *,int,int,int);
    fn real=(fn)dlsym(RTLD_NEXT,"RTPJitterBufferInit");
    uint64_t format=cp_media_audio_format(jitter);
    cp_audio_geometry geometry;
    struct stream *s=NULL;
    unsigned i;
    int err;
    stream_mutation_lock();
    pthread_mutex_lock(&lock);
    int duplicate=find(jitter)!=NULL;
    pthread_mutex_unlock(&lock);
    if(duplicate){stream_mutation_unlock();return kStateErr;}
    err=real?real(jitter,rate,bpf,latency):kUnsupportedErr;
    if(err || cp_audio_geometry_for(format,&geometry)) {stream_mutation_unlock();return err;}
    pthread_mutex_lock(&lock);
    for(i=0;i<AUDIO_STREAMS;++i) if(!streams[i].jitter) { s=&streams[i]; break; }
    if(!s || stream_generation==UINT64_MAX || rate!=(int)geometry.rate || bpf!=(int)(geometry.channels*2)) err=kStateErr;
    else {
        s->queue=cp_audio_queue_create(format,latency<0?0:(unsigned)latency);
        if(!s->queue) err=kUnsupportedErr;
        else { s->format=format; s->geometry=geometry;s->generation=++stream_generation;s->jitter=jitter; }
    }
    pthread_mutex_unlock(&lock);
    if(err) {
        typedef void (*free_fn)(void *);
        free_fn release=(free_fn)dlsym(RTLD_NEXT,"RTPJitterBufferFree");
        if(release) release(jitter);
    }
    stream_mutation_unlock();
    LOG_INFO(LOG_MODULE,"compressed stream format=%#llx rate=%d bpf=%d status=%d",
             (unsigned long long)format,rate,bpf,err);
    return err;
}
HOOK_EXPORT int RTPJitterBufferPutBusyNode(void *jitter,void *node)
{
    typedef int (*fn)(void *,void *);
    struct stream_ticket ticket=stream_capture(jitter);
    struct stream *s=stream_begin(ticket,&rx_lock);
    if(ticket.stream && !s)return kStateErr;
    /* SocketRecvFrom already queued the authenticated AU before stock's
     * unconditional Swap16Mem. Return this unused native node exactly once. */
    fn real=(fn)dlsym(RTLD_NEXT,s?"RTPJitterBufferPutFreeNode":"RTPJitterBufferPutBusyNode");
    int result=real?real(jitter,node):kUnsupportedErr;
    if(s)pthread_mutex_unlock(&rx_lock);
    return result;
}
HOOK_EXPORT int RTPJitterBufferRead(void *jitter,void *buffer,size_t bytes)
{
    typedef int (*fn)(void *,void *,size_t);
    int result;
    struct stream_ticket ticket=stream_capture(jitter);
    struct stream *s=stream_begin(ticket,&rx_lock);
    if(s) {
        result=bytes%2?kParamErr:cp_audio_queue_read(s->queue,buffer,bytes/2,now_ns());
        pthread_mutex_unlock(&rx_lock); return result;
    }
    if(ticket.stream)return kStateErr; /* Never fall through a stale owned call. */
    fn real=(fn)dlsym(RTLD_NEXT,"RTPJitterBufferRead");
    return real?real(jitter,buffer,bytes):kUnsupportedErr;
}
HOOK_EXPORT void RTPJitterBufferFree(void *jitter)
{
    typedef void (*fn)(void *);
    struct stream_ticket ticket=stream_capture(jitter);
    stream_mutation_lock();
    pthread_mutex_lock(&lock);
    if(!stream_mutation_matches(jitter,ticket)) {
        pthread_mutex_unlock(&lock);stream_mutation_unlock();return;
    }
    struct stream *s=find(jitter);
    pthread_mutex_unlock(&lock);
    if(s) {
        cp_audio_queue_destroy(s->queue); cp_audio_codec_destroy(s->encoder);
    }
    fn real=(fn)dlsym(RTLD_NEXT,"RTPJitterBufferFree");
    if(real) real(jitter);
    pthread_mutex_lock(&lock);
    if(s)memset(s,0,sizeof(*s));
    pthread_mutex_unlock(&lock);
    stream_mutation_unlock();
}
HOOK_EXPORT int RTPJitterBufferReset(void *jitter)
{
    typedef int (*fn)(void *);
    struct stream_ticket ticket=stream_capture(jitter);
    stream_mutation_lock();
    pthread_mutex_lock(&lock);
    if(!stream_mutation_matches(jitter,ticket)) {
        pthread_mutex_unlock(&lock);stream_mutation_unlock();return kStateErr;
    }
    struct stream *s=find(jitter);
    if(s && stream_generation==UINT64_MAX) {
        pthread_mutex_unlock(&lock);stream_mutation_unlock();return kStateErr;
    }
    if(s)s->generation=++stream_generation;
    pthread_mutex_unlock(&lock);
    if(s) {
        cp_audio_queue_reset(s->queue); cp_audio_codec_reset(s->encoder);
        s->input_frames=0;
    }
    fn real=(fn)dlsym(RTLD_NEXT,"RTPJitterBufferReset");
    int result=real?real(jitter):kUnsupportedErr;
    stream_mutation_unlock();return result;
}
int cp_wireless_audio_receive(void *jitter,uint32_t timestamp,const void *au,size_t length)
{
    int err=kStateErr;
    struct stream *s=stream_begin(stream_capture(jitter),&rx_lock);
    if(s) err=cp_audio_queue_put(s->queue,timestamp,au,length,now_ns());
    if(s)pthread_mutex_unlock(&rx_lock);
    return err;
}
int cp_wireless_audio_encode(void *jitter,uint32_t timestamp,const uint8_t *pcm_be,
                             size_t length,cp_audio_emit emit,void *context)
{
    int err=kStateErr;
    size_t i;
    uint8_t au[CP_AUDIO_AU_MAX];
    if(!pcm_be || length%2 || !emit) return kParamErr;
    struct stream *s=stream_begin(stream_capture(jitter),&tx_lock);
    if(!s)return err;
    if(!s->geometry.opus) goto exit;
    if(!s->encoder) s->encoder=cp_audio_codec_create(s->format,1);
    if(!s->encoder) goto exit;
    if(s->input_frames && s->input_timestamp+s->input_frames!=timestamp) {
        s->input_frames=0; cp_audio_codec_reset(s->encoder);
    }
    err=0;
    for(i=0;i<length;i+=2) {
        if(!s->input_frames) s->input_timestamp=timestamp+(uint32_t)(i/2);
        s->input_pcm[s->input_frames++]=(int16_t)(((uint16_t)pcm_be[i]<<8)|pcm_be[i+1]);
        if(s->input_frames==s->geometry.frames) {
            int n=cp_audio_codec_encode(s->encoder,s->input_pcm,s->input_frames,au,sizeof(au));
            s->input_frames=0;
            if(n<=0 || emit(context,au,(size_t)n,s->input_timestamp)) { err=kStateErr; break; }
        }
    }
exit:
    pthread_mutex_unlock(&tx_lock);
    return err;
}
#if !HOOK_AUDIO
HOOK_EXPORT uint32_t AirPlayAudioFormatToASBD(uint64_t fmt,uint32_t *asbd,uint32_t *bits)
{
    typedef uint32_t (*fn)(uint64_t,uint32_t *,uint32_t *);
    if(cp_wireless_audio_asbd(fmt,asbd,bits)) return 0;
    fn real=(fn)dlsym(RTLD_NEXT,"AirPlayAudioFormatToASBD");
    return real?real(fmt,asbd,bits):(uint32_t)kUnsupportedErr;
}
#endif
