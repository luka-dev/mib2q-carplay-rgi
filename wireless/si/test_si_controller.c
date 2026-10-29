/* Execute the actual parser/ledger/policy with observable native-operation
 * boundaries. Native ARM helpers and the physical HU are separate checks. */
#include "si_controller.h"
#include "si_envv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct mock { struct si_native_status s; unsigned adds, removes, starts, stops, child_events; int child_alive; };
static int snap(void *v,struct si_native_status *s) { *s=((struct mock *)v)->s; return 1; }
static int upsert(void *v,uint32_t *id,const char *key,const char *name) {
    struct mock *m=v; assert(*key && *name); ++m->adds; if (!*id) *id=100; return SI_OK;
}
static int remove_phone(void *v,uint32_t id) { assert(id==100); ++((struct mock *)v)->removes; return SI_OK; }
static int start(void *v,uint32_t id,uint32_t app) {
    struct mock *m=v;
    if (id==100) assert(si_control_prepare(id));
    m->s.app=(int32_t)app; m->s.state=16; m->s.device=id; ++m->starts; return SI_OK;
}
static int stop(void *v,uint32_t id) {
    struct mock *m=v; assert(m->s.device==id); ++m->stops;
    m->s.app=-1; m->s.state=0; m->s.device=0; return SI_OK;
}
static int alive(void *v,uint32_t pid) { assert(pid==1234||pid==1235); return ((struct mock *)v)->child_alive; }
static int child_state(void *v,uint32_t kind,uint32_t state,uint32_t cookie) {
    (void)kind;(void)cookie;
    struct mock *m=v;++m->child_events;m->s.state=(int32_t)state;return SI_OK;
}
static const struct si_native_ops ops={snap,upsert,remove_phone,start,stop,alive,child_state};
static uint32_t serial;
static struct si_request request(unsigned op) {
    struct si_request r; memset(&r,0,sizeof(r));
    r.operation=(uint8_t)op; r.request_id=++serial; r.si_epoch=0x1122334455667788ULL;
    r.daemon_epoch=7; r.link_generation=3;
    strcpy(r.phone_key,"bt:01:02:03:04:05:06"); strcpy(r.name,"iPhone"); return r;
}
static struct si_reply execute(struct mock *m,struct si_request *r) {
    struct si_reply reply; unsigned slot;
    if (si_control_submit(r,&slot,&reply)) { si_control_execute(slot,m); si_control_result(slot,&reply); }
    return reply;
}
static void init(struct mock *m) {
    memset(m,0,sizeof(*m)); m->s.app=-1; serial=0;
    si_control_init(0x1122334455667788ULL,&ops);
    struct si_request hello=request(SI_HELLO); hello.si_epoch=0;
    assert(execute(m,&hello).result==SI_OK);
}
static void put32(unsigned char *p,uint32_t n) { for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i)); }
static void parser(void) {
    unsigned char f[SI_WIRE_SIZE]={0}, response[SI_WIRE_SIZE]; struct si_request r;
    memcpy(f,"SIW2",4); f[4]=2; f[5]=SI_UPSERT_PHONE;
    put32(f+8,SI_WIRE_SIZE); put32(f+12,1); put32(f+16,5); put32(f+24,7); put32(f+28,3);
    strcpy((char *)f+32,"bt:01:02:03:04:05:06"); strcpy((char *)f+64,"iPhone");
    assert(si_wire_decode(f,&r));
    for(unsigned i=0;i<32;++i) {
        unsigned char old=f[i];
        if (i<12 || (i>=164)) { f[i]^=0x80; assert(!si_wire_decode(f,&r)); f[i]=old; }
    }
    f[164]=1; assert(!si_wire_decode(f,&r)); f[164]=0;
    f[64]=0xc0; f[65]=0x80; f[66]=0; assert(!si_wire_decode(f,&r));
    f[64]=0xed; f[65]=0xa0; f[66]=0x80; f[67]=0; assert(!si_wire_decode(f,&r));
    f[64]=0xf4; f[65]=0x90; f[66]=0x80; f[67]=0x80; f[68]=0; assert(!si_wire_decode(f,&r));
    memset(f+64,0,96); memcpy(f+64,"\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82",8); assert(si_wire_decode(f,&r));
    struct si_reply reply={0}; reply.result=SI_STALE; reply.app=-1; reply.si_epoch=0x1122334455667788ULL;
    si_wire_reply(&reply,response); assert(!memcmp(response,"SIR2",4)); assert(response[32]==SI_STALE && response[44]==255);
    unsigned x=47426351;
    for(unsigned n=0;n<20000;++n) {
        unsigned char mutated[SI_WIRE_SIZE]; memcpy(mutated,f,sizeof(f));
        x=x*1664525+1013904223; unsigned index=x%SI_WIRE_SIZE;
        x=x*1664525+1013904223; mutated[index]^=(unsigned char)(1+(x%255));
        (void)si_wire_decode(mutated,&r);
    }
}
static void child_state_lifetime(void) {
    struct mock m;init(&m);struct si_request add=request(SI_UPSERT_PHONE),activate=request(SI_ACTIVATE);
    assert(execute(&m,&add).result==SI_OK&&execute(&m,&activate).result==SI_OK);
    m.child_alive=1;si_control_child_started(1,1234);si_control_tick(&m);
    struct si_child_state event={0x1122334455667788ULL,1234,1,3,1,1,0,0},decoded;
    unsigned char frame[SI_CHILD_STATE_SIZE];si_child_state_encode(&event,frame);
    assert(si_child_state_decode(frame,sizeof(frame),&decoded)&&decoded.epoch==event.epoch&&
           decoded.child==event.child&&decoded.session==event.session&&decoded.link==event.link&&
           decoded.sequence==event.sequence&&decoded.state==event.state&&!decoded.kind&&!decoded.cookie);
    for(unsigned length=0;length<sizeof(frame);++length)assert(!si_child_state_decode(frame,length,&decoded));
    frame[168]=1;assert(!si_child_state_decode(frame,sizeof(frame),&decoded));frame[168]=0;
    unsigned slot;struct si_reply reply;
    assert(si_control_submit_child_state(&event,&slot,&reply));
    si_control_execute(slot,&m);si_control_result(slot,&reply);assert(reply.result==SI_OK&&m.child_events==1);
    assert(!si_control_submit_child_state(&event,&slot,&reply)&&reply.result==SI_OK&&m.child_events==1);
    event.state=5;assert(!si_control_submit_child_state(&event,&slot,&reply)&&reply.result==SI_BAD_REQUEST);
    event.state=1;++event.sequence;
    for(unsigned wrong=0;wrong<4;++wrong) {
        struct si_child_state old=event;
        if(wrong==0)++old.epoch;if(wrong==1)++old.link;if(wrong==2)++old.session;if(wrong==3)++old.child;
        assert(!si_control_submit_child_state(&old,&slot,&reply)&&reply.result==SI_STALE);
    }
    assert(si_control_submit_child_state(&event,&slot,&reply));si_control_child_started(1,1235);
    si_control_execute(slot,&m);si_control_result(slot,&reply);assert(reply.result==SI_STALE&&m.child_events==1);
    si_control_child_started(1,1234);++event.sequence;
    assert(si_control_submit_child_state(&event,&slot,&reply));
    /* Deliver after a new attempt, including PID reuse. */
    m.s.app=-1;m.s.device=0;m.child_alive=0;si_control_tick(&m);
    activate=request(SI_ACTIVATE);assert(execute(&m,&activate).session_generation==2);
    si_control_child_started(2,1234);m.child_alive=1;
    si_control_execute(slot,&m);si_control_result(slot,&reply);assert(reply.result==SI_STALE&&m.child_events==1);
    event.session=2;event.sequence=1;
    assert(si_control_submit_child_state(&event,&slot,&reply));m.s.device=1;
    si_control_execute(slot,&m);si_control_result(slot,&reply);assert(reply.result==SI_STALE&&m.child_events==1);
    unsigned random=17;
    for(unsigned i=0;i<20000;++i) {
        unsigned char mutation[SI_CHILD_STATE_SIZE];memcpy(mutation,frame,sizeof(frame));
        random=random*1664525+1013904223;unsigned index=random%sizeof(frame);
        random=random*1664525+1013904223;mutation[index]^=(unsigned char)(1+random%255);
        (void)si_child_state_decode(mutation,sizeof(mutation),&decoded);
    }
    puts("SI bound ASI state: source tuple, duplicate ACK, delayed PID/session/wired handoff and 20000 mutations PASS");
    for(unsigned kind=SI_ASI_STATE;kind<=SI_ASI_RESTART;++kind) {
        init(&m);add=request(SI_UPSERT_PHONE);activate=request(SI_ACTIVATE);
        assert(execute(&m,&add).result==SI_OK&&execute(&m,&activate).result==SI_OK);
        m.child_alive=1;si_control_child_started(1,1234);si_control_tick(&m);
        event.epoch=0x1122334455667788ULL;event.child=1234;event.session=1;event.link=3;
        event.sequence=1;event.kind=kind;event.cookie=kind==SI_ASI_WATCHDOG?65535:0;
        event.state=kind==SI_ASI_STATE?1:kind==SI_ASI_WATCHDOG?3:kind==SI_ASI_FLUSH?255:0;
        si_child_state_encode(&event,frame);assert(si_child_state_decode(frame,sizeof(frame),&decoded));
        assert(decoded.kind==kind&&decoded.cookie==event.cookie&&decoded.state==event.state);
        assert(si_control_submit_child_state(&event,&slot,&reply));si_control_execute(slot,&m);
        si_control_result(slot,&reply);assert(reply.result==SI_OK&&m.child_events==1);
        ++event.sequence;assert(si_control_submit_child_state(&event,&slot,&reply));
        si_control_child_started(1,1235);si_control_execute(slot,&m);si_control_result(slot,&reply);
        assert(reply.result==SI_STALE&&m.child_events==1);
        frame[4]=1;assert(!si_child_state_decode(frame,sizeof(frame),&decoded));
    }
    puts("SI all ASI controls: state/watchdog/flush/teardown/restart carry the original child context PASS");
}
int main(void) {
    child_state_lifetime();
    parser(); struct mock m; init(&m);
    struct si_request add=request(SI_UPSERT_PHONE); struct si_reply result; unsigned slot;
    assert(si_control_submit(&add,&slot,&result)==1);
    unsigned duplicate; assert(si_control_submit(&add,&duplicate,&result)==0 && result.result==SI_PENDING && duplicate==slot);
    si_control_execute(slot,&m); si_control_execute(slot,&m); si_control_result(slot,&result);
    assert(result.result==SI_OK && result.device_id==100 && m.adds==1);
    assert(!si_control_prepare(100)); /* discovery must not auto-start media */
    assert(execute(&m,&add).result==SI_OK && m.adds==1);
    struct si_request conflict=add; strcpy(conflict.name,"different");
    assert(execute(&m,&conflict).result==SI_BAD_REQUEST && m.adds==1);
    struct si_request rename=request(SI_UPSERT_PHONE); strcpy(rename.name,"Renamed");
    assert(execute(&m,&rename).device_id==100 && m.adds==2);
    struct si_request stale=request(SI_ACTIVATE); stale.si_epoch++;
    assert(execute(&m,&stale).result==SI_STALE && m.starts==0);
    stale=request(SI_ACTIVATE); stale.link_generation--;
    assert(execute(&m,&stale).result==SI_STALE && m.starts==0);
    m.s.app=0; m.s.device=1;
    struct si_request activate=request(SI_ACTIVATE);
    assert(execute(&m,&activate).result==SI_BUSY && !m.stops);
    m.s.app=-1; m.s.device=0; activate=request(SI_ACTIVATE);
    result=execute(&m,&activate); assert(result.result==SI_OK && result.session_generation==1 && m.starts==1);
    uint32_t session,link; assert(si_control_child(&session,&link) && session==1 && link==3);
    assert(si_envv_child_mode(1,1,si_control_child(&session,&link))==1);
    si_control_child_started(session,1234); m.child_alive=1;
    struct si_request wrong=request(SI_CANCEL); wrong.session_generation=99;
    assert(execute(&m,&wrong).result==SI_STALE && !m.stops);
    assert(si_control_before_connect(&m,1,0)==1 && m.stops==1);
    assert(si_envv_child_mode(1,1,si_control_child(&session,&link))==-1);
    assert(si_envv_child_mode(0,0,0)==0 && si_envv_child_mode(-1,1,1)==-1);
    si_control_tick(&m); assert(m.starts==1); /* live old PID blocks new child */
    m.child_alive=0; si_control_tick(&m); assert(m.starts==2 && m.s.device==1);
    assert(!si_control_child(&session,&link));
    m.s.app=-1; m.s.device=0; activate=request(SI_ACTIVATE);
    result=execute(&m,&activate); assert(result.result==SI_OK && result.session_generation==2);
    wrong=request(SI_CANCEL); wrong.session_generation=1;
    assert(execute(&m,&wrong).result==SI_STALE);
    struct si_request remove=request(SI_REMOVE_PHONE); remove.session_generation=2;
    result=execute(&m,&remove); assert(result.result==SI_OK && !result.present && m.removes==1);
    assert(!si_control_prepare(100));
    add=request(SI_UPSERT_PHONE); assert(execute(&m,&add).device_id==100);
    struct si_request old=add;
    for(unsigned i=0;i<40;++i) { struct si_request s=request(SI_SNAPSHOT); assert(execute(&m,&s).result==SI_OK); }
    assert(execute(&m,&old).result==SI_STALE); /* replay outside result cache */
    struct si_request hello=request(SI_HELLO); hello.daemon_epoch=8; hello.request_id=1; hello.si_epoch=0;
    assert(execute(&m,&hello).result==SI_OK);
    struct si_request previous=request(SI_UPSERT_PHONE); assert(execute(&m,&previous).result==SI_STALE);
    previous.operation=SI_HELLO; assert(execute(&m,&previous).result==SI_STALE);
    /* A new Bluetooth link cannot steal an already active IP session. */
    const int active_states[]={3,5};
    for(unsigned state_case=0;state_case<2;++state_case) {
    init(&m); add=request(SI_UPSERT_PHONE); assert(execute(&m,&add).result==SI_OK);
    activate=request(SI_ACTIVATE); result=execute(&m,&activate);
    assert(result.result==SI_OK); m.s.state=active_states[state_case];
    struct si_request other_link=request(SI_UPSERT_PHONE); ++other_link.link_generation;
    assert(execute(&m,&other_link).result==SI_BUSY && m.adds==1);
    struct si_request detached=request(SI_DETACH_PEER); detached.session_generation=result.session_generation;
    assert(execute(&m,&detached).result==SI_OK && !m.stops && !m.removes);
    si_control_tick(&m); assert(m.s.app==0 && !m.removes && !si_control_prepare(100));
    assert(si_envv_child_mode(1,1,si_control_child(&session,&link))==-1);
    m.s.app=-1; m.s.device=0; si_control_tick(&m);
    assert(m.removes==1); /* deferred presence removal after native media ends */
    }
    /* Work admitted before takeover is checked again on the controller. */
    init(&m); add=request(SI_UPSERT_PHONE);
    assert(si_control_submit(&add,&slot,&result)); unsigned queued=slot;
    hello=request(SI_HELLO); hello.daemon_epoch=9; hello.request_id=1; hello.si_epoch=0;
    assert(execute(&m,&hello).result==SI_OK);
    si_control_execute(queued,&m); si_control_result(queued,&result);
    assert(result.result==SI_STALE && !m.adds);
    init(&m);
    for(unsigned i=0;i<SI_CONTROL_REQUESTS;++i) { struct si_request r=request(SI_SNAPSHOT); assert(si_control_submit(&r,&slot,&result)==1); }
    struct si_request full=request(SI_SNAPSHOT); assert(!si_control_submit(&full,&slot,&result) && result.result==SI_BUSY);
    si_control_reject(0); /* one completed retained slot can be reused */
    full=request(SI_SNAPSHOT); assert(si_control_submit(&full,&slot,&result)==1);
    /* Worker media results are values; native cancellation stays on AO and
     * rejects a stale epoch/child or an already-preempting wired context. */
    init(&m);add=request(SI_UPSERT_PHONE);assert(execute(&m,&add).result==SI_OK);
    activate=request(SI_ACTIVATE);result=execute(&m,&activate);
    si_control_child_started(result.session_generation,1234);m.child_alive=1;si_control_tick(&m);
    struct si_media_target target;assert(si_control_media_target(&target));
    struct si_media_target bad_target=target;bad_target.child++;
    si_control_media_ended(&bad_target);si_control_tick(&m);assert(!m.stops);
    bad_target=target;bad_target.epoch++;
    si_control_media_ended(&bad_target);si_control_tick(&m);assert(!m.stops);
    si_control_media_ended(&target);si_control_tick(&m);assert(m.stops==1);
    si_control_media_ended(&target);si_control_tick(&m);assert(m.stops==1);
    assert(!si_control_media_target(&target));
    puts("SI wire/controller: PASS (native boundaries, stale/replay/full queue, UTF-8, 20000 mutations)");
    return 0;
}
