/* Pinned MHI2Q ABI. All mutations run on SI's controller thread. Native
 * constructors/copies/destructors own every util::UnicodeString8. */
#include "si_native.h"
#include "si_protocol.h"
#include "si_runtime_guard.h"
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define DP_OFF 264u
#define INFO_SIZE 200u
#define RECORD_SIZE 208u
static uint32_t get(const void *p, unsigned off) { return *(const uint32_t *)((const char *)p+off); }
static void put(void *p, unsigned off, uint32_t v) { *(uint32_t *)((char *)p+off)=v; }
static void *ptr(uint32_t v) { return (void *)(uintptr_t)v; }
static void *data(void *ctx) { return ptr(((struct si_native_context *)ctx)->controller+DP_OFF); }
static void *find(void *dp, uint32_t id) {
    uint32_t n=get(dp,440), array=get(dp,448);
    if (n>100 || (n && !array)) return NULL;
    for (uint32_t i=0;i<n;++i) {
        void *r=ptr(get(ptr(array),i*4));
        if (r && get(r,0)==id) return r;
    }
    return NULL;
}
static void publish(void *ctx) {
    struct si_native_context *c=ctx;
    ((void (*)(void *,void *,int))(uintptr_t)0x183678)(ptr(c->impl),ptr(c->controller+DP_OFF+232),1);
}
static int snapshot(void *ctx, struct si_native_status *s) {
    void *dp=data(ctx);
    s->app=(int32_t)get(dp,1240); s->state=(int32_t)get(dp,1244); s->device=get(dp,1256);
    return 1;
}
static void string_set(void *s, const char *v) {
    /* libutil setString @0x402a8: encoding 3 preserves validated UTF-8;
     * encoding 2 expands each input byte as a separate codepoint. */
    ((void (*)(void *,const char *,int,unsigned,int))(uintptr_t)0x115274)(s,v,3,0,0);
}
static int receiver_available(void) {
    const char *path=getenv("MHI2Q_DIO_OWNER_LOCK");
    if (!path || !*path) path="/mnt/persist/.mhi2q_dio_owner.lock";
    int fd=open(path,O_RDWR);
    if (fd<0) return errno==ENOENT;
    struct flock held; memset(&held,0,sizeof(held));
    held.l_type=F_WRLCK; held.l_whence=SEEK_SET;
    int ok=fcntl(fd,F_GETLK,&held)==0; close(fd);
    /* After an SI restart an old DIO may still own the kernel lock.
     * Reject admission before creating a new native FSM context. Never
     * trust a PID file or let old ASI notifications target a new attempt. */
    return ok && (held.l_type==F_UNLCK ||
        (held.l_pid>1 && (uint32_t)held.l_pid==si_control_owner_pid()));
}
static int upsert(void *ctx, uint32_t *id, const char *key, const char *name) {
    void *dp=data(ctx);
    /* A live record is updated in place; calling addUniqueID on a present
     * identity would deliberately append a random suffix and allocate an ID. */
    void *record=*id?find(dp,*id):NULL;
    if (record) {
        if (get(record,28)!=2) return SI_DENIED;
        string_set((char *)record+36,name); publish(ctx); return SI_OK;
    }
    union { uint64_t alignment; unsigned char bytes[RECORD_SIZE]; } temporary;
    memset(&temporary,0,sizeof(temporary));
    ((void (*)(void *))(uintptr_t)0x169850)(temporary.bytes);
    void *info=temporary.bytes+8;
    *(uint16_t *)((char *)info+8)=0x05ac;
    put(info,20,2); put(info,24,2);
    /* Some legacy init-info readers index one of three USB configuration
     * slots without a transport check. Bound that metadata index to zero;
     * transport=2 and the entry guards prohibit all physical USB operations. */
    put(info,188,0); put(info,192,0);
    string_set((char *)info+28,name); string_set((char *)info+68,key);
    uint32_t native_id=((uint32_t (*)(void *,void *))(uintptr_t)SI_VA_UPSERT)(dp,info);
    if (native_id && (!*id || native_id==*id)) {
        *id=native_id;
        ((void (*)(void *,uint32_t,void *))(uintptr_t)0x16ec90)(dp,native_id,info);
    }
    for (unsigned i=0;i<4;++i)
        ((void (*)(void *))(uintptr_t)0x1153b8)((char *)info+28+40*i);
    if (!native_id || native_id!=*id || !find(dp,*id)) return SI_UNAVAILABLE;
    publish(ctx); return SI_OK;
}
static int remove_phone(void *ctx, uint32_t id) {
    void *dp=data(ctx), *r=find(dp,id);
    if (!r) return SI_OK;
    if (get(r,28)!=2 || ((int32_t)get(dp,1240)!=-1 && get(dp,1256)==id)) return SI_BUSY;
    ((void (*)(void *,uint32_t))(uintptr_t)0x169984)(dp,id);
    publish(ctx); return find(dp,id)?SI_UNAVAILABLE:SI_OK;
}
static int activate(void *ctx, uint32_t id, uint32_t app) {
    struct si_native_context *c=ctx;
    void *dp=data(ctx);
    if (!find(dp,id)) return SI_NOT_FOUND;
    if ((int32_t)get(dp,1240)!=-1 && get(dp,1256)!=id) return SI_BUSY;
    if (!receiver_available()) return SI_BUSY;
    ((void (*)(void *,uint32_t,uint32_t))(uintptr_t)0x165390)(ptr(c->controller),id,app);
    return get(dp,1256)==id && get(dp,1240)==app ? SI_OK:SI_DENIED;
}
static int cancel(void *ctx, uint32_t id) {
    struct si_native_context *c=ctx; void *dp=data(ctx);
    if ((int32_t)get(dp,1240)==-1 || get(dp,1256)!=id) return SI_OK;
    void *state=((void *(*)(void *))(uintptr_t)0x165198)(ptr(c->controller));
    if (!state) return SI_UNAVAILABLE;
    uint32_t vt=get(state,0), fn=get(ptr(vt),32);
    if (!fn) return SI_UNAVAILABLE;
    /* Native onJob_disconnect(deviceID, removal=1): no user deactivation bit. */
    ((void (*)(void *,uint32_t,int))(uintptr_t)fn)(state,id,1);
    return SI_OK;
}
static int alive(void *ctx, uint32_t pid) {
    (void)ctx; return pid && (kill((int)pid,0)==0 || errno==EPERM);
}
static int child_state(void *ctx,uint32_t kind,uint32_t value,uint32_t cookie) {
    struct si_native_context *c=ctx;
    if(kind>SI_ASI_RESTART)return SI_BAD_REQUEST;
    /* The same registered listener and enum conversion used by the stock
     * ASI wrapper. Already on its controller thread: do not enqueue another
     * unbound enum after checking the source generation. */
    void *source=ptr(c->controller+3272+8);
    const uint32_t constructors[]={0x1274b0,0x127440,0x1273cc,0x12735c,0x1272f4};
    void *event=((void *(*)(void))(uintptr_t)constructors[kind])();
    if(!event)return SI_UNAVAILABLE;
    put(event,8,get(source,8));put(event,16,get(source,16));put(event,20,get(source,20));
    if(kind==SI_ASI_STATE)put(event,28,((uint32_t (*)(uint32_t))(uintptr_t)0x170cc4)(value));
    if(kind==SI_ASI_WATCHDOG){*(uint16_t *)((char *)event+26)=(uint16_t)cookie;put(event,28,value);}
    if(kind==SI_ASI_FLUSH)*((uint8_t *)event+25)=(uint8_t)value;
    /* A zero token is the stock broadcast-to-AO route, not an error. The
     * native execute function resolves both forms without another queue. */
    ((void (*)(void *))(uintptr_t)get(ptr(get(event,0)),8))(event);
    ((void (*)(void *))(uintptr_t)get(ptr(get(event,0)),4))(event);
    return SI_OK;
}
const struct si_native_ops si_native_operations={snapshot,upsert,remove_phone,activate,cancel,alive,child_state};

