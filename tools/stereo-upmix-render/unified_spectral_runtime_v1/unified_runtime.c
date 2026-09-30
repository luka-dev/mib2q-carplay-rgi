/* Independent float/int8 neural runtime for the fixed width48 grouped-time spectral graph.
 * Matrices: symmetric row int8; activations: dynamic per vector; int32 sums.
 * Norms, biases, recurrent state and nonlinearities remain float32.
 * No allocation during predict. All temporal/scene state belongs to caller.
 */
#include "unified_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

#define TENSORS 480
#define MAX_INPUT 534
#define W 48
#define GROUPS 4
#define GROUP_WIDTH 12
#define CHANNELS 8
#define KERNEL 7
#define LOCAL 24
#define GLOBAL 24
#define DECODER 24
typedef struct {
    char name[96];
    unsigned ndim, rows, cols, storage;
    float *data, *scale;
    int8_t *q;
} Tensor;
typedef struct { const Tensor *weight, *bias; } Linear;
typedef struct { const Tensor *weight, *bias; unsigned n; } Norm;
typedef struct { Linear input, hidden; unsigned size; } Gru;
typedef struct {
    Norm time_norm, frequency_norm;
    Gru time[GROUPS];
    Linear time_proj, neighbours, down, full_band[CHANNELS], up;
} Block;
struct UmxModel {
    Tensor tensors[TENSORS];
    Norm enc_norm[UMX_BANDS];
    Linear enc[UMX_BANDS], interface, dec_first[UMX_BANDS], dec_last[UMX_BANDS], scene;
    Block blocks[2];
    Gru global;
    unsigned lo[UMX_BANDS], hi[UMX_BANDS];
    float log_mean, log_std, level_alpha;
    unsigned mode, depth, local, global_width, decoder, tensor_count;
};

