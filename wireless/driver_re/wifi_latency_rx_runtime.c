/* Pinned driver integration candidate. Never writes original table padding.
 * Requires build-time complete driver/stack fingerprint, full-entry wrappers,
 * root's real recursive PI mutex, module-lifetime RW cookie/sidecar storage,
 * and loader refusal to unload until wl_rx_runtime_quiescent().
 */
#include "wifi_latency_rx_runtime.h"
#include "wifi_latency_policy.h"
#include "rx_timer_churn_policy.h"

#define RX_ENTRIES 32u
#define RX_COOKIES 128u
#define VA_LOOKUP 0x46bd4u
#define CO_CAPTURED 8u
struct rx_entry {
    uint32_t used, generation;
    void *table, *priv, *adapter;
    uint32_t policy[RXTC_STATE_WORDS];
    struct wl_rx_age age;
    struct rx_cookie *current;
};
struct rx_cookie {
    uint32_t callout[6]; /* pinned io-pkt callout_init writes exactly24B */
    struct rx_entry *entry;
    uint32_t generation, epoch, used;
};
struct rx_frame {
    struct rx_frame *prev;
    void *priv, *packet;
    uint32_t sequence, arrived, deferred_generation;
    struct rx_entry *deferred;
};
static struct rx_entry entries[RX_ENTRIES];
static struct rx_cookie cookies[RX_COOKIES];
static struct rx_frame *frame;
static struct wl_rx_runtime_stats stats;
static _Atomic uint32_t ever_enrolled;

#ifdef WL_RX_RUNTIME_TEST
extern void wl_test_co_init(void *);
extern void wl_test_co_reset(void *, int, void (*)(void *), void *);
extern void wl_test_co_stop(void *);
extern void *wl_test_lookup(void *, uint32_t, const void *);
#define co_init wl_test_co_init
#define co_reset wl_test_co_reset
#define co_stop wl_test_co_stop
#define lookup wl_test_lookup
#else
_Static_assert(sizeof(void *) == 4, "pinned ARM32 only");
#define co_init ((void (*)(void *))(uintptr_t)0x1af650u)
#define co_reset ((void (*)(void *, int, void (*)(void *), void *))(uintptr_t)0x1af6b8u)
#define co_stop ((void (*)(void *))(uintptr_t)0x1af95cu)
static void *lookup(void *priv, uint32_t tid, const void *peer) {
    return ((void *(*)(void *, uint32_t, const void *))(wl_native_base()+VA_LOOKUP))(priv,tid,peer);
}
#endif
static uint32_t rd(void *p, uint32_t off) { return *(uint32_t *)((uint8_t *)p+off); }
static void *ptr(void *p, uint32_t off) { return (void *)(uintptr_t)rd(p,off); }
static void byte(void *p, uint32_t off, uint8_t v) { *((uint8_t *)p+off)=v; }
static int32_t delta(uint32_t a,uint32_t b) { return (int32_t)(a-b); }
static struct rx_entry *find(void *table) {
    for(uint32_t i=0;i<RX_ENTRIES;++i) if(entries[i].used && entries[i].table==table) return &entries[i];
    return 0;
}
static void native_timer(struct rx_entry *e, uint32_t start) {
    void *adapter=e->adapter;
    void *timer=ptr(e->table,32);
    if(start) {
        typedef uint32_t (*fn)(void *,void *,uint32_t,uint32_t);
        ((fn)ptr(adapter,106*4))(ptr(adapter,0),timer,0,50);
        byte(e->table,36,1);
    } else {
        typedef uint32_t (*fn)(void *,void *);
        ((fn)ptr(adapter,107*4))(ptr(adapter,0),timer);
        byte(e->table,36,0);
    }
}
/* Global PI mutex held. callout_stop serializes capture under callout_mtx.
 * Once it returns, clearedbit8 means no captured callback can still appear;
 * setbit8 keeps stablecookie until callback releasesit. No stop return ACK.
 */
