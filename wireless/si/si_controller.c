#include "si_controller.h"
#include <pthread.h>
#include <string.h>
#include <time.h>

struct phone {
    char key[SI_PHONE_KEY_SIZE], name[SI_PHONE_NAME_SIZE];
    uint32_t id, daemon, link, session, child;
    uint32_t state_sequence;
    int present, removing, media_cancelled;
    uint64_t seen_ms;
};
struct transaction {
    int state; /* 0 free, 1 queued, 2 executing, 3 retained result */
    uint64_t serial;
    struct si_request request;
    struct si_reply reply;
};
static struct {
    uint64_t epoch, serial;
    uint32_t generation, active, wired_pending, wired_app, daemon_epoch;
    struct si_native_ops ops;
    struct phone phones[SI_CONTROL_PHONES];
    struct transaction requests[SI_CONTROL_REQUESTS];
    struct { uint32_t epoch, high; } daemons[8];
    struct si_native_status native;
    struct si_media_target ended;
    int media_pending;
} control;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
#define SI_CHILD_EVENT 8u
uint64_t si_control_epoch(void) { return control.epoch; }

static uint64_t now_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return (uint64_t)t.tv_sec*1000+(unsigned)t.tv_nsec/1000000;
}
static struct phone *phone_by_key(const char *key) {
    for (unsigned i=0;i<SI_CONTROL_PHONES;++i)
        if (control.phones[i].key[0] && !strcmp(control.phones[i].key,key)) return &control.phones[i];
    return NULL;
}
static struct phone *phone_by_id(uint32_t id) {
    if (id) for (unsigned i=0;i<SI_CONTROL_PHONES;++i)
        if (control.phones[i].id==id) return &control.phones[i];
    return NULL;
}
static void fill(struct si_reply *r, struct phone *p) {
    r->si_epoch=control.epoch; r->app=control.native.app;
    r->app_state=control.native.state; r->active_device=control.native.device;
    if (p) {
        r->device_id=p->id; r->session_generation=p->session;
        r->child_pid=p->child; r->present=(uint32_t)p->present;
    }
}
void si_control_init(uint64_t epoch, const struct si_native_ops *ops) {
    pthread_mutex_lock(&lock);
    memset(&control,0,sizeof(control)); control.epoch=epoch;
    control.ops=*ops; control.native.app=-1;
    pthread_mutex_unlock(&lock);
}

int si_control_submit(const struct si_request *r, unsigned *slot, struct si_reply *reply) {
    pthread_mutex_lock(&lock);
    memset(reply,0,sizeof(*reply)); reply->operation=r->operation; reply->request_id=r->request_id;
    fill(reply,phone_by_key(r->phone_key));
    if (r->operation<SI_HELLO || r->operation>SI_DETACH_PEER) {
        reply->result=SI_BAD_REQUEST; pthread_mutex_unlock(&lock); return 0;
    }
    if (r->operation!=SI_HELLO && (r->si_epoch!=control.epoch || r->daemon_epoch!=control.daemon_epoch)) {
        reply->result=SI_STALE; pthread_mutex_unlock(&lock); return 0;
    }
    for (unsigned i=0;i<SI_CONTROL_REQUESTS;++i) {
        struct transaction *t=&control.requests[i];
        if (t->state && t->request.operation!=SI_CHILD_EVENT && t->request.daemon_epoch==r->daemon_epoch &&
            t->request.request_id==r->request_id) {
            if (memcmp(&t->request,r,sizeof(*r))) reply->result=SI_BAD_REQUEST;
            else { *reply=t->reply; if (t->state!=3) reply->result=SI_PENDING; }
            *slot=i; pthread_mutex_unlock(&lock); return 0;
        }
    }
    unsigned daemon=8;
    for (unsigned i=0;i<8;++i) if (control.daemons[i].epoch==r->daemon_epoch) { daemon=i; break; }
    if (r->operation==SI_HELLO && daemon!=8 && control.daemon_epoch && r->daemon_epoch!=control.daemon_epoch) {
        reply->result=SI_STALE; pthread_mutex_unlock(&lock); return 0;
    }
    if (daemon==8) for (unsigned i=0;i<8;++i) if (!control.daemons[i].epoch) { daemon=i; break; }
    if (daemon==8 || (control.daemons[daemon].epoch && r->request_id<=control.daemons[daemon].high)) {
        reply->result=daemon==8?SI_BUSY:SI_STALE; pthread_mutex_unlock(&lock); return 0;
    }
    unsigned chosen=SI_CONTROL_REQUESTS; uint64_t oldest=UINT64_MAX;
    for (unsigned i=0;i<SI_CONTROL_REQUESTS;++i) {
        struct transaction *t=&control.requests[i];
        if (!t->state) { chosen=i; break; }
        if (t->state==3 && t->serial<oldest) { oldest=t->serial; chosen=i; }
    }
    if (chosen==SI_CONTROL_REQUESTS) { reply->result=SI_BUSY; pthread_mutex_unlock(&lock); return 0; }
    struct transaction *t=&control.requests[chosen];
    memset(t,0,sizeof(*t)); t->request=*r; t->reply=*reply;
    t->reply.result=SI_PENDING; t->serial=++control.serial; t->state=1;
    control.daemons[daemon].epoch=r->daemon_epoch; control.daemons[daemon].high=r->request_id;
    *slot=chosen; *reply=t->reply;
    pthread_mutex_unlock(&lock); return 1;
}
void si_control_reject(unsigned slot) {
    pthread_mutex_lock(&lock);
    if (slot<SI_CONTROL_REQUESTS && control.requests[slot].state==1) {
        control.requests[slot].state=3;
        control.requests[slot].reply.result=SI_UNAVAILABLE;
    }
    pthread_mutex_unlock(&lock);
}
void si_control_result(unsigned slot, struct si_reply *r) {
    pthread_mutex_lock(&lock);
    if (slot<SI_CONTROL_REQUESTS) *r=control.requests[slot].reply;
    else { memset(r,0,sizeof(*r)); r->result=SI_BAD_REQUEST; }
    pthread_mutex_unlock(&lock);
}

