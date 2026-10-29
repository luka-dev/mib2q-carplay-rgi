/* Actual media admission/teardown code; CF and key derivation are fixtures.
 * Host libc exports are renamed. ARM32 pointer ABI/crypto remain QEMU checks. */
#define ENABLE_LOGGING 0
#define close test_media_close
#define sendto test_media_sendto
#include "../hook/wireless/media.c"
#undef close
#undef sendto
#include <assert.h>
#include <time.h>

struct object { uint32_t kind; int type; uint64_t id, format; };
static struct object request={1}, array={2}, descriptions[4];
static long stream_count;
static int streams_present=1, encrypted=1, derive_error, derive_calls, derive_fail_call;
static int compressed_enabled;
static int buffered_available,buffered_result,buffered_calls;
int spa_buffered_wireless_validate(void *session,const void *descriptor)
{ assert(session&&descriptor);++buffered_calls;return buffered_available?buffered_result:kUnsupportedErr; }
int cp_wireless_audio_enabled(void) { return compressed_enabled; }
uint64_t cp_audio_codec_formats(void) { return CP_AUDIO_AAC|CP_AUDIO_OPUS; }
int cp_wireless_audio_receive(void *j,uint32_t t,const void *p,size_t n)
{ (void)j;(void)t;(void)p;(void)n;assert(!"not an audio packet fixture");return -1; }
int cp_wireless_audio_encode(void *j,uint32_t t,const uint8_t *p,size_t n,cp_audio_emit emit,void *c)
{ (void)j;(void)t;(void)p;(void)n;(void)emit;(void)c;assert(!"not an audio packet fixture");return -1; }
/* Contract suites do not feed ciphertext; never stand in for real crypto. */
int cp_pair_open(const uint8_t *k,const uint8_t *n,const void *a,size_t al,const void *c,size_t l,const uint8_t *t,void *p)
{(void)k;(void)n;(void)a;(void)al;(void)c;(void)l;(void)t;(void)p;assert(!"unexpected crypto call in admission fixture");return kAuthenticationErr;}
/* Model an audio worker stalled while owning media_lock. A 110 header/body
 * must still get through the production reader; no crypto claim is made here.
 * The fixture transport also verifies registry locks are released before I/O. */
