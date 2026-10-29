/* QNX 6.5 read-only /proc observer. Does not stop, signal, ptrace, or inject. */
#define _POSIX_C_SOURCE 200112L
#include <stdint.h>
#include <sys/procfs.h>
#include <sys/neutrino.h>
#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define LIMIT 512
struct previous { uint64_t start,cpu; unsigned sample; int present; };
static uint64_t now_ns(void)
{struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
int rl_parse_unsigned(const char *text,unsigned minimum,unsigned maximum,unsigned *out)
{
    unsigned v=0;const unsigned char *s=(const unsigned char *)text;
    if(!s || !*s || !out || minimum>maximum)return EINVAL;
    for(;*s;++s){unsigned d;if(*s<'0'||*s>'9')return EINVAL;d=*s-'0';
        if(v>UINT_MAX/10u || (v==UINT_MAX/10u && d>UINT_MAX%10u))return ERANGE;
        v=v*10u+d;}
    if(v<minimum || v>maximum)return ERANGE;
    *out=v;return 0;
}

int main(int argc,char **argv)
{
    unsigned pid,samples=20,interval=500,iteration;
    char path[64];int fd;uint64_t last_time=0;struct previous previous[LIMIT];
    if(argc<2 || argc>4 || rl_parse_unsigned(argv[1],1,INT_MAX,&pid) ||
       (argc>2 && rl_parse_unsigned(argv[2],1,120,&samples)) ||
       (argc>3 && rl_parse_unsigned(argv[3],100,5000,&interval))){
        fprintf(stderr,"usage: thread_observer PID [samples:1..120] [interval_ms:100..5000]\n");return 2;}
    snprintf(path,sizeof(path),"/proc/%u/as",pid);fd=open(path,O_RDONLY);
    if(fd<0){perror(path);return 1;}
    memset(previous,0,sizeof(previous));
    puts("sample,mono_ns,pid,tid,start_ns,state,base_priority,effective_priority,policy,last_cpu,cpu_delta_ns,wall_delta_ns,cpu_one_core_percent,blocked_ns,delta_valid");
    for(iteration=0;iteration<samples;++iteration){
        uint64_t timestamp=now_ns(),delta=timestamp>last_time?timestamp-last_time:0;
        unsigned tid=1,count=0;
        for(;count<LIMIT;++count){
            procfs_status s;int error,valid=0;uint64_t cpu=0;
            memset(&s,0,sizeof(s));s.tid=tid;
            error=devctl(fd,DCMD_PROC_TIDSTATUS,&s,sizeof(s),NULL);
            if(error==ESRCH)break;
            if(error){fprintf(stderr,"TIDSTATUS tid=%u error=%d\n",tid,error);close(fd);return 1;}
            if((unsigned)s.tid<tid){fprintf(stderr,"nonprogressing tid\n");close(fd);return 1;}
            /* QNX returns the next existing tid when a requested tid vanished. */
            tid=(unsigned)s.tid;
            if(tid<LIMIT){struct previous *p=&previous[tid];
                valid=p->present && p->sample+1==iteration && p->start==s.start_time && s.sutime>=p->cpu && delta;
                if(valid)cpu=s.sutime-p->cpu;
                p->present=1;p->sample=iteration;p->start=s.start_time;p->cpu=s.sutime;}
            printf("%u,%llu,%u,%u,%llu,%u,%u,%u,%u,%u,%llu,%llu,%.3f,%llu,%d\n",
                iteration,(unsigned long long)timestamp,pid,tid,(unsigned long long)s.start_time,
                s.state,s.real_priority,s.priority,s.policy,s.last_cpu,
                (unsigned long long)cpu,(unsigned long long)delta,
                valid?100.0*(double)cpu/(double)delta:0.0,
                (unsigned long long)s.nsec_since_block,valid);
            if(tid==UINT_MAX)break;
            ++tid;
        }
        if(count==LIMIT)fprintf(stderr,"thread scan truncated at %u entries\n",LIMIT);
        fflush(stdout);last_time=timestamp;
        if(iteration+1<samples){struct timespec wait={interval/1000,(long)(interval%1000)*1000000L};
            while(nanosleep(&wait,&wait) && errno==EINTR){} }
    }
    close(fd);return 0;
}
