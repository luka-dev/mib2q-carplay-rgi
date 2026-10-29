/* Process assertions for disposable QNX guests only. */
#ifndef QNX_TEST_PROCESS_H
#define QNX_TEST_PROCESS_H
#include <devctl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/procfs.h>
#include <sys/neutrino.h>
#include <sys/wait.h>
#include <unistd.h>
/* Orphans can remain as zombies in a guest without a reaping init. A zombie
 * has already released its descriptors/locks and must never be signalled. */
static inline int qnx_test_alive(pid_t pid)
{
    if(pid<=1)return 0;
    if(kill(pid,0)<0&&errno==ESRCH)return 0;
    char path[80];snprintf(path,sizeof(path),"/proc/%ld/as",(long)pid);
    int fd=open(path,O_RDONLY);if(fd<0)return 1;
    procfs_info info;int ok=!devctl(fd,DCMD_PROC_INFO,&info,sizeof(info),NULL);
    close(fd);return !ok||!(info.flags&_NTO_PF_ZOMBIE);
}
static inline int qnx_test_image(pid_t pid,pid_t parent,const char *binary)
{
    char path[80];snprintf(path,sizeof(path),"/proc/%ld/as",(long)pid);
    int fd=open(path,O_RDONLY);if(fd<0)return 0;
    procfs_info info;union{procfs_debuginfo info;char bytes[sizeof(procfs_debuginfo)+1024];}debug={0};
    int ok=!devctl(fd,DCMD_PROC_INFO,&info,sizeof(info),NULL)&&(parent<0||info.parent==parent)&&
        !devctl(fd,DCMD_PROC_MAPDEBUG_BASE,&debug,sizeof(debug),NULL);
    close(fd);if(!ok)return 0;
    const char *actual=debug.info.path;if(*actual=='/')++actual;if(*binary=='/')++binary;
    return !strcmp(actual,binary);
}
static inline pid_t qnx_test_child(pid_t parent,const char *image)
{
    DIR *d=opendir("/proc");if(!d)return -1;
    struct dirent *e;pid_t found=-1;
    while((e=readdir(d))){char *end;long n=strtol(e->d_name,&end,10);
        if(!*e->d_name||*end||n<=1)continue;
        if(qnx_test_image((pid_t)n,parent,image)){found=(pid_t)n;break;}}
    closedir(d);return found;
}
static inline pid_t qnx_test_owner(const char *path)
{
    int fd=open(path,O_RDWR);struct flock lock={0};if(fd<0)return -1;
    lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
    int ok=!fcntl(fd,F_GETLK,&lock);close(fd);
    return ok&&lock.l_type==F_WRLCK?lock.l_pid:-1;
}
#endif
