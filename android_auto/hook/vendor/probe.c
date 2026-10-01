/* SPDX-License-Identifier: GPL-3.0-or-later
 * SQ5 MU0918 capture-only cluster probe.
 * Protocol injection/layout approach adapted from the local
 * chopinwong01/mhi2-android-auto-video-vc video_sink_hook.c (GPL-3.0).
 * Unlike its player hook, this builds NO GAL callback-handler or renderer.
 * A private VideoSink vtable owns every callback-dependent method. Setup
 * reproduces the verified sendConfig(2) + setVideoFocus(1,true) protocol part.
 * Fixed 800x480@30 baseline; no gal.json edits, decoder, DMDT, or TCP listener.
 */
#include "capture.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef int (*register_fn)(void *, void *);
typedef void (*config_fn)(void *, int,int,int,int,int,int,int);
typedef void (*discovery_fn)(void *, void *);
typedef void (*sendconfig_fn)(void *, int);
typedef void (*focus_fn)(void *, int, int);
typedef int (*ack_fn)(void *, int, unsigned);
typedef void (*queue_fn)(void *, unsigned char, void *, unsigned);
typedef void (*route_fn)(void *, unsigned char, const void *);
typedef int (*open_fn)(void *, unsigned char, const void *);

static register_fn real_register;
static config_fn real_config;
static discovery_fn real_discovery, real_input_discovery;
static sendconfig_fn send_config;
static focus_fn set_focus;
static ack_fn ack_frames;
static queue_fn queue_plain, queue_encrypted;
static route_fn real_route;
static open_fn real_open;
static void (*real_version)(void *);
static void *primary, *secondary, *pending, *sink_vtable, *input_endpoint, *base_vtable;
static uint32_t private_vtable[18]; /* 8-byte header, sixteen method pointers */
static uint32_t input_vtable[10];
static unsigned video_service, input_service;
static unsigned input_channel = 256;
static int enabled, registered, attempted, streaming, captured_session;
static unsigned long frame_count, frame_bytes, ack_count;
static unsigned long channel_messages[256];
static FILE *logfile;
static size_t logbytes;
static pthread_mutex_t loglock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t u32(const void *p, unsigned off)
{ uint32_t v; memcpy(&v, (const char *)p+off, 4); return v; }
static void put32(void *p, unsigned off, uint32_t v)
{ memcpy((char *)p+off, &v, 4); }
static unsigned u8(const void *p, unsigned off)
{ return ((const unsigned char *)p)[off]; }

void probe_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    struct timespec t;
    int n, k;
    clock_gettime(CLOCK_MONOTONIC, &t);
    n = snprintf(line, sizeof(line), "t=%ld.%03ld ", (long)t.tv_sec, t.tv_nsec/1000000);
    va_start(ap, fmt); vsnprintf(line+n, sizeof(line)-(size_t)n-2, fmt, ap); va_end(ap);
    k = (int)strlen(line); line[k++]='\n';
    pthread_mutex_lock(&loglock);
    if (logfile && logbytes + (size_t)k <= 1024u*1024u) {
        fwrite(line, 1, (size_t)k, logfile); fflush(logfile); logbytes += (size_t)k;
    }
    pthread_mutex_unlock(&loglock);
}

static void *sym(const char *s) { return dlsym(RTLD_NEXT, s); }

/* Only append to a fresh protobuf-lite SSO unknown-field string. */
static int unknown(void *object, const unsigned char *data, unsigned n)
{
    if (!object || n > 15 || u32(object,0x1c)!=15 || u32(object,0x18)!=0) return 0;
    memcpy((char *)object+8,data,n); ((char *)object)[8+n]=0;
    put32(object,0x18,n); return 1;
}

static void *last_service(void *response)
{
    unsigned n = u32(response,0x2c);
    void *array = (void *)(uintptr_t)u32(response,0x28);
    if (!n || n>128 || !array) return NULL;
    return (void *)(uintptr_t)u32(array,4*(n-1));
}

static const unsigned char *payload(const void *shared, unsigned skip, unsigned *size)
{
    const void *io;
    uint32_t base, off, end;
    *size=0;
    if (!shared) return NULL;
    io=(void *)(uintptr_t)u32(shared,4);
    if (!io) return NULL;
    base=u32(io,0); off=u32(io,8); end=u32(io,12);
    if (!base || end<off || skip>end-off || end-off-skip>4u*1024u*1024u ||
        (uint64_t)base+end>UINT32_MAX) return NULL;
    *size=end-off-skip;
    return (const unsigned char *)(uintptr_t)(base+off+skip);
}

void _ZN9VideoSink16addDiscoveryInfoEP24ServiceDiscoveryResponse(void *,void *);

