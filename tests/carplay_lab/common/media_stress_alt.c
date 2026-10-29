/* Paced valid video through the unmodified production AltAV worker and
 * renderer framing. A loopback proxy supplies the shared network model. */
#define ENABLE_LOGGING 0
#include "../../../hook/altscreen/altscreen_av.c"
#include "../../../altscreen_render/frame_io.h"
#include <assert.h>
static int listener;
static const char *prefix;
static unsigned connections,received;
static uint64_t now_us(void) {return alt_timing_now_us();}
static void *peer(void *unused)
{
    (void)unused;
    for(;;) {
        int fd=accept(listener,NULL,NULL);if(fd<0)break;
        struct timeval timeout={15,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        char path[1024];snprintf(path,sizeof(path),"%s-%u.h264",prefix,connections++);
        FILE *out=fopen(path,"wb");assert(out);
        uint8_t hello[8],*bytes=NULL;uint32_t cap=0,n;alt_frame_meta_t meta;
        alt_wire_put32(hello,ALT_WIRE_HELLO);alt_wire_put32(hello+4,2);assert(send(fd,hello,8,0)==8);
        alt_wire_state_t state={0};unsigned pictures=0;int outcome;
        while(!(outcome=av_read_frame_ex(fd,&bytes,&cap,&n,&meta))) {
            assert(!alt_wire_accept(&state,n,&meta));
            if(!(meta.flags&ALT_WIRE_CONFIG)) {
                if(!pictures)assert(meta.flags&ALT_WIRE_IDR);
                ++pictures;++received;
                printf("FRAME %llu %llu %llu %u\n",(unsigned long long)meta.sequence,
                    (unsigned long long)meta.received_us,(unsigned long long)now_us(),n);fflush(stdout);
            }
            assert(fwrite(bytes,1,n,out)==n);
        }
        assert(outcome==-1);free(bytes);fclose(out);close(fd);
        if(!pictures)unlink(path);
    }
    return NULL;
}
int main(int argc,char **argv)
{
    assert(argc==3);signal(SIGPIPE,SIG_IGN);alarm(90);prefix=argv[2];
    FILE *in=fopen(argv[1],"rb");assert(in);uint32_t n,count;uint8_t config[4096];
    assert(fread(&n,4,1,in)==1 && n<sizeof(config));assert(fread(config,1,n,in)==n);
    assert(fread(&count,4,1,in)==1 && count>=90 && count<=1800);
    uint8_t **frames=calloc(count,sizeof(*frames));uint32_t *sizes=calloc(count,sizeof(*sizes));size_t total=0;
    for(unsigned i=0;i<count;++i) {
        assert(fread(sizes+i,4,1,in)==1 && sizes[i]<ALT_WIRE_MAX_PAYLOAD);total+=sizes[i];assert(total<128u*1024u*1024u);
        frames[i]=malloc(sizes[i]);assert(frames[i] && fread(frames[i],1,sizes[i],in)==sizes[i]);
    }
    fclose(in);listener=socket(AF_INET,SOCK_STREAM,0);assert(listener>=0);
    struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(!bind(listener,(void *)&addr,sizeof(addr)) && !listen(listener,4));socklen_t len=sizeof(addr);assert(!getsockname(listener,(void *)&addr,&len));
    pthread_t thread;assert(!pthread_create(&thread,NULL,peer,NULL));
    printf("READY %u %llu\n",ntohs(addr.sin_port),(unsigned long long)now_us());fflush(stdout);
    unsigned port;assert(scanf("%u",&port)==1 && port>1024 && port<65536);
    altav_init("127.0.0.1",(int)port);assert(altav_claim(1));assert(!altav_async_start(NULL,NULL));assert(!altav_enqueue_config(1,config,n));
    /* Both senders start on a common host monotonic epoch delivered on stdin. */
    unsigned long long epoch;assert(scanf("%llu",&epoch)==1);
    unsigned rejected=0;
    for(unsigned i=0;i<count;++i) {
        uint64_t due=(uint64_t)epoch+(uint64_t)i*1000000/30;
        uint64_t stamp;
        while((stamp=now_us())<due){uint64_t delay=due-stamp;usleep((useconds_t)(delay>10000?10000:delay));}
        uint8_t *owned=malloc(sizes[i]);assert(owned);memcpy(owned,frames[i],sizes[i]);uint32_t capacity=sizes[i];alt_avcc_frame_t parsed;
        assert(!alt_avcc_prepare(owned,sizes[i],4,1,&parsed));
        alt_frame_meta_t meta={0};meta.sequence=i+1;meta.received_us=due;
        if(altav_enqueue_owned(1,&owned,&capacity,&parsed,&meta))++rejected;
        free(owned);
    }
    pthread_mutex_lock(&g_forward.lock);
    while(g_forward.busy || g_forward.head)pthread_cond_wait(g_forward.cond,&g_forward.lock);
    pthread_mutex_unlock(&g_forward.lock);
    altav_shutdown();shutdown(listener,SHUT_RDWR);close(listener);pthread_join(thread,NULL);
    printf("DONE %u %u %u\n",connections,received,rejected);fflush(stdout);
    for(unsigned i=0;i<count;++i)free(frames[i]);free(frames);free(sizes);return 0;
}
