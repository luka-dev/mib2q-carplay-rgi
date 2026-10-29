/* QNX-only test peripherals: native SoundIF, AudioStreamPrepare/Start,
 * audio threads, decryption and jitter remain real. Only QSA hardware and
 * the absent HMI's explicit audio-type ACK are supplied here. */
#include <sys/asoundlib.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <devctl.h>
#include <dlfcn.h>
#include <pthread.h>
struct snd_pcm;
static int device_fds[16];
static struct snd_pcm *devices[16];
static pthread_mutex_t device_lock=PTHREAD_MUTEX_INITIALIZER;
struct snd_pcm {int sockets[2],capture;snd_pcm_channel_params_t params;size_t samples,nonzero,bad;};
static void audio_checkpoint(struct snd_pcm *p) {
    char path[100],pending[108];
    const char *session=getenv("MHI2Q_CP_SI_SESSION");if(!session)abort();
    snprintf(path,sizeof(path),"/mnt/persist/integrated-audio-%s",session);
    snprintf(pending,sizeof(pending),"%s.tmp",path);
    FILE *f=fopen(pending,"w");if(!f)abort();
    if(fprintf(f,"%lu %lu %lu\n",(unsigned long)p->samples,(unsigned long)p->nonzero,(unsigned long)p->bad)<0||fclose(f)||rename(pending,path))abort();
}
int snd_pcm_open_name(snd_pcm_t **out,char *name,int mode) {
    struct snd_pcm *p=calloc(1,sizeof(*p));if(!p)return -ENOMEM;
    p->capture=(mode&SND_PCM_OPEN_CAPTURE)!=0;
    if(socketpair(AF_UNIX,SOCK_STREAM,0,p->sockets)){free(p);return -errno;}
    pthread_mutex_lock(&device_lock);
    unsigned i;for(i=0;i<16;++i)if(!device_fds[i]){device_fds[i]=p->sockets[0];devices[i]=p;break;}
    pthread_mutex_unlock(&device_lock);
    if(i==16){close(p->sockets[0]);close(p->sockets[1]);free(p);return -EMFILE;}
    if(p->capture && write(p->sockets[1],"1",1)!=1)abort();
    *out=p;fprintf(stderr,"[native-audio] QSA device opened %s\n",name);return 0;
}
int snd_pcm_close(snd_pcm_t *p) {
    if(!p->capture)audio_checkpoint(p);
    fprintf(stderr,"[native-audio] QSA close samples=%lu nonzero=%lu bad=%lu\n",(unsigned long)p->samples,(unsigned long)p->nonzero,(unsigned long)p->bad);
    pthread_mutex_lock(&device_lock);
    for(unsigned i=0;i<16;++i)if(device_fds[i]==p->sockets[0]){device_fds[i]=0;devices[i]=NULL;}
    pthread_mutex_unlock(&device_lock);
    close(p->sockets[0]);close(p->sockets[1]);free(p);return 0;
}
/* CQNXSoundIF metadata ioctl, confirmed at stock libairplay+0x4891c.
 * Only descriptors created above belong to this test peripheral. */