int si_control_submit_child_state(const struct si_child_state *s,unsigned *slot,struct si_reply *r) {
    pthread_mutex_lock(&lock);
    memset(r,0,sizeof(*r));r->operation=SI_CHILD_EVENT;r->request_id=s->sequence;
    struct phone *p=phone_by_id(control.active);fill(r,p);r->result=SI_STALE;
    if(!p||s->epoch!=control.epoch||s->child!=p->child||s->session!=p->session||
       s->link!=p->link||s->kind>SI_ASI_RESTART||!s->sequence||!control.ops.child_state)goto rejected;
    for(unsigned i=0;i<SI_CONTROL_REQUESTS;++i) {
        struct transaction *t=&control.requests[i];
        if(t->state&&t->request.operation==SI_CHILD_EVENT&&t->request.child_pid==s->child&&
           t->request.session_generation==s->session&&t->request.si_epoch==s->epoch&&
           t->request.state_sequence==s->sequence) {
            if(t->request.native_state!=s->state||t->request.link_generation!=s->link||
               t->request.event_kind!=s->kind||t->request.event_cookie!=s->cookie)r->result=SI_BAD_REQUEST;
            else {*r=t->reply;if(t->state!=3)r->result=SI_PENDING;}
            *slot=i;goto rejected;
        }
    }
    if(s->sequence<=p->state_sequence)goto rejected;
    unsigned chosen=SI_CONTROL_REQUESTS;uint64_t oldest=UINT64_MAX;
    for(unsigned i=0;i<SI_CONTROL_REQUESTS;++i) {
        struct transaction *t=&control.requests[i];
        if(!t->state){chosen=i;break;}
        if(t->state==3&&t->serial<oldest){oldest=t->serial;chosen=i;}
    }
    if(chosen==SI_CONTROL_REQUESTS){r->result=SI_BUSY;goto rejected;}
    struct transaction *t=&control.requests[chosen];memset(t,0,sizeof(*t));
    t->request.operation=SI_CHILD_EVENT;t->request.si_epoch=s->epoch;
    t->request.daemon_epoch=p->daemon;t->request.link_generation=s->link;
    t->request.session_generation=s->session;t->request.child_pid=s->child;
    t->request.native_state=s->state;t->request.state_sequence=s->sequence;
    t->request.event_kind=s->kind;t->request.event_cookie=s->cookie;
    memcpy(t->request.phone_key,p->key,sizeof(p->key));
    t->reply=*r;t->reply.result=SI_PENDING;t->serial=++control.serial;t->state=1;
    *slot=chosen;*r=t->reply;pthread_mutex_unlock(&lock);return 1;
rejected:
    pthread_mutex_unlock(&lock);return 0;
}

/* Only the controller thread calls execute/tick and native operations. Never
 * hold the bookkeeping mutex across a native call: prepare and spawn reenter. */
