/*
 * audio.c — Phase-4 AUDIO backend for our from-source libairplay.
 *
 * Provides the AudioStream* API the AirPlay engine calls (contract:
 * AccessorySDK/Support/AudioUtils.h), on the QNX Sound Architecture (QSA)
 * plugin API — the exact set the stock libairplay imports from libasound.so.2:
 *   snd_pcm_open_name / snd_pcm_plugin_{params,prepare,setup,write,read,status,flush} / snd_pcm_close
 *
 * One AudioStream = one PCM direction, chosen by kAudioStreamProperty_Input:
 *   output stream: I/O thread PULLs PCM via the engine's output callback -> plugin_write (playback)
 *   input stream:  I/O thread plugin_read (capture) -> PUSHes PCM to the engine's input callback
 * Software volume (RampVolume) scales S16 samples in the output path.
 *
 * STATUS: compiles against QSA headers; real audio device only on the unit
 * (VM x86 has no io-audio). QNX 6.5 ARMv7 gcc 4.4: no __thread.
 */
#include "CommonServices.h"
#include "DebugServices.h"
#include "LogUtils.h"
#include "CFUtils.h"
#include "AudioUtils.h"

#include <sys/asoundlib.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ulog_define( AudioStream, kLogLevelNotice, kLogFlags_Default, "AudioStream", NULL );
#define as_ulog( LEVEL, ... )  ulog( &log_category_from_name( AudioStream ), (LEVEL), __VA_ARGS__ )

typedef struct
{
    CFRuntimeBase                   base;

    AudioStreamBasicDescription     asbd;       /* format (from kAudioStreamProperty_Format) */
    Boolean                         input;      /* true = capture, false = playback */
    uint32_t                        bufBytes;   /* IO buffer size */
    int32_t                         threadPrio;
    char                            threadName[32];

    AudioStreamOutputCallback_f     outCB;   void *outCtx;
    AudioStreamInputCallback_f      inCB;    void *inCtx;

    snd_pcm_t *                     pcm;
    int                             pcmFd;

    volatile double                 volume;     /* 0..1, applied to S16 output */
    volatile int                    running;
    pthread_t                       thread;

}   AudioStreamImp;

/* ---- CF runtime class ----------------------------------------------------- */
static void _AudioStreamFinalize( CFTypeRef inCF );
static dispatch_once_t   gInitOnce = 0;
static CFTypeID          gTypeID   = _kCFRuntimeNotATypeID;
static const CFRuntimeClass kAudioStreamClass =
{
    0, "AudioStream", NULL, NULL, _AudioStreamFinalize, NULL, NULL, NULL, NULL, NULL, NULL
};
static void _GetTypeID( void *ctx ) { (void)ctx; gTypeID = _CFRuntimeRegisterClass( &kAudioStreamClass ); }

CFTypeID AudioStreamGetTypeID( void )
{
    dispatch_once_f( &gInitOnce, NULL, _GetTypeID );
    return gTypeID;
}

/* ---- API: Create / Finalize ----------------------------------------------- */
OSStatus AudioStreamCreate( AudioStreamRef *outStream )
{
    OSStatus         err;
    AudioStreamImp * me;
    size_t           extraLen = sizeof( *me ) - sizeof( me->base );

    me = (AudioStreamImp *) _CFRuntimeCreateInstance( NULL, AudioStreamGetTypeID(), (CFIndex) extraLen, NULL );
    require_action( me, exit, err = kNoMemoryErr );
    memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
    me->volume   = 1.0;
    me->bufBytes = 4096;
    *outStream = (AudioStreamRef) me;
    err = kNoErr;
exit:
    return err;
}

static void _AudioStreamFinalize( CFTypeRef inCF )
{
    AudioStreamImp * const me = (AudioStreamImp *) inCF;
    AudioStreamStop( (AudioStreamRef) me, false );
}

/* ---- API: callbacks / context --------------------------------------------- */
void AudioStreamSetOutputCallback( AudioStreamRef s, AudioStreamOutputCallback_f fn, void *ctx )
{ AudioStreamImp * const me = (AudioStreamImp *) s; me->outCB = fn; me->outCtx = ctx; }

void AudioStreamSetInputCallback( AudioStreamRef s, AudioStreamInputCallback_f fn, void *ctx )
{ AudioStreamImp * const me = (AudioStreamImp *) s; me->inCB = fn; me->inCtx = ctx; }

