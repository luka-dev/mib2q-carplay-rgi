/* Wireless AU codecs. PCM ownership, buffering and resource routing remain
 * outside this file. No constructors, QNX TLS, resampler or decoder stubs. */
#include "audio_codec.h"
#include "../audio/apac_bridge.h"
#include <opus.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

extern const spa_apac_codec_api_t *spa_aac_codec_api(void);
struct cp_audio_codec {
    cp_audio_geometry geometry;
    OpusEncoder *encoder;
    OpusDecoder *decoder;
    spa_apac_decoder_t *aac;
};
/* Cinemo's lazy class/plugin registration is shared with type103. Serialize
 * wireless creations; normal decoding uses one codec per stream. */
static pthread_mutex_t create_lock = PTHREAD_MUTEX_INITIALIZER;

int cp_audio_geometry_for(uint64_t format, cp_audio_geometry *g)
{
    if (!g) return -1;
    memset(g, 0, sizeof(*g));
    switch (format) {
    case UINT64_C(1)<<22: g->rate=44100; g->channels=2; g->frames=1024; break;
    case UINT64_C(1)<<23: g->rate=48000; g->channels=2; g->frames=1024; break;
    case UINT64_C(1)<<28: g->rate=16000; g->channels=1; g->frames=320; g->opus=1; break;
    case UINT64_C(1)<<29: g->rate=24000; g->channels=1; g->frames=480; g->opus=1; break;
    case UINT64_C(1)<<30: g->rate=48000; g->channels=1; g->frames=960; g->opus=1; break;
    default: return -1;
    }
    return 0;
}

cp_audio_codec *cp_audio_codec_create(uint64_t format, int encoder)
{
    cp_audio_codec *c=calloc(1,sizeof(*c));
    int err=0;
    if (!c || cp_audio_geometry_for(format,&c->geometry)) goto failed;
    if (c->geometry.opus) {
        if (encoder) {
            c->encoder=opus_encoder_create(c->geometry.rate,1,OPUS_APPLICATION_VOIP,&err);
            if (err || !c->encoder || opus_encoder_ctl(c->encoder,OPUS_SET_BITRATE(32000)) ||
                opus_encoder_ctl(c->encoder,OPUS_SET_COMPLEXITY(5))) goto failed;
        } else {
            c->decoder=opus_decoder_create(c->geometry.rate,1,&err);
            if (err || !c->decoder) goto failed;
        }
    } else {
        unsigned ch=0,frames=0,rate=0;
        /* MPEG4 DecSpecificInfo: AAC-LC, explicit sample-rate index, stereo. */
        unsigned asc=(2u<<11) | ((c->geometry.rate==48000?3u:4u)<<7) | (2u<<3);
        uint8_t cookie[4]={5,2,(uint8_t)(asc>>8),(uint8_t)asc};
        if (encoder) goto failed; /* Never advertise AAC microphone input. */
        pthread_mutex_lock(&create_lock);
        err=spa_aac_codec_api()->create(cookie,sizeof(cookie),&c->aac,&ch,&frames,&rate);
        pthread_mutex_unlock(&create_lock);
        if (err || ch!=2 || frames!=1024 || rate!=c->geometry.rate) goto failed;
    }
    return c;
failed:
    cp_audio_codec_destroy(c);
    return NULL;
}

void cp_audio_codec_destroy(cp_audio_codec *c)
{
    if (!c) return;
    if (c->encoder) opus_encoder_destroy(c->encoder);
    if (c->decoder) opus_decoder_destroy(c->decoder);
    if (c->aac) spa_aac_codec_api()->destroy(c->aac);
    free(c);
}
void cp_audio_codec_reset(cp_audio_codec *c)
{
    if (!c) return;
    if (c->encoder) opus_encoder_ctl(c->encoder,OPUS_RESET_STATE);
    if (c->decoder) opus_decoder_ctl(c->decoder,OPUS_RESET_STATE);
    if (c->aac) spa_aac_codec_api()->reset(c->aac);
}
int cp_audio_codec_decode(cp_audio_codec *c,const void *au,size_t length,
                         int16_t *pcm,size_t samples,unsigned *frames)
{
    int n;
    if (frames) *frames=0;
    if (!c || !au || !length || length>CP_AUDIO_AU_MAX || !pcm || !frames ||
        samples < c->geometry.frames*c->geometry.channels) return -1;
    if (c->decoder) {
        n=opus_packet_get_nb_samples(au,(opus_int32)length,c->geometry.rate);
        if (n!=(int)c->geometry.frames) return -1;
        n=opus_decode(c->decoder,au,(opus_int32)length,pcm,c->geometry.frames,0);
        if (n<0) return -1;
        *frames=(unsigned)n;
        return 0;
    }
    if (c->aac) {
        uint32_t got=0;
        if (spa_aac_codec_api()->decodeS16(c->aac,au,length,pcm,samples,&got) ||
            got>samples/c->geometry.channels) return -1;
        *frames=got;
        return 0;
    }
    return -1;
}
int cp_audio_codec_encode(cp_audio_codec *c,const int16_t *pcm,unsigned frames,
                         void *au,size_t capacity)
{
    if (!c || !c->encoder || !pcm || !au || frames!=c->geometry.frames ||
        !capacity || capacity>CP_AUDIO_AU_MAX) return -1;
    return opus_encode(c->encoder,pcm,frames,au,(opus_int32)capacity);
}
static pthread_mutex_t formats_lock=PTHREAD_MUTEX_INITIALIZER;
static int formats_probed;
static uint64_t formats=CP_AUDIO_OPUS;
static void probe_aac(void)
{
    unsigned bit;
    for (bit=22;bit<=23;++bit) {
        cp_audio_codec *c=cp_audio_codec_create(UINT64_C(1)<<bit,0);
        if (c) { formats|=UINT64_C(1)<<bit; cp_audio_codec_destroy(c); }
    }
}
uint64_t cp_audio_codec_formats(void)
{
    uint64_t result;
    /* The native AAC factory can start threads and synchronously wait for
     * them. QNX's global pthread_once initialization lock must not span it. */
    pthread_mutex_lock(&formats_lock);
    if(!formats_probed) { probe_aac(); formats_probed=1; }
    result=formats;
    pthread_mutex_unlock(&formats_lock);
    return result;
}