int si_native_child_transport(void) {
    uint32_t controller=get(ptr(SI_CONTROLLER_PTR),0);
    if (!controller) return -1;
    void *dp=ptr(controller+DP_OFF);
    if (get(dp,1240)!=0) return -1;
    uint32_t id=get(dp,1256); void *r=find(dp,id);
    if (r && get(r,28)==1) return 0;
    if (si_control_wireless(id) || (r && get(r,28)==2)) return 1;
    return -1;
}

static int wireless_handler(void *handler, uint32_t *id) {
    void *dp=ptr(get(handler,40)); *id=get(dp,1256);
    void *r=find(dp,*id);
    return si_control_wireless(*id) || (r && get(r,28)==2);
}
static void event(void *handler, uint32_t state) {
    ((void (*)(void *,uint32_t *))(uintptr_t)0x12584c)((char *)handler+248,&state);
}
static void reset_jobs(void *h, uint32_t pending) {
    put(h,48,pending); put(h,52,UINT32_MAX);
    for (unsigned off=56;off<=76;off+=4) put(h,off,UINT32_MAX);
    put(h,80,0);
}
int32_t si_native_prepare(void *h, uint32_t retry, uint32_t unused) {
    (void)unused; uint32_t id;
    if (!wireless_handler(h,&id)) return 0;
    /* A WLAN record with no current adapter admission must never fall
     * through into stock USB switching. Emit native error state instead. */
    if (!receiver_available() || !si_control_prepare(id)) { event(h,11); return 1; }
    void *dp=ptr(get(h,40)); put(dp,1272,0);
    reset_jobs(h,retry?3u:1u);
    if (!retry) event(h,16);
    ((void (*)(void *,int))(uintptr_t)0x17f2b8)(h,1);
    return 1;
}
int32_t si_native_cleanup(void *h, uint32_t error, uint32_t port_reset) {
    (void)error; (void)port_reset; uint32_t id;
    if (!wireless_handler(h,&id)) return 0;
    reset_jobs(h,2); event(h,18);
    ((void (*)(void *,int))(uintptr_t)0x17f2b8)(h,1);
    return 1;
}
int32_t si_native_connect(void *controller, uint32_t device, uint32_t app) {
    struct si_native_context ctx={(uint32_t)(uintptr_t)controller,get(controller,0xcc4)};
    void *dp=data(&ctx), *r=find(dp,device);
    /* Discovery alone cannot launch a receiver. The bootstrap must first
     * admit this authenticated peer with ACTIVATE; ignore premature HMI
     * auto-connect requests without creating a stock FSM context. */
    if (r && get(r,28)==2) return app!=0 || !si_control_prepare(device);
    if (!r || get(r,28)!=1 || app>3 ||
        !((int (*)(void *,uint32_t,uint32_t))(uintptr_t)0x16925c)(dp,app,device) ||
        !((int (*)(void *,uint32_t))(uintptr_t)0x169498)(dp,app) ||
        !((int (*)(void *,uint32_t,uint32_t))(uintptr_t)0x168f8c)(dp,device,app)) return 0;
    return si_control_before_connect(&ctx,device,app);
}
int32_t si_native_carplay_device(void *dp, uint32_t app, uint32_t device) {
    /* Native 169518 ->1691c4 tests connector type/USB slot verification.
     * Coding/FEC/method gates precede it in native onJob_connect. Physical
     * connector discovery cannot validate a WLAN peer. Only a current
     * bootstrap admission for this exact native WLAN record supplies that
     * transport capability; every USB/unadmitted call follows original code. */
    uint32_t controller=get(ptr(SI_CONTROLLER_PTR),0);
    if (!controller || dp!=ptr(controller+DP_OFF) || app!=0) return 0;
    void *r=find(dp,device);
    return r && get(r,28)==2 && si_control_prepare(device);
}