/* ---- API: SetProperty ----------------------------------------------------- */
OSStatus _AudioStreamSetProperty( CFTypeRef inObject, CFStringRef inProperty, CFTypeRef inValue )
{
    AudioStreamImp * const me = (AudioStreamImp *) inObject;
    if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
    {
        CFGetData( inValue, &me->asbd, sizeof( me->asbd ), NULL, NULL );
    }
    else if( CFEqual( inProperty, kAudioStreamProperty_Input ) )   me->input    = CFGetBoolean( inValue, NULL );
    else if( CFEqual( inProperty, kAudioStreamProperty_Volume ) )  me->volume   = CFGetDouble( inValue, NULL );
    else if( CFEqual( inProperty, kAudioStreamProperty_IOBufferSize ) ) me->bufBytes = (uint32_t) CFGetInt64( inValue, NULL );
    else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) ) me->threadPrio = (int32_t) CFGetInt64( inValue, NULL );
    /* other keys accepted/ignored */
    return kNoErr;
}

OSStatus AudioStreamRampVolume( AudioStreamRef s, double inFinalVolume, double inDurationSecs, dispatch_queue_t inQueue )
{
    AudioStreamImp * const me = (AudioStreamImp *) s;
    (void)inDurationSecs; (void)inQueue;   /* ponytail: instantaneous set; add ramp on-device if audible */
    me->volume = inFinalVolume;
    return kNoErr;
}

/* ---- QSA setup ------------------------------------------------------------ */
static OSStatus _OpenPCM( AudioStreamImp *me )
{
    snd_pcm_channel_params_t   params;
    int                        dir = me->input ? SND_PCM_OPEN_CAPTURE : SND_PCM_OPEN_PLAYBACK;
    int                        chn = me->input ? SND_PCM_CHANNEL_CAPTURE : SND_PCM_CHANNEL_PLAYBACK;
    int                        rc;

    rc = snd_pcm_open_name( &me->pcm, "default", dir | SND_PCM_OPEN_NONBLOCK );
    require_action( rc >= 0, exit, as_ulog( kLogLevelWarning, "snd_pcm_open_name: %s\n", snd_strerror( rc ) ) );

    memset( &params, 0, sizeof( params ) );
    params.mode                  = SND_PCM_MODE_BLOCK;
    params.channel               = chn;
    params.start_mode            = me->input ? SND_PCM_START_DATA : SND_PCM_START_FULL;
    params.stop_mode             = SND_PCM_STOP_STOP;
    params.format.interleave     = 1;
    params.format.format         = SND_PCM_SFMT_S16_LE;
    params.format.rate           = me->asbd.mSampleRate ? (int) me->asbd.mSampleRate : 44100;
    params.format.voices         = me->asbd.mChannelsPerFrame ? me->asbd.mChannelsPerFrame : 2;
    params.buf.block.frag_size   = me->bufBytes;
    params.buf.block.frags_min   = 1;
    params.buf.block.frags_max   = 4;

    rc = snd_pcm_plugin_params( me->pcm, &params );
    require_action( rc >= 0, exit, as_ulog( kLogLevelWarning, "plugin_params: %s\n", snd_strerror( rc ) ) );
    rc = snd_pcm_plugin_prepare( me->pcm, chn );
    require_action( rc >= 0, exit, as_ulog( kLogLevelWarning, "plugin_prepare: %s\n", snd_strerror( rc ) ) );

    /* A capture read may otherwise block forever with no microphone data;
     * AudioStreamStop joins this thread before closing the PCM handle. */
    rc = snd_pcm_nonblock_mode( me->pcm, 1 );
    require_action( rc >= 0, exit, as_ulog( kLogLevelWarning, "nonblock_mode: %s\n", snd_strerror( rc ) ) );

    me->pcmFd = snd_pcm_file_descriptor( me->pcm, chn );
    require_action( me->pcmFd >= 0, exit, as_ulog( kLogLevelWarning, "pcm_file_descriptor: %d\n", me->pcmFd ) );
    return kNoErr;
exit:
    if( me->pcm ) { snd_pcm_close( me->pcm ); me->pcm = NULL; }
    return kUnknownErr;
}

