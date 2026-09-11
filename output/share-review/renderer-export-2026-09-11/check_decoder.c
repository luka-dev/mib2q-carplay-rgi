/* PRIVATE: synthetic component; never copied into the source handout. */
#include "sink.h"
#include "output_backend.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct output_backend { void *buffers[2]; unsigned int size; };
static OMX_COMPONENTTYPE component;
static OMX_CALLBACKTYPE callbacks;
static void *app;
static OMX_BUFFERHEADERTYPE *pending[2];
static unsigned int queued, live_headers, initialized, handle_live;
static unsigned int starts, destroys, submitted, configs, presented;
static int reject_input;

static OMX_ERRORTYPE command(OMX_HANDLETYPE h, OMX_COMMANDTYPE cmd,
                            OMX_U32 value, OMX_PTR data) {
    (void)data;
    assert(h==&component && cmd==OMX_CommandStateSet);
    if (value==OMX_StateIdle) queued=0;
    return callbacks.EventHandler(h,app,OMX_EventCmdComplete,cmd,value,NULL);
}
static OMX_ERRORTYPE get_parameter(OMX_HANDLETYPE h, OMX_INDEXTYPE index, OMX_PTR p) {
    OMX_PARAM_PORTDEFINITIONTYPE *pd=p;
    assert(h==&component && index==OMX_IndexParamPortDefinition && pd->nPortIndex<2);
    pd->nBufferCountActual=2; pd->nBufferSize=64;
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE set_parameter(OMX_HANDLETYPE h, OMX_INDEXTYPE index, OMX_PTR p) {
    OMX_PARAM_PORTDEFINITIONTYPE *pd=p;
    assert(h==&component && index==OMX_IndexParamPortDefinition && pd->nPortIndex==0);
    assert(pd->format.video.eCompressionFormat==OMX_VIDEO_CodingAVC);
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE use_buffer(OMX_HANDLETYPE h, OMX_BUFFERHEADERTYPE **out,
                               OMX_U32 port, OMX_PTR private_data, OMX_U32 size,
                               OMX_U8 *memory) {
    assert(h==&component && port<2 && memory);
    *out=calloc(1,sizeof(**out)); assert(*out);
    (*out)->pBuffer=memory; (*out)->nAllocLen=size; (*out)->pAppPrivate=private_data;
    ++live_headers;
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE allocate_buffer(OMX_HANDLETYPE h, OMX_BUFFERHEADERTYPE **out,
                                    OMX_U32 port, OMX_PTR private_data, OMX_U32 size) {
    assert(port==0);
    return use_buffer(h,out,port,private_data,size,calloc(1,size));
}
static OMX_ERRORTYPE free_buffer(OMX_HANDLETYPE h, OMX_U32 port, OMX_BUFFERHEADERTYPE *b) {
    assert(h==&component && live_headers);
    if (port==0) free(b->pBuffer);
    free(b); --live_headers;
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE fill_buffer(OMX_HANDLETYPE h, OMX_BUFFERHEADERTYPE *b) {
    assert(h==&component && queued<2);
    pending[queued++]=b;
    return OMX_ErrorNone;
}
static OMX_ERRORTYPE empty_buffer(OMX_HANDLETYPE h, OMX_BUFFERHEADERTYPE *b) {
    OMX_U32 flags=b->nFlags;
    assert(h==&component && b->nFilledLen<=b->nAllocLen && b->nOffset==0);
    if (reject_input) { reject_input=0; return OMX_ErrorUndefined; }
    assert(flags & OMX_BUFFERFLAG_ENDOFFRAME);
    ++submitted;
    callbacks.EmptyBufferDone(h,app,b);
    if (flags & OMX_BUFFERFLAG_CODECCONFIG) { ++configs; return OMX_ErrorNone; }
    assert(queued);
    b=pending[--queued];
    memset(b->pBuffer,0x5a,16); b->nFilledLen=16; b->nOffset=0;
    callbacks.FillBufferDone(h,app,b);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE OMX_Init(void) { assert(!initialized); initialized=1; return OMX_ErrorNone; }
OMX_ERRORTYPE OMX_Deinit(void) {
    assert(initialized && !handle_live && !live_headers);
    initialized=0; return OMX_ErrorNone;
}
OMX_ERRORTYPE OMX_GetHandle(OMX_HANDLETYPE *out, OMX_STRING name,
                           OMX_PTR private_data, OMX_CALLBACKTYPE *cb) {
    assert(initialized && !handle_live && strcmp(name,"synthetic.component")==0);
    memset(&component,0,sizeof(component));
    component.SendCommand=command; component.GetParameter=get_parameter;
    component.SetParameter=set_parameter; component.UseBuffer=use_buffer;
    component.AllocateBuffer=allocate_buffer; component.FreeBuffer=free_buffer;
    component.EmptyThisBuffer=empty_buffer; component.FillThisBuffer=fill_buffer;
    callbacks=*cb; app=private_data; *out=&component; handle_live=1; ++starts;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE OMX_FreeHandle(OMX_HANDLETYPE h) {
    assert(h==&component && handle_live && !live_headers);
    handle_live=0; return OMX_ErrorNone;
}
int output_backend_get_config(output_config *c) {
    c->component_name="synthetic.component"; c->width=32; c->height=16;
    c->input_port=0; c->output_port=1; return 0;
}
int output_backend_configure_decoder(OMX_HANDLETYPE h,const output_config *c) {
    assert(h==&component && c->output_port==1); return 0;
}
output_backend_t *output_backend_create(const output_config *c, unsigned int n, unsigned int size) {
    output_backend_t *o=calloc(1,sizeof(*o));
    assert(c->width==32 && n==2 && o); o->size=size;
    for (unsigned int i=0;i<n;++i) { o->buffers[i]=calloc(1,size); assert(o->buffers[i]); }
    return o;
}
void *output_backend_pointer(output_backend_t *o,unsigned int i) { assert(i<2); return o->buffers[i]; }
unsigned int output_backend_capacity(output_backend_t *o,unsigned int i) { assert(i<2); return o->size; }
int output_backend_present(output_backend_t *o,unsigned int i,const OMX_BUFFERHEADERTYPE *b) {
    assert(i<2 && b->pBuffer==o->buffers[i] && b->nFilledLen==16);
    for (unsigned int j=0;j<b->nFilledLen;++j) assert(b->pBuffer[j]==0x5a);
    ++presented; return 0;
}
void output_backend_destroy(output_backend_t *o) {
    assert(!handle_live && !live_headers && !initialized);
    free(o->buffers[0]); free(o->buffers[1]); free(o); ++destroys;
}
int main(void) {
    const uint8_t config[]={0,0,0,1,0x67,0x12,0,0,0,1,0x68,0x34};
    const uint8_t frame[]={0,0,0,1,0x65,0x56};
    uint8_t large[65]={0,0,0,1,0x65};
    assert(av_sink_open()==0);
    av_sink_frame(config,sizeof(config));
    assert(starts==1 && submitted==1 && configs==1 && !presented);
    av_sink_frame(frame,sizeof(frame));
    assert(submitted==2 && presented==1);
    av_sink_frame(large,sizeof(large));
    assert(submitted==2);
    reject_input=1; av_sink_frame(frame,sizeof(frame));
    assert(submitted==2);
    for (unsigned int i=0;i<12;++i) av_sink_frame(frame,sizeof(frame));
    assert(submitted==14 && presented==13);
    callbacks.EventHandler(&component,app,OMX_EventPortSettingsChanged,1,0,NULL);
    av_sink_frame(frame,sizeof(frame)); assert(submitted==14);
    av_sink_session_end(); assert(destroys==1 && !handle_live && !live_headers);
    av_sink_frame(frame,sizeof(frame));
    assert(starts==2 && submitted==14); /* Previous config did not leak into this session. */
    av_sink_frame(config,sizeof(config)); av_sink_frame(frame,sizeof(frame));
    assert(configs==2 && submitted==16 && presented==14);
    av_sink_session_end(); av_sink_close();
    assert(destroys==2 && !initialized && !live_headers);
    puts("decoder mock passed: two sessions, config isolation, buffer recycle, rejected/oversized input, output error, release order");
    return 0;
}