int si_control_media_target(struct si_media_target *out) {
    pthread_mutex_lock(&lock);
    struct phone *p=phone_by_id(control.active);
    int valid=p && p->child>1 && !p->media_cancelled && control.native.app==0 && control.native.device==p->id;
    if(valid){out->epoch=control.epoch;out->device=p->id;out->session=p->session;out->link=p->link;out->child=p->child;}
    pthread_mutex_unlock(&lock);return valid;
}
void si_control_media_ended(const struct si_media_target *target) {
    pthread_mutex_lock(&lock);control.ended=*target;control.media_pending=1;pthread_mutex_unlock(&lock);
}
void si_control_tick(void *ctx) {
    struct si_native_status state;
    if (!control.ops.snapshot(ctx,&state)) return;
    pthread_mutex_lock(&lock); control.native=state;
    struct phone *active=phone_by_id(control.active);
    uint32_t pid=active?active->child:0;
    pthread_mutex_unlock(&lock);
    int alive=pid && control.ops.alive(ctx,pid);
    pthread_mutex_lock(&lock);
    if (active && state.app==-1 && !alive) {
        active->child=0; control.active=0;
    }
    uint32_t media_cancel=0;
    if(control.media_pending) {
        struct si_media_target t=control.ended;control.media_pending=0;
        if(active && !active->media_cancelled && t.epoch==control.epoch && t.device==control.active &&
           t.session==active->session && t.link==active->link && t.child==active->child &&
           state.app==0 && state.device==active->id) {
            active->media_cancelled=1;media_cancel=active->id;
        }
    }
    uint32_t wired=0, app=0;
    if (!control.active && state.app==-1 && control.wired_pending) {
        wired=control.wired_pending; app=control.wired_app; control.wired_pending=0;
    }
    pthread_mutex_unlock(&lock);
    for (unsigned i=0;i<SI_CONTROL_PHONES;++i) {
        struct phone *p=&control.phones[i];
        pthread_mutex_lock(&lock);
        uint32_t id=p->removing && p->id!=state.device?p->id:0;
        pthread_mutex_unlock(&lock);
        if (id && control.ops.remove(ctx,id)==SI_OK) {
            pthread_mutex_lock(&lock); p->present=0; p->removing=0; pthread_mutex_unlock(&lock);
        }
    }
    if(media_cancel)(void)control.ops.cancel(ctx,media_cancel);
    if (wired) (void)control.ops.activate(ctx,wired,app);
}