static int secondary_setup(void *sink, int type)
{
    probe_log("secondary.setup service=%u channel=%u codec=%d",video_service,u8(sink,5),type);
    if (type!=3) return -8; /* Stock accepts H.264 codec type 3 only. */
    send_config(sink,2); /* Verified stock handleSetup's status value. */
    set_focus(sink,1,1);
    probe_log("secondary.config_response indices=1 focus=1 renderer=none");
    return 0;
}
static int secondary_configuration(void *sink, int config)
{
    /* +0x18 is the session set by MediaSinkBase::handleStart, NOT config index. */
    probe_log("secondary.configuration index=%d session=%u",config,u32(sink,0x18));
    return config==0 ? 0 : -8;
}
static void secondary_start(void *sink, int session)
{
    (void)sink;
    if (captured_session) capture_finish("subsequent_playback_not_concatenated");
    captured_session=1; streaming=1; frame_count=frame_bytes=ack_count=0;
    probe_log("secondary.start session=%d service=%u channel=%u",session,video_service,u8(sink,5));
}
static void secondary_stop(void *sink, int session)
{
    (void)sink; streaming=0;
    probe_log("secondary.stop session=%d frames=%lu bytes=%lu acks=%lu",session,frame_count,frame_bytes,ack_count);
    capture_finish("playback_stop");
}
static void secondary_codec(void *sink, void *data, unsigned size)
{
    (void)sink;
    probe_log("secondary.codec_config bytes=%u",size);
    if (size<=65536) capture_codec(data,size);
}
static void secondary_data(void *sink, uint64_t timestamp, const void *shared, unsigned skip)
{
    unsigned size;
    const unsigned char *p=payload(shared,skip,&size);
    int rc;
    ++frame_count;
    if (p && size) { frame_bytes+=size; capture_frame(p,size); }
    rc=ack_frames(sink,(int)u32(sink,0x18),1);
    ++ack_count;
    if (frame_count<=3 || frame_count%300==0)
        probe_log("secondary.frame count=%lu bytes=%u total=%lu pts=%llu nal_mask=0x%x ack_rc=%d session=%u gate=%u channel=%u",
                  frame_count,size,frame_bytes,(unsigned long long)timestamp,
                  p?capture_nals(p,size):0,rc,u32(sink,0x18),u8(sink,4),u8(sink,5));
}
static int secondary_focus(void *sink, const void *request)
{
    int mode=request?(int)u32(request,0x2c):0;
    probe_log("secondary.focus_request mode=%d",mode);
    if (mode!=1 && mode!=2) return 0;
    set_focus(sink,mode,0); return 1;
}
static int secondary_closed(void *sink, unsigned char channel)
{
    int (*f)(void *,unsigned char)=sym("_ZN20ProtocolEndpointBase15onChannelClosedEh");
    secondary_stop(sink,(int)u32(sink,0x18));
    return f?f(sink,channel):0;
}
static void secondary_destroy(void *sink)
{
    void (*f)(void *)=sym("_ZN9VideoSinkD1Ev");
    capture_finish("sink_destructor"); registered=0; secondary=NULL; input_channel=256;
    if(f) f(sink);
}
static void secondary_delete(void *sink)
{
    void (*f)(void *)=sym("_ZN9VideoSinkD0Ev");
    capture_finish("sink_deleting_destructor"); registered=0; secondary=NULL; input_channel=256;
    if(f) f(sink);
}
static void input_discovery(void *endpoint, void *response)
{
    unsigned char input[14]={0x0a,0x0c,0x08,0,0x22,0x08,0x0a,0x04,0x17,0x80,0x80,0x04,0x28,1};
    (void)endpoint;
    input[3]=(unsigned char)input_service;
    probe_log("discovery.input service=%u display_id=1 metadata_ok=%d",input_service,unknown(response,input,sizeof(input)));
}
static int input_route(void *endpoint,unsigned char channel,unsigned short id,const void *shared)
{
    (void)shared;
    probe_log("input.message channel=%u id=0x%04x",channel,id);
    if(id==0x8002) {
        unsigned char reply[4]={0x80,3,8,0};
        queue_encrypted((void *)(uintptr_t)u32(endpoint,8),channel,reply,sizeof(reply));
        probe_log("input.binding response=ok");
        return 0;
    }
    return -4;
}

