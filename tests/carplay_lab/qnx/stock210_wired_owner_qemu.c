/* Disposable SI launcher: execute the real wired wrapper and stock DIO.
 * USB enumeration and a wired phone are outside this ownership test. */
#define _POSIX_C_SOURCE 200809L
#include <devctl.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/procfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void pause_short(void) { struct timespec t={0,100000000}; nanosleep(&t,NULL); }
static int owner_is(pid_t pid)
{
    int fd=open("/mnt/pairing/owner.lock",O_RDWR); struct flock lock={0};
    if(fd<0)return 0;
    lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
    int ok=!fcntl(fd,F_GETLK,&lock)&&lock.l_type==F_WRLCK&&lock.l_pid==pid;
    close(fd);return ok;
}
static int stock_image_and_wired_hook(pid_t pid)
{
    char path[80];snprintf(path,sizeof(path),"/proc/%ld/as",(long)pid);
    int fd=open(path,O_RDONLY),count=0,found=0;
    if(fd<0)return 0;
    union {procfs_debuginfo info;char bytes[sizeof(procfs_debuginfo)+1024];} debug={0};
    if(devctl(fd,DCMD_PROC_MAPDEBUG_BASE,&debug,sizeof(debug),NULL))goto done;
    const char *image=debug.info.path;if(*image=='/')++image;
    static int logged_image;
    if(strstr(image,"dio_manager")&&!logged_image){printf("[wired-stock-qemu] kernel executable=%s\n",image);fflush(stdout);logged_image=1;}
    if(strcmp(image,"mnt/pairing/bin/dio_manager"))goto done;
    if(devctl(fd,DCMD_PROC_MAPINFO,NULL,0,&count)||count<1||count>4096)goto done;
    procfs_mapinfo *maps=calloc((size_t)count,sizeof(*maps));
    if(!maps)goto done;
    int allocated=count;
    if(!devctl(fd,DCMD_PROC_MAPINFO,maps,(size_t)count*sizeof(*maps),&count)) {
        if(count>allocated)count=allocated;
        for(int i=0;i<count;++i) {
            memset(&debug,0,sizeof(debug));debug.info.vaddr=maps[i].vaddr;
            if(devctl(fd,DCMD_PROC_MAPDEBUG,&debug,sizeof(debug),NULL))continue;
            static int logged_hook;
            if(strstr(debug.info.path,"carplay_hook")&&!logged_hook){printf("[wired-stock-qemu] kernel hook map=%s\n",debug.info.path);fflush(stdout);logged_hook=1;}
            if(strstr(debug.info.path,"libcarplay_hook.so")&&maps[i].offset==0) {
                unsigned char actual[256],expected[256];
                int source=open("/mnt/pairing/wired-hooks/libcarplay_hook.so",O_RDONLY);
                if(source>=0){
                    if(read(source,expected,sizeof(expected))==(ssize_t)sizeof(expected)&&
                       pread(fd,actual,sizeof(actual),(off_t)maps[i].vaddr)==(ssize_t)sizeof(actual)&&
                       !memcmp(actual,expected,sizeof(actual)))found=1;
                    close(source);
                }
            }
            if(strstr(debug.info.path,"libcarplay_wireless")||strstr(debug.info.path,"libdio_receiver_bridge")) {
                found=0;break;
            }
        }
    }
    free(maps);
done:close(fd);return found;
}
static int probe(void)
{
    pid_t p=fork();if(!p){execl("/mnt/pairing/bin/mhi2q_mdns_probe","mhi2q_mdns_probe",(char*)NULL);_exit(127);}
    if(p<0)return -1;
    for(int i=0;i<30;++i){int s=0;pid_t r=waitpid(p,&s,WNOHANG);
        if(r==p)return WIFEXITED(s)&&WEXITSTATUS(s)==0?0:-1;
        pause_short();}
    kill(p,SIGKILL);waitpid(p,NULL,0);return -1;
}
int main(void)
{
    unsetenv("LD_PRELOAD");unsetenv("MHI2Q_DIO_OWNER_CLAIMED");
    unsetenv("MHI2Q_DIO_GENERATION_PUBLISHED");unsetenv("MHI2Q_CP_MDNS_MANAGED");
    setenv("H","/mnt/pairing/wired-hooks",1);
    setenv("DIODIR","/mnt/pairing/bin",1);
    setenv("WLOG","/mnt/pairing/wired-wrapper.log",1);
    setenv("OWNER_FILE","/mnt/pairing/supervisor.owner",1);
    setenv("IPL_CONFIG_DIR","/etc/eso/production",1);
    setenv("IPL_CONFIG_DIR_DIO_MANAGER","/etc/eso/production",1);
    pid_t child=fork();if(!child){execl("/mnt/pairing/bin/sh","sh",
        "/mnt/pairing/wired-hooks/carplay_startup.sh",(char*)NULL);_exit(127);}
    if(child<0)return 1;
    printf("[wired-stock-qemu] SI child pid=%ld\n",(long)child);fflush(stdout);
    int result=1,status=0,reaped=0;
    for(int i=0;i<350;++i){
        if(waitpid(child,&status,WNOHANG)==child){reaped=1;goto done;}
        if(owner_is(child)&&stock_image_and_wired_hook(child)) {
            printf("[wired-stock-qemu] wrapper became stock DIO pid=%ld; wired hook mapped; owner exclusive\n",(long)child);
            fflush(stdout);result=0;break;
        }
        pause_short();
    }
    if(result)goto done;
    if(probe()) {puts("[wired-stock-qemu] mDNS unavailable while wired owner active");result=1;goto done;}
    for(int i=0;i<30;++i){if(!owner_is(child)){result=1;goto done;}pause_short();}
done:
    if(!reaped){kill(child,SIGTERM);
        for(int i=0;i<100;++i){if(waitpid(child,&status,WNOHANG)==child){reaped=1;break;}pause_short();}
    }
    if(!reaped){
        /* No USB disconnect producer exists in this stand. Also exercise the
         * SI watchdog's bounded forced-stop case, for this known child only. */
        printf("[wired-stock-qemu] idle wired DIO TERM deadline; fixture SIGKILL pid=%ld\n",(long)child);fflush(stdout);
        kill(child,SIGKILL);waitpid(child,&status,0);
        if(!WIFSIGNALED(status)||WTERMSIG(status)!=SIGKILL)result=1;
    } else if(!WIFEXITED(status)||WEXITSTATUS(status))result=1;
    if(!result){int ready=0;for(int i=0;i<80;++i){if(!probe()){ready=1;break;}pause_short();}
        if(!ready){puts("[wired-stock-qemu] mDNS did not recover after wired shutdown");result=1;}}
    if(!result){puts("[wired-stock-qemu] real wired owner released; mDNS recovered");fflush(stdout);}
    return result;
}
