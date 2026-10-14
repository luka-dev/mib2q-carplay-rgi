/* Independent phone-side assertions; included only by the stock210 fixture. */
static int media_mic_socket(void)
{
    int fd=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in a;struct timeval timeout={3,0};
    if(fd<0)return -1;
    memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(6011);
    if(bind(fd,(struct sockaddr *)&a,sizeof(a))){close(fd);return -1;}
    (void)setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));return fd;
}
static int media_event_mic_check(int event_fd,int mic_fd,const uint8_t shared[32],seal_fn seal,open_fn open_sealed)
{
    uint8_t rkey[32],wkey[32],nonce[8],prefix[2],cipher[1024],tag[16],plain[1025];
    int i;
    static const char response[]="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    if(hkdf32(shared,32,"Events-Salt","Events-Write-Encryption-Key",rkey) ||
       hkdf32(shared,32,"Events-Salt","Events-Read-Encryption-Key",wkey))return -1;
    for(i=0;i<2;++i){
        size_t n;
        if(read_all(event_fd,prefix,2))return -2;
        n=prefix[0]|((size_t)prefix[1]<<8);
        if(n>1024 || read_all(event_fd,cipher,n) || read_all(event_fd,tag,16))return -3;
        memset(nonce,0,8);nonce[0]=(uint8_t)i;
        if(open_sealed(rkey,nonce,prefix,2,cipher,n,plain,tag))return -4;
        plain[n]=0;
        if(!strstr((char *)plain,"POST /hook-media-test HTTP/1.1") || !strstr((char *)plain,"event-check"))return -5;
        prefix[0]=sizeof(response)-1;prefix[1]=0;
        seal(wkey,nonce,prefix,2,response,sizeof(response)-1,cipher,tag);
        if(i==1)tag[0]^=1;
        if(write_all(event_fd,prefix,2) || write_all(event_fd,cipher,sizeof(response)-1) || write_all(event_fd,tag,16))return -6;
    }
    HOOK_TRACE("[stock210-phone] encrypted events request/response and bad tag sent\n");
    if(hkdf32(shared,32,"DataStream-Salt2","DataStream-Input-Encryption-Key",rkey))return -7;
    size_t total=0;unsigned first_seq=0;
    for(i=0;i<2;++i){
        uint8_t packet[1500],pcm[1500];size_t j;
        ssize_t n=recv(mic_fd,packet,sizeof(packet),0);
        {
            char line[128];int m=snprintf(line,sizeof(line),"[stock210-phone] mic packet=%d bytes=%d errno=%d seq=%u\n",i,(int)n,errno,n>3?((unsigned)packet[2]<<8)|packet[3]:0);
            if(m>0)(void)write_all(2,line,(size_t)m);
        }
        if(n<36 || n>1452 || packet[0]!=0x80)return -8;
        unsigned seq=((unsigned)packet[2]<<8)|packet[3];
        if(!i)first_seq=seq;
        if(seq!=((first_seq+(unsigned)i)&65535))return -8;
        uint32_t stamp=((uint32_t)packet[4]<<24)|((uint32_t)packet[5]<<16)|((uint32_t)packet[6]<<8)|packet[7];
        if(stamp!=total/4)return -9;
        memset(nonce,0,8);nonce[0]=(uint8_t)i;
        if(memcmp(nonce,packet+n-8,8) || open_sealed(rkey,nonce,packet+4,8,packet+12,(size_t)n-36,pcm,packet+n-24))return -10;
        for(j=0;j<(size_t)n-36;j+=2)if(pcm[j]!=0x12 || pcm[j+1]!=0x34)return -11;
        total+=(size_t)n-36;
    }
    if(total!=1440)return -12;
    HOOK_TRACE("[stock210-phone] microphone encryption, splitting, sequence and timestamp verified\n");
    return 0;
}
