/* ARM/QNX numerical and state check, no real-device performance claim.
 * Include the exact runtime TU to exercise its private scalar/NEON dot kernel.
 * Fixture: UMXQA001 uint32 streams/frames then input, float expected, int8 expected
 * for each stream/frame. Reads only32/64 TRAIN prefix frames, not unseen audio.
 */
#include "unified_runtime.c"
#define INPUT_FLOATS (UMX_BINS*4)
#define OUTPUT_FLOATS (UMX_SOURCES*UMX_BINS*4+4*UMX_BANDS*UMX_FAMILIES)
static int kernel_check(void) {
    unsigned sizes[]={1,7,16,24,32,534},i,j,t;volatile int8_t x[534],w[534];
    for(t=0;t<4;t++)for(i=0;i<6;i++) {
        int32_t exact=0,actual;
        for(j=0;j<sizes[i];j++) {
            x[j]=(int8_t)(t==0?127:t==1?-127:(int)((j*97+13)%255)-127);
            w[j]=(int8_t)(t==0?127:t==1?127:(int)((j*71+37+t)%255)-127);
            exact+=(int32_t)x[j]*w[j];
        }
        actual=dot_int8((const int8_t*)x,(const int8_t*)w,sizes[i]);if(actual!=exact){fprintf(stderr,"ARM_DOT_CASE n=%u pattern=%u exact=%ld actual=%ld x0=%d w0=%d\n",sizes[i],t,(long)exact,(long)actual,(int)x[0],(int)w[0]);return 0;}
    }
    return 1;
}
static int check_model(int argc,char **argv) {
    UmxModel *model=NULL;UmxState *states=NULL;UmxOutput *output=NULL;
    float *fixture=NULL,*expected=NULL;FILE *f=NULL;char magic[8],error[192];
    unsigned header[2],stream,frame,source,bin,c,d,mode,nframes,nstreams;size_t record_floats,total;
    double numerator[4]={0},denominator[4]={0},scene_max=0;int passed=1,status=1;
    if(argc!=4){fprintf(stderr,"usage: unified_runtime_arm_check MODEL FIXTURE float|int8\n");return 2;}
    if(!strcmp(argv[3],"float"))mode=0;else if(!strcmp(argv[3],"int8"))mode=1;else return 2;
    {
        unsigned char bytes[16];unsigned k;void *(*volatile fill)(void*,int,size_t)=memset;
        fill(bytes,-127,sizeof(bytes));
        fprintf(stderr,"QNX_MEMSET_NEGATIVE_BYTES");for(k=0;k<16;k++)fprintf(stderr," %02x",bytes[k]);fprintf(stderr,"\n");
    }
    if(!kernel_check()){fprintf(stderr,"ARM_DOT_EXACT_FAIL\n");return 1;}
    model=umx_load(argv[1],error,sizeof(error));if(!model){fprintf(stderr,"%s\n",error);goto done;}
    if(model->mode!=mode)goto done;
    f=fopen(argv[2],"rb");if(!f)goto done;
    if(fread(magic,1,8,f)!=8||memcmp(magic,"UMXQA001",8)||fread(header,4,2,f)!=2||!header[0]||!header[1]||header[0]>4||header[1]>64)goto done;
    nstreams=header[0];nframes=header[1];record_floats=INPUT_FLOATS+2*OUTPUT_FLOATS;total=(size_t)nstreams*nframes*record_floats;
    fixture=(float*)malloc(total*sizeof(float));states=(UmxState*)calloc(nstreams,sizeof(*states));output=(UmxOutput*)malloc(sizeof(*output));
    if(!fixture||!states||!output||fread(fixture,sizeof(float),total,f)!=total||fgetc(f)!=EOF)goto done;
    /* Frame first interleaves independent streams; Windows references were
       processed whole-stream first. Mutable shared state would fail parity. */
    for(frame=0;frame<nframes;frame++)for(stream=0;stream<nstreams;stream++) {
        float *input=fixture+((size_t)stream*nframes+frame)*record_floats;
        expected=input+INPUT_FLOATS+mode*OUTPUT_FLOATS;
        if(umx_predict(model,&states[stream],(const float (*)[2][2])input,output))goto done;
        if(output->dense_macs!=1540128)goto done;
        for(source=0;source<4;source++)for(bin=0;bin<UMX_BINS;bin++)for(c=0;c<2;c++) {
            unsigned x=(bin*2+c)*2,o=((source*UMX_BINS+bin)*2+c)*2;
            double ar=input[x]*output->mask[source][bin][c][0]-input[x+1]*output->mask[source][bin][c][1];
            double ai=input[x]*output->mask[source][bin][c][1]+input[x+1]*output->mask[source][bin][c][0];
            double er=input[x]*expected[o]-input[x+1]*expected[o+1],ei=input[x]*expected[o+1]+input[x+1]*expected[o];
            if(bin==0||bin==UMX_BINS-1)ai=ei=0;
            numerator[source]+=(ar-er)*(ar-er)+(ai-ei)*(ai-ei);denominator[source]+=er*er+ei*ei;
        }
        for(d=0;d<4;d++) {
            const float *actual=d==0?&output->presence[0][0]:d==1?&output->membership[0][0]:d==2?&output->pan[0][0]:&output->update[0][0];
            for(bin=0;bin<UMX_BANDS*UMX_FAMILIES;bin++) {
                double error=fabs(actual[bin]-expected[16400+d*288+bin]);if(!isfinite(error))goto done;
                if(error>scene_max)scene_max=error;
            }
        }
    }
    for(source=0;source<4;source++)if(!isfinite(numerator[source])||denominator[source]<=0||numerator[source]/denominator[source]>(mode?1e-4:1e-7))passed=0;
    if(scene_max>(mode?.05:1e-4))passed=0;
    printf("ARM_RUNTIME_CHECK {\"mode\":\"%s\",\"passed\":%s,\"streams_interleaved\":%u,\"frames_per_stream\":%u,\"dot_cases_exact\":24,\"source_estimate_nse\":[%.12g,%.12g,%.12g,%.12g],\"scene_max_abs_error\":%.12g,\"scope\":\"QEMU ARM/QNX numerical execution only; not head-unit deadlines or int8 acoustic success\"}\n",argv[3],passed?"true":"false",nstreams,nframes,numerator[0]/denominator[0],numerator[1]/denominator[1],numerator[2]/denominator[2],numerator[3]/denominator[3],scene_max);
    status=passed?0:1;
done:
    if(status)fprintf(stderr,"ARM_RUNTIME_NUMERIC_FAIL\n");
    if(f)fclose(f);
    free(fixture);free(states);free(output);umx_free(model);return status;
}

int main(int argc,char **argv) {
    char *args[4];int first,second;
    if(argc!=4){fprintf(stderr,"usage: unified_runtime_arm_check FLOAT_MODEL INT8_MODEL FIXTURE\n");return 2;}
    args[0]=argv[0];args[1]=argv[1];args[2]=argv[3];args[3]="float";
    first=check_model(4,args);
    args[1]=argv[2];args[3]="int8";second=check_model(4,args);
    return first||second;
}