static void build_sink(void *receiver,void *main_sink)
{
    unsigned id;
    void *router=(void *)(uintptr_t)u32(main_sink,8);
    if(attempted || !enabled) return;
    attempted=1;
    if(router!=receiver || u32(main_sink,0)!=(uint32_t)(uintptr_t)sink_vtable+8) {
        probe_log("abi.refused reason=primary_vtable_or_router"); return;
    }
    /* Two free one-byte-varint IDs. Input is reserved by this probe. */
    for(id=64;id<126;id+=2)
        if(!u32(router,(id+64)*4) && !u32(router,(id+65)*4)) break;
    if(id>=126) { probe_log("secondary.refused no_service_pair"); return; }
    secondary=calloc(1,0x50);
    input_endpoint=calloc(1,0x14);
    if(!secondary || !input_endpoint) { free(secondary); free(input_endpoint); secondary=input_endpoint=NULL; return; }
    video_service=id; input_service=id+1;
    memcpy(private_vtable,sink_vtable,sizeof(private_vtable));
#define SLOT(n,fn) private_vtable[2+(n)]=(uint32_t)(uintptr_t)(fn)
    SLOT(0,secondary_destroy); SLOT(1,secondary_delete); SLOT(2,secondary_closed);
    SLOT(7,_ZN9VideoSink16addDiscoveryInfoEP24ServiceDiscoveryResponse);
    SLOT(8,secondary_setup); SLOT(9,secondary_codec); SLOT(10,secondary_data);
    SLOT(11,secondary_start); SLOT(12,secondary_stop); SLOT(13,secondary_configuration);
    SLOT(14,secondary_focus);
#undef SLOT
    put32(secondary,0,(uint32_t)(uintptr_t)(private_vtable+2));
    put32(secondary,8,(uint32_t)(uintptr_t)router);
    ((unsigned char *)secondary)[12]=(unsigned char)id;
    put32(secondary,0x14,3); put32(secondary,0x18,UINT32_MAX);
    put32(secondary,0x1c,8); ((unsigned char *)secondary)[0x30]=1;
    put32(secondary,0x4c,700);
    memcpy(input_vtable,base_vtable,sizeof(input_vtable));
    input_vtable[2+5]=(uint32_t)(uintptr_t)input_route;
    input_vtable[2+7]=(uint32_t)(uintptr_t)input_discovery;
    put32(input_endpoint,0,(uint32_t)(uintptr_t)(input_vtable+2));
    put32(input_endpoint,8,(uint32_t)(uintptr_t)router);
    ((unsigned char *)input_endpoint)[12]=(unsigned char)input_service;
    real_config(secondary,1,30,0,0,160,3,10000);
    if(u32(secondary,0x44)-u32(secondary,0x40)!=8) {
        probe_log("secondary.refused configuration_vector"); return;
    }
    registered=real_register(receiver,secondary)!=0;
    if(registered) registered=real_register(receiver,input_endpoint)!=0;
    probe_log("secondary.register ok=%d video_service=%u input_service=%u codec=800x480@30 callbacks=private renderer=none",
              registered,video_service,input_service);
}

int _ZN11GalReceiver15registerServiceEP20ProtocolEndpointBase(void *r,void *e)
{
    int rc=real_register?real_register(r,e):0;
    if(enabled && rc && e && e==pending) { primary=e; build_sink(r,e); }
    return rc;
}
void _ZN9VideoSink25addSupportedConfigurationEiiiiiii(void *s,int a,int b,int c,int d,int e,int f,int g)
{
    if(enabled && s!=secondary) pending=s;
    if(real_config) real_config(s,a,b,c,d,e,f,g);
}
void _ZN9VideoSink16addDiscoveryInfoEP24ServiceDiscoveryResponse(void *sink,void *response)
{
    void *service,*media;
    unsigned char meta[4]={0x30,0,0x38,0};
    if(real_discovery) real_discovery(sink,response);
    if(!registered || !response || (sink!=primary && sink!=secondary)) return;
    service=last_service(response);
    if(!service || u32(service,0x58)!=u8(sink,12)) { probe_log("discovery.error service_mismatch"); return; }
    media=(void *)(uintptr_t)u32(service,0x2c);
    if(sink==secondary) meta[1]=meta[3]=1;
    probe_log("discovery.video service=%u cluster=%d metadata_ok=%d",u8(sink,12),sink==secondary,unknown(media,meta,4));
}
void _ZN11InputSource16addDiscoveryInfoEP24ServiceDiscoveryResponse(void *s,void *r)
{
    void *entry;
    const unsigned char tag[2]={0x28,0};
    if(real_input_discovery) real_input_discovery(s,r);
    if(!registered || !r) return;
    entry=last_service(r);
    if(entry && (u32(entry,0x20)&8))
        probe_log("discovery.main_input display_id=0 metadata_ok=%d",unknown((void *)(uintptr_t)u32(entry,0x30),tag,2));
}
void _ZN10Controller18sendVersionRequestEv(void *s)
{
    unsigned char msg[6]={0,1,0,1,0,7};
    if(!enabled || !registered) { if(real_version) real_version(s); return; }
    if(s && u8(s,4)) {
        queue_plain((void *)(uintptr_t)u32(s,8),(unsigned char)u8(s,5),msg,sizeof(msg));
        probe_log("version.sent major=1 minor=7");
    }
}
int _ZN13MessageRouter20handleChannelOpenReqEhRK18ChannelOpenRequest(void *r,unsigned char ch,const void *req)
{
    unsigned service=req?u8(req,0x2c):255;
    int rc;
    rc=real_open?real_open(r,ch,req):-4;
    if(registered && service==input_service && rc==0) input_channel=ch;
    probe_log("channel.open role=%s service=%u channel=%u result=%d",
              registered && service==video_service?"cluster_video":
              registered && service==input_service?"cluster_input":"stock",service,ch,rc);
    return rc;
}
void _ZN13MessageRouter12routeMessageEhRK10shared_ptrI8IoBufferE(void *r,unsigned char ch,const void *shared)
{
    unsigned n=0;
    const unsigned char *p;
    if(registered && (ch==input_channel || ch==u8(secondary,5))) {
        p=payload(shared,0,&n);
        if(channel_messages[ch]++<24 && p && n>=2)
            probe_log("channel.message channel=%u service=%u id=0x%02x%02x bytes=%u",ch,u8(r,ch),p[0],p[1],n);
    }
    if(real_route) real_route(r,ch,shared);
}