static unsigned read_u32(FILE *f, int *ok) {
    unsigned char b[4];
    if (fread(b,1,4,f)!=4) { *ok=0; return 0; }
    return (unsigned)b[0]|((unsigned)b[1]<<8)|((unsigned)b[2]<<16)|((unsigned)b[3]<<24);
}
static unsigned read_u16(FILE *f, int *ok) {
    unsigned char b[2];
    if (fread(b,1,2,f)!=2) { *ok=0; return 0; }
    return (unsigned)b[0]|((unsigned)b[1]<<8);
}
static float read_float(FILE *f, int *ok) {
    uint32_t bits=read_u32(f,ok); float v;
    memcpy(&v,&bits,4);
    if (!isfinite(v)) *ok=0;
    return v;
}
static const Tensor *find(const UmxModel *m, const char *name, unsigned ndim, unsigned rows, unsigned cols) {
    unsigned i;
    for (i=0;i<m->tensor_count;i++) {
        const Tensor *t=&m->tensors[i];
        if (!strcmp(t->name,name)) return t->ndim==ndim && t->rows==rows && t->cols==cols ? t : NULL;
    }
    return NULL;
}
static int linear_bind(const UmxModel *m, Linear *l, const char *prefix, unsigned in, unsigned out) {
    char name[96];
    snprintf(name,sizeof(name),"%s.weight",prefix); l->weight=find(m,name,2,out,in);
    snprintf(name,sizeof(name),"%s.bias",prefix); l->bias=find(m,name,1,out,1);
    return l->weight && l->bias;
}
static int norm_bind(const UmxModel *m, Norm *l, const char *prefix, unsigned n) {
    char name[96];
    snprintf(name,sizeof(name),"%s.weight",prefix); l->weight=find(m,name,1,n,1);
    snprintf(name,sizeof(name),"%s.bias",prefix); l->bias=find(m,name,1,n,1); l->n=n;
    return l->weight && l->bias;
}
static int gru_bind(const UmxModel *m, Gru *g, const char *prefix, unsigned in, unsigned hidden, int reverse) {
    char name[96]; const char *tail=reverse ? "_reverse" : "";
    snprintf(name,sizeof(name),"%s.weight_ih_l0%s",prefix,tail);g->input.weight=find(m,name,2,3*hidden,in);
    snprintf(name,sizeof(name),"%s.bias_ih_l0%s",prefix,tail);g->input.bias=find(m,name,1,3*hidden,1);
    snprintf(name,sizeof(name),"%s.weight_hh_l0%s",prefix,tail);g->hidden.weight=find(m,name,2,3*hidden,hidden);
    snprintf(name,sizeof(name),"%s.bias_hh_l0%s",prefix,tail);g->hidden.bias=find(m,name,1,3*hidden,1);
    g->size=hidden;
    return g->input.weight && g->input.bias && g->hidden.weight && g->hidden.bias;
}
void umx_free(UmxModel *m) {
    unsigned i; if (!m) return;
    for (i=0;i<TENSORS;i++) {free(m->tensors[i].data);free(m->tensors[i].scale);free(m->tensors[i].q);}
    free(m);
}
UmxModel *umx_load(const char *path, char *error, unsigned error_size) {
    FILE *f=NULL; UmxModel *m=NULL; int ok=1; unsigned i,j,n,offset=0; char magic[8],name[96];
    f=fopen(path,"rb"); if (!f) goto fail;
    m=(UmxModel*)calloc(1,sizeof(*m)); if (!m) goto fail;
    if (fread(magic,1,8,f)!=8 || memcmp(magic,"UMXSPN01",8) || read_u32(f,&ok)!=1) goto fail;
    m->mode=read_u32(f,&ok);
    if (m->mode>1 || read_u32(f,&ok)!=48) goto fail;
    m->depth=read_u32(f,&ok);m->local=read_u32(f,&ok);
    m->global_width=read_u32(f,&ok);m->decoder=read_u32(f,&ok);
    if (read_u32(f,&ok)!=48 || m->depth!=2 || m->local!=24 ||
        m->global_width!=24 || m->decoder!=24) goto fail;
    m->log_mean=read_float(f,&ok);m->log_std=read_float(f,&ok);m->level_alpha=read_float(f,&ok);
    m->tensor_count=read_u32(f,&ok);
    if (m->log_std<=0 || m->level_alpha<=0 || m->level_alpha>=1 ||
        m->tensor_count!=TENSORS) goto fail;
    for (i=0;i<m->tensor_count;i++) {
        Tensor *t=&m->tensors[i]; unsigned length=read_u16(f,&ok);
        t->ndim=read_u16(f,&ok);t->rows=read_u32(f,&ok);t->cols=read_u32(f,&ok);t->storage=read_u32(f,&ok);
        if (!ok || length<1 || length>=sizeof(t->name) || t->rows<1 || t->rows>1424 || t->cols<1 || t->cols>MAX_INPUT ||
            (t->ndim!=1 && t->ndim!=2) || (t->ndim==1 && t->cols!=1) || t->storage>1 ||
            t->storage!=(m->mode && t->ndim==2 ? 1u : 0u)) goto fail;
        if (fread(t->name,1,length,f)!=length) goto fail;
        t->name[length]=0;
        for (j=0;j<i;j++) if (!strcmp(t->name,m->tensors[j].name)) goto fail;
        n=t->rows*t->cols;
        if (t->storage) {
            t->scale=(float*)malloc(t->rows*sizeof(float));t->q=(int8_t*)malloc(n);
            if (!t->scale || !t->q) goto fail;
            for (j=0;j<t->rows;j++) {t->scale[j]=read_float(f,&ok);if (t->scale[j]<=0) goto fail;}
            if (fread(t->q,1,n,f)!=n) goto fail;
            for (j=0;j<n;j++) if (t->q[j]==-128) goto fail;
        } else {
            t->data=(float*)malloc(n*sizeof(float));if (!t->data) goto fail;
            for (j=0;j<n;j++) t->data[j]=read_float(f,&ok);
        }
        if (!ok) goto fail;
    }
    if (fgetc(f)!=EOF) goto fail;
    fclose(f);f=NULL;
    for (i=0;i<UMX_BANDS;i++) {
        unsigned width=i<11 ? 4 : i<22 ? 8 : i<35 ? 16 : i<43 ? 32 : i<47 ? 85 : 89;
        m->lo[i]=offset;m->hi[i]=offset+width;offset+=width;
        snprintf(name,sizeof(name),"enc.%u.0",i);if (!norm_bind(m,&m->enc_norm[i],name,width*6)) goto fail;
        snprintf(name,sizeof(name),"enc.%u.1",i);if (!linear_bind(m,&m->enc[i],name,width*6,W)) goto fail;
        snprintf(name,sizeof(name),"dec.%u.0",i);if (!linear_bind(m,&m->dec_first[i],name,m->local+m->global_width,m->decoder)) goto fail;
        snprintf(name,sizeof(name),"dec.%u.2",i);if (!linear_bind(m,&m->dec_last[i],name,m->decoder,width*16)) goto fail;
    }
    for (i=0;i<m->depth;i++) {
        Block *b=&m->blocks[i];
        snprintf(name,sizeof(name),"blocks.%u.time_norm",i);if (!norm_bind(m,&b->time_norm,name,W)) goto fail;
        for (j=0;j<GROUPS;j++) {
            snprintf(name,sizeof(name),"blocks.%u.time.%u",i,j);
            if (!gru_bind(m,&b->time[j],name,GROUP_WIDTH,GROUP_WIDTH,0)) goto fail;
        }
        snprintf(name,sizeof(name),"blocks.%u.time_proj",i);if (!linear_bind(m,&b->time_proj,name,W,W)) goto fail;
        snprintf(name,sizeof(name),"blocks.%u.frequency_norm",i);if (!norm_bind(m,&b->frequency_norm,name,W)) goto fail;
        snprintf(name,sizeof(name),"blocks.%u.neighbours",i);if (!linear_bind(m,&b->neighbours,name,KERNEL,W)) goto fail;
        snprintf(name,sizeof(name),"blocks.%u.down",i);if (!linear_bind(m,&b->down,name,W,CHANNELS)) goto fail;
        for (j=0;j<CHANNELS;j++) {
            snprintf(name,sizeof(name),"blocks.%u.full_band.%u",i,j);
            if (!linear_bind(m,&b->full_band[j],name,UMX_BANDS,UMX_BANDS)) goto fail;
        }
        snprintf(name,sizeof(name),"blocks.%u.up",i);if (!linear_bind(m,&b->up,name,CHANNELS,W)) goto fail;
    }
    if (!linear_bind(m,&m->interface,"interface",W,m->local) || !gru_bind(m,&m->global,"global_gru",m->local,m->global_width,0) ||
        !linear_bind(m,&m->scene,"scene",m->local+m->global_width,18)) goto fail;
    return m;
fail:
    if (f) fclose(f);
    umx_free(m);
    if (error && error_size) snprintf(error,error_size,"Invalid, incomplete or unsupported unified model");
    return NULL;
}

