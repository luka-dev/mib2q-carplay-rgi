/* Independent sender and real local renderer-protocol peer. The production
 * hook must decrypt stream111 and forward through its ordinary AltAV worker. */
#include "paired_alt_fixture.h"
#include "../../../altscreen_render/frame_io.h"
static int alt_forward_result;
static int features_mode(void) {
    const char *mode=getenv("MHI2Q_INTEGRATED_FAULT");return mode&&!strcmp(mode,"features");
}
static void *alt_forward_peer(void *argument) {
    int listener=(int)(intptr_t)argument,fd=accept(listener,NULL,NULL);close(listener);
    if(fd<0){__sync_lock_test_and_set(&alt_forward_result,-1);return NULL;}
    struct timeval deadline={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&deadline,sizeof(deadline));
    uint8_t hello[8];alt_wire_put32(hello,ALT_WIRE_HELLO);alt_wire_put32(hello+4,2);
    uint8_t *buffer=NULL,received[sizeof(alt_fixture_expected)];uint32_t capacity=0;size_t used=0;
    int okay=!write_all(fd,hello,sizeof(hello));
    alt_wire_state_t state={0};
    for(unsigned i=0;okay&&i<2;++i) {
        uint32_t length=0;alt_frame_meta_t meta;
        if(av_read_frame_ex(fd,&buffer,&capacity,&length,&meta)||
           alt_wire_accept(&state,length,&meta)||meta.version!=2||
           meta.flags!=(i?ALT_WIRE_IDR:ALT_WIRE_CONFIG)||length>sizeof(received)-used)okay=0;
        else {memcpy(received+used,buffer,length);used+=length;}
    }
    okay=okay&&used==sizeof(alt_fixture_expected)&&!memcmp(received,alt_fixture_expected,used);
    if(okay) {
        static const char digits[]="0123456789abcdef";
        char line[sizeof(received)*2+80];
        int prefix=snprintf(line,sizeof(line),"[integrated-features] forwarded-alt-annexb=");
        for(size_t i=0;i<used;++i){line[prefix+2*i]=digits[received[i]>>4];line[prefix+2*i+1]=digits[received[i]&15];}
        line[prefix+used*2]='\n';write_all(1,line,(size_t)prefix+used*2+1);
        puts("[integrated-features] production paired111 AltAV TCP forwarding PASS");fflush(stdout);
    }
    free(buffer);close(fd);__sync_lock_test_and_set(&alt_forward_result,okay?1:-1);return NULL;
}
static int alt_forward_start(void) {
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    int reuse=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(19831);
    if(bind(fd,(struct sockaddr *)&address,sizeof(address))||listen(fd,1)){close(fd);return -1;}
    pthread_t thread;alt_forward_result=0;
    if(pthread_create(&thread,NULL,alt_forward_peer,(void *)(intptr_t)fd)){close(fd);return -1;}
    pthread_detach(thread);return 0;
}
static int alt_forward_send(const uint8_t *response,size_t size,const uint8_t shared[32],seal_fn seal) {
    int port=stream_data_port(response,size,2),fd=open_screen_port(port);if(fd<0)return -1;
    uint8_t header[128]={0};uint32_t length=sizeof(alt_fixture_config);float width=1440,height=542;
    memcpy(header,&length,4);header[4]=1;memcpy(header+16,&width,4);memcpy(header+20,&height,4);
    memcpy(header+40,&width,4);memcpy(header+44,&height,4);
    if(write_all(fd,header,sizeof(header))||write_all(fd,alt_fixture_config,sizeof(alt_fixture_config))){close(fd);return -1;}
    uint8_t key[32],nonce[8]={0},cipher[sizeof(alt_fixture_frame)],tag[16];
    if(hkdf32(shared,32,"DataStream-Salt3","DataStream-Output-Encryption-Key",key)){close(fd);return -1;}
    memset(header,0,sizeof(header));length=sizeof(cipher)+sizeof(tag);memcpy(header,&length,4);
    seal(key,nonce,header,sizeof(header),alt_fixture_frame,sizeof(alt_fixture_frame),cipher,tag);
    if(write_all(fd,header,sizeof(header))||write_all(fd,cipher,sizeof(cipher))||write_all(fd,tag,sizeof(tag))){close(fd);return -1;}
    for(unsigned i=0;i<500&&!__sync_val_compare_and_swap(&alt_forward_result,0,0);++i)usleep(10000);
    if(__sync_val_compare_and_swap(&alt_forward_result,0,0)!=1){close(fd);return -1;}
    /* Keep the actual stream socket until the native session finalizer owns
     * teardown. Process exit closes it after the lifecycle assertion. */
    return fd;
}
