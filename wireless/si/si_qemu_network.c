/* Disposable loopback-only network identity. The native SDK waits nine
 * seconds for a non-loopback MAC, longer than SI's 10-second child budget.
 * Real HU networking is unchanged; this file is linked only by qemu_si_dio. */
#include <stdint.h>
#include <string.h>
uint64_t GetPrimaryMACAddress(uint8_t mac[6], int *error) {
    static const uint8_t identity[6]={2,0,0,0,0,1};
    if(mac) memcpy(mac,identity,sizeof(identity));
    if(error) *error=0;
    return UINT64_C(0x020000000001);
}

/* The emulated io-pkt lacks QNX IP_PKTINFO. This established lab shim lets
 * factory mdnsd run; it does not prove multicast behavior on the vehicle. */
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
extern char *__progname;
int setsockopt(int fd,int level,int option,const void *value,socklen_t size) {
    static int (*real)(int,int,int,const void *,socklen_t);
    if(!real) real=(int (*)(int,int,int,const void *,socklen_t))dlsym(RTLD_NEXT,"setsockopt");
    if(!real)return -1;
    int result=real(fd,level,option,value,size);
    if(level==IPPROTO_IP && option==25) {
        fprintf(stderr,"[si-network-qemu] IP_PKTINFO caller=%s result=%d errno=%d\n",
                __progname?__progname:"?",result,errno);
        if(result<0)return 0;
    }
    return result;
}
