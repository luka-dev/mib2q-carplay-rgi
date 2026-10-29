/* Native QNX fault matrix for the production runtime and stock DNS-SD.
 * The inner process is controlled here; real DIO/media refusal is tested by
 * the independent phone's active-stop scenario, not by this helper. */
#define _POSIX_C_SOURCE 200809L
#include <devctl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/procfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "qnx_test_process.h"

#define ROOT "/mnt/pairing/bin/"
#define INNER_PID "/mnt/pairing/fault-inner.pid"
#define MDNS_SOCKET "/var/run/mdnsd/mdnsd"
static volatile sig_atomic_t stopping;
static const char *test_name;
static struct stat owned_socket;
static void stop_signal(int n){(void)n;stopping=1;}
static void pause_ms(int n){struct timespec t={n/1000,(n%1000)*1000000L};nanosleep(&t,NULL);}
static uint64_t now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void require(int ok,const char *what){if(!ok){fprintf(stderr,"[runtime-fault-qemu] FAIL %s: %s errno=%d\n",test_name,what,errno);exit(1);}}
static int alive(pid_t pid){return qnx_test_alive(pid);}
static pid_t inner_pid(void){long n=0;FILE *f=fopen(INNER_PID,"r");if(f){if(fscanf(f,"%ld",&n)!=1)n=0;fclose(f);}return (pid_t)n;}
static int child_image(pid_t pid,pid_t parent,const char *binary)
{
    char path[80];snprintf(path,sizeof(path),"/proc/%ld/as",(long)pid);
    int fd=open(path,O_RDONLY);if(fd<0)return 0;
    procfs_info info;union {procfs_debuginfo info;char bytes[sizeof(procfs_debuginfo)+1024];} debug={0};
    int ok=!devctl(fd,DCMD_PROC_INFO,&info,sizeof(info),NULL)&&info.parent==parent&&
        !devctl(fd,DCMD_PROC_MAPDEBUG_BASE,&debug,sizeof(debug),NULL);
    close(fd);if(!ok)return 0;
    const char *actual=debug.info.path;if(*actual=='/')++actual;if(*binary=='/')++binary;
    return !strcmp(actual,binary);
}
static pid_t responder_child(pid_t runtime)
{
    DIR *d=opendir("/proc");if(!d)return -1;
    struct dirent *e;pid_t found=-1;
    while((e=readdir(d))){char *end;long n=strtol(e->d_name,&end,10);
        if(!*e->d_name||*end||n<=1)continue;
        if(child_image((pid_t)n,runtime,ROOT"mdnsd")){found=(pid_t)n;break;}}
    closedir(d);return found;
}
static int wait_status(pid_t p,int ms)
{
    uint64_t end=now_ms()+(unsigned)ms;int s=0;
    do {pid_t r=waitpid(p,&s,WNOHANG);if(r==p)return WIFEXITED(s)?WEXITSTATUS(s):128+WTERMSIG(s);
        if(r<0&&errno!=EINTR)return -2;
        pause_ms(20);}while(now_ms()<end);
    return -1;
}
static int probe(void)
{
    pid_t p=fork();if(!p){unsetenv("LD_PRELOAD");execl(ROOT"mhi2q_mdns_probe","mhi2q_mdns_probe",(char*)NULL);_exit(127);}
    if(p<0)return -1;
    int s=wait_status(p,700);if(s==-1){kill(p,SIGKILL);(void)wait_status(p,700);}return s;
}
static void await_ready(void){for(int i=0;i<30;++i){if(!probe())return;pause_ms(100);}require(0,"stock mDNS ready");}
static void clean_case(const char *name)
{
    test_name=name;unlink(INNER_PID);
    unsetenv("MHI2Q_FAULT_REFUSE");
    require(probe()==2,"previous responder and IPC removed");
}
static pid_t start_runtime(int refuse)
{
    pid_t p=fork();if(!p){unsetenv("LD_PRELOAD");setenv("MHI2Q_FAULT_ROLE","inner",1);
        if(refuse)setenv("MHI2Q_FAULT_REFUSE","1",1);
        execl(ROOT"wireless_runtime","wireless_runtime",ROOT"runtime-fault-qemu",(char*)NULL);_exit(127);}
    require(p>1,"fork runtime");return p;
}
static pid_t await_inner(pid_t runtime)
{
    for(int i=0;i<100;++i){pid_t p=inner_pid();if(p>1&&child_image(p,runtime,ROOT"runtime-fault-qemu"))return p;pause_ms(50);}
    require(0,"controlled inner started");return -1;
}
static pid_t owned_responder(pid_t runtime)
{
    pid_t p=responder_child(runtime);require(p>1,"owned stock responder identified by kernel parent and executable");
    require(!stat(MDNS_SOCKET,&owned_socket),"owned responder endpoint captured");return p;
}
static void recover_test_socket(pid_t previous)
{
    struct stat current;
    if(stat(MDNS_SOCKET,&current)){require(errno==ENOENT,"endpoint recovery stat");return;}
    require(!alive(previous)&&qnx_test_child(-1,ROOT"mdnsd")<1,"no live responder during fixture recovery");
    require(current.st_dev==owned_socket.st_dev&&current.st_ino==owned_socket.st_ino,"same captured stale endpoint");
    require(!unlink(MDNS_SOCKET),"fixture stale endpoint removal");
    puts("[runtime-fault-qemu] explicit fixture recovery removed captured stale IPC");fflush(stdout);
}
static void stop_orphan(pid_t p)
{
    require(alive(p),"preserved child still alive");kill(p,SIGKILL);
    for(int i=0;i<100&&alive(p);++i)pause_ms(20);
    require(!alive(p),"preserved test child removed");
}
static void pass(void){printf("[runtime-fault-qemu] PASS %s\n",test_name);fflush(stdout);}
static int inner(void)
{
    signal(SIGTERM,getenv("MHI2Q_FAULT_REFUSE")?SIG_IGN:stop_signal);
    FILE *f=fopen(INNER_PID,"w");if(!f)return 2;fprintf(f,"%ld\n",(long)getpid());fclose(f);
    while(!stopping)pause_ms(20);
    return 0;
}
int main(void)
{
    if(getenv("MHI2Q_FAULT_ROLE"))return inner();
    signal(SIGCHLD,SIG_DFL);unsetenv("LD_PRELOAD");
    setenv("LD_LIBRARY_PATH","/mnt/pairing/bin:/proc/boot:/eso/lib",1);
    setenv("MHI2Q_CP_MDNS_LOCK","/mnt/pairing/fault-runtime.lock",1);
    setenv("MHI2Q_DIO_OWNER_LOCK","/mnt/pairing/fault-owner.lock",1);
    setenv("MHI2Q_CP_MDNS_BINARY",ROOT"mdnsd",1);
    setenv("MHI2Q_CP_MDNS_PROBE",ROOT"mhi2q_mdns_probe",1);
    setenv("MHI2Q_CP_MDNS_PRELOAD",ROOT"libdio_capture_probe.so",1);
    setenv("MHI2Q_CP_MDNS_PROBE_MS","500",1);
    setenv("MHI2Q_CP_MDNS_HEALTH_MS","200",1);
    setenv("MHI2Q_CP_RUNTIME_STOP_MS","1000",1);
    puts("[runtime-fault-qemu] begin");fflush(stdout);

    clean_case("exclusive runtime and ordered stop");
    pid_t runtime=start_runtime(0),inner=await_inner(runtime),mdns=owned_responder(runtime);
    pid_t duplicate=start_runtime(0);require(wait_status(duplicate,3000)==75,"duplicate runtime rejected");
    require(responder_child(duplicate)<0,"duplicate starts no responder");
    kill(runtime,SIGTERM);require(wait_status(runtime,6000)==0,"normal stop result");
    require(!alive(inner)&&!alive(mdns),"owned children cleaned");pass();

    clean_case("external responder adoption and preservation");
    pid_t external=fork();if(!external){setenv("LD_PRELOAD",ROOT"libdio_capture_probe.so",1);execl(ROOT"mdnsd","mdnsd","-f",(char*)NULL);_exit(127);}
    require(external>1,"external responder fork");await_ready();
    runtime=start_runtime(0);inner=await_inner(runtime);
    require(responder_child(runtime)<0,"adoption launches no duplicate");
    kill(runtime,SIGTERM);require(wait_status(runtime,6000)==0,"adopted stop result");
    require(!alive(inner)&&alive(external)&&!probe(),"external responder remains ready");pass();

    test_name="unresponsive external IPC fails closed";unlink(INNER_PID);
    kill(external,SIGSTOP);runtime=start_runtime(0);
    require(wait_status(runtime,4000)==1,"hung initial IPC bounded");
    require(!inner_pid()&&alive(external),"no inner or signal to external responder");
    require(responder_child(runtime)<0,"no duplicate on hung IPC");
    kill(external,SIGCONT);await_ready();pass();
    kill(external,SIGTERM);require(wait_status(external,5000)>=0,"external cleanup");

    clean_case("owned responder crash and stale IPC refusal");runtime=start_runtime(0);inner=await_inner(runtime);mdns=owned_responder(runtime);
    kill(mdns,SIGKILL);require(wait_status(runtime,6000)==1,"owned crash fails runtime");
    require(!alive(inner)&&!alive(mdns),"crash leaves no owned children");
    /* SIGKILL leaves the factory Unix socket behind. Production must refuse
     * this uncertain endpoint, not unlink it or launch a duplicate. */
    unlink(INNER_PID);runtime=start_runtime(0);
    require(wait_status(runtime,4000)==1&&!inner_pid()&&responder_child(runtime)<0,"stale IPC refuses restart without new children");
    recover_test_socket(mdns);pass();

    clean_case("owned responder IPC loss");runtime=start_runtime(0);inner=await_inner(runtime);mdns=owned_responder(runtime);
    kill(mdns,SIGSTOP);require(wait_status(runtime,9000)==1,"lost IPC and stopped responder bounded");
    require(!alive(inner)&&!alive(mdns),"unresponsive owned children cleaned");recover_test_socket(mdns);pass();

    clean_case("inner stop refusal preserves discovery");runtime=start_runtime(1);inner=await_inner(runtime);mdns=owned_responder(runtime);
    kill(runtime,SIGTERM);require(wait_status(runtime,5000)==1,"inner refusal bounded");
    require(alive(inner)&&alive(mdns)&&!probe(),"refusing inner and responder preserved");pass();
    stop_orphan(inner);kill(mdns,SIGTERM);for(int i=0;i<100&&alive(mdns);++i)pause_ms(50);
    require(!alive(mdns),"preserved responder cleanup");

    clean_case("held QNX owner lock preserves discovery");
    int fd=open("/mnt/pairing/fault-owner.lock",O_RDWR|O_CREAT,0600);struct flock lock={0};
    lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
    require(fd>=0&&!fcntl(fd,F_SETLK,&lock),"hold actual QNX owner lock");
    require(!fcntl(fd,F_SETFD,FD_CLOEXEC),"owner descriptor close-on-exec");
    runtime=start_runtime(0);inner=await_inner(runtime);mdns=owned_responder(runtime);
    kill(runtime,SIGTERM);require(wait_status(runtime,5000)==1,"busy owner refuses responder teardown");
    require(!alive(inner)&&alive(mdns)&&!probe(),"discovery preserved with held lock");
    close(fd);kill(mdns,SIGTERM);for(int i=0;i<100&&alive(mdns);++i)pause_ms(50);
    require(!alive(mdns),"busy owner recovery cleanup");pass();
    require(probe()==2,"matrix final IPC absent");
    puts("[runtime-fault-qemu] all native runtime fault cases passed");fflush(stdout);return 0;
}
