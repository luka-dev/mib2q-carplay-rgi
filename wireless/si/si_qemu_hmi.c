/* Disposable HMI input for the native QNX SI/DIO fixture. No production
 * build links this file. Execute the real DSICarplay::startService on the
 * existing native setup thread, after ASI setup delivery; never a worker.
 *
 * ServiceConfiguration and its empty app/resource vectors are borrowed
 * views for this synchronous call. Native startService checks their owner
 * words but neither retains nor destroys these arguments. UnicodeString
 * is constructed/copied/destroyed by libutil, including the BT list copy.
 * This proves configuration-before-queue, not real HMI or audio focus.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#ifdef MHI2Q_SI_QEMU_INTEGRATED
/* The physical HMI calls the public DSI entrypoints from its own event
 * thread.  This disposable peer does the same only after native Screen is
 * READY and the independent phone has accepted the encrypted Events socket.
 * It deliberately never reaches into AirPlay HID internals. */
static void *si_qemu_hid_events(void *opaque) {
    uint32_t *manager=opaque;
    char ready[128];
    const char *session=getenv("MHI2Q_CP_SI_SESSION");
    int i;
    if(!session || snprintf(ready,sizeof(ready),
                            "/dev/shmem/integrated-events-ready-%s",session)
                       >=(int)sizeof(ready)) abort();
    (void)unlink(ready);
    /* SI/DIO startup includes a deliberate 30 s native budget.  A test
     * observation timeout must never abort the receiver and turn a missing
     * HID event into a synthetic DIO crash. */
    for(i=0;i<700;++i) {
        if(manager[544/4]==2 && access(ready,F_OK)==0) break;
        usleep(50000);
    }
    if(i==700) {
        fputs("SI-HMI-QEMU: native HID observation timed out\n",stderr);
        return NULL;
    }
    void *dsi=(void *)(uintptr_t)manager[480/4];
    if(!dsi) abort();
    ((int (*)(void *,int))(uintptr_t)0x167ed0)(dsi,3);
    ((void (*)(void *,unsigned,int))(uintptr_t)0x167f3c)(dsi,1,0);
    ((void (*)(void *,unsigned,int))(uintptr_t)0x167f3c)(dsi,1,1);
    fputs("SI-HMI-QEMU: native DSI rotary/select/release submitted\n",stderr);
    return NULL;
}
static uintptr_t metadata_sink[66];
/* Synthetic position service output, entering the original native GPS job
 * queue. Actual vehicle GPS/radio remains a separate HU validation. */
static void gps_no_delete(void *p,void *context){(void)p;(void)context;}
static void *si_qemu_vehicle_gps(void *unused) {
    (void)unused;FILE *mode=fopen("/proc/boot/integrated-fault","r");char profile[32]={0};
    if(!mode)return NULL;
    int read=fscanf(mode,"%31s",profile);fclose(mode);if(read!=1||strcmp(profile,"features"))return NULL;
    char request[100];snprintf(request,sizeof(request),"/dev/shmem/integrated-gps-%s",getenv("MHI2Q_CP_SI_SESSION"));
    for(unsigned i=0;i<900&&access(request,F_OK);++i)usleep(50000);
    if(access(request,F_OK))return NULL;
    static uint32_t strings[2][10],items[2][3],vector[3];
    const char *bodies[]={"GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",
                          "GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W"};
    void (*ctor)(void *)=dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString8C1Ev");
    void (*set)(void *,const char *,int,unsigned,int)=dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString89setStringEPKcNS_20UnicodeString_common8EncodingEjb");
    void (*share)(void *,void *,void (*)(void *,void *),void *)=dlsym(RTLD_DEFAULT,"_ZN4util20SharedPtrDefaultBaseC2EPvPFvS1_S1_ES1_");
    void (*destroy)(void *)=dlsym(RTLD_DEFAULT,"_ZN4util20SharedPtrDefaultBaseD2Ev");
    if(!ctor||!set||!share||!destroy)abort();
    for(unsigned i=0;i<2;++i) {
        unsigned sum=0;for(const char *p=bodies[i];*p;++p)sum^=(unsigned char)*p;
        char sentence[161];snprintf(sentence,sizeof(sentence),"$%s*%02X",bodies[i],sum);
        ctor(strings[i]);set(strings[i],sentence,3,0,0);share(items[i],strings[i],gps_no_delete,NULL);
    }
    vector[0]=2;vector[1]=(uint32_t)(uintptr_t)items;vector[2]=2;
    uint32_t shared[3]={0},source[6]={0};share(shared,vector,gps_no_delete,NULL);source[2]=3;
    int result=((int (*)(void *,void *))(uintptr_t)0x1252b4)(source,shared);
    destroy(shared);
    fputs(result?"SI-FEATURES-QEMU: native GPS job enqueued PASS\n":"SI-FEATURES-QEMU: native GPS enqueue FAIL\n",stderr);
    return NULL;
}
static int (*native_position)(void *,unsigned,int);
static int si_qemu_position(void *sink,unsigned elapsed,int valid) {
    int result=native_position(sink,elapsed,valid);
    if(valid==1 && (elapsed==4200||elapsed==5600)) {
        char line[100];int n=snprintf(line,sizeof(line),
            "SI-METADATA-QEMU: native DSI elapsed=%u valid=1\n",elapsed);
        if(n>0&&n<(int)sizeof(line))(void)write(2,line,(size_t)n);
    }
    return result;
}
#endif

