/* Explicit diagnostic client, not an automatic test/rollback agent.
 * op req epoch(hex) daemon link session key name [socket]
 * Mutations are sent only for the operation explicitly requested. */
#include "si_wire.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
static void put(unsigned char *p,unsigned off,uint64_t value,unsigned n) {
    for(unsigned i=0;i<n;++i) p[off+i]=(unsigned char)(value>>(i*8));
}
static uint64_t get(const unsigned char *p,unsigned off,unsigned n) {
    uint64_t v=0; for(unsigned i=0;i<n;++i) v|=(uint64_t)p[off+i]<<(i*8); return v;
}
static uint64_t number(const char *s,int base,uint64_t max) {
    char *end; errno=0; uint64_t n=strtoull(s,&end,base);
    if (errno || !*s || *s=='-' || *s=='+' || *end || n>max) { fprintf(stderr,"invalid integer\n"); exit(2); }
    return n;
}
static int transfer(int fd,unsigned char *p,size_t n,int writing) {
    while(n) {
        ssize_t s=writing?write(fd,p,n):read(fd,p,n);
        if(s<0 && errno==EINTR) continue;
        if(s<=0)return -1;
        p+=s;n-=(size_t)s;
    }
    return 0;
}
int main(int argc,char **argv) {
    if(argc!=9 && argc!=10) { fprintf(stderr,"usage: %s op req epoch(hex) daemon link session key name [socket]\n",argv[0]);return 2; }
    unsigned char f[SI_WIRE_SIZE]={0};memcpy(f,"SIW2",4);f[4]=2;
    f[5]=(unsigned char)number(argv[1],10,7);put(f,8,SI_WIRE_SIZE,4);
    put(f,12,number(argv[2],10,UINT32_MAX),4);put(f,16,number(argv[3],16,UINT64_MAX),8);
    put(f,24,number(argv[4],10,UINT32_MAX),4);put(f,28,number(argv[5],10,UINT32_MAX),4);
    put(f,160,number(argv[6],10,UINT32_MAX),4);
    if(strlen(argv[7])>=SI_PHONE_KEY_SIZE || strlen(argv[8])>=SI_PHONE_NAME_SIZE) return 2;
    strcpy((char *)f+32,argv[7]);strcpy((char *)f+64,argv[8]);
    struct si_request checked;if(!si_wire_decode(f,&checked)) return 2;
    struct sockaddr_un addr;memset(&addr,0,sizeof(addr));addr.sun_family=AF_UNIX;
    const char *path=argc==10?argv[9]:"/tmp/mhi2q_si_control.sock";
    if(strlen(path)>=sizeof(addr.sun_path))return 2;
    strcpy(addr.sun_path,path);int fd=socket(AF_UNIX,SOCK_STREAM,0);
    struct timeval timeout={3,0};
    if(fd<0 || setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) ||
       setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)) ||
       connect(fd,(struct sockaddr *)&addr,sizeof(addr)) || transfer(fd,f,sizeof(f),1) || transfer(fd,f,sizeof(f),0)) {
        perror("SI control");return 1;
    }
    close(fd);
    if(memcmp(f,"SIR2",4) || f[4]!=2 || get(f,8,4)!=SI_WIRE_SIZE ||
       get(f,12,4)!=checked.request_id || f[5]!=checked.operation) return 1;
    printf("{\"epoch\":\"%llx\",\"result\":%u,\"device\":%u,\"session\":%u,\"app\":%d,\"state\":%d,\"active_device\":%u,\"child\":%u,\"present\":%u}\n",
        (unsigned long long)get(f,16,8),(unsigned)get(f,32,4),(unsigned)get(f,36,4),
        (unsigned)get(f,40,4),(int32_t)get(f,44,4),(int32_t)get(f,48,4),
        (unsigned)get(f,52,4),(unsigned)get(f,56,4),(unsigned)get(f,60,4));
    return get(f,32,4)==SI_OK?0:3;
}