struct screen_contention {
    pthread_mutex_t lock;
    pthread_cond_t done;
    int complete, calls;
    uint8_t header[128], body[32];
};
static int read_unencrypted_screen(void *net,size_t min,size_t max,void *out,
                                    size_t *got,int flags,int timeout)
{
    struct screen_contention *s=net;
    assert(min==max && flags==7 && timeout==9);
    assert(!pthread_mutex_trylock(&screen_readers_lock));
    pthread_mutex_unlock(&screen_readers_lock);
    assert(s->calls<2 && max==(s->calls?sizeof(s->body):sizeof(s->header)));
    memcpy(out,s->calls?s->body:s->header,max);*got=max;++s->calls;
    return 0;
}
static void *read_screen_during_audio(void *net)
{
    struct screen_contention *s=net;
    uint8_t header[128],body[32];size_t got=0;
    assert(!screen_read(net,sizeof(header),sizeof(header),header,&got,7,9));
    assert(got==sizeof(header) && !memcmp(header,s->header,got));
    assert(!screen_read(net,sizeof(body),sizeof(body),body,&got,7,9));
    assert(got==sizeof(body) && !memcmp(body,s->body,got));
    pthread_mutex_lock(&s->lock);s->complete=1;
    pthread_cond_signal(&s->done);pthread_mutex_unlock(&s->lock);
    return NULL;
}
static void check_screen_audio_contention(void)
{
    struct screen_contention s={.lock=PTHREAD_MUTEX_INITIALIZER,.done=PTHREAD_COND_INITIALIZER};
    struct screen_reader r={.net=&s,.real=read_unencrypted_screen};
    pthread_t worker;
    struct timespec deadline;
    int complete;
    cp210_set(s.header,0,sizeof(s.body));s.header[4]=1;
    memset(s.body,0x5a,sizeof(s.body));
    pthread_mutex_lock(&screen_readers_lock);assert(!readers[0]);readers[0]=&r;
    pthread_mutex_unlock(&screen_readers_lock);
    pthread_mutex_lock(&media_lock);
    assert(!pthread_create(&worker,NULL,read_screen_during_audio,&s));
    assert(!clock_gettime(CLOCK_REALTIME,&deadline));deadline.tv_sec+=2;
    pthread_mutex_lock(&s.lock);
    while(!s.complete) {
        int err=pthread_cond_timedwait(&s.done,&s.lock,&deadline);
        if(err==ETIMEDOUT)break;
        assert(!err);
    }
    complete=s.complete;
    pthread_mutex_unlock(&s.lock);
    /* Release and join even on negative control: no stuck test worker. */
    pthread_mutex_unlock(&media_lock);assert(!pthread_join(worker,NULL));
    pthread_mutex_lock(&screen_readers_lock);readers[0]=NULL;
    pthread_mutex_unlock(&screen_readers_lock);
    assert(complete && "wireless 110 waited behind the audio/media lock");
    assert(s.calls==2 && !r.body && !r.nonce);
    uint8_t header[128];size_t got=123;
    assert(screen_read(&s,sizeof(header),sizeof(header),header,&got,7,9)==kStateErr && !got);
    pthread_cond_destroy(&s.done);pthread_mutex_destroy(&s.lock);
    puts("wireless110: header/body independent of held audio/media lock, registry unlocked before I/O PASS");
}
static void once_done(void) {}
static void *str(void *a,const char *s,unsigned enc) { (void)a;(void)enc;return (void *)s; }
static void release(const void *p) { (void)p; }
static uint32_t type(const void *p) { return ((const struct object *)p)->kind; }
static uint32_t dict_type(void) { return 1; }
static uint32_t array_type(void) { return 2; }
static void *get_value(const void *d,const void *k) {
    return d==&request && streams_present && !strcmp(k,"streams") ? &array : NULL;
}
static long count(const void *a) { (void)a;return stream_count; }
static void *at(const void *a,long i) { (void)a;return &descriptions[i]; }
static int64_t integer(void *d,const void *k,int *e) {
    struct object *o=d;
    if(e)*e=0;
    if(!strcmp(k,"type"))return o->type;
    if(!strcmp(k,"streamConnectionID"))return o->id;
    if(!strcmp(k,"audioFormat"))return o->format;
    if(e)*e=-6705;
    return 0;
}
static void set_integer(void *d,const void *k,int64_t n) {
    struct object *o=d;
    if(!strcmp(k,"type"))o->type=(int)n;
    else if(!strcmp(k,"streamConnectionID"))o->id=(uint64_t)n;
    else assert(!"unexpected mutation");
}
int cp_pair_is_encrypted(const cp_pair_connection *p) { (void)p;return encrypted; }
int cp_pair_derive(cp_pair_connection *p,const char *s,const char *i,uint8_t k[32]) {
    (void)p;assert(s && i);memset(k,0,32);
    return ++derive_calls==derive_fail_call?-6754:derive_error;
}
static void reset(void) {
    memset(sessions,0,sizeof(sessions));
    descriptions[0]=(struct object){1,110,1,UINT64_C(1)<<2};
    stream_count=1;streams_present=1;array.kind=2;encrypted=1;derive_error=0;
    derive_calls=derive_fail_call=0;
    buffered_available=buffered_result=buffered_calls=0;
}
void *test_media_request(void) { return &request; }
void test_media_reset(void) { reset(); }
void test_media_encrypted(int value) { encrypted=value; }

