/* Exact MU1316 CSIService transport adapter. SI owns the native DIO PID and
 * ASI registration. Wireless iAP2 belongs to the external bootstrap daemon;
 * native USB/Cinemo setup must never run for an SI-bound WLAN generation.
 * Install before native controller jobs start; no constructor, text writes,
 * synthetic session-start notification or worker-thread FSM mutation.
 */
#include "si_guard.h"
#ifdef MHI2Q_BRIDGE_SI_TRANSPORT
#include "now_playing.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef SI_TRANSPORT_GUARD_TEST
extern uint32_t si_fixture_vtable[27];
#define SERVICE_TABLE (si_fixture_vtable)
#define NOTIFY_SLOT (&si_fixture_vtable[26])
#else
#define SERVICE_TABLE ((volatile uint32_t *)(uintptr_t)0x1a4e18)
#define NOTIFY_SLOT ((volatile uint32_t *)(uintptr_t)0x1a4e80)
#endif
static void *manager;
static void *service;
static int setup_reported, installed;
static uint32_t get(void *p,unsigned n) {return *(uint32_t *)((char *)p+n);}
static void set(void *p,unsigned n,uint32_t v) {*(uint32_t *)((char *)p+n)=v;}
static int bound(void) {
    struct mhi2q_si_binding b=mhi2q_si_environment();
    return b.enabled && b.valid && b.parent==(uint32_t)getppid();
}
static void state(void *self,unsigned value) {
    ((void (*)(void *,unsigned))(uintptr_t)0x164934)(self,value);
}
static int setup_complete(void *self) {
    /* Bootstrap already owns a ready mDNS responder. Report transport setup,
     * then let HMI's native DSICarplay::startService configure the manager and
     * start AirPlay. Starting here freezes the initial 800x480/zero-HID
     * values before that configuration arrives (physical HU run 03). */
    if (__sync_bool_compare_and_swap(&setup_reported,0,1)) {
        state(self,1);
#ifdef MHI2Q_BRIDGE_SI_TRANSPORT
        (void)mhi2q_now_playing_capture(manager);
#endif
        fputs("[dio-si] external transport setup complete; awaiting native HMI configuration\n",stderr);
#ifdef MHI2Q_SI_QEMU_HMI
        /* Explicit HMI test peer, linked only into the disposable QNX
         * fixture. Production waits for the actual Java/DSI configuration. */
        extern void mhi2q_si_qemu_hmi_start(void *);
        mhi2q_si_qemu_hmi_start(manager);
#endif
    }
    return 1;
}
static int init(void *self,void *device,void *client) {
    if(!bound() || !self || !device || !get(device,0) || !get(device,4) ||
       (service && service!=self) || get(self,112)!=(uint32_t)(uintptr_t)manager) return 0;
    /* Native SharedPtr assignment retains SDeviceInfo; original destructor
     * releases it. The existing ASI client owns the reply registration. */
    ((void (*)(void *,void *))(uintptr_t)0x113178)((char *)self+584,device);
    if(!get(self,536)) set(self,536,(uint32_t)(uintptr_t)client);
#ifdef MHI2Q_BRIDGE_SI_TRANSPORT
    extern int mhi2q_si_state_install(void *);
    if(!mhi2q_si_state_install((void *)(uintptr_t)get(self,536)))return 0;
#endif
    (void)__sync_lock_test_and_set(&service,self);
    return setup_complete(self);
}
static int setup(void *self) {
    if(!bound() || self!=service) return 0;
    /* No USB role swap, second iAP2 connection or early AirPlay thread. */
    return setup_complete(self);
}
static void notify(void *self,unsigned event) {
#ifdef MHI2Q_BRIDGE_SI_TRANSPORT
    if(bound())(void)mhi2q_now_playing_capture(manager);
#endif
    ((void (*)(void *,unsigned))(uintptr_t)0x1650dc)(self,event);
}
static void notify_base44(void *base,unsigned event) {
    notify((char *)base-44,event);
}
int mhi2q_si_transport_initialized(void *native_manager) {
    void *current=__sync_val_compare_and_swap(&service,NULL,NULL);
    return bound() && __sync_val_compare_and_swap(&setup_reported,0,0) &&
        native_manager==manager && current && get(current,536);
}
int mhi2q_si_transport_install(void *native_manager) {
    struct mhi2q_si_binding b=mhi2q_si_environment();
    const char *mode=getenv("MHI2Q_DIO_OWNER_MODE");
    if(!b.enabled) return 0;
    if(!bound() || !mode || strcmp(mode,"wireless") || !native_manager) return -1;
#ifdef MHI2Q_BRIDGE_SI_TRANSPORT
    extern int mhi2q_si_location_install(void);
    if(!mhi2q_si_location_install())return -1;
#endif
    volatile uint32_t *table=SERVICE_TABLE;
    if(installed) return manager==native_manager &&
        table[0]==(uint32_t)(uintptr_t)init && table[1]==(uint32_t)(uintptr_t)setup &&
        table[14]==(uint32_t)(uintptr_t)notify &&
        *NOTIFY_SLOT==(uint32_t)(uintptr_t)notify_base44 ? 1:-1;
    const uint32_t expected[]={0x165348,0x16652c,0x1652a0,0x1655a4,
        0x164ac0,0x164ea8,0x164f54,0x166b60,0x166d20,0x167174,
        0x164738,0x164d54,0x16471c,0x16615c,0x1650dc};
    for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);++i)
        if(table[i]!=expected[i]) return -1;
    volatile uint32_t *notify_slot=NOTIFY_SLOT;
    if(*notify_slot!=0x1650d4) return -1;
    manager=native_manager;
    table[0]=(uint32_t)(uintptr_t)init;
    table[1]=(uint32_t)(uintptr_t)setup;
    table[14]=(uint32_t)(uintptr_t)notify;
    *notify_slot=(uint32_t)(uintptr_t)notify_base44;
    __sync_synchronize();
    installed=1;
    fputs("[dio-si] SI-bound wireless transport installed\n",stderr);
    return 1;
}
