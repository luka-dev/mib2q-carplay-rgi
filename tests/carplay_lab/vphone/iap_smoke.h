/* Native CoreAccessories protocol smoke only; no USB forwarding or auth override. */
static unsigned iap_smoke_synack_seen, iap_smoke_synack_sequence, iap_smoke_certificate_seen;
static int iap_stream_fd=-1;
static unsigned iap_stream_lock;

static int iap_stream_write(const unsigned char *bytes, unsigned long n) {
    if(iap_stream_fd<0)return -1;
    while(n){long sent=send(iap_stream_fd,bytes,n,0);if(sent<=0)return -1;bytes+=sent;n-=(unsigned long)sent;}
    return 0;
}

static int iap_stream_connect(const char *nonce) {
    /* Only the owned VM's fixed host-side NAT bridge; no external destination. */
    struct { unsigned char len,family;unsigned short port;unsigned address;unsigned char zero[8]; } address={16,2,0,0,{0}};
    struct {long seconds;int microseconds;} timeout={1,0};
    int enabled=1;
    address.port=htons(32505);
    if(inet_pton(2,"192.168.64.1",&address.address)!=1)return -1;
    iap_stream_fd=socket(2,1,0);
    if(iap_stream_fd<0)return -1;
    if(setsockopt(iap_stream_fd,0xffff,0x1006,&timeout,sizeof(timeout))||
       setsockopt(iap_stream_fd,0xffff,0x1005,&timeout,sizeof(timeout))||
       setsockopt(iap_stream_fd,0xffff,0x1022,&enabled,sizeof(enabled))||
       setsockopt(iap_stream_fd,6,1,&enabled,sizeof(enabled))||
       connect(iap_stream_fd,&address,sizeof(address))||
       iap_stream_write((const unsigned char *)nonce,16)){
        close(iap_stream_fd);iap_stream_fd=-1;return -1;
    }
    return 0;
}

static int iap_smoke_valid_frame(const unsigned char *bytes, unsigned long count) {
    if (!bytes || count<9 || count>512 || bytes[0]!=0xff || bytes[1]!=0x5a ||
            ((unsigned long)bytes[2]*256+bytes[3])!=count || count==10) return 0;
    unsigned sum=0; for (unsigned i=0;i<9;i++) sum+=bytes[i];
    if (sum&255) return 0;
    sum=0; for (unsigned long i=9;i<count;i++) sum+=bytes[i];
    return !(sum&255);
}

