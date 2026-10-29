/* Opt-in network association backend for our FE01 VM's native airplayd.
 * Socket traffic, CarKit, pairing, authentication and video remain native.
 * No shared-cache file or Apple launchd plist is modified. */
typedef unsigned int u32;
typedef unsigned long size_t;
typedef const void *CFRef;
typedef void *CFMutableRef;
typedef struct __sFILE FILE;
extern void *dlopen(const char *,int), *dlsym(void *,const char *);
extern const char *getprogname(void);
extern int strcmp(const char *,const char *),memcmp(const void *,const void *,size_t);
extern void *memcpy(void *,const void *,size_t),*malloc(size_t);
extern void free(void *);
extern FILE *fopen(const char *,const char *);
extern size_t fread(void *,size_t,size_t,FILE *);
extern int fclose(FILE *),fprintf(FILE *,const char *,...);
extern int getpid(void);
extern int usleep(u32);
extern void *mmap(void *,size_t,int,int,int,long);
extern int munmap(void *,size_t),vm_protect(u32,unsigned long,unsigned long,int,int);
extern u32 mach_task_self_;
extern void sys_icache_invalidate(void *,size_t);
extern void *dispatch_queue_create(const char *,void *);
extern long long dispatch_time(long long,long long);
extern void dispatch_after_f(long long,void *,void *,void (*)(void *));
extern void os_unfair_lock_lock(void *),os_unfair_lock_unlock(void *);
extern CFRef kCFBooleanTrue;
extern const unsigned char kCFTypeDictionaryKeyCallBacks[],kCFTypeDictionaryValueCallBacks[],kCFTypeArrayCallBacks[];
extern CFRef CFStringCreateWithCString(void *,const char *,u32);
extern unsigned char CFStringGetCString(CFRef,char *,long,u32);
extern CFRef CFDataCreate(void *,const unsigned char *,long);
extern const unsigned char *CFDataGetBytePtr(CFRef);
extern long CFDataGetLength(CFRef);
extern CFRef CFPropertyListCreateWithData(void *,CFRef,unsigned long,long *,CFRef *);
extern unsigned long CFGetTypeID(CFRef),CFDictionaryGetTypeID(void),CFStringGetTypeID(void),CFDataGetTypeID(void);
extern CFMutableRef CFDictionaryCreateMutable(void *,long,const void *,const void *);
extern CFRef CFDictionaryGetValue(CFRef,CFRef);
extern void CFDictionarySetValue(CFMutableRef,CFRef,CFRef),CFRelease(CFRef);
extern CFRef CFRetain(CFRef),CFArrayCreate(void *,const CFRef *,long,const void *);
extern CFRef SCDynamicStoreCreate(void *,CFRef,void *,void *);
extern unsigned char SCDynamicStoreNotifyValue(CFRef,CFRef);
extern u32 IOServiceGetMatchingService(u32,void *),IORegistryEntryFromPath(u32,const char *);
extern void *IOServiceMatching(const char *);
extern CFRef IORegistryEntryCreateCFProperty(u32,CFRef,void *,u32);
extern int IOObjectRelease(u32);
extern int socket(int,int,int),bind(int,const void *,u32),close(int),inet_pton(int,const char *,void *);

#define CONFIG "/var/mobile/Library/Application Support/MHI2CarPlayLab/wireless-network.plist"
#define LOG "/var/mobile/Library/Caches/mhi2-wireless-adapter.log"
#define STRIP(p) __builtin_ptrauth_strip((void *)(p),0)
#define SIGN(p) __builtin_ptrauth_sign_unauthenticated((void *)(p),0,0)
static char uuid[64],local_ip[32];
static void *events;
static unsigned map_lock;
typedef void (*Handler)(void *,unsigned,CFRef,CFRef);
typedef int (*RegisterHandler)(void *,CFRef,Handler);
typedef int (*RegisterEvent)(void *,CFRef,unsigned);
static RegisterHandler register_handler_original;
static RegisterEvent register_event_original;
static struct {void *helper;CFRef client;Handler handler;} handlers[64];
static unsigned handler_count;

