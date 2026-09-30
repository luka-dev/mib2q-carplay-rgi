#define _POSIX_C_SOURCE 200809L
#include "unified_audio_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
static double now(void) {
#ifdef _WIN32
    LARGE_INTEGER value,frequency;QueryPerformanceCounter(&value);QueryPerformanceFrequency(&frequency);
    return (double)value.QuadPart/(double)frequency.QuadPart;
#else
    struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
    return (double)t.tv_sec+(double)t.tv_nsec/1e9;
#endif
}
static int order(const void *a,const void *b){double x=*(const double*)a,y=*(const double*)b;return x>y?1:x<y?-1:0;}
int main(int argc,char **argv) {
    UmxModel *model=NULL;UmxAudioStream *stream=NULL;FILE *input=NULL,*output=NULL;
    char error[192],magic[8];unsigned header[3],i,j,samples,hops,frames=0;
    double *times=NULL,sum=0;float in[UMX_HOP][2],out[UMX_HOP][2];int status=1,identity;
    if(argc!=4){fprintf(stderr,"usage: unified_audio_cli MODEL|identity INPUT_PCM OUTPUT_PCM\n");return 2;}
    identity=!strcmp(argv[1],"identity");
    if(!identity){model=umx_load(argv[1],error,sizeof(error));if(!model){fprintf(stderr,"%s\n",error);goto done;}}
    input=fopen(argv[2],"rb");if(!input)goto done;
    if(fread(magic,1,8,input)!=8 || memcmp(magic,"UMXPC001",8) || fread(header,4,3,input)!=3 ||
       !header[0] || !header[1] || header[0]>40 || header[1]>48000*120 || header[2]!=2)goto done;
    samples=header[1];hops=(samples+UMX_HOP-1)/UMX_HOP+3;
    times=(double*)malloc((size_t)header[0]*hops*sizeof(double));stream=umx_audio_create(model);
    output=fopen(argv[3],"wb");if(!times||!stream||!output)goto done;
    if(fwrite("UMXAU001",1,8,output)!=8)goto done;
    header[1]=samples+UMX_DELAY;if(fwrite(header,4,3,output)!=3)goto done;
    for(i=0;i<header[0];i++) {
        unsigned read=0,written=0;
        umx_audio_reset(stream);
        for(j=0;j<hops;j++) {
            unsigned count=read<samples ? (samples-read<UMX_HOP ? samples-read:UMX_HOP) : 0;
            unsigned emit=header[1]-written<UMX_HOP ? header[1]-written:UMX_HOP;
            double start,elapsed;memset(in,0,sizeof(in));
            if(count && fread(in,sizeof(float)*2,count,input)!=count)goto done;
            read+=count;start=now();if(start<0 || umx_audio_process(stream,in,out))goto done;
            elapsed=now()-start;times[frames++]=elapsed;sum+=elapsed;
            if(emit && fwrite(out,sizeof(float)*2,emit,output)!=emit)goto done;
            written+=emit;
        }
        if(read!=samples || written!=header[1])goto done;
    }
    if(fgetc(input)!=EOF || fflush(output))goto done;
    qsort(times,frames,sizeof(double),order);
    fprintf(stderr,"{\"frames\":%u,\"identity\":%s,\"audio_state_bytes\":%u,\"neural_dense_macs_frame\":%.0f,\"neural_dense_macs_s_at_93_75fps\":%.0f,\"algorithmic_array_delay_samples\":%u,\"hop_acquisition_max_samples\":%u,\"stream_ms_mean\":%.9f,\"stream_ms_p99\":%.9f,\"stream_ms_max\":%.9f,\"scope\":\"standalone FFT+neural+eligible iFFT/WOLA; no renderer, scheduling, transport, int8-quality or device qualification\"}\n",frames,identity?"true":"false",umx_audio_state_bytes(),(double)umx_audio_prediction(stream)->dense_macs,(double)umx_audio_prediction(stream)->dense_macs*93.75,UMX_DELAY,UMX_HOP,sum/frames*1000,times[(frames-1)*99/100]*1000,times[frames-1]*1000);
    status=0;
done:
    if(status)fprintf(stderr,"unified audio execution failed; own partial output preserved\n");
    if(input)fclose(input);
    if(output)fclose(output);
    free(times);umx_audio_free(stream);umx_free(model);return status;
}