static int iap_smoke(const char *peer_nonce, unsigned peer_seconds, int bluetooth, int stream_mode) {
    if(stream_mode&&(!peer_nonce||!bluetooth||iap_stream_connect(peer_nonce)))return 54;
    void *framework=dlopen("/System/Library/PrivateFrameworks/CoreAccessories.framework/CoreAccessories",2);
    if (!framework) return 45;
    const char **names=dlsym(framework,"kACCEndpoint_Protocol_Strings");
    const char **types=dlsym(framework,"kACCEndpoint_TransportType_Strings");
    const char **connections=dlsym(framework,"kACCConnection_Type_Strings");
    if (!names || !types || !connections || strcmp(names[4],"iAP2") || strcmp(types[0],"USB Device Mode") || strcmp(connections[6],"USB") ||
            strcmp(types[2],"Bluetooth Classic") || strcmp(connections[2],"Bluetooth")) return 46;
    /* Fixed prototypes are required: arm64 C varargs use stack arguments. */
    void *send=dlsym(dlopen("/usr/lib/libobjc.A.dylib",2),"objc_msgSend");
    if (!send) return 50;
    void *cls=objc_getClass("ACCTransportClient");
    void *client=cls ? objc_msgSend(cls,sel_registerName("sharedClient")) : 0;
    if (!client) return 47;
    void *connection_info=0,*observer=0;
    if(bluetooth){
        cls=objc_getClass("ACCConnectionInfo");
        connection_info=cls?objc_msgSend(cls,sel_registerName("sharedInstance")):0;
        if(!connection_info)return 52;
        /* The getters read the native observer cache, not a synchronous
         * server query. A retained delegate activates real notifications. */
        cls=objc_getClass("NSObject");
        observer=cls?objc_msgSend(cls,sel_registerName("new")):0;
        if(!observer||!((int (*)(void *,void *,void *,void *))send)(connection_info,sel_registerName("registerDelegate:withFilter:"),observer,0))return 52;
    }
    /* Bluetooth identifiers become IAPAppAccessoryMacAddressKey. Match the
     * owned daemon's advertised Bluetooth component, not a human label. */
    CFRef identifier=str(bluetooth?"02:00:00:00:00:01":"mhi2-vphone-native-iap2-smoke");
    unsigned connection_type=bluetooth?2:6,endpoint_transport=bluetooth?2:0;
    printf("native iAP2 profile connection_type=%u endpoint_transport=%u endpoint_protocol=4\n",connection_type,endpoint_transport);
    void *connection=((void *(*)(void *,void *,u32,CFRef))send)(client,sel_registerName("createConnectionWithType:andIdentifier:"),connection_type,identifier);
    CFRelease(identifier); printf("native %s connection created=%d\n",bluetooth?"Bluetooth":"USB",connection!=0);fflush(0);
    if (!connection) return 48;
    void *properties=((void *(*)(void *,void *,void *))send)(client,sel_registerName("propertiesForConnectionWithUUID:"),connection);
    const char *property_text=properties?objc_msgSend(objc_msgSend(properties,sel_registerName("description")),sel_registerName("UTF8String")):0;
    printf("native connection properties=%.4000s\n",property_text?property_text:"<nil>");fflush(0);
    void (^outgoing)(void *,void *,void *)=^(void *conn,void *endpoint,void *data) {
        (void)conn; (void)endpoint;
        unsigned long count=(unsigned long)objc_msgSend(data,sel_registerName("length"));
        const unsigned char *bytes=objc_msgSend(data,sel_registerName("bytes"));
        char hex[1025]; unsigned long shown = count < 512 ? count : 512;
        if (!bytes) shown = 0;
        for (unsigned long i=0; i<shown; i++) {
            hex[i*2] = "0123456789abcdef"[bytes[i] >> 4];
            hex[i*2+1] = "0123456789abcdef"[bytes[i] & 15];
        }
        hex[shown*2] = 0;
        printf("native iAP2 outgoing length=%lu hex=%s\n",count,hex);fflush(0);
        if(stream_mode&&bytes&&count&&count<=8192){
            unsigned char header[4]={(unsigned char)(count>>24),(unsigned char)(count>>16),(unsigned char)(count>>8),(unsigned char)count};
            os_unfair_lock_lock(&iap_stream_lock);
            if(iap_stream_write(header,4)||iap_stream_write(bytes,count))printf("native iAP2 stream send failed\n");
            os_unfair_lock_unlock(&iap_stream_lock);
        }
        if (iap_smoke_valid_frame(bytes, count) && bytes[4]==0xc0 && bytes[6]==1) {
            __atomic_store_n(&iap_smoke_synack_sequence,bytes[5],__ATOMIC_RELAXED);
            __atomic_store_n(&iap_smoke_synack_seen,1,__ATOMIC_RELEASE);
        }
        static const unsigned char certificate_request[] = {0x40,0x40,0,6,0xaa,0};
        if (iap_smoke_valid_frame(bytes,count) && count==16 && bytes[7]==10 && !memcmp(bytes+9,certificate_request,6))
            __atomic_store_n(&iap_smoke_certificate_seen,1,__ATOMIC_RELEASE);
    };
    void *endpoint=((void *(*)(void *,void *,u32,u32,void *,void *,void *,int))send)(client,sel_registerName("createEndpointWithTransportType:andProtocol:andIdentifier:andDataOutHandler:forConnectionWithUUID:publishConnection:"),endpoint_transport,4,0,outgoing,connection,peer_nonce!=0||bluetooth);
    printf("native iAP2 endpoint created=%d\n",endpoint!=0);fflush(0);
    if (!endpoint) { ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyConnectionWithUUID:"),connection); return 49; }
    int synchronized = 0;
    if(stream_mode){
        unsigned char buffer[8196];unsigned used=0;
        long until=time(0)+(long)peer_seconds;
        while(time(0)<until){
            long got=recv(iap_stream_fd,buffer+used,sizeof(buffer)-used,0);
            if(got==0){synchronized=1;break;}
            if(got<0){if(*__error()==35||*__error()==4)continue;break;}
            used+=(unsigned)got;
            while(used>=4){
                unsigned n=((unsigned)buffer[0]<<24)|((unsigned)buffer[1]<<16)|((unsigned)buffer[2]<<8)|buffer[3];
                if(!n||n>8192)goto stream_done;
                if(used<n+4)break;
                CFRef packet=CFDataCreate(0,buffer+4,n);
                ((void (*)(void *,void *,CFRef,void *))send)(client,sel_registerName("processIncomingData:forEndpointWithUUID:"),packet,endpoint);
                CFRelease(packet);
                unsigned remaining=used-n-4;
                for(unsigned i=0;i<remaining;i++)buffer[i]=buffer[n+4+i];
                used=remaining;
            }
        }
stream_done:
        os_unfair_lock_lock(&iap_stream_lock);
        int stream_fd=iap_stream_fd;iap_stream_fd=-1;
        close(stream_fd);
        os_unfair_lock_unlock(&iap_stream_lock);
    } else if (peer_nonce) {
        char incoming[128], finished[128];
        snprintf(incoming,sizeof(incoming),"/private/var/tmp/mhi2-vphone-lab/iap-peer-%s.inbox",peer_nonce);
        snprintf(finished,sizeof(finished),"/private/var/tmp/mhi2-vphone-lab/iap-peer-%s.done",peer_nonce);
        unsigned char buffer[65536]; size_t consumed=0;
        for (unsigned iteration=0;iteration<peer_seconds;iteration++) {
            FILE *input=fopen(incoming,"rb"); size_t length=0;
            if (input) { length=fread(buffer,1,sizeof(buffer),input);fclose(input); }
            while (length>=consumed+4) {
                unsigned n=((unsigned)buffer[consumed]<<24)|((unsigned)buffer[consumed+1]<<16)|((unsigned)buffer[consumed+2]<<8)|buffer[consumed+3];
                if (!n || n>8192 || length-consumed-4<n) break;
                CFRef packet=CFDataCreate(0,buffer+consumed+4,n);
                ((void (*)(void *,void *,CFRef,void *))send)(client,sel_registerName("processIncomingData:forEndpointWithUUID:"),packet,endpoint);
                CFRelease(packet); consumed+=4+n;
            }
            input=fopen(finished,"rb");
            if (input) { fclose(input); synchronized=1; break; }
            sleep(1);
        }
    } else {
        static const unsigned char detect[]={255,85,2,0,238,16};
        static const unsigned char syn[]={255,90,0,23,128,1,0,0,15,1,8,16,0,15,160,1,244,3,3,10,0,1,50};
        CFRef packet=CFDataCreate(0,detect,sizeof(detect));
        ((void (*)(void *,void *,CFRef,void *))send)(client,sel_registerName("processIncomingData:forEndpointWithUUID:"),packet,endpoint);CFRelease(packet);
        sleep(1);
        packet=CFDataCreate(0,syn,sizeof(syn));
        ((void (*)(void *,void *,CFRef,void *))send)(client,sel_registerName("processIncomingData:forEndpointWithUUID:"),packet,endpoint);CFRelease(packet);
        sleep(1);
        synchronized=__atomic_load_n(&iap_smoke_synack_seen,__ATOMIC_ACQUIRE);
        if (synchronized) {
            unsigned char ack[]={255,90,0,9,64,1,0,0,0};
            ack[6]=(unsigned char)__atomic_load_n(&iap_smoke_synack_sequence,__ATOMIC_RELAXED);
            unsigned sum=0;for(unsigned i=0;i<8;i++)sum+=ack[i];ack[8]=(unsigned char)(0-sum);
            packet=CFDataCreate(0,ack,sizeof(ack));
            ((void (*)(void *,void *,CFRef,void *))send)(client,sel_registerName("processIncomingData:forEndpointWithUUID:"),packet,endpoint);CFRelease(packet);
            printf("native SYNACK acknowledged=%u%c",ack[6],10);fflush(0);
        }
        sleep(4);
    }
    if(bluetooth){
        properties=((void *(*)(void *,void *,void *))send)(client,sel_registerName("propertiesForConnectionWithUUID:"),connection);
        property_text=properties?objc_msgSend(objc_msgSend(properties,sel_registerName("description")),sel_registerName("UTF8String")):0;
        printf("native final connection properties=%.4000s\n",property_text?property_text:"<nil>");fflush(0);
        int observed_connection=-1,observed_transport=-1,observed_protocol=-1;
        for(unsigned i=0;i<5;i++){
            sleep(1);
            observed_connection=((int (*)(void *,void *,void *))send)(connection_info,sel_registerName("accessoryConnectionType:"),connection);
            observed_transport=((int (*)(void *,void *,void *,void *))send)(connection_info,sel_registerName("accessoryEndpointTransportType:connection:"),endpoint,connection);
            observed_protocol=((int (*)(void *,void *,void *,void *))send)(connection_info,sel_registerName("accessoryEndpointProtocolType:connection:"),endpoint,connection);
            if(observed_connection==2&&observed_transport==2&&observed_protocol==4)break;
        }
        printf("native iAP2 readback connection_type=%d endpoint_transport=%d endpoint_protocol=%d\n",observed_connection,observed_transport,observed_protocol);fflush(0);
        if((observed_connection!=2||observed_transport!=2||observed_protocol!=4)&&
           (observed_connection!=11||observed_transport!=18||observed_protocol!=0)){
            ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyEndpointWithUUID:"),endpoint);
            ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyConnectionWithUUID:"),connection);
            return 53;
        }
    }
    ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyEndpointWithUUID:"),endpoint);
    ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyConnectionWithUUID:"),connection);
    printf("native smoke connection cleanup complete%c",10);
    return synchronized && (peer_nonce || __atomic_load_n(&iap_smoke_certificate_seen,__ATOMIC_ACQUIRE)) ? 0 : 51;
}