static CFRef string(const char *value){return CFStringCreateWithCString(0,value,0x08000100);}
static CFRef get(CFRef dict,const char *key){CFRef k=string(key);if(!dict||!k){if(k)CFRelease(k);return 0;}CFRef v=CFDictionaryGetValue(dict,k);CFRelease(k);return v;}
static int put(CFMutableRef dict,const char *key,CFRef value){CFRef k=string(key);if(!dict||!k||!value){if(k)CFRelease(k);return 0;}CFDictionarySetValue(dict,k,value);CFRelease(k);return 1;}
static int put_string(CFMutableRef dict,const char *key,const char *value){CFRef s=string(value);int ok=put(dict,key,s);if(s)CFRelease(s);return ok;}
static int read_string(CFRef dict,const char *key,char *out,long capacity){CFRef v=get(dict,key);return v&&CFGetTypeID(v)==CFStringGetTypeID()&&CFStringGetCString(v,out,capacity,0x08000100);}
static void note(const char *text){FILE *f=fopen(LOG,"a");if(f){fprintf(f,"pid=%d %s\n",getpid(),text);fclose(f);}}

static int owned_vm(CFRef config){
    char expected[48],actual[48];
    if(!read_string(config,"UDID",expected,sizeof(expected)))return 0;
    u32 chosen=IORegistryEntryFromPath(0,"IODeviceTree:/chosen");
    CFRef key=string("chip-id"),chip=IORegistryEntryCreateCFProperty(chosen,key,0,0);CFRelease(key);IOObjectRelease(chosen);
    int valid=chip&&CFGetTypeID(chip)==CFDataGetTypeID()&&CFDataGetLength(chip)==4&&*(const u32 *)CFDataGetBytePtr(chip)==0xFE01;
    if(chip)CFRelease(chip);
    if(!valid)return 0;
    u32 usb=IOServiceGetMatchingService(0,IOServiceMatching("AppleVirtIOUSBDeviceController"));
    key=string("DeviceDescription");CFRef description=IORegistryEntryCreateCFProperty(usb,key,0,0);CFRelease(key);IOObjectRelease(usb);
    valid=description&&CFGetTypeID(description)==CFDictionaryGetTypeID()&&read_string(description,"uniqueDeviceID",actual,sizeof(actual))&&!strcmp(actual,expected);
    if(description)CFRelease(description);
    return valid;
}

static int load_config(void){
    unsigned char bytes[4096];FILE *f=fopen(CONFIG,"rb");if(!f)return 0;
    size_t n=fread(bytes,1,sizeof(bytes),f);fclose(f);if(!n||n==sizeof(bytes))return 0;
    CFRef data=CFDataCreate(0,bytes,(long)n),config=CFPropertyListCreateWithData(0,data,0,0,0);CFRelease(data);
    int valid=config&&CFGetTypeID(config)==CFDictionaryGetTypeID()&&get(config,"Enabled")==kCFBooleanTrue&&owned_vm(config)&&
        read_string(config,"CarPlayUUID",uuid,sizeof(uuid))&&read_string(config,"LocalIP",local_ip,sizeof(local_ip));
    if(config)CFRelease(config);
    if(!valid)return 0;
    struct {unsigned char len,family;unsigned short port;u32 address;unsigned char zero[8];} address={16,2,0,0,{0}};
    int fd=socket(2,2,0);valid=fd>=0&&inet_pton(2,local_ip,&address.address)==1&&!bind(fd,&address,sizeof(address));
    if(fd>=0)close(fd);
    return valid;
}

