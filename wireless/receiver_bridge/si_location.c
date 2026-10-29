/* MU1316 native CNMEAProvider -> CIAP2ServiceJob_SendGPSData boundary.
 * Preserve the actual vehicle position service and its native queue. Only
 * the final delivery switches from USB/Cinemo to the admitted iAP2 daemon. */
#include "si_guard.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
static uint32_t word(const void *p,unsigned n){return *(const uint32_t *)((const char *)p+n);}
static void send_location(void *self,const void *shared) {
    (void)self;struct mhi2q_si_binding b=mhi2q_si_environment();
    if(!b.valid||!b.enabled||b.parent!=(uint32_t)getppid()||!shared||!word(shared,0)||!word(shared,4))return;
    const void *vector=(const void *)(uintptr_t)word(shared,4);
    unsigned count=word(vector,0);const char *items=(const char *)(uintptr_t)word(vector,4);
    if(!items||count>16)return;
    int (*deliver)(const char *,size_t)=dlsym(RTLD_DEFAULT,"mhi2q_wireless_iap2_nmea");
    if(!deliver)return;
    for(unsigned i=0;i<count;++i) {
        const void *entry=items+i*12;
        if(!word(entry,0)||!word(entry,4))continue;
        const void *string=(const void *)(uintptr_t)word(entry,4);
        unsigned length=word(string,8);if(!length||length>160)continue;
        const char *data=(const char *)string+12;
        if((word(string,4)>>16)&1) {
            const void *storage=(const void *)(uintptr_t)word(string,12);
            if(!storage)continue;
            data=(const char *)(uintptr_t)word(storage,0);
        }
        if(data)(void)deliver(data,length);
    }
}
static void send_location_base4(void *self,const void *value){send_location((char *)self-4,value);}
int mhi2q_si_location_install(void) {
    volatile uint32_t *primary=(volatile uint32_t *)(uintptr_t)0x1a5734;
    volatile uint32_t *secondary=(volatile uint32_t *)(uintptr_t)0x1a578c;
    if(*primary==(uint32_t)(uintptr_t)send_location&&*secondary==(uint32_t)(uintptr_t)send_location_base4)return 1;
    if(*primary!=0x17b01c||*secondary!=0x17b014)return 0;
    *primary=(uint32_t)(uintptr_t)send_location;*secondary=(uint32_t)(uintptr_t)send_location_base4;
    __sync_synchronize();fputs("[dio-si] native vehicle NMEA delivery installed\n",stderr);return 1;
}