/* recover a stopped channel (underrun on playback / overrun on capture) */
static void _Recover( AudioStreamImp *me, int chn )
{
    snd_pcm_channel_status_t st;
    memset( &st, 0, sizeof( st ) );
    st.channel = chn;
    if( snd_pcm_plugin_status( me->pcm, &st ) >= 0 &&
        ( st.status == SND_PCM_STATUS_UNDERRUN || st.status == SND_PCM_STATUS_OVERRUN ) )
    {
        snd_pcm_plugin_prepare( me->pcm, chn );
    }
}

static void _ApplyVolume( int16_t *s, size_t n, double vol )
{
    size_t i;
    if( vol >= 0.999 ) return;
    for( i = 0; i < n; ++i ) s[i] = (int16_t) ( s[i] * vol );
}

static int _WaitPCM( AudioStreamImp *me )
{
    fd_set set;
    struct timeval timeout = { 0, 100000 };
    int rc;
    FD_ZERO( &set );
    FD_SET( me->pcmFd, &set );
    rc = me->input ? select( me->pcmFd + 1, &set, NULL, NULL, &timeout ) :
                     select( me->pcmFd + 1, NULL, &set, NULL, &timeout );
    if( rc < 0 && errno != EINTR ) usleep( 5000 );
    return rc;
}

/* ---- I/O thread ----------------------------------------------------------- */
static void * _IOThread( void *arg )
{
    AudioStreamImp * const me = (AudioStreamImp *) arg;
    uint8_t *buf = (uint8_t *) malloc( me->bufBytes );
    uint32_t sampleTime = 0;
    if( !buf ) return NULL;

    while( me->running )
    {
        if( _WaitPCM( me ) <= 0 || !me->running ) continue;
        if( me->input )   /* capture -> engine */
        {
            int n = snd_pcm_plugin_read( me->pcm, buf, me->bufBytes );
            if( n == -EAGAIN || n == 0 ) continue;
            if( n < 0 ) { _Recover( me, SND_PCM_CHANNEL_CAPTURE ); usleep( 5000 ); continue; }
            if( me->inCB ) me->inCB( sampleTime, 0, buf, (size_t) n, me->inCtx );
            sampleTime += (uint32_t) ( n / ( me->asbd.mBytesPerFrame ? me->asbd.mBytesPerFrame : 2 ) );
        }
        else              /* engine -> playback */
        {
            size_t written = 0;
            memset( buf, 0, me->bufBytes );
            if( me->outCB ) me->outCB( sampleTime, 0, buf, me->bufBytes, me->outCtx );
            _ApplyVolume( (int16_t *) buf, me->bufBytes / 2, me->volume );
            while( me->running && written < me->bufBytes )
            {
                int n = snd_pcm_plugin_write( me->pcm, buf + written, me->bufBytes - written );
                if( n > 0 ) { written += (size_t) n; continue; }
                if( n == -EAGAIN || n == 0 ) { _WaitPCM( me ); continue; }
                _Recover( me, SND_PCM_CHANNEL_PLAYBACK );
                usleep( 5000 );
                break;
            }
            sampleTime += me->bufBytes / ( me->asbd.mBytesPerFrame ? me->asbd.mBytesPerFrame : 4 );
        }
    }
    free( buf );
    return NULL;
}

/* ---- API: Prepare / Start / Stop ------------------------------------------ */
OSStatus AudioStreamPrepare( AudioStreamRef inStream )
{
    return _OpenPCM( (AudioStreamImp *) inStream );
}

OSStatus AudioStreamStart( AudioStreamRef inStream )
{
    AudioStreamImp * const me = (AudioStreamImp *) inStream;
    if( !me->pcm ) { OSStatus e = _OpenPCM( me ); if( e ) return e; }
    me->running = 1;
    if( pthread_create( &me->thread, NULL, _IOThread, me ) != 0 ) { me->running = 0; return kUnknownErr; }
    return kNoErr;
}

void AudioStreamStop( AudioStreamRef inStream, Boolean inDrain )
{
    AudioStreamImp * const me = (AudioStreamImp *) inStream;
    if( me->running )
    {
        me->running = 0;
        pthread_join( me->thread, NULL );
    }
    if( me->pcm )
    {
        if( inDrain ) snd_pcm_plugin_flush( me->pcm, me->input ? SND_PCM_CHANNEL_CAPTURE : SND_PCM_CHANNEL_PLAYBACK );
        snd_pcm_close( me->pcm );
        me->pcm = NULL;
    }
}
