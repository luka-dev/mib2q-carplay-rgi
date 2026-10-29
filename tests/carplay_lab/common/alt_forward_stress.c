/* Real production async forwarder, valid multi-AU video and bounded send
 * blockage. The Python runner independently decodes every TCP generation. */
#define ENABLE_LOGGING 0
#include <assert.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/socket.h>
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
static int held,entered,released;
static ssize_t slow_writev(int fd,const struct iovec *v,int n)
{
    pthread_mutex_lock(&gate);
    if(held){entered=1;pthread_cond_broadcast(&wake);while(!released)pthread_cond_wait(&wake,&gate);held=0;}
    pthread_mutex_unlock(&gate);return writev(fd,v,n);
}
#define writev slow_writev
#include "../../../hook/altscreen/altscreen_av.c"
#undef writev
#include "../../../altscreen_render/frame_io.h"
static int listener;
static const char *prefix;
static unsigned connections,received;
static void *peer(void *unused)
{
    (void)unused;
    for(;;){
        int fd=accept(listener,NULL,NULL);if(fd<0)break;
        char path[1024];snprintf(path,sizeof(path),"%s-%u.h264",prefix,connections++);
        FILE *out=fopen(path,"wb");assert(out);
        uint8_t hello[8],*bytes=NULL;uint32_t cap=0,n;alt_frame_meta_t meta;
        alt_wire_put32(hello,ALT_WIRE_HELLO);alt_wire_put32(hello+4,2);
        assert(send(fd,hello,8,0)==8);
        unsigned pictures=0;alt_wire_state_t state={0};
        while(!av_read_frame_ex(fd,&bytes,&cap,&n,&meta)){
            assert(!alt_wire_accept(&state,n,&meta));
            if(!(meta.flags&ALT_WIRE_CONFIG)){
                if(!pictures)assert(meta.flags&ALT_WIRE_IDR);
                ++pictures;++received;
            }
            assert(fwrite(bytes,1,n,out)==n);
        }
        assert(pictures);free(bytes);fclose(out);close(fd);
    }
    return NULL;
}
static void drain(void)
{
    pthread_mutex_lock(&g_forward.lock);
    while(g_forward.busy || g_forward.head)pthread_cond_wait(g_forward.cond,&g_forward.lock);
    pthread_mutex_unlock(&g_forward.lock);
}
int main(int argc,char **argv)
{
    assert(argc==4);alarm(25);signal(SIGPIPE,SIG_IGN);prefix=argv[2];const int faultmode=atoi(argv[3]);held=faultmode;
    FILE *in=fopen(argv[1],"rb");assert(in);
    uint32_t len,count;assert(fread(&len,4,1,in)==1 && len<4096);
    uint8_t config[4096];assert(fread(config,1,len,in)==len);
    assert(fread(&count,4,1,in)==1 && count>=75 && count<=300);
    uint8_t **frames=calloc(count,sizeof(*frames));uint32_t *sizes=calloc(count,sizeof(*sizes));
    for(unsigned i=0;i<count;++i){assert(fread(sizes+i,4,1,in)==1 && sizes[i]<ALT_WIRE_MAX_PAYLOAD);frames[i]=malloc(sizes[i]);assert(frames[i]&&fread(frames[i],1,sizes[i],in)==sizes[i]);}
    fclose(in);listener=socket(AF_INET,SOCK_STREAM,0);assert(listener>=0);
    struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(!bind(listener,(void *)&addr,sizeof(addr))&&!listen(listener,4));socklen_t alen=sizeof(addr);assert(!getsockname(listener,(void *)&addr,&alen));
    pthread_t thread;assert(!pthread_create(&thread,NULL,peer,NULL));
    altav_init("127.0.0.1",ntohs(addr.sin_port));assert(altav_claim(1));assert(!altav_async_start(NULL,NULL));assert(!altav_enqueue_config(1,config,len));
    unsigned failures=0;
    for(unsigned i=0;i<count;++i){
        if(faultmode && i==1){pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&wake,&gate);pthread_mutex_unlock(&gate);}
        uint8_t *owned=malloc(sizes[i]);memcpy(owned,frames[i],sizes[i]);uint32_t cap=sizes[i];alt_avcc_frame_t parsed;
        assert(!alt_avcc_prepare(owned,sizes[i],4,1,&parsed));
        alt_frame_meta_t meta={0};meta.sequence=i+1;meta.received_us=alt_timing_now_us();
        int result=altav_enqueue_owned(1,&owned,&cap,&parsed,&meta);free(owned);
        if(result)++failures;
        if(i==48 && faultmode){pthread_mutex_lock(&gate);released=1;pthread_cond_broadcast(&wake);pthread_mutex_unlock(&gate);}
        if(!faultmode || i>=48)drain();
    }
    drain();altav_shutdown();shutdown(listener,SHUT_RDWR);close(listener);pthread_join(thread,NULL);
    assert(received && (atoi(argv[3])?failures>0:received==count));
    printf("ALT_VALID_VIDEO_PASS connections=%u received=%u rejected=%u\n",connections,received,failures);
    for(unsigned i=0;i<count;++i)free(frames[i]);free(frames);free(sizes);return 0;
}
