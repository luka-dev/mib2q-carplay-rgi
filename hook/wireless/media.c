/* Stock210-only media adapters. See docs/wireless-carplay/STOCK210_MEDIA_CRYPTO_ABI.md.
 * All SDK objects remain private to pairing_sdk.c. Here CF objects are stock's.
 * This module is opt-in until the complete offline/media validation passes. */
#include "media.h"
#include "stock210_abi.h"
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>

DEFINE_LOG_MODULE(WMEDIA);
#define SESSIONS 8
#ifdef MHI2Q_WIRELESS_ALT_QEMU
#define STREAMS 4
#else
#define STREAMS 3
#endif
#define HISTORY 32
#define FRAME_LIMIT (16u * 1024u * 1024u)
#define ERR_STATE (-6709)
#define ERR_PARAM (-6705)
#define ERR_UNSUPPORTED (-6735)
#define ERR_AUTH (-6754)

enum { S_EVENT_CLIENT=704, S_MAIN=720, S_ALT=2896, S_SCREEN=5140,
       SCREEN_HEADER=72, SCREEN_AES_VALID=484, NET_READ=20 };
struct media_stream {
    int active, type;
    uint64_t id, send_nonce, receive_nonce;
    int failed;
    uint8_t read_key[32], write_key[32];
    uint16_t send_sequence;
    int sequence_set;
};
struct media_session {
    void *owner, *session;
    int retired;
    uint8_t event_read[32], event_write[32];
    struct media_stream streams[STREAMS];
    uint64_t used[HISTORY];
    unsigned used_count;
};
static struct media_session sessions[SESSIONS];
static pthread_mutex_t media_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t event_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { int used, fd; cp_pair_connection *transport; } events[SESSIONS];
static void erase(void *p, size_t n) { volatile uint8_t *b=p; while (n--) *b++=0; }
static void nonce_bytes(uint64_t value, uint8_t out[8])
{ unsigned i; for (i=0;i<8;++i) out[i]=(uint8_t)(value>>(8*i)); }