int run_media_contract_tests(void) {
    check_screen_audio_contention();
    pthread_once(&cf_once,once_done);
    cf.string=str;cf.release=release;cf.type=type;cf.dict_type=dict_type;
    cf.array_type=array_type;cf.get=get_value;cf.count=count;cf.at=at;cf.integer=integer;cf.set_integer=set_integer;cf.ready=1;
    for(int t=100;t<=111;++t) {
        reset();descriptions[0].type=t;
        int expected=(t==100 || t==101 || t==110)?0:kUnsupportedErr;
        assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==expected);
        assert((sessions[0].owner!=NULL)==(expected==0));
    }
    reset();encrypted=0;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr);
    reset();derive_error=-6754;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==derive_error);
    assert(!sessions[0].owner);
    reset();derive_fail_call=3; // Events succeed; stream-key derivation fails.
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kAuthenticationErr);
    assert(!sessions[0].owner);
    reset();array.kind=1;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kParamErr);
    reset();descriptions[0].id=0;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kParamErr);
    reset();stream_count=STREAMS+2;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kUnsupportedErr);
    /* Buffered APAP is owned by the wired audio module: no realtime ID or
     * KDF key, authenticated control remains mandatory, errors roll back. */
    reset();buffered_available=1;descriptions[0]=(struct object){1,103,0,0};
    assert(!cp_media_prepare((void *)1,(void *)2,&request,(void *)3));
    assert(buffered_calls==1 && sessions[0].used_count==0 && derive_calls==2);
    assert(descriptions[0].type==103 && !descriptions[0].id);
    reset();buffered_available=1;encrypted=0;descriptions[0]=(struct object){1,103,0,0};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr && !buffered_calls);
    reset();buffered_available=1;buffered_result=kParamErr;
    stream_count=2;descriptions[1]=(struct object){1,103,0,0};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kParamErr);
    assert(!sessions[0].owner);
    reset();buffered_available=1;stream_count=2;
    descriptions[0]=descriptions[1]=(struct object){1,103,0,0};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kUnsupportedErr);
    assert(!sessions[0].owner);
    for(int invalid=0;invalid<3;++invalid) {
        reset();descriptions[0].type=100;
        descriptions[0].format=invalid==0?0:invalid==1?3:UINT64_C(1)<<20;
        assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kUnsupportedErr);
    }
    compressed_enabled=1;
    for(unsigned bit=22;bit<=30;++bit) {
        reset();descriptions[0].type=100;descriptions[0].format=UINT64_C(1)<<bit;
        int supported=bit==22 || bit==23 || bit>=28;
        assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==(supported?0:kUnsupportedErr));
    }
    uint8_t native_session[6000]={0};void *main_session=native_session;
    compressed_enabled=0;
    reset();descriptions[0]=(struct object){1,102,77,UINT64_C(1)<<22};
    assert(cp_media_prepare((void *)1,main_session,&request,(void *)3)==kUnsupportedErr);
    assert(descriptions[0].type==102 && !sessions[0].owner);
    compressed_enabled=1;
    reset();descriptions[0]=(struct object){1,102,77,UINT64_C(1)<<22};derive_fail_call=3;
    assert(cp_media_prepare((void *)1,main_session,&request,(void *)3)==kAuthenticationErr);
    assert(descriptions[0].type==102 && !sessions[0].owner);
    reset();descriptions[0]=(struct object){1,102,77,UINT64_C(1)<<22};
    assert(!cp_media_prepare((void *)1,main_session,&request,(void *)3));
    assert(descriptions[0].type==100 && sessions[0].streams[0].type==100);
    assert(sessions[0].streams[0].wire_type==102 && sessions[0].streams[0].id==77);
    hook_setup_ctx_t setup={0};setup.session=main_session;setup.response=&request;
    cp_media_setup_response(&setup);
    assert(descriptions[0].type==102 && descriptions[0].id==77);
    cp_media_teardown_request((void *)99,&request,0);
    assert(descriptions[0].type==102); /* Other sessions never borrow this alias. */
    cp_media_teardown_request(main_session,&request,0);
    assert(descriptions[0].type==100);
    cp_media_teardown(main_session,&request,0);
    assert(!sessions[0].streams[0].active);
    descriptions[0].type=102;
    assert(cp_media_prepare((void *)1,main_session,&request,(void *)3)==kStateErr); /* no nonce reuse */
    for(unsigned bit=28;bit<=30;++bit){
        reset();descriptions[0]=(struct object){1,102,77,UINT64_C(1)<<bit};
        assert(cp_media_prepare((void *)1,main_session,&request,(void *)3)==kUnsupportedErr);
        assert(descriptions[0].type==102 && !sessions[0].owner);
    }
    compressed_enabled=0;
    // A late rejected descriptor must roll back the whole request.
    reset();stream_count=2;descriptions[1]=(struct object){1,111,2,0};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kUnsupportedErr);
    assert(!sessions[0].owner);
    reset();stream_count=2;descriptions[0].type=100;
    descriptions[1]=(struct object){1,101,1,UINT64_C(1)<<2};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr);
    assert(!sessions[0].owner); // Duplicate IDs across distinct stream types.
    reset();stream_count=3;
    descriptions[0]=(struct object){1,100,1,UINT64_C(1)<<2};
    descriptions[1]=(struct object){1,101,2,UINT64_C(1)<<2};
    descriptions[2]=(struct object){1,110,3,0};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==0);
    assert(sessions[0].used_count==3);
    stream_count=1;
    cp_media_teardown((void *)2,&request,0);
    assert(!sessions[0].streams[0].active && sessions[0].streams[1].active);
    // Teardown must not allow streamConnectionID/nonces to be reused.
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr);
    descriptions[0].id=4;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==0);
    cp_media_teardown((void *)2,NULL,0);
    assert(sessions[0].retired);
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr);
    cp_media_forget((void *)1);
    assert(!sessions[0].owner);
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==0);
    reset();streams_present=0;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==0);
    assert(sessions[0].owner && !sessions[0].used_count);
    reset();
    for(uint64_t id=1;id<=HISTORY;++id) {
        descriptions[0].id=id;
        assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==0);
        cp_media_teardown((void *)2,&request,0);
    }
    descriptions[0].id=HISTORY+1;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr);
    reset();
    for(uintptr_t i=1;i<=SESSIONS;++i)
        assert(cp_media_prepare((void *)i,(void *)(i+SESSIONS),&request,(void *)30)==0);
    assert(cp_media_prepare((void *)31,(void *)32,&request,(void *)33)==kStateErr);