static int round_even(float v) {
    float low=floorf(v),rest=v-low; int integer=(int)low;
    return rest>.5f || (rest==.5f && (integer&1)) ? integer+1 : integer;
}
static int32_t dot_int8(const int8_t *x,const int8_t *w,unsigned n) {
    unsigned i=0; int32_t result=0;
#ifdef __ARM_NEON__
    int32x4_t total=vdupq_n_s32(0);
    for (;i+16<=n;i+=16) {
        int8x16_t a=vld1q_s8(x+i),b=vld1q_s8(w+i);
        total=vpadalq_s16(total,vmull_s8(vget_low_s8(a),vget_low_s8(b)));
        total=vpadalq_s16(total,vmull_s8(vget_high_s8(a),vget_high_s8(b)));
    }
    result=vgetq_lane_s32(total,0)+vgetq_lane_s32(total,1)+vgetq_lane_s32(total,2)+vgetq_lane_s32(total,3);
#endif
    for (;i<n;i++) result+=(int32_t)x[i]*(int32_t)w[i];
    return result;
}
static void linear(const Linear *l,const float *x,float *y,uint64_t *macs) {
    const Tensor *w=l->weight; unsigned i,j; int8_t q[MAX_INPUT]; float scale=1;
    if (w->storage) {
        float maximum=0;
        for (j=0;j<w->cols;j++) if (fabsf(x[j])>maximum) maximum=fabsf(x[j]);
        if (maximum>0) scale=maximum/127;
        for (j=0;j<w->cols;j++) {int v=round_even(x[j]/scale);q[j]=(int8_t)(v>127?127:v<-127?-127:v);}
    }
    for (i=0;i<w->rows;i++) {
        float sum=0;
        if (w->storage) sum=(float)dot_int8(q,w->q+i*w->cols,w->cols)*scale*w->scale[i];
        else for (j=0;j<w->cols;j++) sum+=x[j]*w->data[i*w->cols+j];
        y[i]=sum+l->bias->data[i];
    }
    *macs+=(uint64_t)w->rows*w->cols;
}
/* Outputs are frequency-major, four sources, stereo, real/imag. Eligible
 * occupies the first4 of each16 rows. The same vector quantizer/dot order
 * preserves those rows exactly while avoiding three unused training heads. */