int devctl(int fd,int command,void *data,size_t length,int *info) {
    int ours=0;unsigned expected_rate=0,expected_channels=0;pthread_mutex_lock(&device_lock);
    for(unsigned i=0;i<16;++i)if(fd>0&&device_fds[i]==fd){ours=1;expected_rate=devices[i]->params.format.rate;expected_channels=devices[i]->params.format.voices;}
    pthread_mutex_unlock(&device_lock);
    if(ours){
        if((unsigned)command!=0x8018fa01u||length!=24||!data)return ENOTSUP;
        uint32_t rate,bitrate;memcpy(&rate,(char *)data+8,4);memcpy(&bitrate,(char *)data+16,4);
        if(rate!=expected_rate||((unsigned char *)data)[12]!=16||bitrate!=expected_rate*16*expected_channels)return EINVAL;
        if(info)*info=0;
        fputs("[native-audio] native SoundIF metadata accepted by QSA peripheral\n",stderr);
        return 0;
    }
    int (*next)(int,int,void*,size_t,int*)=dlsym(RTLD_NEXT,"devctl");
    return next?next(fd,command,data,length,info):ENOSYS;
}
int snd_pcm_file_descriptor(snd_pcm_t *p,int channel){(void)channel;return p->sockets[0];}
unsigned snd_pcm_plugin_set_disable(snd_pcm_t *p,unsigned mask){(void)p;(void)mask;return 0;}
int snd_pcm_plugin_info(snd_pcm_t *p,snd_pcm_channel_info_t *i) {
    (void)p;i->formats=SND_PCM_FMT_S16_LE;i->rates=SND_PCM_RATE_48000|SND_PCM_RATE_44100|SND_PCM_RATE_16000;
    i->min_rate=8000;i->max_rate=48000;i->min_voices=1;i->max_voices=8;
    i->min_fragment_size=256;i->max_fragment_size=4096;i->max_buffer_size=65536;return 0;
}
int snd_pcm_plugin_params(snd_pcm_t *p,snd_pcm_channel_params_t *v){p->params=*v;fprintf(stderr,"[native-audio] QSA params rate=%d channels=%d format=%d fragment=%d\n",v->format.rate,v->format.voices,v->format.format,v->buf.block.frag_size);return 0;}
int snd_pcm_plugin_setup(snd_pcm_t *p,snd_pcm_channel_setup_t *v){
    v->format=p->params.format;v->mode=p->params.mode;v->buf.block.frag_size=p->params.buf.block.frag_size;
    if(v->buf.block.frag_size<=0)v->buf.block.frag_size=2048;
    v->buf.block.frags=4;v->buf.block.frags_min=1;v->buf.block.frags_max=4;v->msbits_per_sample=16;return 0;
}
int snd_pcm_plugin_prepare(snd_pcm_t *p,int channel){(void)p;(void)channel;return 0;}
int snd_pcm_plugin_flush(snd_pcm_t *p,int channel){(void)p;(void)channel;return 0;}
int snd_pcm_plugin_status(snd_pcm_t *p,snd_pcm_channel_status_t *s){(void)p;s->status=SND_PCM_STATUS_RUNNING;s->free=65536;s->count=0;return 0;}
ssize_t snd_pcm_plugin_write(snd_pcm_t *p,const void *data,size_t length){
    const int16_t *samples=data;size_t before=p->nonzero;
    for(size_t i=0;i<length/2;++i){p->samples++;if(samples[i]){p->nonzero++;if(samples[i]!=0x2000)p->bad++;}}
    if(before==0&&p->nonzero){
        fputs("[native-audio] decrypted PCM reached native SoundIF/QSA write PASS\n",stderr);
    }
    if(p->nonzero!=before)audio_checkpoint(p);
    if(p->bad){fputs("[native-audio] FAIL corrupt playback samples\n",stderr);}
    unsigned rate=p->params.format.rate,voices=p->params.format.voices;
    if(!rate||!voices)return -EINVAL;
    usleep((useconds_t)((uint64_t)length*1000000/(rate*voices*2)));return (ssize_t)length;
}
ssize_t snd_pcm_plugin_read(snd_pcm_t *p,void *data,size_t length){
    unsigned rate=p->params.format.rate,voices=p->params.format.voices;
    if(!p->capture||!rate||!voices||length%2)return -EINVAL;
    int16_t *samples=data;for(size_t i=0;i<length/2;++i)samples[i]=0x1000;
    usleep((useconds_t)((uint64_t)length*1000000/(rate*voices*2)));return (ssize_t)length;
}

static void (*native_type)(void *,int);
static void *native_manager;
static uintptr_t listener_table[66];
static void hmi_audio_type(void *listener,int type) {
    native_type(listener,type);
    void *dsi=*(void **)((char *)native_manager+480);
    int dsi_type=type==2?1:(type==3||type==4?type:2);
    ((void (*)(void *,int))(uintptr_t)0x167b90)(dsi,dsi_type);
    fputs("[native-audio] native DSI HMI audio routing ACK PASS\n",stderr);
}
void si_qemu_audio_install(void *manager) {
    uintptr_t **object=(uintptr_t **)((char *)manager+172),*table=*object;
    if((uintptr_t)table<0x100000||(uintptr_t)table>0x200000)abort();
    memcpy(listener_table,table-2,sizeof(listener_table));unsigned found=0;
    for(unsigned i=0;i<64;++i)if(table[i]==0x1584e0){native_type=(void (*)(void *,int))table[i];listener_table[i+2]=(uintptr_t)hmi_audio_type;found++;}
    if(found!=1)abort();
    native_manager=manager;*object=listener_table+2;
}
