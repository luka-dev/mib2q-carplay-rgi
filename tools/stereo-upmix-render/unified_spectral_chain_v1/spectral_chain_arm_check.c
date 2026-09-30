/* Full spectral PCM chain ARM arithmetic replay. Native references are saved
 * TRAIN prefixes, not oracle audio or a physical deadline/quality benchmark. */
#define main spectral_chain_cli_unused_main
#include "spectral_chain_cli.c"
#undef main

#if defined(__QNXNTO__) && defined(__arm__)
#define SMX_EXECUTION "qnx-arm"
#else
#define SMX_EXECUTION "native-host"
#endif

#define RECORD_SHORTS (UMX_HOP*(2+2*(6+2)))
typedef struct {int argc;char **argv;int result;} GuestTask;
static int check_mode(const char *model_path,const char *fixture_path,unsigned mode) {
    UmxModel *model=NULL;Chain *chains[4]={NULL,NULL,NULL,NULL};FILE *file=NULL;
    int16_t *data=NULL,out[UMX_HOP][6],source[UMX_HOP][2];
    unsigned header[2],stream,hop,i,c;char magic[8],error[192];
    double sum[8]={0},power[8]={0};unsigned maximum[8]={0};int passed=1,status=1;
    model=umx_load(model_path,error,sizeof(error));if(!model){fprintf(stderr,"%s\n",error);goto done;}
    file=fopen(fixture_path,"rb");if(!file)goto done;
    if(fread(magic,1,8,file)!=8||memcmp(magic,"SMXQA001",8)||fread(header,4,2,file)!=2||
       header[0]!=4||header[1]!=64)goto done;
    data=(int16_t*)malloc((size_t)header[0]*header[1]*RECORD_SHORTS*sizeof(int16_t));
    if(!data||fread(data,sizeof(int16_t),(size_t)header[0]*header[1]*RECORD_SHORTS,file)!=(size_t)header[0]*header[1]*RECORD_SHORTS||fgetc(file)!=EOF)goto done;
    for(stream=0;stream<header[0];stream++){chains[stream]=create(model);if(!chains[stream])goto done;}
    /* Interleaving exercises four separately owned neural and renderer states. */
    for(hop=0;hop<header[1];hop++)for(stream=0;stream<header[0];stream++) {
        int16_t *record=data+((size_t)stream*header[1]+hop)*RECORD_SHORTS;
        const int16_t *reference=record+UMX_HOP*2+mode*UMX_HOP*8;
        if(process(chains[stream],(const int16_t (*)[2])record,out,source,NULL))goto done;
        if(umx_audio_prediction(chains[stream]->neural)->dense_macs!=1244928)goto done;
        for(i=0;i<UMX_HOP;i++)for(c=0;c<8;c++) {
            int actual=c<6?out[i][c]:source[i][c-6],expected=reference[i*8+c];
            int delta=actual-expected;unsigned magnitude=(unsigned)(delta<0?-delta:delta);
            sum[c]+=(double)delta*delta;power[c]+=(double)expected*expected;
            if(magnitude>maximum[c])maximum[c]=magnitude;
        }
    }
    for(c=0;c<8;c++) {
        double value=power[c]>0?sum[c]/power[c]:sum[c];
        if(!isfinite(value)||value>(mode?1e-4:1e-7)||(mode==0&&maximum[c]>4))passed=0;
    }
    printf("SPECTRAL_CHAIN_ARM_CHECK {\"mode\":\"%s\",\"passed\":%s,\"streams_interleaved\":4,\"hops_per_stream\":64,\"selected_dense_macs_frame\":1244928,\"neural_dense_macs_s\":116712000,\"channel_source_nse\":[",mode?"int8":"float",passed?"true":"false");
    for(c=0;c<8;c++)printf("%s%.12g",c?",":"",power[c]>0?sum[c]/power[c]:sum[c]);
    printf("],\"channel_source_max_lsb\":[");for(c=0;c<8;c++)printf("%s%u",c?",":"",maximum[c]);
    printf("],\"old_neural_invocations\":0,\"old_blobs\":0,\"worker_stack_bytes\":262144,\"execution\":\"" SMX_EXECUTION "\",\"scope\":\"Full FFT/single model/WOLA/NN-free C arithmetic on saved TRAIN prefix; native-reference parity, no device timing or acoustic promotion\"}\n");
    status=passed?0:1;
done:
    if(status)fprintf(stderr,"SPECTRAL_CHAIN_ARM_FAIL mode=%u; partial evidence retained\n",mode);
    if(file)fclose(file);
    for(stream=0;stream<4;stream++)destroy(chains[stream]);
    free(data);umx_free(model);return status;
}
static void *guest_worker(void *p) {
    GuestTask *task=(GuestTask*)p;int first,second;
    first=check_mode(task->argv[1],task->argv[3],0);
    second=check_mode(task->argv[2],task->argv[3],1);
    task->result=first||second;return NULL;
}
int main(int argc,char **argv) {
    pthread_attr_t attr;pthread_t thread;GuestTask task;int result;
    if(argc!=4){fprintf(stderr,"usage: spectral_chain_arm_check FLOAT_MODEL INT8_MODEL FIXTURE\n");return 2;}
    task.argc=argc;task.argv=argv;task.result=1;
    if(pthread_attr_init(&attr))return 1;
    result=pthread_attr_setstacksize(&attr,262144);
    if(!result)result=pthread_create(&thread,&attr,guest_worker,&task);
    pthread_attr_destroy(&attr);if(result)return 1;
    if(pthread_join(thread,NULL))return 1;
    return task.result;
}