static CFMutableRef network_info(void){
    CFMutableRef d=CFDictionaryCreateMutable(0,0,kCFTypeDictionaryKeyCallBacks,kCFTypeDictionaryValueCallBacks);
    if(!d)return 0;
    if(!put_string(d,"networkInterfaceName","en0")||!put_string(d,"wifiNetworkSSID","MHI2-Native-Lab")||
       !put_string(d,"wifiNetworkCarPlayUUID",uuid)||!put(d,"wifiNetworkIsCarPlay",kCFBooleanTrue)){CFRelease(d);return 0;}
    return d;
}
static int copy_wifi_info(void *helper,CFRef *out){(void)helper;if(!out)return -6705;*out=network_info();return *out?0:-6728;}
static int query_power(void *helper,unsigned char *out){(void)helper;if(!out)return -6705;*out=1;return 0;}
struct Event {void *helper;CFRef client;Handler handler;unsigned type;CFRef info;};
static void deliver(void *context){
    struct Event *e=context;e->handler(e->helper,e->type,e->info,e->client);
    if(e->info)CFRelease(e->info);CFRelease(e->client);CFRelease(e->helper);free(e);
}
static int emit(void *helper,CFRef client,unsigned type){
    Handler handler=0;os_unfair_lock_lock(&map_lock);
    for(unsigned i=0;i<handler_count;i++)if(handlers[i].helper==helper&&handlers[i].client==client)handler=handlers[i].handler;
    os_unfair_lock_unlock(&map_lock);if(!handler)return 0;
    struct Event *e=malloc(sizeof(*e));if(!e)return 0;
    e->helper=(void *)CFRetain(helper);e->client=CFRetain(client);e->handler=handler;e->type=type;e->info=0;
    if(type==3)e->info=network_info();
    if(type==10){
        CFMutableRef d=CFDictionaryCreateMutable(0,0,kCFTypeDictionaryKeyCallBacks,kCFTypeDictionaryValueCallBacks);
        CFRef ip=string(local_ip),list=ip?CFArrayCreate(0,&ip,1,kCFTypeArrayCallBacks):0;if(ip)CFRelease(ip);
        int ok=put_string(d,"networkInterfaceName","en0")&&put(d,"IPv4Addresses",list);if(list)CFRelease(list);
        if(ok)e->info=d;else if(d)CFRelease(d);
    }
    if((type==3||type==10)&&!e->info){CFRelease(e->client);CFRelease(e->helper);free(e);return 0;}
    dispatch_after_f(dispatch_time(0,100000000),events,e,deliver);
    return 1;
}
static int register_handler(void *helper,CFRef client,Handler handler){
    int result=register_handler_original(helper,client,handler);if(result)return result;
    os_unfair_lock_lock(&map_lock);unsigned i;
    for(i=0;i<handler_count;i++)if(handlers[i].helper==helper&&handlers[i].client==client)break;
    /* Keep at most 64 registration identities for this process lifetime;
     * retaining them prevents pointer reuse from matching a stale entry. */
    if(i<64){if(i==handler_count){handlers[i].helper=(void *)CFRetain(helper);handlers[i].client=CFRetain(client);handler_count++;}handlers[i].handler=handler;}
    os_unfair_lock_unlock(&map_lock);return result;
}
static int register_event(void *helper,CFRef client,unsigned type){
    if((type==1||type==2||type==3)&&emit(helper,client,type))return 0;
    int result=register_event_original(helper,client,type);
    if(!result&&type==10)emit(helper,client,type);
    return result;
}

struct Patch {unsigned char *target;unsigned char original[16];unsigned size;};
static struct Patch patches[5];static unsigned patch_count;
static int protect(void *address,unsigned n,int mode){
    unsigned long start=(unsigned long)address&~16383UL,end=((unsigned long)address+n+16383)&~16383UL;
    return vm_protect(mach_task_self_,start,end-start,0,mode);
}
static int write_patch(void *address,const void *bytes,unsigned size){
    if(patch_count>=5||size>16||protect(address,size,1|2|16))return 0;
    struct Patch *p=&patches[patch_count++];p->target=address;p->size=size;memcpy(p->original,address,size);
    memcpy(address,bytes,size);sys_icache_invalidate(address,size);
    return !protect(address,size,1|4)&&!memcmp(address,bytes,size);
}
static void rollback(void){while(patch_count){struct Patch *p=&patches[--patch_count];if(!protect(p->target,p->size,1|2|16)){memcpy(p->target,p->original,p->size);sys_icache_invalidate(p->target,p->size);protect(p->target,p->size,1|4);}}}
static int detour(void *function,void *replacement,void **original){
    if(!function)return 0;unsigned char *target=STRIP(function);u32 words[4];memcpy(words,target,16);
    /* Only the reviewed non-PC-relative prologues are relocatable here. */
    for(unsigned i=0;i<4;i++){
        u32 w=words[i];
        if(!((w==0xD503237F)||((w&0xFFC003FF)==0xD10003FF)||((w&0x3B000000)==0x29000000)||((w&0xFFC003FF)==0x910003FD)))return 0;
    }
    if(original){
        unsigned char *t=mmap(0,4096,1|2,0x1002,-1,0);if(t==(void *)-1)return 0;
        memcpy(t,target,16);u32 jump[2]={0x58000050,0xD61F0200};memcpy(t+16,jump,8);void *back=target+16;memcpy(t+24,&back,8);
        if(protect(t,32,1|4)){munmap(t,4096);return 0;}sys_icache_invalidate(t,32);*original=SIGN(t);
    }
    unsigned char jump[16];u32 instructions[2]={0x58000050,0xD61F0200};memcpy(jump,instructions,8);
    void *raw=STRIP(replacement);memcpy(jump+8,&raw,8);return write_patch(target,jump,16);
}