void mhi2q_si_qemu_hmi_start(void *manager) {
    uint32_t *m=manager;
    void *dsi=(void *)(uintptr_t)m[480/4];
    uint32_t config[25]={0}, string[10]={0};
    uint32_t owner=1;
    typedef void (*ctor_fn)(void *);
    typedef void (*set_fn)(void *,const char *,int,unsigned,int);
    ctor_fn ctor=(ctor_fn)dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString8C1Ev");
    ctor_fn destroy=(ctor_fn)dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString8D1Ev");
    set_fn set_string=(set_fn)dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString89setStringEPKcNS_20UnicodeString_common8EncodingEjb");
    if(!dsi || !ctor || !destroy || !set_string) {
        fputs("SI-HMI-QEMU: FAIL missing native DSI/UnicodeString\n",stderr);
        abort();
    }
    ctor(string);
    set_string(string,"02:00:00:00:00:01",3,0,0);
    uint32_t item[3]={(uint32_t)(uintptr_t)&owner,(uint32_t)(uintptr_t)string,0};
    uint32_t vector[2]={1,(uint32_t)(uintptr_t)item};
    config[24/4]=2; /* explicit simulated HMI: 1024x480, knob+trackpad */
    /* Native tracing reads the display name before the optional-name test. */
    config[44/4]=(uint32_t)(uintptr_t)&owner;
    config[48/4]=(uint32_t)(uintptr_t)string;
    config[60/4]=1024;config[64/4]=1024;
    config[68/4]=(uint32_t)(uintptr_t)&owner;
    config[72/4]=(uint32_t)(uintptr_t)vector;
    config[84/4]=88;config[88/4]=188;config[92/4]=9;
    uint32_t shared[3]={(uint32_t)(uintptr_t)&owner,(uint32_t)(uintptr_t)config,0};
    ((void (*)(void *,const void *))(uintptr_t)0x16a7cc)(dsi,shared);
    destroy(string);
    if(m[548/4]!=1024 || m[552/4]!=480 ||
       *((unsigned char *)manager+372)!=9 || m[364/4]!=1024 || m[368/4]!=1024) {
        fputs("SI-HMI-QEMU: FAIL native configuration mismatch\n",stderr);
        abort();
    }
    char line[160];
    int length=snprintf(line,sizeof(line),"SI-HMI-QEMU: native startService configured %ux%u hid=%u touch=%u/%u\n",
                        m[548/4],m[552/4],*((unsigned char *)manager+372),m[364/4],m[368/4]);
    if(length>0 && length<(int)sizeof(line)) (void)write(2,line,(size_t)length);
#ifdef MHI2Q_SI_QEMU_INTEGRATED
    extern void si_qemu_audio_install(void *);
    /* Observe the real DSI sink after the native queue/manager dispatch;
     * forward every call unchanged. Only the absent HMI is modeled. */
    uintptr_t **sink=dsi,*table=*sink;
    memcpy(metadata_sink,table-2,sizeof(metadata_sink));
    native_position=(int (*)(void *,unsigned,int))table[23];
    metadata_sink[25]=(uintptr_t)si_qemu_position;*sink=metadata_sink+2;
    si_qemu_audio_install(manager);
    /* Observation only: phone does not HELLO or mutate SI's daemon epoch. */
    FILE *context=fopen("/dev/shmem/integrated-context","w");
    if(!context)abort();
    fprintf(context,"%s %s %s %ld %ld\n",getenv("MHI2Q_CP_SI_EPOCH"),
            getenv("MHI2Q_CP_SI_SESSION"),getenv("MHI2Q_CP_SI_LINK"),(long)getpid(),(long)getppid());
    fclose(context);
    pthread_t hid_thread;
    if(pthread_create(&hid_thread,NULL,si_qemu_hid_events,manager)) abort();
    (void)pthread_detach(hid_thread);
    pthread_t gps_thread;
    if(pthread_create(&gps_thread,NULL,si_qemu_vehicle_gps,NULL))abort();
    (void)pthread_detach(gps_thread);
#endif
}
