/* Native ASI controls for an admitted wireless child. The object/this and
 * destructors stay native; every control carries its SI birth context. */
#include "si_guard.h"
#include "../si/si_child_state.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define STATE_QUEUE 16u
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static struct si_child_state queue[STATE_QUEUE],birth;
static unsigned head,count,sequence,delivered_sequence;
static int running;
static uint32_t parent;
static char endpoint[104];
static uint32_t *object;
/* RTTI/offset prefix plus six interface methods and two destructors. */
static uint32_t table[10];
static void failed(void) {
    fputs("[dio-si] bound state delivery failed; terminating wireless child\n",stderr);
    kill(getpid(),SIGTERM);
}
static int exchange(const struct si_child_state *s) {
    int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return -1;
    struct timeval timeout={2,0};
    if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))||
       setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout))){close(fd);return -1;}
    struct sockaddr_un address;memset(&address,0,sizeof(address));address.sun_family=AF_UNIX;
    strcpy(address.sun_path,endpoint);
    if(connect(fd,(struct sockaddr *)&address,sizeof(address))){close(fd);return -1;}
    unsigned char frame[SI_CHILD_STATE_SIZE];si_child_state_encode(s,frame);
    for(unsigned direction=0;direction<2;++direction) {
        size_t used=0;
        while(used<sizeof(frame)) {
            ssize_t n=direction?recv(fd,frame+used,sizeof(frame)-used,0):send(fd,frame+used,sizeof(frame)-used,0);
            if(n<0&&errno==EINTR)continue;
            if(n<=0){close(fd);return -1;}used+=(size_t)n;
        }
    }
    close(fd);
    fprintf(stderr,"[dio-si] bound state seq=%u state=%u result=%u\n",s->sequence,s->state,si_cs_get(frame+32));
    return memcmp(frame,"SIR2",4)||frame[4]!=2||frame[5]!=8||
        si_cs_get(frame+8)!=sizeof(frame)||si_cs_get(frame+12)!=s->sequence||
        (si_cs_get(frame+16)|((uint64_t)si_cs_get(frame+20)<<32))!=s->epoch||
        si_cs_get(frame+32)!=0 ? -1:0;
}
static void *worker(void *unused) {
    (void)unused;sigset_t signals;sigemptyset(&signals);sigaddset(&signals,SIGPIPE);
    if(pthread_sigmask(SIG_BLOCK,&signals,NULL)){failed();return NULL;}
    for(;;) {
        pthread_mutex_lock(&mutex);
        while(!count)pthread_cond_wait(&changed,&mutex);
        struct si_child_state s=queue[head];pthread_mutex_unlock(&mutex);
        /* An ACK lost after execution is safe to retry: SI retains the
         * result by child/session/sequence and never executes it twice. */
        int result=-1;
        for(unsigned attempt=0;attempt<3;++attempt) {
            if((uint32_t)getppid()!=parent)break;
            result=exchange(&s);if(!result)break;usleep(100000);
        }
        if(result){failed();return NULL;}
        pthread_mutex_lock(&mutex);head=(head+1)%STATE_QUEUE;--count;
        delivered_sequence=s.sequence;pthread_mutex_unlock(&mutex);
    }
}
static void terminal_barrier(unsigned target) {
    struct timespec start,now;
    if(clock_gettime(CLOCK_MONOTONIC,&start)){failed();return;}
    for(;;) {
        pthread_mutex_lock(&mutex);int done=delivered_sequence>=target;pthread_mutex_unlock(&mutex);
        if(done)return;
        if(clock_gettime(CLOCK_MONOTONIC,&now)||
           (now.tv_sec-start.tv_sec)*1000+(now.tv_nsec-start.tv_nsec)/1000000>=7000) {
            failed();return;
        }
        usleep(10000);
    }
}
static void *emit_event(void *self,unsigned kind,unsigned state,unsigned cookie) {
    if(self!=object||kind>SI_ASI_RESTART){failed();return NULL;}
    pthread_mutex_lock(&mutex);
    if(count==STATE_QUEUE||sequence==UINT32_MAX){pthread_mutex_unlock(&mutex);failed();return NULL;}
    struct si_child_state s=birth;s.sequence=++sequence;s.state=state;
    s.kind=kind;s.cookie=cookie;
    queue[(head+count)%STATE_QUEUE]=s;++count;pthread_cond_signal(&changed);
    pthread_mutex_unlock(&mutex);
    /* exitProcess emits DEINITIALIZED immediately before process teardown.
     * An async queue alone can lose it: SI then sees an unexpected death and
     * restarts DIO instead of completing cleanup. Wait only at this terminal
     * boundary, outside the queue lock, with a monotonic deadline. */
    if(kind==SI_ASI_STATE&&(state==4||state==6))terminal_barrier(s.sequence);
    return NULL;
}
static void *emit(void *self,unsigned state){return emit_event(self,SI_ASI_STATE,state,0);}
static void *watchdog(void *self,unsigned cookie,unsigned state){return emit_event(self,SI_ASI_WATCHDOG,state,(uint16_t)cookie);}
static void *flush_done(void *self,unsigned result){return emit_event(self,SI_ASI_FLUSH,(uint8_t)result,0);}
static void *teardown(void *self){return emit_event(self,SI_ASI_TEARDOWN,0,0);}
static void *restart(void *self){return emit_event(self,SI_ASI_RESTART,0,0);}
/* SI already owns the authenticated phone name/identity. Native wireless
 * SMetaInfo supplies a generic USB volume name; never let it overwrite that
 * identity or enqueue an uncorrelated old-child metadata event. */
