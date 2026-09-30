#define _POSIX_C_SOURCE 200809L
#include "unified_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define INPUT_FLOATS (UMX_BINS*4)
#define OUTPUT_FLOATS (UMX_SOURCES*UMX_BINS*4+4*UMX_BANDS*UMX_FAMILIES)
static double now(void) {
#ifdef _WIN32
    LARGE_INTEGER value,frequency;QueryPerformanceCounter(&value);QueryPerformanceFrequency(&frequency);
    return (double)value.QuadPart/(double)frequency.QuadPart;
#else
    struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
    return (double)t.tv_sec+(double)t.tv_nsec/1e9;
#endif
}
static int order(const void *x,const void *y) {double a=*(const double*)x,b=*(const double*)y;return a>b?1:a<b?-1:0;}
int main(int argc,char **argv) {
    UmxModel *model=NULL;UmxState *state=NULL;UmxOutput *out=NULL;FILE *input=NULL,*output=NULL;
    float spectrum[UMX_BINS][2][2];char error[192],magic[8];unsigned header[3],i,j,frames;
    double *times=NULL,sum=0;uint64_t macs=0;int status=1;
    if(argc!=4){fprintf(stderr,"usage: unified_runtime_cli MODEL INPUT_SPECTRA OUTPUT\n");return 2;}
    model=umx_load(argv[1],error,sizeof(error));if(!model){fprintf(stderr,"%s\n",error);goto done;}
    input=fopen(argv[2],"rb");if(!input)goto done;
    if(fread(magic,1,8,input)!=8 || memcmp(magic,"UMXSP001",8) || fread(header,4,3,input)!=3 ||
       !header[0] || !header[1] || header[0]>40 || header[1]>20000 || header[2]!=INPUT_FLOATS)goto done;
    frames=header[0]*header[1];times=(double*)malloc(frames*sizeof(double));state=(UmxState*)calloc(1,sizeof(*state));out=(UmxOutput*)malloc(sizeof(*out));
    if(!times||!state||!out)goto done;
    output=fopen(argv[3],"wb");if(!output)goto done;
    if(fwrite("UMXOT001",1,8,output)!=8)goto done;
    header[2]=OUTPUT_FLOATS;if(fwrite(header,4,3,output)!=3)goto done;
    for(i=0;i<header[0];i++) {
        umx_reset(state);
        for(j=0;j<header[1];j++) {
            double start,elapsed;
            if(fread(spectrum,sizeof(float),INPUT_FLOATS,input)!=INPUT_FLOATS)goto done;
            start=now();if(start<0 || umx_predict(model,state,spectrum,out))goto done;
            elapsed=now()-start;times[i*header[1]+j]=elapsed;sum+=elapsed;macs+=out->dense_macs;
            if(fwrite(out->mask,sizeof(float),UMX_SOURCES*UMX_BINS*4,output)!=UMX_SOURCES*UMX_BINS*4 ||
               fwrite(out->presence,sizeof(float),UMX_BANDS*UMX_FAMILIES,output)!=UMX_BANDS*UMX_FAMILIES ||
               fwrite(out->membership,sizeof(float),UMX_BANDS*UMX_FAMILIES,output)!=UMX_BANDS*UMX_FAMILIES ||
               fwrite(out->pan,sizeof(float),UMX_BANDS*UMX_FAMILIES,output)!=UMX_BANDS*UMX_FAMILIES ||
               fwrite(out->update,sizeof(float),UMX_BANDS*UMX_FAMILIES,output)!=UMX_BANDS*UMX_FAMILIES)goto done;
        }
    }
    if(fgetc(input)!=EOF || fflush(output))goto done;
    qsort(times,frames,sizeof(double),order);
    fprintf(stderr,"{\"frames\":%u,\"dense_macs_per_frame\":%.0f,\"dense_macs_s_at_93_75fps\":%.0f,\"neural_ms_mean\":%.9f,\"neural_ms_p50\":%.9f,\"neural_ms_p99\":%.9f,\"neural_ms_max\":%.9f,\"state_bytes\":%u,\"scope\":\"standalone neural path only; excludes FFT, synthesis, DSP, scheduler, IO; no device or acoustic claim\"}\n",
        frames,(double)macs/frames,(double)macs/frames*93.75,sum/frames*1000,
        times[frames/2]*1000,times[(frames-1)*99/100]*1000,times[frames-1]*1000,(unsigned)sizeof(*state));
    status=0;
done:
    if(status)fprintf(stderr,"unified runtime execution failed; own partial output preserved\n");
    if(input)fclose(input);
    if(output)fclose(output);
    free(times);free(state);free(out);umx_free(model);
    return status;
}
