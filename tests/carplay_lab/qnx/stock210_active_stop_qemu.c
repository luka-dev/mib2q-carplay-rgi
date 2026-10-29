/* Included by the synthetic phone only. Preserve real active media on TERM,
 * then exercise explicit recovery after the phone sends TEARDOWN. */
#include "qnx_test_process.h"
static pid_t active_owner,active_mdns,active_daemon,active_monitor;
static int active_fail(const char *step){printf("[active-stop-qemu] FAIL %s\n",step);fflush(stdout);return -1;}
static int active_probe(void)
{
    pid_t p=spawnl(P_NOWAIT,"/mnt/pairing/bin/mhi2q_mdns_probe","mhi2q_mdns_probe",(char*)NULL);
    if(p<=1)return -1;
    for(int i=0;i<30;++i){int s=0;if(waitpid(p,&s,WNOHANG)==p)return WIFEXITED(s)&&WEXITSTATUS(s)==0?0:-1;usleep(100000);}
    kill(p,SIGKILL);waitpid(p,NULL,0);return -1;
}
static int lifecycle_active_stop(void)
{
    long runtime=0;FILE *f=fopen("/dev/shmem/stock210_supervisor_pid","r");
    if(!f)return -1;
    int read=fscanf(f,"%ld",&runtime);fclose(f);if(read!=1||runtime<=1)return -1;
    pid_t inner=qnx_test_child((pid_t)runtime,"/mnt/pairing/bin/wireless_dio_supervisor_bin");
    active_owner=qnx_test_owner("/mnt/pairing/owner.lock");
    active_mdns=qnx_test_child((pid_t)runtime,"/mnt/pairing/bin/mdnsd");
    active_daemon=qnx_test_child(inner,"/mnt/pairing/bin/mhi2q-wireless-daemon");
    active_monitor=qnx_test_child(inner,"/mnt/pairing/bin/sh");
    if(active_monitor<1)active_monitor=qnx_test_child(inner,"/bin/sh");
    printf("[active-stop-qemu] runtime=%ld inner=%ld owner=%ld mdns=%ld daemon=%ld monitor=%ld\n",
           runtime,(long)inner,(long)active_owner,(long)active_mdns,(long)active_daemon,(long)active_monitor);fflush(stdout);
    if(inner<=1||active_owner<=1||active_mdns<=1||active_daemon<=1||active_monitor<=1||
       lifecycle_bridge(3)!=3||active_probe())return -1;
    if(kill((pid_t)runtime,SIGTERM))return -1;
    int seen=0;
    for(int i=0;i<150;++i){long pid=0;int code=-1;f=fopen("/mnt/pairing/runtime-exit","r");
        if(f){read=fscanf(f,"%ld %d",&pid,&code);fclose(f);
            if(read==2&&pid==runtime&&code==1){seen=1;break;}}
        usleep(100000);}
    if(!seen||qnx_test_owner("/mnt/pairing/owner.lock")!=active_owner||
       !qnx_test_image(active_owner,-1,"/mnt/pairing/bin/dio_manager")||
       !qnx_test_image(active_daemon,-1,"/mnt/pairing/bin/mhi2q-wireless-daemon")||
       lifecycle_bridge(3)!=3||active_probe())return -1;
    puts("[active-stop-qemu] real media BUSY; TERM bounded; same owner, daemon and mDNS preserved");fflush(stdout);
    return 0;
}
static int active_stop_known(pid_t pid,const char *image)
{
    if(!qnx_test_alive(pid))return 0;
    if(!qnx_test_image(pid,-1,image)&&
       !(pid==active_monitor&&qnx_test_image(pid,-1,"/bin/sh")))return -1;
    if(kill(pid,SIGTERM))return -1;
    for(int i=0;i<50;++i){if(!qnx_test_alive(pid))return 0;usleep(100000);}
    return -1;
}
static int lifecycle_active_recovery(void)
{
    for(int i=0;i<100&&(qnx_test_owner("/mnt/pairing/owner.lock")>1||qnx_test_alive(active_owner));++i)usleep(100000);
    if(qnx_test_owner("/mnt/pairing/owner.lock")>1||qnx_test_alive(active_owner))return active_fail("owner release after TEARDOWN");
    puts("[active-stop-qemu] owner released after phone TEARDOWN");fflush(stdout);
    if(active_stop_known(active_monitor,"/mnt/pairing/bin/sh"))return active_fail("preserved monitor cleanup");
    if(active_stop_known(active_daemon,"/mnt/pairing/bin/mhi2q-wireless-daemon"))return active_fail("preserved daemon cleanup");
    /* TERM can interrupt the IE monitor after its fake command completed but
     * before Rust removed the pending lease. Only this disposable guest may
     * clear it: the recorded daemon and every fake CLI must already be dead.
     * This does not model recovery of a real driver ioctl on the HU. */
    for(int i=0;i<50&&qnx_test_child(-1,"/mnt/pairing/bin/fake_uaputl")>1;++i)usleep(100000);
    if(qnx_test_child(-1,"/mnt/pairing/bin/fake_uaputl")>1)return active_fail("fake IE command still running");
    FILE *pending=fopen("/tmp/mhi2q_wifi_control.pending","r");
    if(pending){long recorded=0;
        int got=fscanf(pending,"uaputl pending, daemon pid=%ld; recover HU before clearing",&recorded);fclose(pending);
        if(got!=1||recorded!=active_daemon||unlink("/tmp/mhi2q_wifi_control.pending"))return active_fail("foreign or uncleared fake IE lease");
        puts("[active-stop-qemu] stale fake-uaputl lease verified and cleared; no hardware ioctl");fflush(stdout);
    }else if(errno!=ENOENT)return active_fail("fake IE lease inspection");
    /* The independent phone does not inherit the supervisor shell's unit
     * environment. Supply the same synthetic identity to this cleanup child. */
    unsetenv("LD_PRELOAD");
    setenv("MHI2Q_CP_BT_MAC","00:01:02:03:04:05",1);
    setenv("MHI2Q_CP_DEVICE_ID","02:00:00:00:00:01",1);
    setenv("MHI2Q_CP_IE_INDEX","2",1);
    setenv("MHI2Q_CP_UAPUTL","/mnt/pairing/bin/fake_uaputl",1);
    pid_t clear=spawnl(P_NOWAIT,"/mnt/pairing/bin/mhi2q-wireless-daemon","mhi2q-wireless-daemon","--clear-ie",(char*)NULL);
    if(clear<=1||lifecycle_wait(clear))return active_fail("explicit IE cleanup command");
    struct{unsigned queries,sets,clears,present,unavailable;char hex[514];}ie={0};
    int fd=open("/dev/shmem/wifi_ie_probe_state",O_RDONLY);if(fd<0)return active_fail("IE state open");
    ssize_t got=read(fd,&ie,sizeof(ie));close(fd);
    if(got!=(ssize_t)sizeof(ie)||ie.present||!ie.clears)return active_fail("IE state after cleanup");
    if(active_probe()||active_stop_known(active_mdns,"/mnt/pairing/bin/mdnsd"))return active_fail("preserved mDNS cleanup");
    puts("[active-stop-qemu] active media refusal and explicit recovery passed");fflush(stdout);return 0;
}
