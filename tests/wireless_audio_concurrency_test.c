/* Production media + codec adapters, real Opus. Native jitter/socket calls are
 * controlled fixtures. QNX additionally links real private SDK packet AEAD. */
#define ENABLE_LOGGING 0
#include "../hook/wireless/media.h"
#include <assert.h>
#include <time.h>
#include <unistd.h>
static void *test_lookup(void *,const char *);
static int test_audio_mutex_lock(pthread_mutex_t *);
#define dlsym test_lookup
#define pthread_mutex_lock test_audio_mutex_lock
#define sendto media_sendto
#define close media_close
#include "../hook/wireless/media.c"
#undef close
#undef sendto
#include "../hook/wireless/audio_stream.c"
#undef dlsym
#undef pthread_mutex_lock

static struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    int hold_send,send_entered,release_send;
    int hold_recv,recv_entered,release_recv,rx_done,life_entered,life_done;
    unsigned native_inits,native_frees,native_resets,send_calls,open_calls;
    int fail_send;
    pthread_mutex_t *pause_gate;
    pthread_t pause_thread;
    int armed,paused,resume;
    uint8_t incoming[1452],write_key[32];size_t incoming_len;
} test={.lock=PTHREAD_MUTEX_INITIALIZER,.changed=PTHREAD_COND_INITIALIZER};
static uint8_t native_session[6000],native_pcm[960*2];
static uint8_t encoded[CP_AUDIO_AU_MAX];static unsigned encoded_len;
static int16_t expected[960];
static void *jitter(void) { return native_session+S_MAIN+32; }
static int test_audio_mutex_lock(pthread_mutex_t *mutex)
{
    pthread_mutex_lock(&test.lock);
    if(test.armed && mutex==test.pause_gate && pthread_equal(pthread_self(),test.pause_thread)) {
        test.armed=0;test.paused=1;pthread_cond_broadcast(&test.changed);
        while(!test.resume)pthread_cond_wait(&test.changed,&test.lock);
    }
    pthread_mutex_unlock(&test.lock);
    return pthread_mutex_lock(mutex);
}
static void signal_flag(int *flag)
{ pthread_mutex_lock(&test.lock);*flag=1;pthread_cond_broadcast(&test.changed);pthread_mutex_unlock(&test.lock); }
static void wait_flag(int *flag)
{
    struct timespec deadline;assert(!clock_gettime(CLOCK_REALTIME,&deadline));deadline.tv_sec+=3;
    pthread_mutex_lock(&test.lock);
    while(!*flag)assert(!pthread_cond_timedwait(&test.changed,&test.lock,&deadline));
    pthread_mutex_unlock(&test.lock);
}
#ifndef WIRELESS_TEST_REAL_CRYPTO
/* Identity/tag canary only. Real cryptographic authentication is the QNX test. */
void cp_pair_seal(const uint8_t *key,const uint8_t *nonce,const void *aad,size_t al,
                  const void *plain,size_t n,void *cipher,uint8_t tag[16])
{ (void)nonce;(void)aad;(void)al;memcpy(cipher,plain,n);memcpy(tag,key,16); }
int cp_pair_open(const uint8_t *key,const uint8_t *nonce,const void *aad,size_t al,
                 const void *cipher,size_t n,const uint8_t tag[16],void *plain)
{
    (void)nonce;(void)aad;(void)al;
    if(memcmp(key,tag,16)){memset(plain,0,n);return kAuthenticationErr;}
    memmove(plain,cipher,n);return 0;
}
#endif
static int native_init(void *j,int rate,int bpf,int latency)
{ assert(j==jitter() && rate==48000 && bpf==2 && !latency);++test.native_inits;return 0; }
static void native_free(void *j)
{ assert(j==jitter());pthread_mutex_lock(&test.lock);++test.native_frees;pthread_mutex_unlock(&test.lock); }
static int native_reset(void *j)
{ assert(j==jitter());++test.native_resets;return 0; }
static int native_read(void *j,void *out,size_t n)
{ (void)j;memset(out,0,n);return 0; }
static ssize_t native_send(int fd,const void *packet,size_t n,int flags,
                           const struct sockaddr *to,socklen_t to_len)
{
    const uint8_t *bytes=packet;uint8_t plain[1416];
    (void)to;(void)to_len;assert(fd==77 && !flags && n>36 && n<=1452);
    pthread_mutex_lock(&test.lock);
    test.send_entered=1;pthread_cond_broadcast(&test.changed);
    while(test.hold_send && !test.release_send)pthread_cond_wait(&test.changed,&test.lock);
    unsigned sequence=test.send_calls++;
    int fail=test.fail_send;
    pthread_mutex_unlock(&test.lock);
    assert(((bytes[2]<<8)|bytes[3])==(int)(400+sequence));
    uint8_t nonce[8];nonce_bytes(sequence,nonce);assert(!memcmp(bytes+n-8,nonce,8));
    assert(!cp_pair_open(test.write_key,nonce,bytes+4,8,bytes+12,n-36,bytes+n-24,plain));
    return fail?(ssize_t)n-1:(ssize_t)n;
}
static int native_receive(int fd,void *out,size_t cap,size_t *n,void *from,
                          size_t from_max,size_t *from_len,uint64_t *ticks,uint32_t *ifindex,char *ifname)
{
    (void)from;(void)from_max;(void)from_len;(void)ticks;(void)ifindex;(void)ifname;
    assert(fd==77);
    pthread_mutex_lock(&test.lock);test.recv_entered=1;pthread_cond_broadcast(&test.changed);
    while(test.hold_recv && !test.release_recv)pthread_cond_wait(&test.changed,&test.lock);
    assert(cap>=test.incoming_len);*n=test.incoming_len;memcpy(out,test.incoming,*n);
    pthread_mutex_unlock(&test.lock);return 0;
}
static void *test_lookup(void *scope,const char *name)
{
    (void)scope;
    if(!strcmp(name,"RTPJitterBufferInit"))return (void *)native_init;
    if(!strcmp(name,"RTPJitterBufferFree"))return (void *)native_free;
    if(!strcmp(name,"RTPJitterBufferReset"))return (void *)native_reset;
    if(!strcmp(name,"RTPJitterBufferRead"))return (void *)native_read;
    if(!strcmp(name,"SocketRecvFrom"))return (void *)native_receive;
    if(!strcmp(name,"sendto"))return (void *)native_send;
    return NULL;
}
static void setup(unsigned identity)
{
    memset(native_session,0,sizeof(native_session));
    media_mutation_lock();
    assert(!sessions[0].owner);
    sessions[0].owner=(void *)1;sessions[0].session=native_session;
    sessions[0].generation=++media_generation;
    struct media_stream *s=&sessions[0].streams[0];
    s->active=1;s->type=s->wire_type=100;s->generation=++media_generation;
    s->audio_format=UINT64_C(1)<<30;s->id=identity;
    for(unsigned i=0;i<32;++i){s->read_key[i]=(uint8_t)(i+identity);s->write_key[i]=(uint8_t)(3*i+identity);}
    memcpy(test.write_key,s->write_key,32);
    cp210_set(native_session+S_MAIN,16,77);cp210_set(native_session+S_MAIN,1600,2);
    media_mutation_unlock();
    assert(!RTPJitterBufferInit(jitter(),48000,2,0));
    pthread_mutex_lock(&test.lock);
    test.hold_send=test.send_entered=test.release_send=0;
    test.hold_recv=test.recv_entered=test.release_recv=0;
    test.rx_done=test.life_entered=test.life_done=test.send_calls=test.fail_send=0;
    pthread_mutex_unlock(&test.lock);
}
static void make_receive(void)
{
    uint8_t *p=test.incoming,nonce[8]={0};memset(p,0,12);p[0]=0x80;p[4]=0;p[5]=0;p[6]=0x23;p[7]=0x28;
    cp_pair_seal(sessions[0].streams[0].read_key,nonce,p+4,8,encoded,encoded_len,p+12,p+12+encoded_len);
    memcpy(p+12+encoded_len+16,nonce,8);test.incoming_len=encoded_len+36;
}
static int receive_packet(void)
{
    uint8_t packet[1452];size_t n=123;
    int err=SocketRecvFrom(77,packet,sizeof(packet),&n,NULL,0,NULL,NULL,NULL,NULL);
    if(err)assert(!n);
    return err;
}
static void *receiver(void *unused)
{
    (void)unused;assert(!receive_packet());usleep(2000);
    int16_t pcm[960];assert(!RTPJitterBufferRead(jitter(),pcm,sizeof(pcm)));
    assert(!memcmp(pcm,expected,sizeof(pcm)));signal_flag(&test.rx_done);return NULL;
}
static void send_mic(uint32_t stamp)
{
    uint8_t packet[12+960];memset(packet,0,12);packet[0]=0x80;packet[2]=1;packet[3]=0x90;
    for(unsigned part=0;part<2;++part) {
        uint32_t ts=stamp+part*480;
        packet[4]=(uint8_t)(ts>>24);packet[5]=(uint8_t)(ts>>16);packet[6]=(uint8_t)(ts>>8);packet[7]=(uint8_t)ts;
        memcpy(packet+12,native_pcm+part*960,960);
        ssize_t result=media_sendto(77,packet,sizeof(packet),0,NULL,0);
        assert(result==(part && test.fail_send?-1:(ssize_t)sizeof(packet)));
    }
}
static void *sender(void *unused) { (void)unused;send_mic(12345);return NULL; }
static void *lifecycle(void *mode)
{
    signal_flag(&test.life_entered);
    switch((uintptr_t)mode) {
        case 0:assert(!RTPJitterBufferReset(jitter()));break;
        case 1:RTPJitterBufferFree(jitter());break;
        default:cp_media_forget((void *)1);break;
    }
    signal_flag(&test.life_done);return NULL;
}
static void blocked_transmit(unsigned mode)
{
    setup(10+mode);make_receive();test.hold_send=1;
    struct stream_ticket old=stream_capture(jitter());
    pthread_t tx,rx,life;
    assert(!pthread_create(&tx,NULL,sender,NULL));wait_flag(&test.send_entered);
    assert(!pthread_create(&life,NULL,lifecycle,(void *)(uintptr_t)mode));wait_flag(&test.life_entered);
    /* Even a waiting free/reset/forget must not acquire RX before stalled TX. */
    assert(!pthread_create(&rx,NULL,receiver,NULL));wait_flag(&test.rx_done);
    pthread_mutex_lock(&test.lock);assert(!test.life_done);pthread_mutex_unlock(&test.lock);
    assert(!pthread_mutex_trylock(&media_lock));pthread_mutex_unlock(&media_lock);
    assert(!pthread_mutex_trylock(&lock));pthread_mutex_unlock(&lock);
    signal_flag(&test.release_send);
    assert(!pthread_join(tx,NULL));assert(!pthread_join(rx,NULL));assert(!pthread_join(life,NULL));
    if(mode!=1)RTPJitterBufferFree(jitter());
    cp_media_forget((void *)1);
    setup(30+mode);assert(!stream_begin(old,&rx_lock));assert(!stream_begin(old,&tx_lock));
    RTPJitterBufferFree(jitter());cp_media_forget((void *)1);
}
static int delayed_receive_result;
static void *delayed_receive(void *unused)
{ (void)unused;delayed_receive_result=receive_packet();return NULL; }
static void receive_across_reuse(void)
{
    setup(55);make_receive();test.hold_recv=1;
    struct audio_ticket old=audio_capture(77);
    struct stream_ticket old_codec=stream_capture(jitter());
    pthread_t worker;assert(!pthread_create(&worker,NULL,delayed_receive,NULL));wait_flag(&test.recv_entered);
    RTPJitterBufferFree(jitter());cp_media_forget((void *)1);
    /* Same fd, session address and jitter address, but new keys/generations. */
    setup(66);
    assert(!audio_begin(&old,&media_tx_lock));assert(!audio_begin(&old,&media_rx_lock));
    assert(!stream_begin(old_codec,&tx_lock));assert(!stream_begin(old_codec,&rx_lock));
    signal_flag(&test.release_recv);assert(!pthread_join(worker,NULL));
    assert(delayed_receive_result==kStateErr && !sessions[0].streams[0].send_nonce);
    make_receive();assert(!receive_packet());
    RTPJitterBufferFree(jitter());cp_media_forget((void *)1);
}
/* Minimal CF request model for selective teardown only; not a CF ABI claim. */
static int teardown_request=1,teardown_array=2,teardown_descriptor=1;
static void *cf_string(void *a,const char *s,unsigned enc) { (void)a;(void)enc;return (void *)s; }
static void cf_release(const void *p) { (void)p; }
static uint32_t cf_type(const void *p) { return *(const int *)p; }
static uint32_t cf_dict_type(void) { return 1; }
static uint32_t cf_array_type(void) { return 2; }
static void *cf_get(const void *p,const void *k)
{ return p==&teardown_request && !strcmp(k,"streams")?&teardown_array:NULL; }
static long cf_count(const void *p) { (void)p;return 1; }
static void *cf_at(const void *p,long i) { (void)p;assert(!i);return &teardown_descriptor; }
static int64_t cf_integer(void *p,const void *k,int *err)
{ (void)p;assert(!strcmp(k,"type"));if(err)*err=0;return 100; }
static int old_reset_result;
static void *late_cleanup(void *argument)
{
    unsigned mode=(unsigned)(uintptr_t)argument;
    pthread_mutex_lock(&test.lock);
    test.pause_gate=mode<2?&tx_lock:&media_tx_lock;
    test.pause_thread=pthread_self();test.armed=1;
    pthread_mutex_unlock(&test.lock);
    switch(mode) {
        case 0:RTPJitterBufferFree(jitter());break;
        case 1:old_reset_result=RTPJitterBufferReset(jitter());break;
        case 2:cp_media_forget((void *)1);break;
        case 3:cp_media_teardown(native_session,NULL,0);break;
        default:cp_media_teardown(native_session,&teardown_request,0);break;
    }
    return NULL;
}
static void cleanup_across_reuse(unsigned mode)
{
    setup(100+mode);test.paused=test.resume=0;
    pthread_t worker;assert(!pthread_create(&worker,NULL,late_cleanup,(void *)(uintptr_t)mode));
    wait_flag(&test.paused);
    RTPJitterBufferFree(jitter());
    if(mode<4) {
        cp_media_forget((void *)1);setup(200+mode);
    } else {
        /* Replace only the stream inside the SAME control session. */
        media_mutation_lock();sessions[0].streams[0].generation=++media_generation;
        media_mutation_unlock();assert(!RTPJitterBufferInit(jitter(),48000,2,0));
    }
    unsigned frees=test.native_frees,resets=test.native_resets;
    uint64_t codec=stream_capture(jitter()).generation,media=sessions[0].streams[0].generation;
    signal_flag(&test.resume);assert(!pthread_join(worker,NULL));
    assert(test.native_frees==frees && test.native_resets==resets);
    assert(sessions[0].owner && !sessions[0].retired && sessions[0].streams[0].active);
    assert(sessions[0].streams[0].generation==media && stream_capture(jitter()).generation==codec);
    if(mode==1)assert(old_reset_result==kStateErr);
    make_receive();receiver(NULL);
    RTPJitterBufferFree(jitter());cp_media_forget((void *)1);
}
int main(void)
{
    alarm(20);
    cp_audio_codec *encoder=cp_audio_codec_create(UINT64_C(1)<<30,1);
    cp_audio_codec *decoder=cp_audio_codec_create(UINT64_C(1)<<30,0);assert(encoder&&decoder);
    int16_t pcm[960];unsigned frames=0;
    for(unsigned i=0;i<960;++i){pcm[i]=(int16_t)((i%80)*300-12000);native_pcm[2*i]=(uint16_t)pcm[i]>>8;native_pcm[2*i+1]=(uint8_t)pcm[i];}
    int n=cp_audio_codec_encode(encoder,pcm,960,encoded,sizeof(encoded));assert(n>0);encoded_len=(unsigned)n;
    assert(!cp_audio_codec_decode(decoder,encoded,encoded_len,expected,960,&frames)&&frames==960);
    cf.string=cf_string;cf.release=cf_release;cf.type=cf_type;cf.dict_type=cf_dict_type;
    cf.array_type=cf_array_type;cf.get=cf_get;cf.count=cf_count;cf.at=cf_at;cf.integer=cf_integer;
    for(unsigned mode=0;mode<3;++mode)blocked_transmit(mode);
    receive_across_reuse();
    for(unsigned mode=0;mode<5;++mode)cleanup_across_reuse(mode);
    setup(77);
    unsigned inits=test.native_inits,frees=test.native_frees;
    assert(RTPJitterBufferInit(jitter(),48000,2,0)==kStateErr);
    assert(test.native_inits==inits && test.native_frees==frees);
    struct stream_ticket before_reset=stream_capture(jitter());assert(!RTPJitterBufferReset(jitter()));
    assert(!stream_begin(before_reset,&rx_lock));assert(!stream_begin(before_reset,&tx_lock));
    uint64_t generation=stream_generation;stream_generation=UINT64_MAX;
    assert(RTPJitterBufferReset(jitter())==kStateErr);stream_generation=generation;
    test.fail_send=1;send_mic(12345);assert(sessions[0].streams[0].send_nonce==1);
    test.fail_send=0;send_mic(12345+960);assert(sessions[0].streams[0].send_nonce==2);
    RTPJitterBufferFree(jitter());cp_media_forget((void *)1);
    for(unsigned i=0;i<AUDIO_STREAMS;++i)assert(!streams[i].jitter);
    cp_audio_codec_destroy(encoder);cp_audio_codec_destroy(decoder);
    alarm(0);
    puts("WIRELESS_AUDIO_CONCURRENCY_PASS RX during blocked mic TX/free/reset/forget; stale fd/slot/jitter rejected; nonce never reused after send failure");
    return 0;
}
