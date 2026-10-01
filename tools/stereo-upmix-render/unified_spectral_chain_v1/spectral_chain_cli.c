#define _POSIX_C_SOURCE 200809L
/* Standalone spectral-context causal source+copied NN-free C renderer. No hook ABI/unit install.
 * Native input is PCM16. Front mixture is delayed exactly1536 samples to match
 * the source estimator; never advance predicted source in a real-time path.
 * One explicit256KB-stack worker owns all stream state. IO is outside timing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include "unified_audio_stream.h"
#include "stereo_upmix.h"
#ifdef _WIN32
#include <windows.h>
#endif

typedef struct {
    UmxModel *model;
    UmxAudioStream *neural;
    struct spa_stereo_upmix *renderer;
    int16_t delay[UMX_DELAY][2];
    unsigned position;
} Chain;
typedef struct {int argc;char **argv;int status;} Task;
static double now(void) {
#ifdef _WIN32
    LARGE_INTEGER v,f;QueryPerformanceCounter(&v);QueryPerformanceFrequency(&f);return(double)v.QuadPart/f.QuadPart;
#else
    struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return(double)t.tv_sec+t.tv_nsec/1e9;
#endif
}
static int order(const void *a,const void *b){double x=*(const double*)a,y=*(const double*)b;return x>y?1:x<y?-1:0;}
static int pcm(float x,int16_t *out) {
    long v;if(!isfinite(x)||fabsf(x)>=1)return -1;
    v=lrintf(x*32768.0f);if(v>32767||v<-32768)return -1;*out=(int16_t)v;return 0;
}
static Chain *create(UmxModel *model) {
    Chain *s=(Chain*)calloc(1,sizeof(*s));struct spa_stereo_upmix_config c;
    if(!s)return NULL;
    s->model=model;s->neural=umx_audio_create(model);
    s->renderer=(struct spa_stereo_upmix*)calloc(1,sizeof(*s->renderer));
    if(!s->neural||!s->renderer)goto fail;
    spa_stereo_upmix_default_config(&c);c.backend=SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    c.rear_gain_q15=(uint32_t)lround(.40*32768);c.front_bass_db10=-30;
    c.front_retention_q15=16384;c.bed_delays=1;c.bed_attack=0;c.rear_body_db10=0;
    c.rear_air_q15=(uint32_t)lround(32768*pow(10.0,-9.0/20.0));
    c.lfe_gain_q15=SPA_STEREO_UPMIX_LFE_Q15_DEFAULT;spa_stereo_upmix_config_set_lfe(&c,1);
    if(spa_stereo_upmix_init(s->renderer,&c))goto fail;
    s->renderer->_runtime._hssr.test_mask_drive=1;
    return s;
fail:
    umx_audio_free(s->neural);free(s->renderer);free(s);return NULL;
}
static void destroy(Chain *s) {
    if(!s)return;
    umx_audio_free(s->neural);free(s->renderer);free(s);
}
static int process(Chain *s,const int16_t input[UMX_HOP][2],int16_t output[UMX_HOP][6],int16_t source[UMX_HOP][2],float source_float[UMX_HOP][2]) {
    float in[UMX_HOP][2],eligible[UMX_HOP][2];int16_t delayed[UMX_HOP][2];unsigned i,c;
    for(i=0;i<UMX_HOP;i++) {
        for(c=0;c<2;c++){delayed[i][c]=s->delay[s->position][c];s->delay[s->position][c]=input[i][c];in[i][c]=input[i][c]/32768.0f;}
        s->position=(s->position+1)%UMX_DELAY;
    }
    if(umx_audio_process(s->neural,in,eligible))return -1;
    if(source_float)memcpy(source_float,eligible,sizeof(eligible));
    for(i=0;i<UMX_HOP;i++)for(c=0;c<2;c++)if(pcm(eligible[i][c],&source[i][c]))return -2;
    s->renderer->_runtime._hssr.test_inject_pcm=&source[0][0];
    s->renderer->_runtime._hssr.test_inject_frames=UMX_HOP;s->renderer->_runtime._hssr.test_inject_pos=0;
    return spa_stereo_upmix_process_s16(s->renderer,&delayed[0][0],UMX_HOP,&output[0][0],UMX_HOP);
}
static int execute(int argc,char **argv) {
    UmxModel *model=NULL;Chain *chain=NULL;FILE *input=NULL,*output=NULL,*sources=NULL,*floats=NULL,*masks=NULL;
    char error[192],magic[8];unsigned header[3],samples,hops,stream,hop,frames=0,overdue=0;
    int16_t in[UMX_HOP][2],out[UMX_HOP][6],eligible[UMX_HOP][2];
    float source_float[UMX_HOP][2];
    double *times=NULL,sum=0;uint64_t checksum=0,dense=0;int status=1,identity;
    if(argc<5||argc>7){fprintf(stderr,"usage: unified_chain MODEL|identity INPUT_PCM16 OUTPUT6 SOURCE2 [SOURCE_FLOAT [MASK_FLOAT]]\n");return 2;}
    identity=!strcmp(argv[1],"identity");
    if(!identity){model=umx_load(argv[1],error,sizeof(error));if(!model){fprintf(stderr,"%s\n",error);goto done;}}
    input=fopen(argv[2],"rb");if(!input)goto done;
    if(fread(magic,1,8,input)!=8||memcmp(magic,"UMX16I01",8)||fread(header,4,3,input)!=3||
       !header[0]||header[0]>40||!header[1]||header[1]>48000*120||header[2]!=2)goto done;
    samples=header[1];hops=(samples+UMX_DELAY+512+UMX_HOP-1)/UMX_HOP;
    times=(double*)malloc((size_t)header[0]*hops*sizeof(double));output=fopen(argv[3],"wb");sources=fopen(argv[4],"wb");
    if(argc>=6)floats=fopen(argv[5],"wb");
    if(argc==7)masks=fopen(argv[6],"wb");
    if(!times||!output||!sources||(argc>=6&&!floats)||(argc==7&&!masks))goto done;
    header[1]=hops*UMX_HOP;header[2]=6;
    if(fwrite("UMX16O01",1,8,output)!=8||fwrite(header,4,3,output)!=3)goto done;
    header[2]=2;if(fwrite("UMX16S01",1,8,sources)!=8||fwrite(header,4,3,sources)!=3)goto done;
    if(floats&&(fwrite("UMXFLS01",1,8,floats)!=8||fwrite(header,4,3,floats)!=3))goto done;
    if(masks) {
        uint32_t mh[6]={1,header[0],hops,UMX_BINS,2,2};
        if(fwrite("UMXMSK01",1,8,masks)!=8||fwrite(mh,4,6,masks)!=6)goto done;
    }
    for(stream=0;stream<header[0];stream++) {
        unsigned read=0;chain=create(model);if(!chain)goto done;
        for(hop=0;hop<hops;hop++) {
            unsigned count=read<samples?(samples-read<UMX_HOP?samples-read:UMX_HOP):0,i,c;
            double start,elapsed;memset(in,0,sizeof(in));
            if(count&&fread(in,sizeof(int16_t)*2,count,input)!=count)goto done;
            read+=count;start=now();if(start<0||process(chain,in,out,eligible,floats?source_float:NULL))goto done;
            elapsed=now()-start;times[frames++]=elapsed;sum+=elapsed;
            if(elapsed>UMX_HOP/48000.0)overdue++;
            dense+=umx_audio_prediction(chain->neural)->dense_macs;
            for(i=0;i<UMX_HOP;i++)for(c=0;c<6;c++)checksum=checksum*33+(uint16_t)out[i][c];
            if(fwrite(out,sizeof(int16_t)*6,UMX_HOP,output)!=UMX_HOP||fwrite(eligible,sizeof(int16_t)*2,UMX_HOP,sources)!=UMX_HOP)goto done;
            if(floats&&fwrite(source_float,sizeof(float)*2,UMX_HOP,floats)!=UMX_HOP)goto done;
            /* Observation only, after timed inference and audio writes. The
             * masks belong to the actual PCM mixture/state, never to stems
             * passed individually through the network. No state is changed. */
            if(masks) {
                if(identity) {
                    const float one[2]={1,0};
                    for(i=0;i<UMX_BINS;i++)for(c=0;c<2;c++)if(fwrite(one,sizeof(float),2,masks)!=2)goto done;
                } else {
                    const UmxOutput *prediction=umx_audio_prediction(chain->neural);
                    if(!prediction||fwrite(prediction->mask[0],sizeof(float),UMX_BINS*2*2,masks)!=UMX_BINS*2*2)goto done;
                }
            }
        }
        if(read!=samples)goto done;
        destroy(chain);chain=NULL;
    }
    if(fgetc(input)!=EOF||fflush(output)||fflush(sources)||(floats&&fflush(floats))||(masks&&fflush(masks)))goto done;
    qsort(times,frames,sizeof(double),order);
    fprintf(stderr,"{\"frames\":%u,\"identity\":%s,\"streams\":%u,\"causal_front_alignment_samples\":1536,\"hop_acquisition_samples\":512,\"neural_dense_macs_s\":%.0f,\"chain_ms_mean\":%.9f,\"chain_ms_p99\":%.9f,\"chain_ms_max\":%.9f,\"hops_over_10_667ms\":%u,\"state_bytes\":%u,\"worker_stack_bytes\":262144,\"output_checksum\":%llu,\"old_neural_invocations\":0,\"old_blobs\":0,\"scope\":\"standalone causal FFT+one model+WOLA+copied NN-free real C renderer; IO excluded, no hook transport, loaded-worker or CarPlay proof\"}\n",frames,identity?"true":"false",header[0],(double)dense/frames*93.75,sum/frames*1000,times[(frames-1)*99/100]*1000,times[frames-1]*1000,overdue,(unsigned)(sizeof(Chain)+sizeof(struct spa_stereo_upmix)+umx_audio_state_bytes()),(unsigned long long)checksum);
    status=0;
done:
    if(status)fprintf(stderr,"UNIFIED_CHAIN_FAILED: partial own outputs retained; no gain/clipping rescue\n");
    if(input)fclose(input);
    if(output)fclose(output);
    if(sources)fclose(sources);
    if(floats)fclose(floats);
    if(masks)fclose(masks);
    destroy(chain);free(times);umx_free(model);return status;
}
static void *worker(void *p){Task *t=(Task*)p;t->status=execute(t->argc,t->argv);return NULL;}
int main(int argc,char **argv) {
    pthread_attr_t attr;pthread_t thread;Task task;int result;
    task.argc=argc;task.argv=argv;task.status=1;
    if(pthread_attr_init(&attr))return 1;
    result=pthread_attr_setstacksize(&attr,262144);
    if(!result)result=pthread_create(&thread,&attr,worker,&task);
    pthread_attr_destroy(&attr);if(result)return 1;
    if(pthread_join(thread,NULL))return 1;
    return task.status;
}