/* Stock CFLite only; none of these are namespaced 280 objects. */
static struct {
    void *(*string)(void *, const char *, unsigned);
    void (*release)(const void *);
    void *(*get)(const void *, const void *);
    uint32_t (*type)(const void *), (*dict_type)(void), (*array_type)(void);
    long (*count)(const void *);
    void *(*at)(const void *, long);
    int64_t (*integer)(void *, const void *, int *);
    void (*set_integer)(void *, const void *, int64_t);
    void (*remove)(void *, const void *);
    void *(*parse)(const void *, size_t, uint32_t, uint32_t, int *);
    void *(*serialize)(void *, const void *, long, long, void **);
    const uint8_t *(*bytes)(const void *);
    long (*length)(const void *);
    int (*body_length)(void *, size_t);
    int ready;
} cf;
static pthread_once_t cf_once = PTHREAD_ONCE_INIT;
static void resolve_cf(void)
{
#define RESOLVE(member,name) *(void **)(&cf.member)=dlsym(RTLD_NEXT,name); if (!cf.member) return
    RESOLVE(string,"CFStringCreateWithCString"); RESOLVE(release,"CFRelease");
    RESOLVE(get,"CFDictionaryGetValue"); RESOLVE(type,"CFGetTypeID");
    RESOLVE(dict_type,"CFDictionaryGetTypeID"); RESOLVE(array_type,"CFArrayGetTypeID");
    RESOLVE(count,"CFArrayGetCount"); RESOLVE(at,"CFArrayGetValueAtIndex");
    RESOLVE(integer,"CFDictionaryGetInt64"); RESOLVE(set_integer,"CFDictionarySetInt64");
    RESOLVE(remove,"CFDictionaryRemoveValue"); RESOLVE(parse,"CFCreateWithPlistBytes");
    RESOLVE(serialize,"CFPropertyListCreateData"); RESOLVE(bytes,"CFDataGetBytePtr");
    RESOLVE(length,"CFDataGetLength"); RESOLVE(body_length,"HTTPMessageSetBodyLength");
#undef RESOLVE
    cf.ready=1;
}
static void *key(const char *s) { return cf.string(NULL,s,0x08000100); }
static void *get(void *dict,const char *name)
{ if(!dict || cf.type(dict)!=cf.dict_type())return NULL; void *k=key(name), *v=k?cf.get(dict,k):NULL; if(k)cf.release(k); return v; }
static int64_t number(void *dict,const char *name,int *err)
{ if(!dict || cf.type(dict)!=cf.dict_type()){if(err)*err=ERR_PARAM;return 0;} void *k=key(name); int64_t n=0; if(k){n=cf.integer(dict,k,err);cf.release(k);}else if(err)*err=ENOMEM; return n; }
static void remove_key(void *dict,const char *name)
{ void *k=key(name); if(k){cf.remove(dict,k);cf.release(k);} }
static void set_number(void *dict,const char *name,int64_t n)
{ void *k=key(name); if(k){cf.set_integer(dict,k,n);cf.release(k);} }
static void *stream_array(void *dict)
{ void *a=get(dict,"streams"); return a && cf.type(a)==cf.array_type()?a:NULL; }
int cp_media_enabled(void)
{ const char *s=getenv("MHI2Q_CP_WIRELESS_MEDIA"); return s && !strcmp(s,"1"); }
int cp_media_available(void)
{ pthread_once(&cf_once,resolve_cf); return cp_media_enabled() && cf.ready; }
#ifdef MHI2Q_WIRELESS_ALT_QEMU
int cp_media_alt_available(void)
{
    const char *flag=getenv("MHI2Q_CP_WIRELESS_ALT_QEMU");
    return cp_dio_owner_is_wireless() && flag && !strcmp(flag,"1") && cp_media_available();
}
#endif

