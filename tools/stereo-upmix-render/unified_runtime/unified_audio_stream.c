/* Track B's independent past-only Hann/WOLA path, float radix2 FFT.
 * Physical endpoints are enforced after the predicted eligible complex mask.
 * No shipping DSP/source files are changed. No malloc in the hop loop.
 */
#include "unified_audio_stream.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct UmxAudioStream {
    const UmxModel *model;
    UmxState neural;
    UmxOutput prediction;
    float history[2][UMX_DELAY];
    float ola[2][UMX_FFT];
    float real[UMX_FFT],imag[UMX_FFT];
    float spectrum[UMX_BINS][2][2];
    float window[UMX_FFT],twiddle_real[UMX_FFT/2],twiddle_imag[UMX_FFT/2];
    unsigned short reverse[UMX_FFT];
};
static void fft(UmxAudioStream *s,int inverse) {
    unsigned i,j,length;
    for(i=0;i<UMX_FFT;i++) {
        j=s->reverse[i];
        if(j>i) {float v=s->real[i];s->real[i]=s->real[j];s->real[j]=v;v=s->imag[i];s->imag[i]=s->imag[j];s->imag[j]=v;}
    }
    for(length=2;length<=UMX_FFT;length*=2) {
        unsigned half=length/2,step=UMX_FFT/length,base;
        for(base=0;base<UMX_FFT;base+=length) for(j=0;j<half;j++) {
            unsigned a=base+j,b=a+half,t=j*step;
            float wr=s->twiddle_real[t],wi=inverse?-s->twiddle_imag[t]:s->twiddle_imag[t];
            float re=wr*s->real[b]-wi*s->imag[b],im=wr*s->imag[b]+wi*s->real[b];
            s->real[b]=s->real[a]-re;s->imag[b]=s->imag[a]-im;
            s->real[a]+=re;s->imag[a]+=im;
        }
    }
    if(inverse) for(i=0;i<UMX_FFT;i++){s->real[i]/=UMX_FFT;s->imag[i]/=UMX_FFT;}
}
unsigned umx_audio_state_bytes(void){return (unsigned)sizeof(UmxAudioStream);}
UmxAudioStream *umx_audio_create(const UmxModel *model) {
    unsigned i,j;const double pi=3.1415926535897932384626433832795;
    UmxAudioStream *s=(UmxAudioStream*)calloc(1,sizeof(*s));if(!s)return NULL;
    s->model=model;
    for(i=0;i<UMX_FFT;i++) {
        unsigned value=i,reversed=0;
        for(j=0;j<11;j++){reversed=(reversed<<1)|(value&1);value>>=1;}
        s->reverse[i]=(unsigned short)reversed;
        s->window[i]=(float)(.5-.5*cos(2*pi*i/UMX_FFT));
        if(i<UMX_FFT/2){s->twiddle_real[i]=(float)cos(2*pi*i/UMX_FFT);s->twiddle_imag[i]=(float)-sin(2*pi*i/UMX_FFT);}
    }
    return s;
}
void umx_audio_free(UmxAudioStream *s){free(s);}
void umx_audio_reset(UmxAudioStream *s) {
    if(!s)return;
    umx_reset(&s->neural);memset(&s->prediction,0,sizeof(s->prediction));
    memset(s->history,0,sizeof(s->history));memset(s->ola,0,sizeof(s->ola));
}
const UmxOutput *umx_audio_prediction(const UmxAudioStream *s){return s?&s->prediction:NULL;}
int umx_audio_process(UmxAudioStream *s,const float input[UMX_HOP][2],float output[UMX_HOP][2]) {
    unsigned c,i;int result;
    if(!s||!input||!output)return -1;
    /* Validate the whole hop before state mutation. */
    for(i=0;i<UMX_HOP;i++)for(c=0;c<2;c++)if(!isfinite(input[i][c])||fabsf(input[i][c])>4)return -2;
    for(c=0;c<2;c++) {
        for(i=0;i<UMX_FFT;i++){s->real[i]=(i<UMX_DELAY?s->history[c][i]:input[i-UMX_DELAY][c])*s->window[i];s->imag[i]=0;}
        memmove(s->history[c],s->history[c]+UMX_HOP,(UMX_DELAY-UMX_HOP)*sizeof(float));
        for(i=0;i<UMX_HOP;i++)s->history[c][UMX_DELAY-UMX_HOP+i]=input[i][c];
        fft(s,0);
        for(i=0;i<UMX_BINS;i++){s->spectrum[i][c][0]=s->real[i];s->spectrum[i][c][1]=(i==0||i==UMX_BINS-1)?0:s->imag[i];}
    }
    if(s->model){result=umx_predict_eligible(s->model,&s->neural,s->spectrum,&s->prediction);if(result)return result;}
    for(c=0;c<2;c++) {
        for(i=0;i<UMX_BINS;i++) {
            float re=s->spectrum[i][c][0],im=s->spectrum[i][c][1];
            if(s->model){float mr=s->prediction.mask[0][i][c][0],mi=s->prediction.mask[0][i][c][1];s->real[i]=re*mr-im*mi;s->imag[i]=re*mi+im*mr;}
            else{s->real[i]=re;s->imag[i]=im;}
        }
        s->imag[0]=s->imag[UMX_BINS-1]=0;
        for(i=UMX_BINS;i<UMX_FFT;i++){s->real[i]=s->real[UMX_FFT-i];s->imag[i]=-s->imag[UMX_FFT-i];}
        fft(s,1);
        for(i=0;i<UMX_FFT;i++) {
            float value=s->ola[c][i]+s->real[i]*s->window[i]/1.5f;
            if(!isfinite(value))return -3;
            s->ola[c][i]=value;
        }
        for(i=0;i<UMX_HOP;i++)output[i][c]=s->ola[c][i];
        memmove(s->ola[c],s->ola[c]+UMX_HOP,(UMX_FFT-UMX_HOP)*sizeof(float));
        memset(s->ola[c]+UMX_FFT-UMX_HOP,0,UMX_HOP*sizeof(float));
    }
    return 0;
}