static void *volume_name(void *self,void *info){(void)self;(void)info;return NULL;}
int mhi2q_si_state_install(void *client) {
    struct mhi2q_si_binding b=mhi2q_si_environment();
    if(!b.valid||!b.enabled||(uint32_t)getppid()!=b.parent||!client)return 0;
    const char *socket_path=getenv("MHI2Q_CP_SI_STATE_SOCKET");
    if(!socket_path||!*socket_path||strlen(socket_path)>=sizeof(endpoint))return 0;
    uint32_t *registration=(uint32_t *)(uintptr_t)((uint32_t *)client)[1];
    if(!registration||registration[1]>4096){fputs("[dio-si] invalid ASI registration\n",stderr);return 0;}
    uint32_t *target=(uint32_t *)(uintptr_t)registration[registration[1]+18];
    if(!target||!target[0]){fputs("[dio-si] ASI state target not ready\n",stderr);return 0;}
    if(running)return target==object&&target[0]==(uint32_t)(uintptr_t)(table+2);
    uint32_t *vtable=(uint32_t *)(uintptr_t)target[0];
    for(unsigned i=0;i<8;++i)if(!vtable[i])return 0;
    memcpy(table,vtable-2,sizeof(table));table[2]=(uint32_t)(uintptr_t)emit;
    table[3]=(uint32_t)(uintptr_t)volume_name;table[4]=(uint32_t)(uintptr_t)watchdog;
    table[5]=(uint32_t)(uintptr_t)flush_done;table[6]=(uint32_t)(uintptr_t)teardown;table[7]=(uint32_t)(uintptr_t)restart;
    birth.epoch=b.epoch;birth.session=b.session;birth.link=b.link;birth.child=(uint32_t)getpid();parent=b.parent;
    strcpy(endpoint,socket_path);
    pthread_t thread;if(pthread_create(&thread,NULL,worker,NULL))return 0;
    pthread_detach(thread);object=target;__sync_synchronize();
    target[0]=(uint32_t)(uintptr_t)(table+2);running=1;
    fputs("[dio-si] generation-bound ASI state publisher installed\n",stderr);return 1;
}
