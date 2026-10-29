/* Disposable RCC peer: send explicit test states through the original COMM
 * FecClient proxy -> native MMX fec queue -> native SI FecAppMMX callback.
 * This is a simulated RCC response, NOT license/signature validation. Never
 * included in HU payloads. Native SI/FEC text and admission fields untouched.
 * ABI verified in MU1316 fec: ctor 11fca0, FecClient impl 11f234/11f28c,
 * queue event8 10e714, native forwarding 114958. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct { uint32_t w[3]; } shared;
typedef struct { uint32_t size; shared *data; uint32_t capacity; } vector;
typedef struct { uint32_t size, capacity; const char *data; } string;
typedef struct { uint32_t w[5]; } uuid;
typedef struct { uuid id; uint32_t instance; } service_id;
typedef struct { service_id *service; uuid *interface; const char *name; } identity;

static void (*osal_init)(void *, int, int);
static const char osal_init_symbol[]="_ZN4osal4OsalC1Ebb";
static void (*error_init)(void *);
static const char error_init_symbol[]="_ZN3ipl22StdoutDumpErrorStorageC1Ev";
static void (*agent_init)(void *, const string *, const string *);
static const char agent_init_symbol[]="_ZN4comm12AgentStarterC1ERKN3ipl12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_";
static void (*status_init)(void *);
static const char status_init_symbol[]="_ZN4comm16AgentStartStatusC1Ev";
static void (*agent_start)(void *, void *, int);
static const char agent_start_symbol[]="_ZN4comm12AgentStarter5startEPNS_25AgentStarterEventReceiverEb";
static int (*agent_wait)(void *, int64_t);
static const char agent_wait_symbol[]="_ZN4comm16AgentStartStatus16waitUntilStartedEx";
static void (*uuid_init)(void *, unsigned, unsigned, unsigned, unsigned short,
                      unsigned char, unsigned char, unsigned char,
                      unsigned char, unsigned char, unsigned char);
static const char uuid_init_symbol[]="_ZN3ipl4UUIDC1Ejjjthhhhhh";
static void (*proxy_init)(void *, const identity *, int, void *);
static const char proxy_init_symbol[]="_ZN4comm5ProxyC2ERKNS_11TrackedBase12IdentityArgsENS_14InterfaceStyleEPNS_17LifecycleListenerE";
static void * (*proxy_connect)(void *);
static const char proxy_connect_symbol[]="_ZN4comm5Proxy7connectEv";
static void (*shared_init)(void *, void *, void (*)(void *, void *), void *);
static const char shared_init_symbol[]="_ZN4util20SharedPtrDefaultBaseC2EPvPFvS1_S1_ES1_";
static void (*shared_destroy)(void *);
static const char shared_destroy_symbol[]="_ZN4util20SharedPtrDefaultBaseD2Ev";

static void release(void *object, void *arg) { (void)arg; free(object); }
static void release_vector(void *object, void *arg) {
    (void)arg;
    vector *v=object;
    for (uint32_t i=0;i<v->size;++i) shared_destroy(&v->data[i]);
    free(v->data); free(v);
}
static void peer_pause(void) { struct timespec t={0,100000000}; nanosleep(&t,NULL); }
static void fail(const char *why) { fprintf(stderr,"SI-FEC-PEER: FAIL %s\n",why); exit(1); }
static void *resolve(const char *symbol) {
    void *p=dlsym(RTLD_DEFAULT,symbol);
    if (!p) { fprintf(stderr,"%s: %s\n",symbol,dlerror()); fail("native symbol"); }
    return p;
}
#define BIND(name) name=(__typeof__(name))resolve(name##_symbol)

int main(int argc, char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    if (argc<3 || (strcmp(argv[1],"grant") && strcmp(argv[1],"deny"))) return 2;
    if (argc>34) return 2;
    if (!dlopen("libcomm.so",RTLD_NOW|RTLD_GLOBAL)) fail(dlerror());
    BIND(osal_init); BIND(error_init); BIND(agent_init); BIND(status_init);
    BIND(agent_start); BIND(agent_wait); BIND(uuid_init); BIND(proxy_init);
    BIND(proxy_connect); BIND(shared_init); BIND(shared_destroy);
    uint32_t state=!strcmp(argv[1],"grant");
    /* Oversized, aligned storage only for native classes whose constructors
     * are imported. The arguments/data types above have exact ARM layouts. */
    uint64_t errors[512]={0}, osal[8]={0}, status[16]={0}, agent[2]={0};
    error_init(errors); osal_init(osal,1,0);
    string name={18,18,"persistence_reader"}, node={5,5,"local"};
    agent_init(agent,&name,&node); status_init(status); agent_start(agent,status,1);
    if (agent_wait(status,10000)!=1) fail("COMM agent start");
    puts("SI-FEC-PEER: COMM agent started; RCC state simulation only");
    service_id sid;
    uuid_init(&sid.id,0xff733e4d,0x6e39,0x49b8,0xb354,0xee,0x9c,0x22,0xf6,0xcf,0xc9);
    sid.instance=1;
    uuid iface;
    uuid_init(&iface,0x41fc1271,0x3ffd,0x552d,0xa498,0x7d,0x1f,0x10,0x5d,0x82,0x5a);
    identity who={&sid,&iface,"asi.fec.FecClient"};
    uint32_t proxy[3]={0};
    proxy_init(proxy,&who,0,NULL);
    if (proxy_connect(proxy)) fail("FecClient connect");
    uint32_t *core=NULL;
    for (unsigned i=0;i<150;++i) {
        core=(uint32_t *)(uintptr_t)proxy[1];
        if (core && ((int (*)(void *))(uintptr_t)((uint32_t *)(uintptr_t)core[0])[2])(core)) break;
        core=NULL; peer_pause();
    }
    if (!core) fail("FecClient proxy not alive");
    vector *v=calloc(1,sizeof(*v));
    if (!v) fail("allocate vector");
    v->size=v->capacity=(uint32_t)(argc-2);
    v->data=calloc(v->size,sizeof(*v->data));
    if (!v->data) fail("allocate shared states");
    for (uint32_t i=0;i<v->size;++i) {
        char *end;
        unsigned long key=strtoul(argv[i+2],&end,16);
        if (*end || key>UINT32_MAX || !key) fail("invalid FSID");
        uint32_t *item=calloc(3,sizeof(*item));
        if (!item) fail("allocate state");
        item[0]=(uint32_t)key; item[1]=i; item[2]=state;
        shared_init(&v->data[i],item,release,NULL);
    }
    shared states;
    shared_init(&states,v,release_vector,NULL);
    void *api=(void *)(uintptr_t)core[core[1]+18];
    if (!api) fail("FecClient API");
    void *result=((void *(*)(void *,const shared *))(uintptr_t)
                 (*(uint32_t **)api)[0])(api,&states);
    if (result) fail("FecClient updateFECs RPC");
    char receipt[128];
    int length=snprintf(receipt,sizeof(receipt),"SI-FEC-PEER: updateFECs sent count=%u state=%u through native COMM\n",v->size,state);
    if(length<0 || length>=(int)sizeof(receipt) || write(1,receipt,(size_t)length)!=length) fail("receipt write");
    /* Keep source arguments alive until the asynchronous native transport
     * has dispatched; receiver deserializes its own owned copy. */
    for (unsigned i=0;i<20;++i) peer_pause();
    shared_destroy(&states);
    puts("SI-FEC-PEER: DONE");
    _Exit(0);
}
