/* Actual media admission/teardown code; CF and key derivation are fixtures.
 * Host libc exports are renamed. ARM32 pointer ABI/crypto remain QEMU checks. */
#define ENABLE_LOGGING 0
#define close test_media_close
#define sendto test_media_sendto
#include "../hook/wireless/media.c"
#undef close
#undef sendto
#include <assert.h>

struct object { uint32_t kind; int type; uint64_t id, format; };
static struct object request={1}, array={2}, descriptions[4];
static long stream_count;
static int streams_present=1, encrypted=1, derive_error, derive_calls, derive_fail_call;
#ifdef MHI2Q_WIRELESS_ALT_QEMU
/* Contract suites do not feed ciphertext; never stand in for real crypto. */
int cp_pair_open(const uint8_t *k,const uint8_t *n,const void *a,size_t al,const void *c,size_t l,const uint8_t *t,void *p)
{(void)k;(void)n;(void)a;(void)al;(void)c;(void)l;(void)t;(void)p;assert(!"unexpected crypto call in admission fixture");return ERR_AUTH;}
#endif
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
}
void *test_media_request(void) { return &request; }
void test_media_reset(void) { reset(); }
void test_media_encrypted(int value) { encrypted=value; }

int run_media_contract_tests(void) {
    pthread_once(&cf_once,once_done);
    cf.string=str;cf.release=release;cf.type=type;cf.dict_type=dict_type;
    cf.array_type=array_type;cf.get=get_value;cf.count=count;cf.at=at;cf.integer=integer;cf.ready=1;
    for(int t=100;t<=111;++t) {
        reset();descriptions[0].type=t;
        int expected=(t==100 || t==101 || t==110)?0:ERR_UNSUPPORTED;
        assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==expected);
        assert((sessions[0].owner!=NULL)==(expected==0));
    }
    reset();encrypted=0;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_STATE);
    reset();derive_error=-6754;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==derive_error);
    assert(!sessions[0].owner);
    reset();derive_fail_call=3; // Events succeed; stream-key derivation fails.
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_AUTH);
    assert(!sessions[0].owner);
    reset();array.kind=1;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_PARAM);
    reset();descriptions[0].id=0;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_PARAM);
    reset();stream_count=STREAMS+1;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_UNSUPPORTED);
    for(int invalid=0;invalid<3;++invalid) {
        reset();descriptions[0].type=100;
        descriptions[0].format=invalid==0?0:invalid==1?3:UINT64_C(1)<<20;
        assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_UNSUPPORTED);
    }
    // A late rejected descriptor must roll back the whole request.
    reset();stream_count=2;descriptions[1]=(struct object){1,111,2,0};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_UNSUPPORTED);
    assert(!sessions[0].owner);
    reset();stream_count=2;descriptions[0].type=100;
    descriptions[1]=(struct object){1,101,1,UINT64_C(1)<<2};
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_STATE);
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
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_STATE);
    descriptions[0].id=4;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==0);
    cp_media_teardown((void *)2,NULL,0);
    assert(sessions[0].retired);
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_STATE);
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
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_STATE);
    reset();
    for(uintptr_t i=1;i<=SESSIONS;++i)
        assert(cp_media_prepare((void *)i,(void *)(i+SESSIONS),&request,(void *)30)==0);
    assert(cp_media_prepare((void *)31,(void *)32,&request,(void *)33)==ERR_STATE);
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    setenv("MHI2Q_DIO_OWNER_MODE","wireless",1);
    setenv("MHI2Q_CP_WIRELESS_MEDIA","1",1);
    setenv("MHI2Q_CP_WIRELESS_ALT_QEMU","1",1);
    assert(cp_media_alt_available());
    reset();descriptions[0].type=111;
    assert(!cp_media_prepare((void *)1,(void *)2,&request,(void *)3));
    uint8_t claimed[32];memset(claimed,0xa5,sizeof(claimed));
    assert(cp_media_alt_claim((void *)2,2,claimed)==ERR_STATE);
    for(unsigned i=0;i<32;++i)assert(!claimed[i]);
    memset(sessions[0].streams[3].read_key,0x5a,32);
    assert(!cp_media_alt_claim((void *)2,1,claimed));
    for(unsigned i=0;i<32;++i)assert(claimed[i]==0x5a);
    assert(cp_media_alt_claim((void *)2,1,claimed)==ERR_STATE);
    for(unsigned i=0;i<32;++i)assert(!claimed[i]);
    cp_media_teardown((void *)2,&request,0);
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_STATE);
    descriptions[0].id=2;
    assert(!cp_media_prepare((void *)1,(void *)2,&request,(void *)3));
    cp_media_teardown((void *)2,NULL,0);
    assert(cp_media_alt_claim((void *)2,2,claimed)==ERR_STATE);
    reset();descriptions[0].type=111;encrypted=0;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_STATE);
    reset();descriptions[0].type=111;derive_fail_call=3;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_AUTH);
    assert(!sessions[0].owner);
    setenv("MHI2Q_DIO_OWNER_MODE","wired",1);
    assert(!cp_media_alt_available());
    reset();descriptions[0].type=111;
    assert(cp_media_prepare((void *)1,(void *)2,&request,(void *)3)==ERR_UNSUPPORTED);
    unsetenv("MHI2Q_CP_WIRELESS_ALT_QEMU");unsetenv("MHI2Q_DIO_OWNER_MODE");
    unsetenv("MHI2Q_CP_WIRELESS_MEDIA");
    puts("QEMU paired111: authenticated admission, one key claim, rollback and nonce reuse rejection PASS");
#endif
    puts("wireless media contract: types, authentication, formats, rollback, nonce reuse and teardown PASS");
    return 0;
}
