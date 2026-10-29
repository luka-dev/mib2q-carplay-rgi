/* Native ARM canary for the exact explicit hardware model. No phone needed. */
#include <sys/asoundlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern size_t mhi2q_audio_peripheral_copy(void *,size_t,unsigned *,unsigned *);
int main(void)
{
    snd_pcm_t *device=NULL;snd_pcm_channel_params_t params;
    int16_t tone[768*6],zero[768*6],captured[768*6];unsigned rate,channels;
    for(unsigned i=0;i<sizeof(tone)/sizeof(tone[0]);++i)tone[i]=(int16_t)(i+1);
    memset(zero,0,sizeof(zero));memset(&params,0,sizeof(params));
    assert(!snd_pcm_open_name(&device,"mpl5_dio_ent",SND_PCM_OPEN_PLAYBACK));
    params.format.rate=48000;params.format.voices=6;
    assert(!snd_pcm_plugin_params(device,&params));
    assert(!snd_pcm_nonblock_mode(device,1));
    assert(snd_pcm_plugin_write(device,tone,sizeof(tone))==(ssize_t)sizeof(tone));
    assert(mhi2q_audio_peripheral_copy(captured,sizeof(captured),&rate,&channels)==sizeof(tone));
    assert(rate==48000 && channels==6 && !memcmp(captured,tone,sizeof(tone)));
    /* Geometry transition must discard old PCM and wait past startup silence. */
    params.format.rate=44100;params.format.voices=2;
    assert(!snd_pcm_plugin_params(device,&params));
    assert(snd_pcm_plugin_write(device,zero,sizeof(zero))==(ssize_t)sizeof(zero));
    assert(!mhi2q_audio_peripheral_copy(captured,sizeof(captured),&rate,&channels));
    assert(snd_pcm_plugin_write(device,tone,sizeof(tone))==(ssize_t)sizeof(tone));
    assert(mhi2q_audio_peripheral_copy(captured,sizeof(captured),&rate,&channels)==sizeof(tone));
    assert(rate==44100 && channels==2 && !memcmp(captured,tone,sizeof(tone)));
    assert(!snd_pcm_close(device));
    puts("[vphone-audio-peripheral] native nonblock/PCM6/stereo-transition canary PASS");
    return 0;
}