int cp_media_normalize_setup(void *message)
{
    int err=0;
    void *dict, *data;
    pthread_once(&cf_once,resolve_cf);
    if(!cf.ready)return ERR_UNSUPPORTED;
    dict=cf.parse(cp210_ptr(message,CP210_MSG_BODY_PTR),cp210_get(message,CP210_MSG_BODY_OFFSET),
                  1,cf.dict_type(),&err);
    if(!dict)return err?err:ERR_PARAM;
    /* Paired modern SETUP has no legacy MFi-SAP AES envelope. The stock
     * dispatcher sees the same request except for its obsolete AES fields. */
    remove_key(dict,"et"); remove_key(dict,"ekey"); remove_key(dict,"eiv");
    data=cf.serialize(NULL,dict,200,0,NULL);
    if(!data)err=ERR_PARAM;
    else {
        long n=cf.length(data);
        if(n<0 || n>CP_PAIR_BODY_MAX)err=ERR_PARAM;
        else {
            /* Preserve parsed URL/header pointers and pipelined extra bytes.
             * Only this dispatch's in-memory plist is replaced. */
            err=cf.body_length(message,(size_t)n);
            if(!err){
                memcpy(cp210_ptr(message,CP210_MSG_BODY_PTR),cf.bytes(data),(size_t)n);
                cp210_set(message,CP210_MSG_BODY_OFFSET,(uint32_t)n);
                cp210_set(message,CP210_MSG_CONTENT_LENGTH,(uint32_t)n);
                cp210_set(message,CP210_MSG_CONTENT_LENGTH+4,0);
            }
        }
        cf.release(data);
    }
    cf.release(dict);
    return err;
}
static struct media_session *find_session(void *session)
{ unsigned i;for(i=0;i<SESSIONS;++i)if(sessions[i].owner && sessions[i].session==session)return &sessions[i];return NULL; }
static int slot_for(int type) {
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    if(type==111 && cp_media_alt_available())return 3;
#endif
    return type==100?0:type==101?1:type==110?2:-1;
}
#ifdef MHI2Q_WIRELESS_ALT_QEMU
int cp_media_alt_claim(void *session, uint64_t connection_id, uint8_t key[32])
{
    int err=ERR_STATE;
    struct media_session *m;
    if(!key)return ERR_PARAM;
    erase(key,32);
    if(!cp_media_alt_available())return ERR_UNSUPPORTED;
    pthread_mutex_lock(&media_lock);
    m=find_session(session);
    if(m && !m->retired && m->streams[3].active && !m->streams[3].failed &&
       connection_id && m->streams[3].id==connection_id) {
        memcpy(key,m->streams[3].read_key,32);
        /* Claim exactly once, even if the following socket setup fails. */
        m->streams[3].failed=1;
        err=0;
    }
    pthread_mutex_unlock(&media_lock);
    return err;
}
#endif
static int derive_stream(cp_pair_connection *pair,struct media_stream *stream)
{
    char salt[64];
    snprintf(salt,sizeof(salt),"DataStream-Salt%llu",(unsigned long long)stream->id);
    int err=cp_pair_derive(pair,salt,"DataStream-Output-Encryption-Key",stream->read_key);
    if(!err)err=cp_pair_derive(pair,salt,"DataStream-Input-Encryption-Key",stream->write_key);
    return err;
}
int cp_media_prepare(void *owner,void *session,void *request,cp_pair_connection *pair)
{
    struct media_session pending, *m;
    void *array;
    long count,i;
    unsigned j;
    int err=0, fresh=0;
    pthread_once(&cf_once,resolve_cf);
    if(!cf.ready || !cp_pair_is_encrypted(pair))return ERR_STATE;
    pthread_mutex_lock(&media_lock);
    m=find_session(session);
    if(!m)for(j=0;j<SESSIONS;++j)if(sessions[j].owner==owner){err=ERR_STATE;goto exit;}
    if(!m){for(j=0;j<SESSIONS;++j)if(!sessions[j].owner){m=&sessions[j];break;}fresh=1;}
    if(!m || m->retired){err=ERR_STATE;goto exit;}
    pending=*m;
    if(fresh){
        pending.owner=owner;pending.session=session;
        err=cp_pair_derive(pair,"Events-Salt","Events-Read-Encryption-Key",pending.event_read);
        if(!err)err=cp_pair_derive(pair,"Events-Salt","Events-Write-Encryption-Key",pending.event_write);
        if(err)goto exit;
    }
    array=stream_array(request);
    if(get(request,"streams") && !array){err=ERR_PARAM;goto exit;}
    count=array?cf.count(array):0;
    if(count<0 || count>STREAMS){err=ERR_UNSUPPORTED;goto exit;}
    for(i=0;i<count;++i){
        void *desc=cf.at(array,i);
        if(!desc || cf.type(desc)!=cf.dict_type()){err=ERR_PARAM;goto exit;}
        int type=(int)number(desc,"type",&err), slot=slot_for(type);
        if(err || slot<0){err=ERR_UNSUPPORTED;goto exit;}
        struct media_stream *s=&pending.streams[slot];
        if(s->active || pending.used_count==HISTORY){err=ERR_STATE;goto exit;}
        uint64_t id=(uint64_t)number(desc,"streamConnectionID",&err);
        if(err || !id){err=ERR_PARAM;goto exit;}
        for(j=0;j<pending.used_count;++j)if(pending.used[j]==id){err=ERR_STATE;goto exit;}
        if(type!=110 && type!=111){
            uint64_t format=(uint64_t)number(desc,"audioFormat",&err);
            /* Target worker always swaps 16-bit PCM. No compressed or 24-bit paths. */
            uint64_t pcm16=0;
            for(j=2;j<=11;++j)pcm16|=UINT64_C(1)<<j;
            pcm16|=(UINT64_C(1)<<14)|(UINT64_C(1)<<15);
            if(err || !format || (format&(format-1)) || !(format&pcm16)) {err=ERR_UNSUPPORTED;goto exit;}
        }
        memset(s,0,sizeof(*s));s->type=type;s->id=id;s->active=1;
        err=derive_stream(pair,s);if(err)goto exit;
        pending.used[pending.used_count++]=id;
    }
    *m=pending;
exit:
    erase(&pending,sizeof(pending));
    pthread_mutex_unlock(&media_lock);
    if(err)LOG_WARN(LOG_MODULE,"SETUP crypto rejected status=%d",err);
    return err;
}
void cp_media_setup_response(hook_setup_ctx_t *setup)
{
    struct media_session *m;
    pthread_mutex_lock(&media_lock);
    m=find_session(setup->session);
    if(m && !setup->result && setup->response){
        void *a=stream_array(setup->response);
        long i,n=a?cf.count(a):0;
        for(i=0;i<n;++i){
            void *desc=cf.at(a,i);
            int slot=slot_for((int)number(desc,"type",NULL));
            if(slot>=0 && m->streams[slot].active)
                set_number(desc,"streamConnectionID",(int64_t)m->streams[slot].id);
        }
    }
    /* On failure stock performs teardown itself before returning. Keep the
     * history until the HTTP finalizer so IDs/nonces cannot be reused. */
    pthread_mutex_unlock(&media_lock);
}
void cp_media_teardown(void *session,void *request,int reason)
{
    unsigned i;
    (void)reason;
    pthread_mutex_lock(&media_lock);
    struct media_session *m=find_session(session);
    if(m){
        for(i=0;i<STREAMS;++i){
            int stopped=!request;
            if(request){void *a=stream_array(request);long j,n=a?cf.count(a):0;
                for(j=0;j<n;++j)if(number(cf.at(a,j),"type",NULL)==m->streams[i].type)stopped=1;}
            if(stopped)erase(&m->streams[i],sizeof(m->streams[i]));
        }
        if(!request){m->retired=1;erase(m->event_read,32);erase(m->event_write,32);}
    }
    pthread_mutex_unlock(&media_lock);
}
void cp_media_forget(void *owner)
{
    unsigned i;
    pthread_mutex_lock(&media_lock);
    for(i=0;i<SESSIONS;++i)if(sessions[i].owner==owner)erase(&sessions[i],sizeof(sessions[i]));
    pthread_mutex_unlock(&media_lock);
}

