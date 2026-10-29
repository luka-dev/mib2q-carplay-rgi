#include "listener_policy.h"
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int reuse(int fd) {
    int value=-1;socklen_t size=sizeof(value);
    assert(!getsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&value,&size));return value;
}
int main(void) {
    struct sockaddr_in address;memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;assert(inet_pton(AF_INET,"127.0.0.1",&address.sin_addr)==1);
    address.sin_port=htons(5000);
    int server=socket(AF_INET,SOCK_STREAM,0);assert(server>=0);
    assert(!mhi2q_prepare_airplay_listener(server,(const struct sockaddr *)&address,sizeof(address),0));
    assert(!reuse(server));
    address.sin_port=htons(5001);
    assert(!mhi2q_prepare_airplay_listener(server,(const struct sockaddr *)&address,sizeof(address),1));
    assert(!reuse(server));address.sin_port=htons(5000);
    int udp=socket(AF_INET,SOCK_DGRAM,0);assert(udp>=0);
    assert(!mhi2q_prepare_airplay_listener(udp,(const struct sockaddr *)&address,sizeof(address),1));
    assert(!reuse(udp));close(udp);
    assert(!mhi2q_prepare_airplay_listener(server,(const struct sockaddr *)&address,sizeof(address),1));
    assert(reuse(server));
    /* Use a private ephemeral port on the host; apply the identical socket
     * option policy without claiming the user's real port 5000. */
    address.sin_port=0;assert(!bind(server,(const struct sockaddr *)&address,sizeof(address)));
    socklen_t size=sizeof(address);assert(!getsockname(server,(struct sockaddr *)&address,&size));
    assert(!listen(server,1));
    int client=socket(AF_INET,SOCK_STREAM,0);assert(client>=0);
    assert(!connect(client,(const struct sockaddr *)&address,sizeof(address)));
    int accepted=accept(server,NULL,NULL);assert(accepted>=0);
    close(accepted);char byte;assert(read(client,&byte,1)==0);close(client);close(server);
    int replacement=socket(AF_INET,SOCK_STREAM,0);assert(replacement>=0);
    struct sockaddr_in policy=address;policy.sin_port=htons(5000);
    assert(!mhi2q_prepare_airplay_listener(replacement,(const struct sockaddr *)&policy,sizeof(policy),1));
    assert(!bind(replacement,(const struct sockaddr *)&address,sizeof(address)));
    assert(!listen(replacement,1));
    /* Reuse must not permit a second simultaneous listener. */
    int competing=socket(AF_INET,SOCK_STREAM,0);assert(competing>=0);
    assert(!mhi2q_prepare_airplay_listener(competing,(const struct sockaddr *)&policy,sizeof(policy),1));
    assert(bind(competing,(const struct sockaddr *)&address,sizeof(address))<0);
    close(competing);close(replacement);
    puts("AirPlay listener: fixed-port TCP policy, TIME_WAIT reuse, disabled/foreign/UDP isolation and exclusive listener PASS");
}
