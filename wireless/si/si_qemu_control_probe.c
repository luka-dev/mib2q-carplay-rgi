/* Disposable QNX acceptance client. Real SIW2 socket/queue and native C++
 * discovery operations; /proc reads observe the actual SI vectors. No native
 * memory writes, fake service replies or physical USB operations. */
#include "si_wire.h"
#include "si_protocol.h"
#include "si_runtime_guard.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/procfs.h>
#include <devctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <spawn.h>
#include <sys/wait.h>

/* One write per receipt: unbuffered printf otherwise interleaves with the
 * native tracing stream between each formatted field on the QNX console. */
static int report(const char *format,...) {
    char line[512]; va_list args;va_start(args,format);
    int size=vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(size>0 && size<(int)sizeof(line)) (void)write(1,line,(size_t)size);
    return size;
}
#define printf report
static void put(unsigned char *p, unsigned off, uint64_t v, unsigned n) {
    for (unsigned i=0; i<n; ++i) p[off+i]=(unsigned char)(v>>(i*8));
}
static uint64_t get(const unsigned char *p, unsigned off, unsigned n) {
    uint64_t v=0; for (unsigned i=0; i<n; ++i) v|=(uint64_t)p[off+i]<<(i*8); return v;
}
static void require(int ok, const char *what) {
    if (!ok) { fprintf(stderr,"SI-NATIVE: FAIL %s errno=%d\n",what,errno); exit(1); }
}
static int memory=-1;
static unsigned last_observation;
static uint32_t word(uint32_t va) {
    uint32_t value;
    require(pread(memory,&value,sizeof(value),(off_t)va)==sizeof(value),"read SI memory");
    return value;
}
static uint32_t dp;
static uint32_t session, fixture_si_pid, last_child;
static char pairing_identity[253];
static uint32_t count(void) { return word(dp+440); }
static uint32_t stored(void) { return word(dp+544); }
static void check_record(uint32_t id, const char *name) {
    uint32_t array=word(dp+448), n=count();
    require(n && n<=100 && array,"present vector valid");
    for (uint32_t i=0; i<n; ++i) {
        uint32_t entry=word(array+i*4);
        if (entry && word(entry)==id) {
            require(word(entry+28)==2 && word(entry+32)==2,"native WLAN transport and CarPlay method");
            /* UnicodeString8::utf8/libutil 0x3fdfc and raw-copy 0x311ec:
             * +8 byte length; inline +12, or refcounter at +12 -> buffer. */
            uint32_t string=entry+36, data=string+12;
            if ((word(string+4)>>16)&1) data=word(word(string+12));
            char bytes[SI_PHONE_NAME_SIZE];
            size_t length=strlen(name);
            require(word(string+8)==length &&
                    pread(memory,bytes,length,(off_t)data)==(ssize_t)length &&
                    !memcmp(bytes,name,length),"native UTF-8 name preserved");
            return;
        }
    }
    require(0,"device present in native vector");
}
static void transfer(int fd, unsigned char *p, size_t n, int writing) {
    while (n) {
        ssize_t used=writing?write(fd,p,n):read(fd,p,n);
        if (used<0 && errno==EINTR) continue;
        require(used>0,"SIW2 transfer"); p+=used; n-=(size_t)used;
    }
}
static void ask(unsigned op, uint32_t request, uint64_t epoch, const char *name,
                unsigned expected, unsigned char *reply) {
    unsigned char frame[SI_WIRE_SIZE]={0};
    memcpy(frame,"SIW2",4); frame[4]=2; frame[5]=(unsigned char)op;
    put(frame,8,sizeof(frame),4); put(frame,12,request,4); put(frame,16,epoch,8);
    put(frame,24,12345,4); put(frame,28,1,4);
    put(frame,160,session,4);
    strcpy((char *)frame+32,"02:11:22:33:44:55");
    strcpy((char *)frame+64,name);
    struct si_request decoded;
    require(si_wire_decode(frame,&decoded),"fixture frame valid");
    int fd=socket(AF_UNIX,SOCK_STREAM,0);
    require(fd>=0,"socket");
    struct timeval timeout={3,0};
    require(!setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) &&
            !setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)),"socket deadline");
    struct sockaddr_un address;
    memset(&address,0,sizeof(address)); address.sun_family=AF_UNIX;
    strcpy(address.sun_path,"/tmp/mhi2q_si_control.sock");
    require(!connect(fd,(struct sockaddr *)&address,sizeof(address)),"connect SI endpoint");
    transfer(fd,frame,sizeof(frame),1); transfer(fd,reply,SI_WIRE_SIZE,0); close(fd);
    require(!memcmp(reply,"SIR2",4) && reply[4]==2 && reply[5]==op &&
            get(reply,8,4)==SI_WIRE_SIZE && get(reply,12,4)==request,"reply binding");
    printf("SI-NATIVE: op=%u req=%u result=%u device=%u present=%u\n",op,request,
           (unsigned)get(reply,32,4),(unsigned)get(reply,36,4),(unsigned)get(reply,60,4));
    require(get(reply,32,4)==expected,"operation result");
}
static uint32_t mdns_pid(void) {
    FILE *file=fopen("/var/run/mdnsd/mdnsd.pid","r");unsigned long pid=0;
    require(file && fscanf(file,"%lu",&pid)==1 && pid>1 && pid<=INT32_MAX,"native mDNS pidfile");
    fclose(file);require(!kill((pid_t)pid,0),"external mDNS alive");return (uint32_t)pid;
}
static int receiver(uint64_t epoch, unsigned operation) {
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0) return -1;
    struct timeval deadline={7,0};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&deadline,sizeof(deadline));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&deadline,sizeof(deadline));
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr)); addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK); addr.sin_port=htons(57343);
    if(connect(fd,(struct sockaddr *)&addr,sizeof(addr))) { close(fd);return -1; }
    unsigned char frame[32]={0}, reply[8];
    memcpy(frame,"CPRX",4);frame[4]=3;frame[5]=(unsigned char)operation;
    put(frame,8,1,4);put(frame,12,5000,2);put(frame,16,epoch,8);put(frame,24,session,4);
    ssize_t sent=send(fd,frame,sizeof(frame),0), got=recv(fd,reply,sizeof(reply),MSG_WAITALL);
    if(sent==sizeof(frame) && got==sizeof(reply) && !memcmp(reply,"CPRA",4) &&
       !reply[5] && operation==5) {
        unsigned char length[2];char identity[253];
        transfer(fd,length,2,0);unsigned size=(unsigned)get(length,0,2);
        require(size && size<sizeof(identity),"bounded receiver pairing identity");
        transfer(fd,(unsigned char *)identity,size,0);identity[size]=0;
        if(pairing_identity[0]) require(!strcmp(pairing_identity,identity),"pairing identity retained on reconnect");
        else memcpy(pairing_identity,identity,size+1);
    }
    if(sent==sizeof(frame) && got==sizeof(reply) && !memcmp(reply,"CPRA",4) &&
       !reply[5] && (operation==6 || operation==7)) {
        unsigned char state[4];transfer(fd,state,sizeof(state),0);
        require(!state[1] && !state[2] && !state[3] && state[0]<=7,"bounded media observation");
        last_observation=state[0];
    }
    close(fd);
    if(sent!=sizeof(frame) || got!=sizeof(reply) || memcmp(reply,"CPRA",4)) return -1;
    return reply[5];
}
static int native_child(uint32_t child) {
    char path[64];snprintf(path,sizeof(path),"/proc/%u/as",child);
    int fd=open(path,O_RDONLY);if(fd<0)return 0;
    union {procfs_debuginfo info;char bytes[sizeof(procfs_debuginfo)+512];} object;
    memset(&object,0,sizeof(object));
    int ok=!devctl(fd,DCMD_PROC_MAPDEBUG_BASE,&object,sizeof(object),NULL);
    procfs_info info;memset(&info,0,sizeof(info));
    ok=ok && !devctl(fd,DCMD_PROC_INFO,&info,sizeof(info),NULL) &&
        info.parent==(pid_t)fixture_si_pid;
    object.bytes[sizeof(object.bytes)-1]=0;
    close(fd);
    return ok && (!strcmp(object.info.path,"/ramdisk/dio_manager") ||
                  !strcmp(object.info.path,"ramdisk/dio_manager"));
}
static void launched(uint64_t epoch, unsigned request, unsigned char *reply) {
    session=(uint32_t)get(reply,40,4);
    require(session!=0,"active session generation");
    uint32_t child=0;
    for (unsigned i=0;i<45;++i) {
        sleep(1);
        ask(SI_SNAPSHOT,request+i,epoch,"Renamed QEMU",SI_OK,reply);
        child=(uint32_t)get(reply,56,4);
        printf("SI-NATIVE: launch app=%d state=%u child=%u\n",
               (int32_t)get(reply,44,4),(unsigned)get(reply,48,4),child);
        if(child>1 && native_child(child)) {
            int ready=receiver(epoch,1);
            printf("SI-NATIVE: native DIO CPRX ready=%d\n",ready);
            if(ready==0) {
                require(receiver(epoch^1,1)==2,"stale CPRX epoch rejected");
                require(receiver(epoch,5)==0,"native receiver pairing identity available");
                require(receiver(epoch,6)==0 && last_observation==0,"read-only idle media snapshot");
                require(receiver(epoch^1,7)==2,"stale liveness epoch rejected");
                require(receiver(epoch,7)==0 && last_observation==0,"idle health check preserves receiver");
                require(receiver(epoch,1)==0,"readiness preserved after observation");
                puts("SI-NATIVE: bound media status and health idle safety PASS");
                last_child=child;
                puts("SI-NATIVE: SI-owned DIO receiver ready PASS");
                return;
            }
        }
    }
    require(0,"native SI-owned DIO launched");
}
int main(int argc, char **argv) {
    int dio=argc==2 && !strcmp(argv[1],"--dio");
    setvbuf(stdout,NULL,_IONBF,0);
    FILE *file=fopen("/ramdisk/var/run/smartphone_integrator.pid","r");
    long pid=0;
    require(file && fscanf(file,"%ld",&pid)==1 && pid>1,"native SI pidfile");
    fclose(file); fixture_si_pid=(uint32_t)pid;
    char path[64]; snprintf(path,sizeof(path),"/proc/%ld/as",pid);
    memory=open(path,O_RDONLY); require(memory>=0,"open SI read-only");
    uint32_t controller=word(SI_CONTROLLER_PTR);
    require(controller>=0x10000 && controller<0xffff0000,"controller pointer");
    dp=controller+264;
    printf("SI-NATIVE: configuration usb_mode=%u carplay_coding=%u fec_carplay=%u fec_usb=%u\n",
           word(dp+1040),(word(dp+1008)>>8)&1,word(dp+1020),word(dp+1028));
    uint32_t responder=dio?mdns_pid():0;
    uint32_t initial=count(), initial_stored=stored();
    unsigned char reply[SI_WIRE_SIZE], original[SI_WIRE_SIZE];
    ask(SI_HELLO,1,0,"QEMU Phone",SI_OK,reply);
    uint64_t epoch=get(reply,16,8); require(epoch!=0,"nonzero epoch");
    ask(SI_SNAPSHOT,2,epoch,"QEMU Phone",SI_OK,reply);
    require((int32_t)get(reply,44,4)==-1,"SI initially idle");
    ask(SI_UPSERT_PHONE,3,epoch,"QEMU Телефон",SI_OK,reply);
    uint32_t id=(uint32_t)get(reply,36,4);
    require(id && count()==initial+1 && stored()==initial_stored+1,"native upsert counts");
    check_record(id,"QEMU Телефон"); memcpy(original,reply,sizeof(original));
    ask(SI_UPSERT_PHONE,3,epoch,"QEMU Телефон",SI_OK,reply);
    require(!memcmp(original,reply,sizeof(original)) && count()==initial+1,"idempotent exact replay");
    ask(SI_UPSERT_PHONE,4,epoch,"Renamed QEMU",SI_OK,reply);
    require(get(reply,36,4)==id && count()==initial+1 && stored()==initial_stored+1,"rename keeps ID");
    check_record(id,"Renamed QEMU");
    /* This minimal mesh has no persistence/coding/FEC providers. The actual
     * native admission must refuse activation rather than bypass those gates. */
    ask(SI_ACTIVATE,5,epoch,"Renamed QEMU",dio?SI_OK:SI_DENIED,reply);
    if (dio) launched(epoch,100,reply);
    session=(uint32_t)get(reply,40,4);
    if(dio && getenv("MHI2Q_QEMU_PEER_HEALTH")) {
        char epoch_text[32],session_text[32];
        snprintf(epoch_text,sizeof(epoch_text),"%llx",(unsigned long long)epoch);
        snprintf(session_text,sizeof(session_text),"%u",session);
        setenv("MHI2Q_PEER_HEALTH_EPOCH",epoch_text,1);
        setenv("MHI2Q_PEER_HEALTH_SESSION",session_text,1);
        unsetenv("LD_PRELOAD");
        int fd=open("/dev/shmem/independent_srp_ready",O_WRONLY|O_CREAT,0600);
        require(fd>=0,"health phone readiness marker");close(fd);
        pid_t phone=spawnl(P_NOWAIT,"/proc/boot/independent-health-phone","independent-health-phone",(char *)NULL);
        require(phone>1,"independent paired health phone spawn");
        int status=0;require(waitpid(phone,&status,0)==phone && WIFEXITED(status) && !WEXITSTATUS(status),"native peer-health phone acceptance");
    }
    if (!dio) require((int32_t)get(reply,44,4)==-1 && !get(reply,56,4),"denied activation leaves no child/context");
    ask(SI_CANCEL,200,epoch,"Renamed QEMU",SI_OK,reply);
    if (dio) {
        for(unsigned i=0;i<15;++i) {
            ask(SI_SNAPSHOT,210+i,epoch,"Renamed QEMU",SI_OK,reply);
            if((int32_t)get(reply,44,4)==-1 && !get(reply,56,4)) break;
            sleep(1);
        }
        require((int32_t)get(reply,44,4)==-1 && !get(reply,56,4),"native cancel completes");
        puts("SI-NATIVE: SI-owned DIO cancel PASS");
    }
    ask(SI_REMOVE_PHONE,300,epoch,"Renamed QEMU",SI_OK,reply);
    require(count()==initial && stored()==initial_stored+1,"native present removal retains stored identity");
    ask(SI_UPSERT_PHONE,301,epoch^1,"Stale QEMU",SI_STALE,reply);
    require(count()==initial,"stale epoch cannot insert");
    ask(SI_UPSERT_PHONE,302,epoch,"Reconnected QEMU",SI_OK,reply);
    require(get(reply,36,4)==id && count()==initial+1 && stored()==initial_stored+1,"native stored ID reused");
    check_record(id,"Reconnected QEMU");
    if(dio) {
        uint32_t old_session=session, old_child=last_child;
        session=0;
        ask(SI_ACTIVATE,303,epoch,"Reconnected QEMU",SI_OK,reply);
        launched(epoch,400,reply);
        require(session!=old_session && last_child!=old_child,"fresh SI child/session after reconnect");
        ask(SI_CANCEL,500,epoch,"Reconnected QEMU",SI_OK,reply);
        for(unsigned i=0;i<15;++i) {
            ask(SI_SNAPSHOT,510+i,epoch,"Reconnected QEMU",SI_OK,reply);
            if((int32_t)get(reply,44,4)==-1 && !get(reply,56,4))break;
            sleep(1);
        }
        require((int32_t)get(reply,44,4)==-1 && !get(reply,56,4),"reconnect cancel completes");
        require(kill((pid_t)last_child,0)<0 && errno==ESRCH,"SI child exited and reaped");
        require(mdns_pid()==responder,"external mDNS retained across DIO generations");
        puts("SI-NATIVE: external mDNS preserved PASS");
        puts("SI-NATIVE: SI-owned DIO reconnect identity and cleanup PASS");
    }
    ask(SI_REMOVE_PHONE,700,epoch,"Reconnected QEMU",SI_OK,reply);
    require(count()==initial,"final native cleanup");
    close(memory);
    if(!dio) puts("SI-NATIVE: native admission refusal and idle cancel PASS");
    puts("SI-NATIVE: discovery/replay/rename/remove/reconnect/stale PASS");
    return 0;
}
