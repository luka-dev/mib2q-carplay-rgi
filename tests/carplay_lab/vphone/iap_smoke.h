/* Native CoreAccessories protocol smoke only; no USB forwarding or auth override. */
static unsigned iap_smoke_synack_seen, iap_smoke_synack_sequence, iap_smoke_certificate_seen;

static int iap_smoke_valid_frame(const unsigned char *bytes, unsigned long count) {
    if (!bytes || count<9 || count>512 || bytes[0]!=0xff || bytes[1]!=0x5a ||
            ((unsigned long)bytes[2]*256+bytes[3])!=count || count==10) return 0;
    unsigned sum=0; for (unsigned i=0;i<9;i++) sum+=bytes[i];
    if (sum&255) return 0;
    sum=0; for (unsigned long i=9;i<count;i++) sum+=bytes[i];
    return !(sum&255);
}

static int iap_smoke(const char *peer_nonce, unsigned peer_seconds) {
    void *framework=dlopen("/System/Library/PrivateFrameworks/CoreAccessories.framework/CoreAccessories",2);
    if (!framework) return 45;
    const char **names=dlsym(framework,"kACCEndpoint_Protocol_Strings");
    const char **types=dlsym(framework,"kACCEndpoint_TransportType_Strings");
    const char **connections=dlsym(framework,"kACCConnection_Type_Strings");
    if (!names || !types || !connections || strcmp(names[4],"iAP2") || strcmp(types[0],"USB Device Mode") || strcmp(connections[6],"USB")) return 46;
    /* Fixed prototypes are required: arm64 C varargs use stack arguments. */
    void *send=dlsym(dlopen("/usr/lib/libobjc.A.dylib",2),"objc_msgSend");
    if (!send) return 50;
    void *cls=objc_getClass("ACCTransportClient");
    void *client=cls ? objc_msgSend(cls,sel_registerName("sharedClient")) : 0;
    if (!client) return 47;
    CFRef identifier=str("mhi2-vphone-native-iap2-smoke");
    void *connection=((void *(*)(void *,void *,u32,CFRef))send)(client,sel_registerName("createConnectionWithType:andIdentifier:"),6,identifier);
    CFRelease(identifier); printf("native USB connection created=%d\n",connection!=0);fflush(0);
    if (!connection) return 48;
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
        if (iap_smoke_valid_frame(bytes, count) && bytes[4]==0xc0 && bytes[6]==1) {
            __atomic_store_n(&iap_smoke_synack_sequence,bytes[5],__ATOMIC_RELAXED);
            __atomic_store_n(&iap_smoke_synack_seen,1,__ATOMIC_RELEASE);
        }
        static const unsigned char certificate_request[] = {0x40,0x40,0,6,0xaa,0};
        if (iap_smoke_valid_frame(bytes,count) && count==16 && bytes[7]==10 && !memcmp(bytes+9,certificate_request,6))
            __atomic_store_n(&iap_smoke_certificate_seen,1,__ATOMIC_RELEASE);
    };
    void *endpoint=((void *(*)(void *,void *,u32,u32,void *,void *,void *,int))send)(client,sel_registerName("createEndpointWithTransportType:andProtocol:andIdentifier:andDataOutHandler:forConnectionWithUUID:publishConnection:"),0,4,0,outgoing,connection,peer_nonce!=0);
    printf("native iAP2 endpoint created=%d\n",endpoint!=0);fflush(0);
    if (!endpoint) { ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyConnectionWithUUID:"),connection); return 49; }
    int synchronized = 0;
    if (peer_nonce) {
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
    ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyEndpointWithUUID:"),endpoint);
    ((void (*)(void *,void *,void *))send)(client,sel_registerName("destroyConnectionWithUUID:"),connection);
    printf("native smoke connection cleanup complete%c",10);
    return synchronized && (peer_nonce || __atomic_load_n(&iap_smoke_certificate_seen,__ATOMIC_ACQUIRE)) ? 0 : 51;
}
