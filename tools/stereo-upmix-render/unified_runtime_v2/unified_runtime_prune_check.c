/* Fixed TRAIN spectrum replay: all outputs versus eligible/scene-only.
 * Exact equivalence and actual matrix-count gate, not acoustic qualification.
 */
#include "unified_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INPUT_FLOATS (UMX_BINS*4)
#ifndef UMX_CHECK_ALL_MACS
#define UMX_CHECK_ALL_MACS 1173392
#define UMX_CHECK_SELECTED_MACS 976592
#endif
static int check(const char *model_path,const char *fixture_path,const char *mode) {
    UmxModel *model=NULL;UmxState *all_state=NULL,*selected_state=NULL;
    UmxOutput *all=NULL,*selected=NULL;float *input=NULL;
    char error[192],magic[8];unsigned h[3],frame,stream,source,bin,c,d;FILE *f=NULL;int status=1;
    model=umx_load(model_path,error,sizeof(error));if(!model){fprintf(stderr,"%s\n",error);goto done;}
    f=fopen(fixture_path,"rb");if(!f)goto done;
    if(fread(magic,1,8,f)!=8||memcmp(magic,"UMXSP001",8)||fread(h,4,3,f)!=3||h[0]<1||h[0]>4||h[1]<1||h[1]>2000||h[2]!=INPUT_FLOATS)goto done;
    input=(float*)malloc((size_t)h[0]*h[1]*INPUT_FLOATS*sizeof(float));
    all_state=(UmxState*)calloc(h[0],sizeof(*all_state));selected_state=(UmxState*)calloc(h[0],sizeof(*selected_state));
    all=(UmxOutput*)malloc(sizeof(*all));selected=(UmxOutput*)malloc(sizeof(*selected));
    if(!input||!all_state||!selected_state||!all||!selected||fread(input,sizeof(float),(size_t)h[0]*h[1]*INPUT_FLOATS,f)!=(size_t)h[0]*h[1]*INPUT_FLOATS||fgetc(f)!=EOF)goto done;
    for(frame=0;frame<h[1];frame++)for(stream=0;stream<h[0];stream++) {
        const float (*spectrum)[2][2]=(const float (*)[2][2])(input+((size_t)stream*h[1]+frame)*INPUT_FLOATS);
        if(umx_predict(model,&all_state[stream],spectrum,all)||umx_predict_eligible(model,&selected_state[stream],spectrum,selected))goto done;
        if(all->dense_macs!=UMX_CHECK_ALL_MACS||selected->dense_macs!=UMX_CHECK_SELECTED_MACS)goto done;
        if(memcmp(all->mask[0],selected->mask[0],sizeof(all->mask[0]))||memcmp(&all_state[stream],&selected_state[stream],sizeof(UmxState)))goto done;
        if(memcmp(all->presence,selected->presence,sizeof(all->presence))||memcmp(all->membership,selected->membership,sizeof(all->membership))||
           memcmp(all->pan,selected->pan,sizeof(all->pan))||memcmp(all->update,selected->update,sizeof(all->update)))goto done;
        for(source=1;source<4;source++)for(bin=0;bin<UMX_BINS;bin++)for(c=0;c<2;c++)for(d=0;d<2;d++)if(selected->mask[source][bin][c][d]!=0)goto done;
    }
    printf("UNIFIED_PRUNE_CHECK {\"mode\":\"%s\",\"passed\":true,\"streams_interleaved\":%u,\"frames_per_stream\":%u,\"eligible_scene_state_bit_exact\":true,\"protected_fields_zero\":true,\"all_dense_macs_frame\":%u,\"selected_dense_macs_frame\":%u,\"selected_dense_macs_s\":%.0f,\"scope\":\"exact output-selection equivalence on inspected TRAIN, not original quantization quality recovery or device deadlines\"}\n",mode,h[0],h[1],(unsigned)UMX_CHECK_ALL_MACS,(unsigned)UMX_CHECK_SELECTED_MACS,(double)UMX_CHECK_SELECTED_MACS*93.75);
    status=0;
done:
    if(status)fprintf(stderr,"UNIFIED_PRUNE_CHECK_FAILED %s\n",mode);
    if(f)fclose(f);
    free(input);free(all_state);free(selected_state);free(all);free(selected);umx_free(model);return status;
}
int main(int argc,char **argv) {
    int a,b;if(argc!=4){fprintf(stderr,"usage: prune_check FLOAT_MODEL INT8_MODEL INPUT_SPECTRA\n");return 2;}
    a=check(argv[1],argv[3],"float");b=check(argv[2],argv[3],"int8");return a||b;
}