#ifndef PROBE_TEST
__attribute__((constructor)) static void init(void)
{
    char path[256];
    uintptr_t base;
    const char *opt=getenv("SQ5_CLUSTER_PROBE");
    snprintf(path,sizeof(path),"/tmp/sq5_cluster_probe/probe-%ld.log",(long)getpid());
    logfile=fopen(path,"a");
#define RES(var,name) var=(void *)sym(name)
    RES(real_register,"_ZN11GalReceiver15registerServiceEP20ProtocolEndpointBase");
    RES(real_config,"_ZN9VideoSink25addSupportedConfigurationEiiiiiii");
    RES(real_discovery,"_ZN9VideoSink16addDiscoveryInfoEP24ServiceDiscoveryResponse");
    RES(real_input_discovery,"_ZN11InputSource16addDiscoveryInfoEP24ServiceDiscoveryResponse");
    RES(send_config,"_ZN13MediaSinkBase10sendConfigEi");
    RES(set_focus,"_ZN9VideoSink13setVideoFocusEib");
    RES(ack_frames,"_ZN13MediaSinkBase9ackFramesEij");
    RES(real_open,"_ZN13MessageRouter20handleChannelOpenReqEhRK18ChannelOpenRequest");
    RES(real_route,"_ZN13MessageRouter12routeMessageEhRK10shared_ptrI8IoBufferE");
    RES(queue_plain,"_ZN13MessageRouter24queueOutgoingUnencryptedEhPvj");
    RES(queue_encrypted,"_ZN13MessageRouter13queueOutgoingEhPvj");
    RES(real_version,"_ZN10Controller18sendVersionRequestEv");
    RES(sink_vtable,"_ZTV9VideoSink");
    RES(base_vtable,"_ZTV20ProtocolEndpointBase");
#undef RES
    base=(uintptr_t)real_register-0xe4928;
    enabled=opt && !strcmp(opt,"1") && real_register && real_config && real_discovery &&
        real_input_discovery && send_config && set_focus && ack_frames && real_open &&
        real_route && queue_plain && queue_encrypted && real_version && sink_vtable && base_vtable &&
        (uintptr_t)dlsym(RTLD_DEFAULT,"_ZN3gal14CGALController10s_instanceE")==0x2544a0 &&
        (uintptr_t)dlsym(RTLD_DEFAULT,"_ZTVN3gal14CVideoSinkImplE")==0x24dfb8 &&
        (uintptr_t)sink_vtable==base+0x102b90 && (uintptr_t)base_vtable==base+0x102af0 && (uintptr_t)real_config==base+0xfafc4 &&
        (uintptr_t)send_config==base+0xec748 && (uintptr_t)set_focus==base+0xfa978 &&
        (uintptr_t)ack_frames==base+0xec294;
    probe_log("probe.init build=sq5-cluster-probe-v1 enabled=%d profile=MU0918 capture_only=1",enabled);
    if(enabled && capture_init("/tmp/sq5_cluster_probe")!=0) {
        enabled=0; probe_log("probe.disabled reason=capture_initialization");
    }
}
__attribute__((destructor)) static void finish(void)
{
    capture_join();
    if(logfile) { fclose(logfile); logfile=NULL; }
}
#endif
