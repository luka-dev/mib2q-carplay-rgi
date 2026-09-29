/* Included only by the independent offline client, after read_all/write_all. */
static pid_t lifecycle_phone;
static unsigned lifecycle_cycle=1;
static char lifecycle_identity[253];
static int lifecycle_wait(pid_t child)
{
    unsigned i;int status;
    for(i=0;i<300;++i) {
        pid_t got=waitpid(child,&status,WNOHANG);
        if(got==child) {
            printf("[stock210-lifecycle] child=%ld wait status=%d\n",(long)child,status);fflush(stdout);
            return WIFEXITED(status)&&WEXITSTATUS(status)==0?0:-1;
        }
        if(got<0 && errno==EINTR)continue;
        if(got<0) {printf("[stock210-lifecycle] wait child=%ld errno=%d\n",(long)child,errno);fflush(stdout);return -1;}
        usleep(100000);
    }
    puts("[stock210-lifecycle] wait child timeout");fflush(stdout);return -1;
}
static int lifecycle_start_phone(void)
{
    unsigned i;
    signal(SIGCHLD,SIG_DFL);
    unlink("/dev/shmem/stock210_start_pi");
    setenv("LD_PRELOAD","",1);
    setenv("LD_LIBRARY_PATH","/mnt/pairing/bin:/proc/boot:/eso/lib",1);
    setenv("MHI2Q_PHONE_QEMU_REAL_RECEIVER","1",1);
    setenv("MHI2Q_PHONE_QEMU_HOLD_MEDIA","1",1);
    setenv("MHI2Q_PHONE_QEMU_CONTROL","1",1);
    lifecycle_phone=spawnl(P_NOWAIT,"/mnt/pairing/bin/phone-qemu-probe","phone-qemu-probe",(char *)NULL);
    printf("[stock210-lifecycle] phone pid=%ld\n",(long)lifecycle_phone);fflush(stdout);
    if(lifecycle_phone<0)return -1;
    for(i=0;i<1200;++i) {
        FILE *f=fopen("/dev/shmem/stock210_start_pi","r");
        if(f) {
            char value[253]={0};size_t n=fread(value,1,252,f);fclose(f);
            if(n) {
                if(lifecycle_identity[0] && strcmp(lifecycle_identity,value))return -1;
                strcpy(lifecycle_identity,value);
                printf("[stock210-lifecycle] iAP2 StartSession pi=%s cycle=%u\n",value,lifecycle_cycle);
                fflush(stdout);return 0;
            }
        }
        usleep(100000);
    }
    return -1;
}
static int lifecycle_bridge(unsigned op)
{
    int fd=socket(AF_INET,SOCK_STREAM,0),result=-1;
    struct sockaddr_in address={0};
    struct timeval timeout={5,0};
    uint8_t request[16]={'C','P','R','X',2,0,0,0,0,0,0,0,0,0,0,0},reply[8];
    if(fd<0)return -1;
    request[5]=(uint8_t)op;request[12]=(uint8_t)5000;request[13]=(uint8_t)(5000>>8);
    address.sin_family=AF_INET;address.sin_port=htons(57343);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    if(!connect(fd,(void *)&address,sizeof(address)) && !write_all(fd,request,sizeof(request)) &&
       !read_all(fd,reply,sizeof(reply)) && !memcmp(reply,"CPRA\x02",5))result=reply[5];
    close(fd);return result;
}
static int lifecycle_wired(void)
{
    int busy=lifecycle_bridge(3),unowned=lifecycle_bridge(4);
    printf("[stock210-lifecycle] active QUIESCE=%d unowned DRAIN=%d\n",busy,unowned);fflush(stdout);
    if(busy!=3 || unowned!=1)return -1;
    setenv("LD_PRELOAD","",1);
    setenv("MHI2Q_CP_AIRPLAY_PORT","5000",1);
    setenv("MHI2Q_DIO_OWNER_LOCK","/mnt/pairing/owner.lock",1);
    setenv("MHI2Q_DIO_WIRED_PRIORITY_LOCK","/mnt/pairing/wired.lock",1);
    setenv("MHI2Q_REQUIRE_BRIDGE_QUIESCE","1",1);
    pid_t child=spawnl(P_NOWAIT,"/mnt/pairing/bin/dio_owner_exec","dio_owner_exec","wired","/mnt/pairing/bin/wired-confirm-qemu",(char *)NULL);
    if(child<0 || lifecycle_wait(child))return -1;
    puts("[stock210-lifecycle] wired takeover and release verified");fflush(stdout);return 0;
}
static int lifecycle_stop(void)
{
    long pid=0;FILE *f=fopen("/dev/shmem/stock210_supervisor_pid","r");
    if(!f)return -1;
    int read=fscanf(f,"%ld",&pid);fclose(f);
    if(read!=1 || pid<=1 || kill((pid_t)pid,SIGTERM))return -1;
    return lifecycle_wait(lifecycle_phone);
}
static void lifecycle_dump_info(const uint8_t *body,size_t length)
{
    char line[16400];size_t i,used=(size_t)snprintf(line,sizeof(line),"[stock210-info] ");
    if(length>8000)return;
    for(i=0;i<length;++i)used+=(size_t)snprintf(line+used,sizeof(line)-used,"%02x",body[i]);
    line[used++]='\n';(void)write_all(1,line,used);
}