#ifdef MHI2Q_WIRELESS_ALT
    setenv("MHI2Q_DIO_OWNER_MODE","wireless",1);
    setenv("MHI2Q_CP_WIRELESS_MEDIA","1",1);
    setenv("MHI2Q_CP_WIRELESS_ALT","1",1);
    assert(cp_media_alt_available());
    reset();descriptions[0].type=111;
    assert(!cp_media_prepare((void *)1,(void *)2,&request,(void *)3));
    uint8_t claimed[32];memset(claimed,0xa5,sizeof(claimed));
    assert(cp_media_alt_claim((void *)2,2,claimed)==kStateErr);
    for(unsigned i=0;i<32;++i)assert(!claimed[i]);
    memset(sessions[0].streams[3].read_key,0x5a,32);
    assert(!cp_media_alt_claim((void *)2,1,claimed));
    for(unsigned i=0;i<32;++i)assert(claimed[i]==0x5a);
    assert(cp_media_alt_claim((void *)2,1,claimed)==kStateErr);
    for(unsigned i=0;i<32;++i)assert(!claimed[i]);
    cp_media_teardown((void *)2,&request,0);
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr);
    descriptions[0].id=2;
    assert(!cp_media_prepare((void *)1,(void *)2,&request,(void *)3));
    cp_media_teardown((void *)2,NULL,0);
    assert(cp_media_alt_claim((void *)2,2,claimed)==kStateErr);
    reset();descriptions[0].type=111;encrypted=0;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kStateErr);
    reset();descriptions[0].type=111;derive_fail_call=3;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kAuthenticationErr);
    assert(!sessions[0].owner);
    setenv("MHI2Q_DIO_OWNER_MODE","wired",1);
    assert(!cp_media_alt_available());
    reset();descriptions[0].type=111;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==kUnsupportedErr);
    unsetenv("MHI2Q_CP_WIRELESS_ALT");unsetenv("MHI2Q_DIO_OWNER_MODE");
    unsetenv("MHI2Q_CP_WIRELESS_MEDIA");
    puts("paired111: authenticated admission, one key claim, rollback and nonce reuse rejection PASS");
#endif
    puts("wireless media contract: types, authentication, formats, rollback, nonce reuse and teardown PASS");
    return 0;
}
