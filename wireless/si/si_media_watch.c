/* SI worker-side safety net. No SI/native FSM calls on this thread. The
 * serialized receiver must confirm idle-stop before an ended result is
 * handed back to the controller AO. Keeps cleanup alive across daemon loss. */
#include "si_media_watch.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef SI_MEDIA_WATCH_PORT
#define SI_MEDIA_WATCH_PORT 57343
#endif
static int transfer(int fd,void *data,unsigned size,int writing) {
    unsigned offset=0;
    while(offset<size) {
        ssize_t n=writing?send(fd,(char *)data+offset,size-offset,MSG_NOSIGNAL):recv(fd,(char *)data+offset,size-offset,0);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)return 0;
        offset+=(unsigned)n;
    }
    return 1;
}
static int exchange(const struct si_media_target *t,unsigned operation) {
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    int flags=fcntl(fd,F_GETFL,0),result=-1,error=0;
    int descriptor_flags=fcntl(fd,F_GETFD);
    if(descriptor_flags<0||fcntl(fd,F_SETFD,descriptor_flags|FD_CLOEXEC)){close(fd);return -1;}
    struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;
    if(inet_pton(AF_INET,"127.0.0.1",&address.sin_addr)!=1)goto done;
    address.sin_port=htons(SI_MEDIA_WATCH_PORT);
    if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK))goto done;
    if(connect(fd,(struct sockaddr *)&address,sizeof(address))) {
        if(errno!=EINPROGRESS)goto done;
        fd_set writable;FD_ZERO(&writable);FD_SET(fd,&writable);struct timeval wait={0,200000};
        socklen_t length=sizeof(error);
        if(select(fd+1,NULL,&writable,NULL,&wait)<=0||getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)||error)goto done;
    }
    if(fcntl(fd,F_SETFL,flags))goto done;
    struct timeval timeout={0,200000};
    if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))||setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)))goto done;
    unsigned char request[32]={0},reply[12];memcpy(request,"CPRX",4);request[4]=3;request[5]=(unsigned char)operation;
    for(unsigned i=0;i<4;++i){request[8+i]=(unsigned char)(t->link>>(8*i));request[24+i]=(unsigned char)(t->session>>(8*i));}
    request[12]=(unsigned char)5000;request[13]=(unsigned char)(5000>>8);
    for(unsigned i=0;i<8;++i)request[16+i]=(unsigned char)(t->epoch>>(8*i));
    if(!transfer(fd,request,sizeof(request),1)||!transfer(fd,reply,8,0)||memcmp(reply,"CPRA\x02\0\0\0",8))goto done;
    if(operation==7) {
        if(!transfer(fd,reply+8,4,0)||reply[9]||reply[10]||reply[11])goto done;
        result=reply[8];
    } else result=0;
done:
    close(fd);return result;
}
int si_media_watch(const struct si_media_target *target) {
    if(!target->epoch||!target->session||!target->link||target->child<=1)return 0;
    /* Ended is a native observation, not a network-error inference. Idle STOP
     * atomically closes admission, preventing a late SETUP from racing CANCEL. */
    return exchange(target,7)==4 && exchange(target,2)==0;
}