static void cancel(struct rx_entry *e) {
    struct rx_cookie *c=e->current;
    if(!c) return;
    e->current=0;
    co_stop(c->callout);
    if(c->callout[5]&CO_CAPTURED) ++stats.cancel_captured;
    else {c->used=0; ++stats.cancel_queued;}
}
static void retire(struct rx_entry *e) {
    cancel(e);
    uint32_t in[RXTC_INPUT_WORDS]={RXTC_DELETE,wl_native_ticks(),0,0,e->generation,0,0,0};
    rx_timer_churn_policy(e->policy,in);
    wl_rx_age_retire(&e->age,e->generation);
    e->used=0;
}
/* Caller may resume originalRX timer tail when start_native==0. */
static void fallback(struct rx_entry *e,int start_native) {
    ++stats.fallback;
    byte(e->table,36,0);
    if(start_native) native_timer(e,1);
    retire(e);
}
static int snapshot(struct rx_entry *e,uint32_t *start,uint32_t *window,uint32_t slots[64]) {
    void *adapter=e->adapter;
    void *lock=ptr(e->priv,1533*4);
    typedef uint32_t(*lockfn)(void *,void *);
    lockfn lock_fn=(lockfn)ptr(adapter,110*4), unlock_fn=(lockfn)ptr(adapter,111*4);
    if(!lock_fn||!unlock_fn||!lock) return 0;
    lock_fn(ptr(adapter,0),lock);
    *start=rd(e->table,20); *window=rd(e->table,24);
    uint32_t *native=ptr(e->table,28);
    int valid=*start<4096 && *window && *window<=64 && native;
    if(valid) for(uint32_t i=0;i<*window;++i) slots[i]=native[i];
    unlock_fn(ptr(adapter,0),lock);
    return valid;
}
static int observe(struct rx_entry *e,uint32_t incoming,uint32_t seq,uint32_t arrived,
                   struct wl_rx_age_result *out) {
    uint32_t slots[64],start,window;
    if(!snapshot(e,&start,&window,slots)) return 0;
    return wl_rx_age_observe(&e->age,e->generation,wl_native_ticks(),start,slots,window,
                            incoming,seq,arrived,out)==WL_OK;
}
static void expiry(void *arg);
static int arm(struct rx_entry *e) {
    struct rx_cookie *c=0;
    for(uint32_t i=0;i<RX_COOKIES;++i) if(!cookies[i].used) {c=&cookies[i];break;}
    if(!c) {++stats.pool_exhausted;return 0;}
    int32_t remaining=delta(e->policy[RXTC_PHYSICAL_DUE],wl_native_ticks());
    if(remaining<=0) return -1;
    if(remaining>6) return 0;
    c->used=1;c->entry=e;c->generation=e->generation;c->epoch=e->policy[RXTC_ARM_EPOCH];
    co_init(c->callout);
    e->current=c;
    co_reset(c->callout,remaining,expiry,c);
    ++stats.arm;
    return 1;
}
/* Returns1 success;2 requestflush;0 explicitstock fallback needed. */
static int action(struct rx_entry *e,uint32_t event,uint32_t progressed,
                  uint32_t incoming,uint32_t sequence,uint32_t arrived,uint32_t epoch) {
    struct wl_rx_age_result age;
    if(e->policy[RXTC_ARM_EPOCH]==UINT32_MAX) return 0; /* no epochalias with retainedcookies */
    if(!observe(e,incoming,sequence,arrived,&age)) {++stats.invalid;return 0;}
    uint32_t in[RXTC_INPUT_WORDS]={event,wl_native_ticks(),age.pending!=0,progressed,e->generation,
                                 age.valid,age.due,epoch};
    uint32_t a=rx_timer_churn_policy(e->policy,in);
    if(a&RXTC_IGNORE) return 1;
    if(a&RXTC_STOP) cancel(e);
    byte(e->table,36,(uint8_t)e->policy[RXTC_ARMED]);
    if(a&RXTC_FLUSH) return 2;
    if(a&RXTC_ARM) {
        int armed=arm(e);
        if(armed<0) {e->policy[RXTC_ARMED]=0;e->policy[RXTC_VIRTUAL_VALID]=0;return 2;}
        if(!armed) return 0;
    }
    return 1;
}
static void expiry(void *arg) {
    struct rx_cookie *c=arg;
    if(wl_lock()!=0) return; /* retain cookie forever on brokenmutex, never deref table */
    struct rx_entry *e=c->entry;
    ++stats.callbacks;
    if(!c->used||!e->used||e->generation!=c->generation||e->current!=c||
       e->policy[RXTC_ARM_EPOCH]!=c->epoch||!e->policy[RXTC_ARMED]) {
        ++stats.stale_callback;c->used=0;wl_unlock();return;
    }
    e->current=0;
    int result=action(e,RXTC_EXPIRY,0,0,0,0,c->epoch);
    if(result==2) {++stats.forced_flush;wl_orig_flush((uint8_t *)e->table+32);}
    else if(!result) fallback(e,1);
    c->used=0;
    wl_unlock();
}
static void enroll(void *priv,void *table) {
    if(!table||find(table)||*((uint8_t *)table+36)) return;
    struct rx_entry *e=0;
    for(uint32_t i=0;i<RX_ENTRIES;++i)
        if(!entries[i].used && entries[i].policy[RXTC_GENERATION]<UINT32_MAX-2 &&
           entries[i].policy[RXTC_ARM_EPOCH]<UINT32_MAX) {e=&entries[i];break;}
    if(!e) {++stats.pool_exhausted;return;}
    e->table=table;e->priv=priv;e->adapter=ptr(priv,0);e->current=0;
    uint32_t start,window,slots[64];
    if(!snapshot(e,&start,&window,slots)) return;
    for(uint32_t i=0;i<window;++i) if(slots[i]) return; /* never migrate queued native timers */
    uint32_t in[RXTC_INPUT_WORDS]={RXTC_CREATE,wl_native_ticks(),0,0,0,0,0,0};
    if(rx_timer_churn_policy(e->policy,in)&RXTC_IGNORE) return;
    e->generation=e->policy[RXTC_GENERATION];
    if(wl_rx_age_create(&e->age,e->generation)!=WL_OK) return;
    e->used=1;atomic_store_explicit(&ever_enrolled,1,memory_order_relaxed);++stats.enroll;
}
int wl_rx_runtime_receive(void *priv,uint32_t seq,uint32_t tid,const void *peer,uint32_t type,void *packet) {
    if(wl_lock()!=0) return wl_orig_rx(priv,seq,tid,peer,type,packet);
    struct rx_frame f={frame,priv,packet,seq,wl_native_ticks(),0,0};
    frame=&f;
    int result=wl_orig_rx(priv,seq,tid,peer,type,packet);
    if(f.deferred&&f.deferred->used&&f.deferred->generation==f.deferred_generation) {
        ++stats.forced_flush;wl_orig_flush((uint8_t *)f.deferred->table+32);
    }
    frame=f.prev;
    wl_unlock();return result;
}
uint32_t wl_rx_runtime_tail(void *table,uint32_t oldstart,void *adapter) {
    (void)adapter;
    if(!frame) return 0;
    struct rx_entry *e=find(table);
    if(!e||frame->priv!=e->priv) return 0;
    int result=action(e,RXTC_RX,oldstart!=rd(table,20),(uint32_t)(uintptr_t)frame->packet,
                      frame->sequence,frame->arrived,0);
    if(!result) {fallback(e,0);return 0;}
    if(result==2) {frame->deferred=e;frame->deferred_generation=e->generation;}
    return 1;
}
int wl_rx_runtime_create(void *priv,void *cmd,const void *event) {
    if(wl_lock()!=0) return wl_orig_create(priv,cmd,event);
    int result=wl_orig_create(priv,cmd,event);
    if(event) {
        const uint8_t *p=event;
        uint32_t tid=(((uint32_t)p[8]|((uint32_t)p[9]<<8))>>2)&15;
        enroll(priv,lookup(priv,tid,p+1));
    }
    wl_unlock();return result;
}
void wl_rx_runtime_delete(void *priv,void *table) {
    if(wl_lock()!=0) {wl_orig_delete(priv,table);return;}
    struct rx_entry *e=find(table);
    if(e) {retire(e);byte(table,36,0);}
    wl_orig_delete(priv,table);
    wl_unlock();
}
void wl_rx_runtime_flush(void *ctx) {
    if(wl_lock()!=0) {
        /* Root preparation fails uniformly before firstenrollment and never
         * flips readiness later. Preserve stocktimers in that initialfallback.
         * Unexpected mutexfailure afterenrollment retains state instead.
         */
        if(!atomic_load_explicit(&ever_enrolled,memory_order_relaxed)) wl_orig_flush(ctx);
        return;
    }
    void *table=(uint8_t *)ctx-32;
    if(!find(table)) wl_orig_flush(ctx); /* only unmanaged native timer owns this path */
    else ++stats.stale_callback;
    wl_unlock();
}
int wl_rx_runtime_quiescent(void) {
    if(wl_lock()!=0) return 0;
    int quiet=1;
    for(uint32_t i=0;i<RX_ENTRIES;++i) if(entries[i].used) quiet=0;
    for(uint32_t i=0;i<RX_COOKIES;++i) if(cookies[i].used) quiet=0;
    wl_unlock();return quiet;
}
void wl_rx_runtime_stats(struct wl_rx_runtime_stats *out) {
    if(!out||wl_lock()!=0) return;
    *out=stats;
    wl_unlock();
}