/* Events share stock HTTP message machinery but have their own transport. */
HOOK_EXPORT int HTTPClientCreateWithSocket(void **out,int fd)
{
    typedef int (*fn)(void **,int);
    fn real=(fn)dlsym(RTLD_NEXT,"HTTPClientCreateWithSocket");
    uint8_t r[32],w[32];
    unsigned i;
    int matched=0,err;
    if(!real)return ERR_UNSUPPORTED;
    pthread_mutex_lock(&media_lock);
    for(i=0;i<SESSIONS;++i)if(sessions[i].owner && !sessions[i].retired &&
        (uint8_t *)out==(uint8_t *)sessions[i].session+S_EVENT_CLIENT){
        memcpy(r,sessions[i].event_read,32);memcpy(w,sessions[i].event_write,32);matched=1;break;
    }
    pthread_mutex_unlock(&media_lock);
    if(!matched)return real(out,fd);
    pthread_mutex_lock(&event_lock);
    for(i=0;i<SESSIONS;++i)if(!events[i].used)break;
    err=i==SESSIONS?ENOMEM:cp_pair_transport_create(fd,r,w,&events[i].transport);
    erase(r,32);erase(w,32);
    if(!err){events[i].used=1;events[i].fd=fd;}
    pthread_mutex_unlock(&event_lock);
    if(err)return err;
    err=real(out,fd);
    if(err){
        pthread_mutex_lock(&event_lock);
        if(events[i].used && events[i].fd==fd){
            cp_pair_connection_free(events[i].transport);memset(&events[i],0,sizeof(events[i]));
        }
        pthread_mutex_unlock(&event_lock);
    }
    return err;
}
int cp_media_event_read(int fd,void *message,int *result)
{
    unsigned i;int handled=0;
    pthread_mutex_lock(&event_lock);
    for(i=0;i<SESSIONS;++i)if(events[i].used && events[i].fd==fd){
        *result=cp210_read_message(events[i].transport,message);handled=1;break;
    }
    pthread_mutex_unlock(&event_lock);return handled;
}
int cp_media_event_write(int fd,struct iovec **vectors,int *count,int *result)
{
    unsigned i;int handled=0;
    pthread_mutex_lock(&event_lock);
    for(i=0;i<SESSIONS;++i)if(events[i].used && events[i].fd==fd){
        *result=cp_pair_writev(events[i].transport,vectors,count);handled=1;break;
    }
    pthread_mutex_unlock(&event_lock);return handled;
}
/* HTTPClientInvalidate is asynchronous. Only the actual close relinquishes
 * the fd. Keep crypto attached until then, under the same lock as I/O. */
