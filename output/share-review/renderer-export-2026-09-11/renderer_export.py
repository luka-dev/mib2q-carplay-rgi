"""PRIVATE export recipe: real renderer excerpts, no target output backend."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
NOTICE = '/* Copyright (c) 2026, original project author. */\n'

def uncomment(s):
    return re.sub(r'/\*[\s\S]*?\*/', '', s)

def fn(text, signature):
    start=text.index(signature); brace=text.index('{',start); end=brace+1; depth=1
    while depth:
        if text[end]=='{': depth+=1
        elif text[end]=='}': depth-=1
        end+=1
    return text[start:end]

def tidy(s):
    return re.sub(r'\n[ \t]*\n(?:[ \t]*\n)+','\n\n',s).strip()+'\n'

def export():
    files={}
    original_main=(ROOT/'altscreen_render/main.c').read_text()
    original_sink=(ROOT/'altscreen_render/sink_omx_qnx.c').read_text()
    main=uncomment(original_main)
    sink=uncomment(original_sink)
    # Preserve the actual queue and decoder-worker implementation. Control,
    # presentation status, platform startup and startup watchdog are omitted.
    queue=main[main.index('enum decode_item_type'):main.index('static int decode_video_ready')]
    queue += '\n'+fn(main,'static void decode_worker_stop(')
    queue=queue.replace('altscreen_render:', 'video_receiver:')
    signal_handler=fn(main,'static void on_signal(')
    listener=fn(main,'static int listen_socket(').replace('listen_socket(void)','listen_socket(unsigned short port)')
    listener=listener.replace('htons(ALTAV_PORT)','htons(port)').replace('inet_addr(ALTAV_HOST)','htonl(INADDR_LOOPBACK)')
    files['renderer/main.c']=NOTICE+'''/* Adapted from the production receiver: bounded queue and sink lifecycle. */
#define _POSIX_C_SOURCE 200809L
#include "../altscreen/frame_io.h"
#include "sink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_listen_fd = -1;
static volatile sig_atomic_t g_client_fd = -1;
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001UL
#endif
#define MAX_RECORD_BYTES (4u * 1024u * 1024u)

'''+tidy(queue)+'\n'+signal_handler+'\n\n'+listener+'''

/* Adapted receive loop. Input is a complete, already-plain Annex-B record.
 * Data-plane silence is allowed; socket EOF/errors end a session.
 */
static void serve_client(int fd, uint32_t generation)
{
    int one=1;
    uint8_t *buffer=NULL;
    uint32_t capacity=0, length=0;
    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
    setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&one,sizeof(one));
    while (g_running) {
        int result=frame_read(fd,&buffer,&capacity,&length,MAX_RECORD_BYTES);
        if (result != 0) {
            fprintf(stderr,"video_receiver: record reader ended status=%d\\n",result);
            break;
        }
        if (length && decode_queue_frame(generation,buffer,length) != 0) break;
    }
    free(buffer);
}

int main(int argc, char **argv)
{
    char *end=NULL;
    long port;
    int lfd;
    if (argc!=2) { fprintf(stderr,"usage: %s PORT\\n",argv[0]); return 2; }
    errno=0;
    port=strtol(argv[1],&end,10);
    if (errno || !*argv[1] || *end || port<1 || port>65535) return 2;
    signal(SIGINT,on_signal);
    signal(SIGTERM,on_signal);
    signal(SIGPIPE,SIG_IGN);
    setvbuf(stderr,NULL,_IONBF,0);
    if (av_sink_open()!=0) return 1;
    if (decode_worker_start()!=0) { av_sink_close(); return 1; }
    lfd=listen_socket((unsigned short)port);
    if (lfd<0) { decode_worker_stop(); av_sink_close(); return 1; }
    g_listen_fd=lfd;
    fprintf(stderr,"video_receiver: listening on loopback port %ld\\n",port);
    while (g_running) {
        int fd=accept(lfd,NULL,NULL);
        if (fd<0) { if (errno==EINTR) continue; break; }
        g_client_fd=fd;
        uint32_t generation=decode_queue_begin();
        if (generation) {
            serve_client(fd,generation);
            decode_queue_end(generation);
        }
        if ((int)g_client_fd==fd) { g_client_fd=-1; close(fd); }
    }
    if ((int)g_listen_fd==lfd) { g_listen_fd=-1; close(lfd); }
    decode_worker_stop();
    av_sink_close();
    return 0;
}
'''
    files['renderer/sink.h']=NOTICE+'''#ifndef SHARE_VIDEO_SINK_H
#define SHARE_VIDEO_SINK_H
#include <stdint.h>

/* Lifecycle used by the real receiver worker. One decoder instance per process.
 * open/close run before/after that worker. frame/session_end are serialized.
 * Input starts with one record containing all required SPS/PPS, followed by
 * complete video access units in decode order. Parameter changes require a
 * new session. Mixed configuration+picture records are not supported.
 */
int av_sink_open(void);
void av_sink_frame(const uint8_t *annexb, uint32_t length);
void av_sink_session_end(void);
void av_sink_close(void);
#endif
'''
    # Capture is a real host backend, not a placeholder for the missing display.
    files['renderer/sink_capture.c']=NOTICE+'''/* Adapted host sink: counters, optional raw capture, no display claims. */
#include "sink.h"
#include <stdio.h>
#include <stdlib.h>

static FILE *output;
static unsigned long messages, bytes;
static int write_failed;

int av_sink_open(void)
{
    const char *path=getenv("VIDEO_CAPTURE_FILE");
    if (path && *path) {
        output=fopen(path,"wb");
        if (!output) { perror("video capture"); return -1; }
    }
    return 0;
}

void av_sink_frame(const uint8_t *data, uint32_t length)
{
    if (!data || !length || write_failed) return;
    if (output && fwrite(data,1,length,output)!=length) {
        write_failed=1;
        fprintf(stderr,"video_capture: write failed\\n");
        return;
    }
    ++messages;
    bytes+=length;
}

void av_sink_session_end(void)
{
    if (output && fflush(output)!=0) write_failed=1;
    fprintf(stderr,"video_capture: session end messages=%lu bytes=%lu error=%d\\n",
            messages,bytes,write_failed);
    messages=bytes=0;
}

void av_sink_close(void)
{
    if (output) { fclose(output); output=NULL; }
}
'''
    files['renderer/Makefile']='''CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra

# Host-only receiver/capture build. No hardware backend is linked here.
video_receiver: main.c sink_capture.c sink.h ../altscreen/frame_io.c ../altscreen/frame_io.h
	$(CC) $(CFLAGS) main.c sink_capture.c ../altscreen/frame_io.c -pthread -o $@

.PHONY: clean
clean:
	rm -f video_receiver
'''
    # The real OMX state machine is retained; target output code is replaced by
    # a declared adapter contract, with no supplied fake implementation.
    files['renderer/output_backend.h']=NOTICE+'''#ifndef SHARE_OUTPUT_BACKEND_H
#define SHARE_OUTPUT_BACKEND_H
#include "OMX_Core.h"
#include "OMX_Component.h"

typedef struct output_backend output_backend_t;
typedef struct {
    const char *component_name;
    int width, height;
    OMX_U32 input_port, output_port;
} output_config;

/* Implement these functions for your own platform. No implementation is
 * included. No component name, vendor extension, pixel layout or allocator
 * is selected by the core. The component name must outlive the sink.
 */
int output_backend_get_config(output_config *config);
int output_backend_configure_decoder(OMX_HANDLETYPE decoder, const output_config *config);
output_backend_t *output_backend_create(const output_config *config,
                                        unsigned int count, unsigned int minimum_size);
void *output_backend_pointer(output_backend_t *output, unsigned int index);
unsigned int output_backend_capacity(output_backend_t *output, unsigned int index);

/* Called on the component's callback thread. The buffer remains owned by this
 * call until it returns; consume/copy/synchronize it before returning because
 * the core then recycles it. Return 0 on success, nonzero to latch a sink error.
 * Respect filled length/offset and the output format selected by your adapter.
 */
int output_backend_present(output_backend_t *output, unsigned int index,
                           const OMX_BUFFERHEADERTYPE *header);
/* Called after component buffer headers and the component are released. */
void output_backend_destroy(output_backend_t *output);
#endif
'''
    structure=sink[sink.index('typedef struct {'):sink.index('static ScreenSink g_ss;')]
    structure=structure.replace('screen_egl_present_t','output_backend_t')
    structure=structure.replace('ScreenSink','VideoSink')
    structure=re.sub(r'^.*\b(?:presentWidth|presentHeight|presentOutCount|presentationHealthy|presentConsecutiveFails|presentRecoveryPending|presentRecoveryAttempts|presentNeedsFullRebuild|presentRecoveryAfterMs|viewAreaMode|layoutStyle)\b.*\n','',structure,flags=re.M)
    structure=structure.replace('    OMX_HANDLETYPE', '    output_config outputConfig;\n    OMX_HANDLETYPE',1)
    def sf(signature):
        f=fn(sink,signature).replace('ScreenSink','VideoSink')
        f=f.replace('altscreen_render[qnx]','video_decoder')
        f=f.replace('kInPortIndex','me->outputConfig.input_port').replace('kOutPortIndex','me->outputConfig.output_port')
        return f
    enter=sf('static int _CallbackEnter(')
    leave=sf('static void _CallbackLeave(')
    event='''static OMX_ERRORTYPE _OnEvent(OMX_HANDLETYPE c, OMX_PTR app, OMX_EVENTTYPE event,
                               OMX_U32 d1, OMX_U32 d2, OMX_PTR extra)
{
    VideoSink *me=(VideoSink *)app;
    (void)c; (void)extra;
    pthread_mutex_lock(&me->lock);
    if (event==OMX_EventCmdComplete && d1==OMX_CommandStateSet) {
        me->state=(OMX_STATETYPE)d2;
        me->stateReached=1;
    } else if (event==OMX_EventError ||
               (event==OMX_EventPortSettingsChanged && d1==me->outputConfig.output_port)) {
        me->omxError=1;
        fprintf(stderr,"video_decoder: component error or unsupported output reconfiguration\\n");
    }
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->lock);
    return OMX_ErrorNone;
}'''
    empty=sf('static OMX_ERRORTYPE _OnEmptyDone(')
    present='''static int _PresentOutput(VideoSink *me, OMX_BUFFERHEADERTYPE *header)
{
    OMX_U32 i;
    if (!me->winReady || !header->nFilledLen) return -1;
    for (i=0;i<me->nOutBufs;++i) {
        if (me->outBufs[i]!=header) continue;
        int result=output_backend_present(me->present,(unsigned int)i,header);
        pthread_mutex_lock(&me->lock);
        if (result==0) ++me->outputPosted;
        else { ++me->outputPostFailed; me->omxError=1; }
        pthread_mutex_unlock(&me->lock);
        return result;
    }
    return -1;
}'''
    fill=sf('static OMX_ERRORTYPE _OnFillDone(').replace('_PresentGpu(me, b);','_PresentOutput(me, b);')
    helpers='\n\n'.join(sf(sig) for sig in ('static void _AbsTimeoutMs(', 'static unsigned long long _NowMs(', 'static int _WaitState('))
    configure=sf('static int _ConfigurePorts(')
    cutoff=configure.index('\n    memset(&pd, 0, sizeof(pd));',configure.index('if (OMX_SetParameter'))
    configure=configure[:cutoff]+'''\n    return output_backend_configure_decoder(me->decoder,&me->outputConfig);\n}'''
    make_output='''static OMX_U32 _MakeOutput(VideoSink *me)
{
    OMX_PARAM_PORTDEFINITIONTYPE pd;
    memset(&pd,0,sizeof(pd)); pd.nSize=sizeof(pd); pd.nVersion.nVersion=OMX_VERSION;
    pd.nPortIndex=me->outputConfig.output_port;
    if (OMX_GetParameter(me->decoder,OMX_IndexParamPortDefinition,&pd)) return 0;
    if (!pd.nBufferCountActual || !pd.nBufferSize) return 0;
    me->present=output_backend_create(&me->outputConfig,pd.nBufferCountActual,pd.nBufferSize);
    if (!me->present) return 0;
    me->winReady=1;
    return pd.nBufferCountActual;
}'''
    alloc=sf('static int _AllocInput(').replace('count = pd.nBufferCountActual;', 'count = pd.nBufferCountActual;\n    if (!count || !pd.nBufferSize) return -1;')
    use=sf('static int _UseScreenBuffers(').replace('_UseScreenBuffers','_UseOutputBuffers')
    use=use.replace('screen_egl_present_source_pointer','output_backend_pointer').replace('screen_egl_present_source_capacity','output_backend_capacity')
    a=use.index('        if (!vaddr')
    b=use.index('        if (OMX_UseBuffer',a)
    use=use[:a]+'''        if (!vaddr || capacity < pd.nBufferSize) goto fail;\n'''+use[b:]
    teardown=sf('static void _Teardown(').replace(', int destroyPresent','')
    teardown=teardown.replace('(!destroyPresent || !me->present)','!me->present')
    teardown=teardown.replace('live OMX/EGL storage','live component storage')
    teardown=teardown.replace('    if (destroyPresent) _DestroyPresent(me);', '''    if (me->present) { output_backend_destroy(me->present); me->present=NULL; }''')
    teardown=re.sub(r'^.*\b(?:presentationHealthy|presentConsecutiveFails|presentRecoveryPending|presentRecoveryAttempts|presentNeedsFullRebuild|presentRecoveryAfterMs)\b.*\n','',teardown,flags=re.M)
    teardown=teardown.replace('    if (!destroyPresent && me->present)\n        screen_egl_present_start_loader(me->present);','')
    start=sf('static int _StartDecoder(').replace('(OMX_STRING)kOMXDecoderName','(OMX_STRING)me->outputConfig.component_name')
    start=start.replace('    _EnableDecoderOrderMode(me);','')
    start=start.replace('outCount = _ReadOutputDefinition(me);','outCount = _MakeOutput(me);')
    start=start.replace('outCount == 0 || _MakePresent(me, outCount) != 0','outCount == 0')
    start=start.replace('_UseScreenBuffers','_UseOutputBuffers').replace('_Teardown(me, 0)','_Teardown(me)')
    start=start.replace('native-32m %u Screen window bufs -> EGL GPU RGBA','%u adapter output buffers')
    feed=sf('static void _FeedInput(')
    has_config=sf('static int _HasParameterSet(')
    api='''int av_sink_open(void)
{
    memset(&g_ss,0,sizeof(g_ss));
    if (output_backend_get_config(&g_ss.outputConfig)!=0 || !g_ss.outputConfig.component_name ||
        !*g_ss.outputConfig.component_name || g_ss.outputConfig.width<=0 || g_ss.outputConfig.height<=0 ||
        g_ss.outputConfig.input_port==g_ss.outputConfig.output_port) return -1;
    if (pthread_mutex_init(&g_ss.lock,NULL)!=0) return -1;
    if (pthread_cond_init(&g_ss.cond,NULL)!=0) { pthread_mutex_destroy(&g_ss.lock); return -1; }
    g_ss.width=g_ss.outputConfig.width; g_ss.height=g_ss.outputConfig.height;
    return 0;
}
'''
    frame=sf('void av_sink_frame(').replace('the OMX pool / dio','component resources')
    a=frame.index('    _RecoverPresentationIfNeeded(')
    b=frame.index('    if (!g_ss.started)',a)
    frame=frame[:a]+'''    /* A latched runtime error requires a new session in this excerpt. */
    if (g_ss.started && g_ss.omxError) return;
'''+frame[b:]
    end='''void av_sink_session_end(void)
{
    int worked=g_ss.sessionEverStarted;
    _Teardown(&g_ss);
    g_ss.configLen=0;
    g_ss.sessionEverStarted=0;
    if (worked) { g_ss.startFails=0; g_ss.retryAfterMs=0; }
}

void av_sink_close(void)
{
    _Teardown(&g_ss);
    pthread_cond_destroy(&g_ss.cond);
    pthread_mutex_destroy(&g_ss.lock);
}
'''
    files['renderer/sink_omx.c']=NOTICE+'''/* Adapted real decoder core; output_backend implementation is intentionally separate.
 * This core is for an isolated renderer process. A stalled data callback can
 * force process exit rather than freeing buffers still used by that callback.
 */
#define _POSIX_C_SOURCE 200809L
#include "sink.h"
#include "output_backend.h"
#include "OMX_Video.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

'''+tidy(structure)+'''
static VideoSink g_ss;
#define kOmxStartIdleWaitMs 10000L
#define kOmxStartExecWaitMs 2000L
#define kOmxStopWaitMs 900L
#define kCallbackDrainWaitMs 1500L

'''+tidy('\n\n'.join([enter,leave,event,empty,present,fill]))+'''
static OMX_CALLBACKTYPE kCallbacks={_OnEvent,_OnEmptyDone,_OnFillDone};

'''+tidy('\n\n'.join([helpers,configure,make_output,alloc,use,teardown,start,feed,api,has_config,frame,end]))
    files['renderer/README.md']='''# Renderer source subset

This directory contains substantial excerpts adapted from the actual receiver
and OpenMAX decoder, not a fictional renderer API demonstration.

Included: framed TCP reception; bounded worker queue; per-connection begin/end;
stale queued-frame rejection; sink lifecycle; configuration caching; component
state transitions; input-buffer recycling; output callbacks; bounded waits and
callback draining; a real host counter/capture backend.

## Host receiver

`make -C renderer` builds `renderer/video_receiver` from the host backend.
Run it with one positional TCP port argument; it binds loopback only. There is
no production endpoint built into this subset. No file is written by default.
Set `VIDEO_CAPTURE_FILE` to a path to create/truncate a raw elementary-stream
capture. Use one session per capture when you need one independently playable
stream. Capture counts accepted payload bytes; it does not claim decoded output.

Send records as `[u32 big-endian size][payload]`. Each nonempty payload must be
plain Annex-B data. Start with one configuration record containing all required
SPS/PPS, then send complete video access units. Do not split the configuration
across records; parameter changes require a new session. A zero-length record
is ignored. This is the local receiver's
record format, not a phone-facing protocol. The caller supplies the input.

The queue is bounded; overload ends that connection. The next connection starts
a fresh sink session. Shutdown discards pending work, so wait for the session-end
log before terminating if you need all already-submitted capture bytes flushed.

## Decoder and output boundary

`sink_omx.c` is the adapted real OpenMAX core. It is not part of the host capture
build. It requires your platform's OpenMAX headers/libraries and implementations
of the functions declared in `output_backend.h`. No fake implementations are
supplied: the interface is the explicit boundary where your integration begins.

That backend selects/configures the component, owns suitable output buffers,
exposes their pointers/capacity, consumes decoded output and releases resources.
Component-specific memory layout, extension setup and display binding are not
provided. No graphics presentation implementation, layer registration, loader
assets, display control channel, SDK headers or deployment scripts are included.

The adapted core expects output buffers configurable before the initial state
transition. Dynamic output reconfiguration is reported as an error; full platform
recovery was omitted. Runtime errors require a fresh sink session. Input timestamp
policy remains the excerpt's zero timestamps; a timed-output adapter needs its
own integration. Mixed configuration+picture records are not supported.

The worker queue and core are useful source starting points, not a claim that
arbitrary OpenMAX components can be driven without adaptation. Source-level
adaptation and host/mock checks are distinguished from hardware validation in
the top-level README. The output backend has not been supplied or tested here.
'''
    return files
