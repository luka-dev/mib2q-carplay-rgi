/* Preserve the fixed AirPlay endpoint across an abrupt SI child exit.
 * SO_REUSEADDR permits TIME_WAIT reuse; never enable SO_REUSEPORT or bypass
 * the existing exclusive DIO owner lock. Other sockets keep stock policy. */
#ifndef MHI2Q_LISTENER_POLICY_H
#define MHI2Q_LISTENER_POLICY_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <stddef.h>
static inline int mhi2q_prepare_airplay_listener(int fd,const struct sockaddr *address,
                                                socklen_t length,int enabled) {
    int type=0,one=1; socklen_t size=sizeof(type);
    if(!enabled||!address||length<sizeof(struct sockaddr))return 0;
    if(address->sa_family==AF_INET) {
        if(length<sizeof(struct sockaddr_in)||
           ((const struct sockaddr_in *)address)->sin_port!=htons(5000))return 0;
    } else if(address->sa_family==AF_INET6) {
        if(length<sizeof(struct sockaddr_in6)||
           ((const struct sockaddr_in6 *)address)->sin6_port!=htons(5000))return 0;
    } else return 0;
    if(getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&size)||type!=SOCK_STREAM)return 0;
    return setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
}
#endif
