/* Exercise the production worker/terminal barrier against a real Unix peer.
 * The test never calls the pinned 32-bit native object installer. */
#include "si_state.c"
#include <assert.h>
#include <sys/wait.h>
static void full(int fd,void *bytes,size_t size,int writing) {
    unsigned char *p=bytes;size_t used=0;
    while(used<size) {
        ssize_t n=writing?write(fd,p+used,size-used):read(fd,p+used,size-used);
        if(n<0&&errno==EINTR)continue;
        assert(n>0);used+=(size_t)n;
    }
}
static uint64_t milliseconds(void) {
    struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));
    return (uint64_t)t.tv_sec*1000+(unsigned)t.tv_nsec/1000000;
}
static void run_case(int bad_ack) {
    char path[104];snprintf(path,sizeof(path),"/tmp/si-state-%ld-%d.sock",(long)getpid(),bad_ack);
    unlink(path);int listener=socket(AF_UNIX,SOCK_STREAM,0),notice[2];assert(listener>=0&&!pipe(notice));
    struct sockaddr_un address;memset(&address,0,sizeof(address));address.sun_family=AF_UNIX;strcpy(address.sun_path,path);
    assert(!bind(listener,(struct sockaddr *)&address,sizeof(address))&&!listen(listener,4));
    pid_t child=fork();assert(child>=0);
    if(!child) {
        close(listener);close(notice[0]);strcpy(endpoint,path);parent=(uint32_t)getppid();
        uint32_t target[1]={0};object=target;
        birth.epoch=42;birth.session=7;birth.link=8;birth.child=(uint32_t)getpid();
        pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL));
        emit(target,3);char mark='a';full(notice[1],&mark,1,1); /* Not blocked on ordinary state ACK. */
        uint64_t began=milliseconds();emit(target,4);
        assert(!bad_ack&&milliseconds()-began>=150&&delivered_sequence==2);
        _exit(0);
    }
    close(notice[1]);char mark;full(notice[0],&mark,1,0);assert(mark=='a');close(notice[0]);
    for(unsigned attempt=0;attempt<(bad_ack?4u:2u);++attempt) {
        int fd=accept(listener,NULL,NULL);assert(fd>=0);
        struct timeval timeout={2,0};assert(!setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)));
        unsigned char input[SI_CHILD_STATE_SIZE],ack[SI_CHILD_STATE_SIZE]={0};full(fd,input,sizeof(input),0);
        struct si_child_state event;assert(si_child_state_decode(input,sizeof(input),&event));
        assert(event.child==(uint32_t)child&&event.kind==SI_ASI_STATE&&event.sequence==(attempt?2u:1u)&&event.state==(attempt?4u:3u));
        if(!attempt)usleep(50000);else if(!bad_ack)usleep(200000);
        memcpy(ack,"SIR2",4);ack[4]=2;ack[5]=8;si_cs_put(ack+8,sizeof(ack));si_cs_put(ack+12,event.sequence);
        si_cs_put(ack+16,42);si_cs_put(ack+32,bad_ack&&attempt?2:0);full(fd,ack,sizeof(ack),1);close(fd);
    }
    int status;assert(waitpid(child,&status,0)==child);
    if(bad_ack)assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGTERM);
    else assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);
    close(listener);unlink(path);
}
int main(void) {
    run_case(0);run_case(1);
    puts("ASI publisher: asynchronous ordinary state, terminal ACK barrier, ordered retry and own-child fail-closed PASS");
    return 0;
}
