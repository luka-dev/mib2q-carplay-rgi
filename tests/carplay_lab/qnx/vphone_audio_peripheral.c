/* Explicit lab peripherals. Real iOS, AEAD, codec, native jitter/SoundIF and
 * AudioStream run unchanged. This supplies absent QSA hardware and an HMI
 * audio-type ACK; it cannot establish real Java focus or amplifier routing. */
#include <sys/asoundlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <devctl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

struct snd_pcm {int fd[2],capture;snd_pcm_channel_params_t params;};
static snd_pcm_t *devices[16];
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static uint8_t captured[65536];
static size_t captured_length;
static unsigned captured_rate,captured_channels;
/* Codec isolation only: receive and inspect MainHigh RTCP on a real UDP
 * endpoint. The hardware/focus peripheral does not implement a production
 * RTCP clock/retransmission engine. Record that boundary in every receipt. */
static int control_fd=-1;
static unsigned control_port;
static void *control_receive(void *argument)
{
    int fd=(int)(intptr_t)argument;unsigned packets=0;
    for(;;){
        uint8_t bytes[2048];ssize_t n=recv(fd,bytes,sizeof(bytes),0);
        if(n<0){if(errno==EAGAIN||errno==EINTR)continue;break;}
        if(packets++<4)fprintf(stderr,"[vphone-audio-peripheral] modeled RTCP receive length=%d type=%u; clock/retransmission not modeled\n",
            (int)n,n>1?(unsigned)(bytes[1]&127):0);
    }
    return NULL;
}
unsigned mhi2q_audio_peripheral_control_port(void)
{
    pthread_mutex_lock(&lock);
    if(control_fd<0){
        struct sockaddr_in address;memset(&address,0,sizeof(address));
        address.sin_len=sizeof(address);address.sin_family=AF_INET;
        int fd=socket(AF_INET,SOCK_DGRAM,0);socklen_t size=sizeof(address);
        struct timeval timeout={0,100000};pthread_t thread;
        if(fd>=0 && !setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) &&
           !bind(fd,(void *)&address,sizeof(address)) && !getsockname(fd,(void *)&address,&size) &&
           !pthread_create(&thread,NULL,control_receive,(void *)(intptr_t)fd)){
            pthread_detach(thread);control_fd=fd;control_port=ntohs(address.sin_port);
            fprintf(stderr,"[vphone-audio-peripheral] modeled MainHigh RTCP endpoint port=%u (codec isolation only)\n",control_port);
        }else if(fd>=0)close(fd);
    }
    unsigned port=control_port;pthread_mutex_unlock(&lock);return port;
}
size_t mhi2q_audio_peripheral_copy(void *bytes,size_t capacity,unsigned *rate,unsigned *channels)
{
    pthread_mutex_lock(&lock);size_t n=captured_length<capacity?captured_length:capacity;
    memcpy(bytes,captured,n);*rate=captured_rate;*channels=captured_channels;
    fprintf(stderr,"[vphone-audio-peripheral] PCM snapshot bytes=%u rate=%u channels=%u\n",(unsigned)n,captured_rate,captured_channels);
    pthread_mutex_unlock(&lock);return n;
}
int snd_pcm_open_name(snd_pcm_t **out,char *name,int mode)
{
    snd_pcm_t *p=calloc(1,sizeof(*p));if(!p)return -ENOMEM;
    p->capture=(mode&SND_PCM_OPEN_CAPTURE)!=0;
    if(socketpair(AF_UNIX,SOCK_STREAM,0,p->fd)){free(p);return -errno;}
    pthread_mutex_lock(&lock);unsigned i;
    for(i=0;i<16;++i)if(!devices[i]){devices[i]=p;break;}
    pthread_mutex_unlock(&lock);
    if(i==16){close(p->fd[0]);close(p->fd[1]);free(p);return -EMFILE;}
    if(p->capture && write(p->fd[1],"1",1)!=1)abort();
    *out=p;fprintf(stderr,"[vphone-audio-peripheral] QSA open %s mode=%x capture=%d handle=%p\n",name,mode,p->capture,p);return 0;
}
int snd_pcm_close(snd_pcm_t *p)
{
    pthread_mutex_lock(&lock);for(unsigned i=0;i<16;++i)if(devices[i]==p)devices[i]=NULL;
    pthread_mutex_unlock(&lock);close(p->fd[0]);close(p->fd[1]);free(p);return 0;
}
int devctl(int fd,int command,void *data,size_t length,int *info)
{
    int ours=0;unsigned rate=0,channels=0;
    pthread_mutex_lock(&lock);
    for(unsigned i=0;i<16;++i)if(devices[i]&&devices[i]->fd[0]==fd){ours=1;rate=devices[i]->params.format.rate;channels=devices[i]->params.format.voices;}
    pthread_mutex_unlock(&lock);
    if(ours){
        if((unsigned)command!=0x8018fa01u||length!=24||!data)return ENOTSUP;
        uint32_t r,b;memcpy(&r,(char *)data+8,4);memcpy(&b,(char *)data+16,4);
        unsigned bits=((uint8_t *)data)[12];
        /* SAD describes the MOST container (24-bit slots for spatial PCM),
         * not necessarily QSA's signed-16 application samples. */
        unsigned sad_channels=(r&&bits)?b/r/bits:0;
        if(r!=rate || (bits!=16&&bits!=24) || !sad_channels || sad_channels>8 ||
           b!=r*bits*sad_channels || sad_channels!=channels) {
            fprintf(stderr,"[vphone-audio-peripheral] invalid SAD rate=%u bits=%u bitrate=%u channels=%u QSA=%u/%u\n",r,bits,b,sad_channels,rate,channels);
            return EINVAL;
        }
        fprintf(stderr,"[vphone-audio-peripheral] SAD %uHz/%uch/%ubit QSA=%uch/16bit\n",r,sad_channels,bits,channels);
        if(info)*info=0;
        return 0;
    }
    int (*real)(int,int,void *,size_t,int *)=dlsym(RTLD_NEXT,"devctl");
    return real?real(fd,command,data,length,info):ENOSYS;
}
int snd_pcm_file_descriptor(snd_pcm_t *p,int channel){(void)channel;return p->fd[0];}
int snd_pcm_nonblock_mode(snd_pcm_t *p,int nonblock)
{
    /* Never pass this deliberately private model handle into real libasound:
     * its nonblock function writes native flags at our capture-field offset. */
    int flags=fcntl(p->fd[0],F_GETFL,0);
    if(flags<0 || fcntl(p->fd[0],F_SETFL,nonblock?flags|O_NONBLOCK:flags&~O_NONBLOCK)<0)return -errno;
    return 0;
}
unsigned snd_pcm_plugin_set_disable(snd_pcm_t *p,unsigned flags){(void)p;(void)flags;return 0;}
int snd_pcm_plugin_info(snd_pcm_t *p,snd_pcm_channel_info_t *i)
{
    (void)p;i->formats=SND_PCM_FMT_S16_LE;i->rates=SND_PCM_RATE_48000|SND_PCM_RATE_44100|SND_PCM_RATE_16000;
    i->min_rate=8000;i->max_rate=48000;i->min_voices=1;i->max_voices=8;
    i->min_fragment_size=256;i->max_fragment_size=4096;i->max_buffer_size=65536;return 0;
}
int snd_pcm_channel_info(snd_pcm_t *p,snd_pcm_channel_info_t *i)
{ return snd_pcm_plugin_info(p,i); }
int snd_pcm_plugin_params(snd_pcm_t *p,snd_pcm_channel_params_t *v){p->params=*v;return 0;}
int snd_pcm_plugin_setup(snd_pcm_t *p,snd_pcm_channel_setup_t *v)
{
    v->format=p->params.format;v->mode=p->params.mode;v->buf.block.frag_size=p->params.buf.block.frag_size;
    if(v->buf.block.frag_size<=0)v->buf.block.frag_size=2048;
    v->buf.block.frags=4;v->buf.block.frags_min=1;v->buf.block.frags_max=4;v->msbits_per_sample=16;return 0;
}
int snd_pcm_plugin_prepare(snd_pcm_t *p,int channel){(void)p;(void)channel;return 0;}
int snd_pcm_plugin_flush(snd_pcm_t *p,int channel){(void)p;(void)channel;return 0;}
int snd_pcm_plugin_status(snd_pcm_t *p,snd_pcm_channel_status_t *s){(void)p;s->status=SND_PCM_STATUS_RUNNING;s->free=65536;s->count=0;return 0;}
ssize_t snd_pcm_plugin_write(snd_pcm_t *p,const void *bytes,size_t length)
{
    unsigned rate=p->params.format.rate,channels=p->params.format.voices;
    if(!rate||!channels||length>65536||length%(2*channels))return -EINVAL;
    int nonzero=0;for(size_t i=0;i<length;++i)if(((const uint8_t *)bytes)[i]){nonzero=1;break;}
    static unsigned diagnostic_writes;
    if(diagnostic_writes++<12)fprintf(stderr,"[vphone-audio-peripheral] PCM write bytes=%u rate=%u channels=%u capture=%d nonzero=%d\n",(unsigned)length,rate,channels,p->capture,nonzero);
    pthread_mutex_lock(&lock);
    if(!p->capture && (channels==2||channels==6||channels==8)){
        if(captured_length&&(captured_rate!=rate||captured_channels!=channels))captured_length=0;
        if(nonzero||captured_length) {
            size_t n=sizeof(captured)-captured_length;if(n>length)n=length;
            n-=n%(2*channels);
            captured_rate=rate;captured_channels=channels;memcpy(captured+captured_length,bytes,n);captured_length+=n;
        }else captured_rate=captured_channels=0;
    }
    pthread_mutex_unlock(&lock);
    usleep((useconds_t)((uint64_t)length*1000000/(rate*channels*2)));return (ssize_t)length;
}
ssize_t snd_pcm_plugin_read(snd_pcm_t *p,void *bytes,size_t length)
{
    unsigned rate=p->params.format.rate,channels=p->params.format.voices;
    if(!p->capture||!rate||!channels||length>65536||length%(2*channels))return -EINVAL;
    memset(bytes,0,length); /* No invented speech input. */
    usleep((useconds_t)((uint64_t)length*1000000/(rate*channels*2)));return (ssize_t)length;
}
static void (*native_type)(void *,int);
static void *manager;
static uintptr_t listener[66];
static void hmi_type(void *self,int type)
{
    native_type(self,type);
    if(type<0) {
        fputs("[vphone-audio-peripheral] native audio release; no ready ACK\n",stderr);
        return;
    }
    void *dsi=*(void **)((char *)manager+480);
    int value=type==2?1:(type==3||type==4?type:2);
    ((void (*)(void *,int))(uintptr_t)0x167b90)(dsi,value);
    fprintf(stderr,"[vphone-audio-peripheral] simulated HMI ACK through native DSI type=%d\n",type);
}
void mhi2q_audio_peripheral_install(void *object)
{
    uintptr_t **v=(uintptr_t **)((char *)object+172),*table=*v;
    if((uintptr_t)table<0x100000||(uintptr_t)table>0x200000)abort();
    memcpy(listener,table-2,sizeof(listener));unsigned found=0;
    for(unsigned i=0;i<64;++i)if(table[i]==0x1584e0){native_type=(void (*)(void *,int))table[i];listener[i+2]=(uintptr_t)hmi_type;++found;}
    if(found!=1)abort();
    manager=object;*v=listener+2;
    fputs("[vphone-audio-peripheral] explicit simulated HMI/QSA enabled\n",stderr);
}