static int active;
static void install_network(void){
    if(active||!load_config())return;
    note("association adapter loading");
    void *ap=dlopen("/System/Library/PrivateFrameworks/APTransport.framework/APTransport",2);if(!ap){note("APTransport unavailable");return;}
    unsigned char *create=STRIP(dlsym(ap,"APBrowserCarSessionCreate"));
    if(!create||*(u32 *)(create+0x17c)!=0x52800022||*(u32 *)(create+0x1bc)!=0x52800062||
       (*(u32 *)(create+0x174)&0x7F000000)!=0x34000000){note("VM profile bytes differ; refused");return;}
    events=dispatch_queue_create("mhi2-vphone-wireless-events",0);
    if(!events)return;
    u32 mask=0x52800062;
    if(!detour(dlsym(ap,"APConnectivityHelperRegisterEventHandler"),register_handler,(void **)&register_handler_original)||
       !detour(dlsym(ap,"APConnectivityHelperRegisterForEvent"),register_event,(void **)&register_event_original)||
       !detour(dlsym(ap,"APConnectivityHelperCopyCurrentWiFiNetworkInfo"),copy_wifi_info,0)||
       !detour(dlsym(ap,"APConnectivityHelperQueryWiFiPower"),query_power,0)||
       !write_patch(create+0x17c,&mask,4)){
        rollback();note("network adapter refused and rolled back");return;
    }
    note("association adapter active; native pairing/auth/media unchanged");
    active=1;
}
/* Exercise the exact adapter in a disposable owned helper before installing
 * its loader into the VM environment. It cannot target another process. */
static unsigned probe_event;
static void probe_handler(void *helper,unsigned type,CFRef info,CFRef client){
    (void)helper;char ssid[64],name[64];
    FILE *log=fopen(LOG,"a");if(log){fprintf(log,"probe event=%u arg3_type=%lu arg4_type=%lu\n",type,info?CFGetTypeID(info):0,client?CFGetTypeID(client):0);fclose(log);}
    if(!client||CFGetTypeID(client)!=CFStringGetTypeID()||!CFStringGetCString(client,name,sizeof(name),0x08000100)||strcmp(name,"mhi2-owned-network-probe"))return;
    if(type==3&&info&&read_string(info,"wifiNetworkSSID",ssid,sizeof(ssid))&&!strcmp(ssid,"MHI2-Native-Lab"))
        __atomic_fetch_or(&probe_event,1,__ATOMIC_RELEASE);
    /* Event 10 is delivered by the original APTransport dynamic-store
     * listener, so this validates its actual callback ABI independently. */
    if(type==10&&info&&CFGetTypeID(info)==CFDictionaryGetTypeID()&&read_string(info,"networkInterfaceName",name,sizeof(name)))
        __atomic_fetch_or(&probe_event,2,__ATOMIC_RELEASE);
}
int mhi2_vphone_network_adapter_probe(void){
    if(strcmp(getprogname(),"usb-description"))return -1;
    install_network();if(!active)return 0;
    void *ap=dlopen("/System/Library/PrivateFrameworks/APTransport.framework/APTransport",2);
    void *(*shared)(void)=SIGN(STRIP(dlsym(ap,"APConnectivityHelperGetSharedHelper")));
    void *helper=shared();if(!helper)return -2;
    RegisterHandler reg=SIGN(STRIP(dlsym(ap,"APConnectivityHelperRegisterEventHandler")));
    RegisterEvent subscribe=SIGN(STRIP(dlsym(ap,"APConnectivityHelperRegisterForEvent")));
    CFRef client=string("mhi2-owned-network-probe");
    if(reg(helper,client,probe_handler)||subscribe(helper,client,3)||register_event_original(helper,client,10)){CFRelease(client);return -3;}
    CFRef name=string("mhi2-owned-network-probe"),store=SCDynamicStoreCreate(0,name,0,0),key=string("State:/Network/Interface/en0/IPv4");
    CFRelease(name);if(store){SCDynamicStoreNotifyValue(store,key);CFRelease(store);}CFRelease(key);
    for(unsigned i=0;i<200&&__atomic_load_n(&probe_event,__ATOMIC_ACQUIRE)!=3;i++)usleep(10000);
    CFRelease(client);
    unsigned observed=__atomic_load_n(&probe_event,__ATOMIC_ACQUIRE);
    return observed==3?(int)patch_count:-40-(int)observed;
}
__attribute__((constructor)) static void install(void){
    if(!strcmp(getprogname(),"airplayd"))install_network();
}
