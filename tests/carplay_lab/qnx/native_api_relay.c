/* Owned QEMU ecm1 only: TCP bytes to the real daemon's loopback btstack ABI. */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int copy_bytes(int from,int to) {
    char data[8192];ssize_t n=recv(from,data,sizeof(data),0),at=0;
    if(n<=0)return -1;
    while(at<n){ssize_t sent=send(to,data+at,(size_t)(n-at),0);if(sent<=0)return -1;at+=sent;}
    return 0;
}
int main(void) {
    signal(SIGPIPE,SIG_IGN);
    int listener=socket(AF_INET,SOCK_STREAM,0),enabled=1;
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_port=htons(32504);
    inet_pton(AF_INET,"10.0.3.15",&address.sin_addr);
    if(listener<0||setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&enabled,sizeof(enabled))||
       bind(listener,(struct sockaddr *)&address,sizeof(address))||listen(listener,1)){perror("native-api-relay bind");return 1;}
    puts("[native-api-relay] ready on owned ecm1:32504");fflush(stdout);
    for(;;){
        int peer=accept(listener,NULL,NULL),daemon;
        struct timeval timeout={2,0};
        if(peer<0){if(errno==EINTR)continue;return 2;}
        daemon=socket(AF_INET,SOCK_STREAM,0);
        address.sin_port=htons(57342);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(daemon<0||connect(daemon,(struct sockaddr *)&address,sizeof(address))){if(daemon>=0)close(daemon);close(peer);continue;}
        setsockopt(peer,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        setsockopt(daemon,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        puts("[native-api-relay] real daemon connected");fflush(stdout);
        for(;;){
            fd_set ready;int high=peer>daemon?peer:daemon;
            FD_ZERO(&ready);FD_SET(peer,&ready);FD_SET(daemon,&ready);
            if(select(high+1,&ready,NULL,NULL,NULL)<0){if(errno==EINTR)continue;break;}
            if((FD_ISSET(peer,&ready)&&copy_bytes(peer,daemon))||
               (FD_ISSET(daemon,&ready)&&copy_bytes(daemon,peer)))break;
        }
        close(peer);close(daemon);
        puts("[native-api-relay] disconnected");fflush(stdout);
    }
}