void si_control_execute(unsigned slot, void *ctx) {
    if (slot>=SI_CONTROL_REQUESTS) return;
    pthread_mutex_lock(&lock);
    struct transaction *t=&control.requests[slot];
    if (t->state!=1) { pthread_mutex_unlock(&lock); return; }
    t->state=2; struct si_request r=t->request;
    pthread_mutex_unlock(&lock);
    si_control_tick(ctx);
    pthread_mutex_lock(&lock);
    struct phone *p=phone_by_key(r.phone_key);
    unsigned result=SI_OK;
    if(r.operation==SI_CHILD_EVENT) {
        /* Revalidate at delivery, not merely when the socket accepted it.
         * Native controller code may synchronously reenter our callbacks. */
        int valid=p&&control.active==p->id&&r.si_epoch==control.epoch&&
            p->session==r.session_generation&&p->link==r.link_generation&&
            p->child==r.child_pid&&control.native.app==0&&control.native.device==p->id&&
            r.state_sequence>p->state_sequence&&control.ops.child_state;
        if(valid)p->state_sequence=r.state_sequence;
        pthread_mutex_unlock(&lock);
        result=valid?(unsigned)control.ops.child_state(ctx,r.event_kind,r.native_state,r.event_cookie):SI_STALE;
        pthread_mutex_lock(&lock);t->state=3;t->reply.result=result;fill(&t->reply,p);
        pthread_mutex_unlock(&lock);return;
    }
    if (r.operation!=SI_HELLO && (r.si_epoch!=control.epoch || r.daemon_epoch!=control.daemon_epoch)) result=SI_STALE;
    else if (r.operation==SI_HELLO) control.daemon_epoch=r.daemon_epoch;
    else if (r.operation==SI_UPSERT_PHONE) {
        if (!p) for (unsigned i=0;i<SI_CONTROL_PHONES;++i) {
            if (!control.phones[i].key[0]) { p=&control.phones[i]; break; }
        }
        if (!p) result=SI_BUSY;
        else if (p->removing || (control.active==p->id && p->daemon &&
                 (p->daemon!=r.daemon_epoch || p->link!=r.link_generation) &&
                 (r.session_generation!=p->session || r.link_generation!=p->link))) result=SI_BUSY;
        else if (p->daemon==r.daemon_epoch && p->link>r.link_generation) result=SI_STALE;
    } else if (r.operation!=SI_HELLO && r.operation!=SI_SNAPSHOT && !p) result=SI_NOT_FOUND;
    else if (p && r.operation!=SI_HELLO && r.operation!=SI_SNAPSHOT &&
             (p->daemon!=r.daemon_epoch || p->link!=r.link_generation)) result=SI_STALE;
    if (result==SI_OK && p && r.operation!=SI_UPSERT_PHONE) p->seen_ms=now_ms();
    uint32_t id=p?p->id:0;
    if (result==SI_OK && r.operation==SI_ACTIVATE) {
        if (!p->present || p->removing || control.wired_pending) result=SI_UNAVAILABLE;
        else if (control.native.app!=-1 && control.native.device!=id) result=SI_BUSY;
        else if (control.active==id && p->session) {
            if (r.session_generation && r.session_generation!=p->session) result=SI_STALE;
        } else if (r.session_generation) result=SI_STALE;
        else if (control.generation==UINT32_MAX) result=SI_UNAVAILABLE;
        else { p->session=++control.generation; p->media_cancelled=0; control.active=id; }
    }
    if (result==SI_OK && p && (r.operation==SI_CANCEL || r.operation==SI_REMOVE_PHONE || r.operation==SI_DETACH_PEER) &&
        r.session_generation!=p->session) result=SI_STALE;
    int already=control.native.app!=-1 && control.native.device==id;
    pthread_mutex_unlock(&lock);
    if (result==SI_OK) {
        switch (r.operation) {
        case SI_UPSERT_PHONE:
            result=control.ops.upsert(ctx,&id,r.phone_key,r.name);
            if (result==SI_OK) {
                pthread_mutex_lock(&lock);
                memcpy(p->key,r.phone_key,sizeof(p->key)); memcpy(p->name,r.name,sizeof(p->name));
                p->id=id; p->present=1; p->daemon=r.daemon_epoch; p->link=r.link_generation; p->seen_ms=now_ms();
                pthread_mutex_unlock(&lock);
            }
            break;
        case SI_ACTIVATE:
            if (!already) result=control.ops.activate(ctx,id,0);
            if (result!=SI_OK) { pthread_mutex_lock(&lock); if (control.active==id) control.active=0; pthread_mutex_unlock(&lock); }
            break;
        case SI_CANCEL:
        case SI_REMOVE_PHONE:
        case SI_DETACH_PEER:
            if (r.operation==SI_DETACH_PEER && already &&
                (control.native.state==3 || control.native.state==5)) {
                pthread_mutex_lock(&lock); p->removing=2; pthread_mutex_unlock(&lock);
                break; /* Daemon observed active IP media: retain before/after the native SERVICE_STARTED notification. */
            }
            if (already) {
                if (r.operation!=SI_CANCEL) { pthread_mutex_lock(&lock); p->removing=1; pthread_mutex_unlock(&lock); }
                result=control.ops.cancel(ctx,id);
            } else if (r.operation!=SI_CANCEL) {
                result=control.ops.remove(ctx,id);
                if (result==SI_OK) { pthread_mutex_lock(&lock); p->present=0; pthread_mutex_unlock(&lock); }
            }
            break;
        default: break;
        }
    }
    si_control_tick(ctx);
    pthread_mutex_lock(&lock);
    t->reply.result=result; fill(&t->reply,p); t->state=3;
    pthread_mutex_unlock(&lock);
}
int si_control_wireless(uint32_t id) {
    pthread_mutex_lock(&lock); struct phone *p=phone_by_id(id);
    int yes=p!=NULL; pthread_mutex_unlock(&lock); return yes;
}
int si_control_prepare(uint32_t id) {
    pthread_mutex_lock(&lock); struct phone *p=phone_by_id(id);
    int yes=p && p->present && !p->removing && !control.wired_pending &&
        now_ms()-p->seen_ms<30000 && control.active==id && p->session;
    pthread_mutex_unlock(&lock); return yes;
}
int si_control_before_connect(void *ctx, uint32_t device, uint32_t app) {
    si_control_tick(ctx);
    pthread_mutex_lock(&lock);
    uint32_t current=control.active;
    int preempt=current && current!=device && !phone_by_id(device);
    if (preempt && !control.wired_pending) { control.wired_pending=device; control.wired_app=app; }
    pthread_mutex_unlock(&lock);
    if (preempt) (void)control.ops.cancel(ctx,current);
    return preempt;
}
int si_control_child(uint32_t *session, uint32_t *link) {
    pthread_mutex_lock(&lock); struct phone *p=phone_by_id(control.active);
    int yes=p && p->present && !p->removing && !control.wired_pending;
    if (yes) { *session=p->session; *link=p->link; }
    pthread_mutex_unlock(&lock); return yes;
}
void si_control_child_started(uint32_t session, uint32_t pid) {
    pthread_mutex_lock(&lock); struct phone *p=phone_by_id(control.active);
    if (p && p->session==session) {
        if(p->child!=pid)p->state_sequence=0;
        p->child=pid;p->media_cancelled=0;
    }
    pthread_mutex_unlock(&lock);
}
uint32_t si_control_owner_pid(void) {
    pthread_mutex_lock(&lock); struct phone *p=phone_by_id(control.active);
    uint32_t pid=p?p->child:0; pthread_mutex_unlock(&lock); return pid;
}
