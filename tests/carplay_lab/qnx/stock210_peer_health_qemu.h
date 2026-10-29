/* Independent paired phone: real stock SI/DIO/HTTP/timing lifecycle. No
 * media liveness fields or timeout timestamps are written by this fixture. */
#include <time.h>
static uint64_t peer_health_clock(void) {
    struct timespec ts;if(clock_gettime(CLOCK_MONOTONIC,&ts))return 0;
    return (uint64_t)ts.tv_sec*1000+(unsigned)ts.tv_nsec/1000000;
}
static int peer_health_query(unsigned operation) {
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    struct timeval timeout={2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(57343);
    if(connect(fd,(struct sockaddr *)&address,sizeof(address))) {close(fd);return -1;}
    unsigned char request[32]={0},response[12];memcpy(request,"CPRX",4);request[4]=3;request[5]=(unsigned char)operation;
    uint64_t epoch=strtoull(getenv("MHI2Q_PEER_HEALTH_EPOCH"),NULL,16);
    uint32_t session=(uint32_t)strtoul(getenv("MHI2Q_PEER_HEALTH_SESSION"),NULL,10);
    uint32_t link=1;
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
    const char *link_env=getenv("MHI2Q_PEER_HEALTH_LINK");if(link_env)link=(uint32_t)strtoul(link_env,NULL,10);
#endif
    for(unsigned i=0;i<4;++i)request[8+i]=(unsigned char)(link>>(8*i));
    request[12]=(unsigned char)5000;request[13]=(unsigned char)(5000>>8);
    for(unsigned i=0;i<8;++i)request[16+i]=(unsigned char)(epoch>>(8*i));
    for(unsigned i=0;i<4;++i)request[24+i]=(unsigned char)(session>>(8*i));
    int result=-1;
    if(!write_all(fd,request,sizeof(request)) && !read_all(fd,response,sizeof(response)) &&
       !memcmp(response,"CPRA\x02\0\0\0",8) && !response[9] && !response[10] && !response[11])result=response[8];
    close(fd);return result;
}
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
static int integrated_child_exit(void) {
    pid_t child=(pid_t)strtol(getenv("MHI2Q_PEER_HEALTH_CHILD"),NULL,10);
    if(child<=1)return -11;
    for(unsigned drain=0;drain<200;++drain) {
        if(kill(child,0)<0 && errno==ESRCH) {
            puts("[integrated-media] daemon-driven native child exit PASS");fflush(stdout);
            return 0;
        }
        usleep(100000);
    }
    return -12;
}
static int integrated_partial_record(int control) {
    uint64_t begin=peer_health_clock();
    for(unsigned i=0;i<100;++i) {
        fd_set readable;FD_ZERO(&readable);FD_SET(control,&readable);
        struct timeval wait={0,500000};
        int ready=select(control+1,&readable,NULL,NULL,&wait);
        if(ready>0){
            unsigned char byte;ssize_t count=read(control,&byte,1);
            if(!count || (count<0&&errno==ECONNRESET)){
                uint64_t elapsed=peer_health_clock()-begin;
                printf("[integrated-media] SETUP without RECORD closed after %llu ms\n",(unsigned long long)elapsed);fflush(stdout);
                if(elapsed<28000||elapsed>40000)return -13;
                return integrated_child_exit();
            }
            return -14;
        }
        if(ready<0 && errno!=EINTR)return -15;
    }
    return -16;
}
#endif
static int peer_health_native(int control,struct timing_responder *timing,pthread_t thread) {
    int flags=fcntl(control,F_GETFL,0);
    if(flags<0 || fcntl(control,F_SETFL,flags|O_NONBLOCK))return -8;
    unsigned baseline=timing->responses,healthy_seconds=14;
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
    if(getenv("MHI2Q_INTEGRATED_FAULT") && !strcmp(getenv("MHI2Q_INTEGRATED_FAULT"),"rfcomm-gap"))healthy_seconds=10;
#endif
    for(unsigned i=0;i<healthy_seconds;++i) {
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
        /* Only the real Rust worker drives HEALTH in this profile. */
        if(peer_health_query(6)!=7)return -1;
#else
        if(peer_health_query(7)!=7 || peer_health_query(6)!=7)return -1;
#endif
        fd_set readable;FD_ZERO(&readable);FD_SET(control,&readable);
        struct timeval wait={0,0};
        if(select(control+1,&readable,NULL,NULL,&wait)!=0)return -7;
        sleep(1);
    }
    printf("[peer-health-qemu] periodic timing replies=%u\n",timing->responses-baseline);fflush(stdout);
    if(timing->responses-baseline<3)return -6;
    printf("[peer-health-qemu] static screen, only native timing replies for %us PASS\n",healthy_seconds);fflush(stdout);
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
    const char *crash=getenv("MHI2Q_INTEGRATED_FAULT");
    if(crash && !strcmp(crash,"rfcomm-gap") && access("/dev/shmem/integrated-rfcomm-gap",F_OK)!=0) {
        FILE *marker=fopen("/dev/shmem/integrated-rfcomm-gap","w");if(!marker)return -19;fclose(marker);
        unsigned gap;
        for(gap=0;gap<12;++gap) {
            if(peer_health_query(6)!=7)return -20;
            fd_set readable;FD_ZERO(&readable);FD_SET(control,&readable);struct timeval wait={1,0};
            if(select(control+1,&readable,NULL,NULL,&wait)!=0)return -21;
            if(gap>=4&&!access("/dev/shmem/integrated-semantic-reattached",F_OK))break;
        }
        if(gap==12)return -27;
        puts("[integrated-media] native IP media survived RFCOMM gap and reauthentication PASS");fflush(stdout);
    }

    if(crash && !strcmp(crash,"si-crash") && access("/dev/shmem/integrated-si-crashed",F_OK)!=0) {
        FILE *marker=fopen("/dev/shmem/integrated-si-crashed","w");if(!marker)return -22;fclose(marker);
        pid_t parent=(pid_t)strtol(getenv("MHI2Q_PEER_HEALTH_PARENT"),NULL,10);
        if(parent<=1||kill(parent,SIGKILL))return -23;
        puts("[integrated-media] abrupt owned SI process loss injected");fflush(stdout);
        __sync_lock_test_and_set(&timing->stop,1);pthread_join(thread,NULL);
        return integrated_child_exit();
    }
    if(crash && !strcmp(crash,"daemon-crash") && access("/dev/shmem/integrated-daemon-crashed",F_OK)!=0) {
        FILE *marker=fopen("/dev/shmem/integrated-daemon-crashed","w");if(!marker)return -24;fclose(marker);
        FILE *pid=fopen("/dev/shmem/integrated-daemon.pid","r");long victim=0;
        if(!pid||fscanf(pid,"%ld",&victim)!=1||victim<=1)return -25;
        fclose(pid);if(kill((pid_t)victim,SIGKILL))return -26;
        puts("[integrated-media] abrupt owned daemon process loss injected");fflush(stdout);
    }
    if(crash && !strcmp(crash,"dio-crash") && access("/dev/shmem/integrated-dio-crashed",F_OK)!=0) {
        FILE *marker=fopen("/dev/shmem/integrated-dio-crashed","w");if(!marker)return -18;fclose(marker);
        pid_t victim=(pid_t)strtol(getenv("MHI2Q_PEER_HEALTH_CHILD"),NULL,10);
        if(victim<=1||kill(victim,SIGKILL))return -17;
        puts("[integrated-media] abrupt owned DIO process loss injected");fflush(stdout);
        __sync_lock_test_and_set(&timing->stop,1);pthread_join(thread,NULL);
        return integrated_child_exit();
    }
#endif
    __sync_lock_test_and_set(&timing->stop,1);pthread_join(thread,NULL);
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
    const char *fault=getenv("MHI2Q_INTEGRATED_FAULT");
    if(fault && !strcmp(fault,"icmp")) {
        close(timing->fd);timing->fd=-1;
        puts("[integrated-media] ICMP timing port-unreachable injected");fflush(stdout);
    }
    FILE *silent=fopen("/dev/shmem/integrated-silent","w");
    if(!silent)return -10;
    fputs("1\n",silent);fclose(silent);
#endif
    /* Retain the bound UDP socket: silence models packet loss. Closing it
     * injects ICMP port-unreachable, a different native timing-thread path. */
    puts("[peer-health-qemu] timing responder stopped; control and screen sockets retained");fflush(stdout);
    uint64_t stopped=peer_health_clock();
    for(unsigned i=0;i<32;++i) {
        printf("[peer-health-qemu] silence poll=%u elapsed=%llu before health\n",i,(unsigned long long)(peer_health_clock()-stopped));fflush(stdout);
        int observed=peer_health_query(
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
            6
#else
            7
#endif
        );
        printf("[peer-health-qemu] silence flags=%d before control readiness\n",observed);fflush(stdout);
        fd_set readable;FD_ZERO(&readable);FD_SET(control,&readable);
        struct timeval wait={0,0};
        int ready=select(control+1,&readable,NULL,NULL,&wait);
        unsigned char byte;
        ssize_t count=ready>0?read(control,&byte,1):-1;
        if(count==0 || (ready>0 && count<0 && errno==ECONNRESET)) {
            uint64_t elapsed=peer_health_clock()-stopped;
            printf("[peer-health-qemu] native control close after timing stopped: %llu ms\n",(unsigned long long)elapsed);fflush(stdout);
            if(elapsed<6000 || elapsed>14000)return -2;
#ifdef MHI2Q_INTEGRATED_MEDIA_QEMU
            /* Keep the timing sink bound until the real daemon's CANCEL
             * and native SI reaper have completed. No fixture stop command. */
            return integrated_child_exit();
#else
            /* Keep the silent UDP endpoint alive until native timing teardown
             * finishes. Do not inject ICMP or race parent's CANCEL against it. */
            for(unsigned drain=0;drain<30;++drain) {
                int phase=peer_health_query(6);
                if(phase==4) {
                    puts("[peer-health-qemu] native ended observation after control EOF PASS");fflush(stdout);
                    puts("[peer-health-qemu] silent paired peer closed through native finalizer PASS");fflush(stdout);return 0;
                }
                usleep(100000);
            }
            return -9;
#endif
        }
        if(count>0)return -3;
        if(ready<0 && errno!=EINTR)return -4;
        usleep(500000);
    }
    return -5;
}