HOOK_EXPORT int close(int fd)
{
    typedef int (*fn)(int);
    fn real=(fn)dlsym(RTLD_NEXT,"close");
    unsigned i;int result,saved;
    if(!real){errno=ENOSYS;return -1;}
    pthread_mutex_lock(&event_lock);
    result=real(fd);saved=errno;
    for(i=0;i<SESSIONS;++i)if((result==0 || saved==EBADF) && events[i].used && events[i].fd==fd){
        cp_pair_connection_free(events[i].transport);memset(&events[i],0,sizeof(events[i]));break;
    }
    pthread_mutex_unlock(&event_lock);errno=saved;return result;
}

/* A wrapper for this one NetSocket instance preserves stock timing, command
 * processing, decoder selection, and render callbacks. No TCP proxy/thread. */
typedef int (*net_read_fn)(void *,size_t,size_t,void *,size_t *,int,int);
struct screen_reader {
    void *net;net_read_fn real;
    uint8_t key[32],aad[128];uint64_t nonce;
    size_t body;int encrypted;
};
static struct screen_reader *readers[SESSIONS];
static struct media_stream *screen_stream(void *screen)
{
    unsigned i;
    for(i=0;i<SESSIONS;++i)if(sessions[i].owner && sessions[i].streams[2].active &&
        cp210_ptr(sessions[i].session,S_SCREEN)==screen)return &sessions[i].streams[2];
    return NULL;
}
static int screen_read(void *net,size_t min,size_t max,void *buffer,size_t *length,int flags,int timeout)
{
    unsigned i;struct screen_reader *r=NULL;int err;size_t got=0;
    pthread_mutex_lock(&media_lock);
    for(i=0;i<SESSIONS;++i)if(readers[i] && readers[i]->net==net){r=readers[i];break;}
    pthread_mutex_unlock(&media_lock);
    if(length)*length=0;
    if(!r || min!=max)return ERR_STATE;
    if(!r->body){
        if(max!=sizeof(r->aad))return ERR_STATE;
        err=r->real(net,min,max,buffer,&got,flags,timeout);if(err)return err;
        if(got!=max)return ERR_STATE;
        memcpy(r->aad,buffer,sizeof(r->aad));
        r->body=cp210_get(buffer,0);r->encrypted=r->aad[4]==0;
        if(r->body>FRAME_LIMIT || (r->encrypted && r->body<=16))return ERR_PARAM;
        if(r->encrypted){r->body-=16;cp210_set(buffer,0,(uint32_t)r->body);}
        if(length)*length=max;
        return 0;
    }
    if(max!=r->body)return ERR_STATE;
    err=r->real(net,min,max,buffer,&got,flags,timeout);if(err)return err;
    if(got!=max)return ERR_STATE;
    if(r->encrypted){
        uint8_t tag[16],nonce[8];
        if(r->nonce==UINT64_MAX)return ERR_STATE;
        err=r->real(net,16,16,tag,&got,flags,timeout);if(err)return err;
        if(got!=16)return ERR_STATE;
        nonce_bytes(r->nonce,nonce);
        err=cp_pair_open(r->key,nonce,r->aad,128,buffer,max,tag,buffer);
        if(err){LOG_WARN(LOG_MODULE,"screen authentication rejected");return ERR_AUTH;}
        ++r->nonce;
    }
    r->body=0;
    if(length)*length=max;
    return 0;
}
HOOK_EXPORT int AirPlayReceiverSessionScreen_SetSecurityInfo(void *screen,const void *key_bytes,const void *iv)
{
    typedef int (*fn)(void *,const void *,const void *);
    fn real=(fn)dlsym(RTLD_NEXT,"AirPlayReceiverSessionScreen_SetSecurityInfo");
    int matched;
    pthread_mutex_lock(&media_lock);matched=screen_stream(screen)!=NULL;pthread_mutex_unlock(&media_lock);
    if(!matched)return real?real(screen,key_bytes,iv):ERR_UNSUPPORTED;
    /* A fresh stock screen object must have no previously initialized AES. */
    return ((uint8_t *)screen)[SCREEN_AES_VALID]?ERR_STATE:0;
}
HOOK_EXPORT int AirPlayReceiverSessionScreen_ProcessFrames(void *screen,void *net,int timeout)
{
    typedef int (*fn)(void *,void *,int);
    fn real=(fn)dlsym(RTLD_NEXT,"AirPlayReceiverSessionScreen_ProcessFrames");
    struct screen_reader r;unsigned i;int result;
    if(!real)return ERR_UNSUPPORTED;
    memset(&r,0,sizeof(r));
    pthread_mutex_lock(&media_lock);
    struct media_stream *stream=screen_stream(screen);
    if(!stream){pthread_mutex_unlock(&media_lock);return real(screen,net,timeout);}
    if(stream->failed){pthread_mutex_unlock(&media_lock);return ERR_AUTH;}
    for(i=0;i<SESSIONS;++i)if(!readers[i])break;
    if(i==SESSIONS){pthread_mutex_unlock(&media_lock);return ERR_STATE;}
    r.net=net;r.real=(net_read_fn)cp210_ptr(net,NET_READ);
    memcpy(r.key,stream->read_key,32);r.nonce=stream->receive_nonce;
    readers[i]=&r;cp210_set(net,NET_READ,(uint32_t)(uintptr_t)screen_read);
    pthread_mutex_unlock(&media_lock);
    result=real(screen,net,timeout);
    pthread_mutex_lock(&media_lock);
    cp210_set(net,NET_READ,(uint32_t)(uintptr_t)r.real);readers[i]=NULL;
    stream->receive_nonce=r.nonce;stream->failed=1; /* No resumed TCP stream with reused nonce. */
    pthread_mutex_unlock(&media_lock);
    erase(&r,sizeof(r));return result;
}