static void linear_eligible(const Linear *l,const float *x,float *y,uint64_t *macs) {
    const Tensor *w=l->weight;unsigned i,j;int8_t q[MAX_INPUT];float scale=1;
    if(w->storage) {
        float maximum=0;
        for(j=0;j<w->cols;j++)if(fabsf(x[j])>maximum)maximum=fabsf(x[j]);
        if(maximum>0)scale=maximum/127;
        for(j=0;j<w->cols;j++){int v=round_even(x[j]/scale);q[j]=(int8_t)(v>127?127:v<-127?-127:v);}
    }
    memset(y,0,w->rows*sizeof(float));
    for(i=0;i<w->rows;i++)if(i%16<4) {
        float sum=0;
        if(w->storage)sum=(float)dot_int8(q,w->q+i*w->cols,w->cols)*scale*w->scale[i];
        else for(j=0;j<w->cols;j++)sum+=x[j]*w->data[i*w->cols+j];
        y[i]=sum+l->bias->data[i];
    }
    *macs+=(uint64_t)(w->rows/4)*w->cols;
}
static float depthwise_row(const Linear *l,unsigned row,const float x[KERNEL],uint64_t *macs) {
    const Tensor *w=l->weight;unsigned j;float sum=0;
    if (w->storage) {
        float maximum=0,scale=1;int8_t q[KERNEL];
        for (j=0;j<KERNEL;j++)if(fabsf(x[j])>maximum)maximum=fabsf(x[j]);
        if(maximum>0)scale=maximum/127;
        for(j=0;j<KERNEL;j++){int v=round_even(x[j]/scale);q[j]=(int8_t)(v>127?127:v<-127?-127:v);}
        sum=(float)dot_int8(q,w->q+row*KERNEL,KERNEL)*scale*w->scale[row];
    } else for(j=0;j<KERNEL;j++)sum+=x[j]*w->data[row*KERNEL+j];
    *macs+=KERNEL;
    return sum+l->bias->data[row];
}
static void norm(const Norm *l,const float *x,float *y) {
    unsigned i;double mean=0,var=0;
    for (i=0;i<l->n;i++) mean+=x[i];
    mean/=l->n;
    for (i=0;i<l->n;i++) {double d=x[i]-mean;var+=d*d;}
    var=1/sqrt(var/l->n+1e-5);
    for (i=0;i<l->n;i++) y[i]=(float)((x[i]-mean)*var)*l->weight->data[i]+l->bias->data[i];
}
static float sigmoid(float x) {return 1/(1+expf(-x));}
static void gru(const Gru *g,const float *x,float *state,uint64_t *macs) {
    float a[72],b[72];unsigned i,n=g->size;
    linear(&g->input,x,a,macs);linear(&g->hidden,state,b,macs);
    for (i=0;i<n;i++) {
        float reset=sigmoid(a[i]+b[i]),update=sigmoid(a[n+i]+b[n+i]);
        float candidate=tanhf(a[2*n+i]+reset*b[2*n+i]);
        state[i]=(1-update)*candidate+update*state[i];
    }
}
void umx_reset(UmxState *s) {if(s) memset(s,0,sizeof(*s));}
static int predict_selected(const UmxModel *m,UmxState *s,const float input[UMX_BINS][2][2],UmxOutput *out,int eligible_only) {
    float features[UMX_BINS][6],x[UMX_BANDS][W],normalized[UMX_BANDS][W],frequency[UMX_BANDS][W],narrow[UMX_BANDS][CHANNELS],mixed[UMX_BANDS][CHANNELS];
    float local[UMX_BANDS][LOCAL],mean[LOCAL]={0},z[LOCAL+GLOBAL],work[MAX_INPUT],temp[1424];
    double energy=0;float scale;unsigned i,j,k,d,source,c;uint64_t macs=0;
    if (!m || !s || !input || !out) return -1;
    for(i=0;i<UMX_BINS;i++) for(c=0;c<2;c++) {
        float real=input[i][c][0],imag=input[i][c][1],mag;
        if (!isfinite(real) || !isfinite(imag) || fabsf(real)>8192 || fabsf(imag)>8192 ||
            ((i==0 || i==UMX_BINS-1) && imag!=0)) return -2;
        mag=hypotf(real,imag);energy+=mag;
        features[i][c]=real;features[i][2+c]=imag;
        features[i][4+c]=(logf(mag+1e-5f)-m->log_mean)/m->log_std;
        if (features[i][4+c]>6)features[i][4+c]=6;
        if (features[i][4+c]<-6)features[i][4+c]=-6;
    }
    energy/=UMX_BINS*2;
    if (!s->frames)s->level=(float)energy;
    s->level=(1-m->level_alpha)*s->level+m->level_alpha*(float)energy;
    scale=s->level>1e-6f ? s->level : 1e-6f;
    for(i=0;i<UMX_BINS;i++)for(j=0;j<4;j++)features[i][j]/=scale;
    for(k=0;k<UMX_BANDS;k++) {
        norm(&m->enc_norm[k],&features[m->lo[k]][0],work);linear(&m->enc[k],work,x[k],&macs);
    }
    for(d=0;d<m->depth;d++) {
        const Block *b=&m->blocks[d];
        for(k=0;k<UMX_BANDS;k++) {
            norm(&b->time_norm,x[k],work);
            for(j=0;j<GROUPS;j++)gru(&b->time[j],work+j*GROUP_WIDTH,s->time[d][k]+j*GROUP_WIDTH,&macs);
            linear(&b->time_proj,s->time[d][k],temp,&macs);
            for(j=0;j<W;j++)x[k][j]+=temp[j];
            norm(&b->frequency_norm,x[k],normalized[k]);
        }
        for(k=0;k<UMX_BANDS;k++) {
            for(j=0;j<W;j++) {
                float neighbours[KERNEL];unsigned q;
                for(q=0;q<KERNEL;q++) {
                    int band=(int)k+(int)q-KERNEL/2;
                    neighbours[q]=band<0 || band>=UMX_BANDS ? 0 : normalized[band][j];
                }
                frequency[k][j]=depthwise_row(&b->neighbours,j,neighbours,&macs);
            }
            linear(&b->down,normalized[k],narrow[k],&macs);
            for(j=0;j<CHANNELS;j++)if(narrow[k][j]<0)narrow[k][j]=0;
        }
        for(j=0;j<CHANNELS;j++) {
            for(k=0;k<UMX_BANDS;k++)work[k]=narrow[k][j];
            linear(&b->full_band[j],work,temp,&macs);
            for(k=0;k<UMX_BANDS;k++)mixed[k][j]=temp[k]>0?temp[k]:0;
        }
        for(k=0;k<UMX_BANDS;k++) {
            linear(&b->up,mixed[k],temp,&macs);
            for(j=0;j<W;j++)x[k][j]=x[k][j]+frequency[k][j]+temp[j];
        }
    }
    for(k=0;k<UMX_BANDS;k++) {linear(&m->interface,x[k],local[k],&macs);for(j=0;j<m->local;j++)mean[j]+=local[k][j]/UMX_BANDS;}
    gru(&m->global,mean,s->global,&macs);
    for(k=0;k<UMX_BANDS;k++) {
        memcpy(z,local[k],m->local*sizeof(float));memcpy(z+m->local,s->global,m->global_width*sizeof(float));
        linear(&m->dec_first[k],z,work,&macs);for(j=0;j<m->decoder;j++)work[j]=tanhf(work[j]);
        if(eligible_only)linear_eligible(&m->dec_last[k],work,temp,&macs);
        else linear(&m->dec_last[k],work,temp,&macs);
        for(i=m->lo[k];i<m->hi[k];i++)for(source=0;source<UMX_SOURCES;source++)for(c=0;c<2;c++)for(j=0;j<2;j++)
            out->mask[source][i][c][j]=temp[(((i-m->lo[k])*UMX_SOURCES+source)*2+c)*2+j];
        linear(&m->scene,z,temp,&macs);
        for(j=0;j<UMX_FAMILIES;j++) {
            float presence=sigmoid(temp[j]),pan=tanhf(temp[6+j]),rate=sigmoid(temp[12+j]);
            if (!s->frames) {s->membership[k][j]=presence;s->pan[k][j]=pan;}
            s->membership[k][j]+=rate*(presence-s->membership[k][j]);s->pan[k][j]+=rate*(pan-s->pan[k][j]);
            out->presence[k][j]=presence;out->membership[k][j]=s->membership[k][j];out->pan[k][j]=s->pan[k][j];out->update[k][j]=rate;
        }
    }
    out->dense_macs=macs;s->frames++;
    return 0;
}

int umx_predict(const UmxModel *m,UmxState *s,const float input[UMX_BINS][2][2],UmxOutput *out) {
    return predict_selected(m,s,input,out,0);
}
int umx_predict_eligible(const UmxModel *m,UmxState *s,const float input[UMX_BINS][2][2],UmxOutput *out) {
    return predict_selected(m,s,input,out,1);
}