/* Audio socket wrappers touch only descriptors owned by registered streams. */
static struct media_stream *audio_stream(int fd,void **context)
{
    unsigned i,j;
    for(i=0;i<SESSIONS;++i)if(sessions[i].owner)for(j=0;j<2;++j){
        void *ctx=(uint8_t *)sessions[i].session+(j?S_ALT:S_MAIN);
        if(sessions[i].streams[j].active && (int)cp210_get(ctx,16)==fd){
            if(context)*context=ctx;
            return &sessions[i].streams[j];
        }
    }
    return NULL;
}
HOOK_EXPORT int SocketRecvFrom(int fd,void *buffer,size_t capacity,size_t *length,void *from,
                               size_t from_max,size_t *from_len,uint64_t *ticks,uint32_t *ifindex,char *ifname)
{
    typedef int (*fn)(int,void *,size_t,size_t *,void *,size_t,size_t *,uint64_t *,uint32_t *,char *);
    fn real=(fn)dlsym(RTLD_NEXT,"SocketRecvFrom");
    int err;uint8_t *packet=buffer;
    if(!real)return ERR_UNSUPPORTED;
    err=real(fd,buffer,capacity,length,from,from_max,from_len,ticks,ifindex,ifname);
    if(err)return err;
    pthread_mutex_lock(&media_lock);
    void *ctx=NULL;
    struct media_stream *s=audio_stream(fd,&ctx);
    if(s){
        size_t n=length?*length:0;
        unsigned frame_bytes=cp210_get(ctx,1600);
        if(n<=36 || n>capacity || packet[0]!=0x80 || !frame_bytes ||
            (n-36)%frame_bytes)err=ERR_PARAM;
        else {
            /* Modern iOS (OS build >13A1): timestamp+SSRC are the AAD. */
            err=cp_pair_open(s->read_key,packet+n-8,packet+4,8,packet+12,n-36,packet+n-24,packet+12);
            if(!err)*length=n-24;
        }
        if(err && length)*length=0;
    }
    pthread_mutex_unlock(&media_lock);return err;
}
HOOK_EXPORT ssize_t sendto(int fd,const void *buffer,size_t length,int flags,const struct sockaddr *to,socklen_t to_len)
{
    typedef ssize_t (*fn)(int,const void *,size_t,int,const struct sockaddr *,socklen_t);
    fn real=(fn)dlsym(RTLD_NEXT,"sendto");
    void *ctx=NULL;ssize_t result;size_t offset=0;
    uint8_t packet[1452],nonce[8];const uint8_t *source=buffer;
    if(!real){errno=ENOSYS;return -1;}
    pthread_mutex_lock(&media_lock);
    struct media_stream *s=audio_stream(fd,&ctx);
    if(!s){pthread_mutex_unlock(&media_lock);return real(fd,buffer,length,flags,to,to_len);}
    unsigned frame_bytes=cp210_get(ctx,1600);
    if(length<12 || length>1452 || source[0]!=0x80 || !frame_bytes || frame_bytes>8 ||
        (length-12)%frame_bytes){errno=EINVAL;result=-1;goto exit;}
    if(!s->sequence_set){s->send_sequence=(uint16_t)((source[2]<<8)|source[3]);s->sequence_set=1;}
    uint32_t timestamp=((uint32_t)source[4]<<24)|((uint32_t)source[5]<<16)|((uint32_t)source[6]<<8)|source[7];
    while(offset<length-12){
        size_t n=length-12-offset, limit=1416/frame_bytes*frame_bytes;
        if(n>limit)n=limit;
        if(s->send_nonce==UINT64_MAX){errno=EOVERFLOW;result=-1;goto exit;}
        memcpy(packet,source,12);
        packet[2]=(uint8_t)(s->send_sequence>>8);packet[3]=(uint8_t)s->send_sequence++;
        uint32_t stamp=timestamp+(uint32_t)(offset/frame_bytes);
        packet[4]=(uint8_t)(stamp>>24);packet[5]=(uint8_t)(stamp>>16);packet[6]=(uint8_t)(stamp>>8);packet[7]=(uint8_t)stamp;
        nonce_bytes(s->send_nonce++,nonce);
        cp_pair_seal(s->write_key,nonce,packet+4,8,source+12+offset,n,packet+12,packet+12+n);
        memcpy(packet+12+n+16,nonce,8);
        result=real(fd,packet,12+n+24,flags,to,to_len);
        if(result!=(ssize_t)(12+n+24)){if(result>=0)errno=EIO;result=-1;goto exit;}
        offset+=n;
    }
    result=(ssize_t)length;
exit:
    erase(packet,sizeof(packet));pthread_mutex_unlock(&media_lock);return result;
}
