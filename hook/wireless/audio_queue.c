/* Bounded AU reorder before decoding: predictive codecs must never be decoded
 * in UDP arrival order. Native QSA consumes the resulting S16 frame timeline. */
#include "audio_queue.h"
#include <stdlib.h>
#include <string.h>
#define AU_COUNT 64u
struct au { uint32_t timestamp; unsigned length; uint8_t data[CP_AUDIO_AU_MAX]; };
struct cp_audio_queue {
    cp_audio_codec *codec;
    cp_audio_geometry geometry;
    unsigned count, latency_ms, started, have_input;
    uint64_t deadline;
    uint32_t cursor, input_end;
    unsigned pcm_frames, pcm_offset;
    uint32_t pcm_stamp;
    int16_t pcm[CP_AUDIO_SAMPLES_MAX];
    struct au au[AU_COUNT];
    /* A permutation of all slots: [0,count) is timestamp ordered, the
     * remainder is free. Moving one-byte indices never relocates AU data. */
    uint8_t order[AU_COUNT];
};
static int32_t distance(uint32_t a,uint32_t b) { return (int32_t)(a-b); }
cp_audio_queue *cp_audio_queue_create(uint64_t format,unsigned latency_ms)
{
    cp_audio_queue *q=calloc(1,sizeof(*q));
    if (!q || cp_audio_geometry_for(format,&q->geometry)) goto failed;
    q->codec=cp_audio_codec_create(format,0);
    if (!q->codec) goto failed;
    for (unsigned i=0;i<AU_COUNT;++i) q->order[i]=(uint8_t)i;
    q->latency_ms=latency_ms>300?300:latency_ms;
    return q;
failed:
    cp_audio_queue_destroy(q); return NULL;
}
void cp_audio_queue_destroy(cp_audio_queue *q)
{ if (q) { cp_audio_codec_destroy(q->codec); free(q); } }
void cp_audio_queue_reset(cp_audio_queue *q)
{
    if (!q) return;
    q->count=q->started=q->have_input=q->pcm_frames=q->pcm_offset=0;
    q->deadline=0;
    cp_audio_codec_reset(q->codec);
}
static void pop(cp_audio_queue *q)
{
    uint8_t slot=q->order[0];
    --q->count;
    memmove(q->order,q->order+1,q->count);
    q->order[q->count]=slot;
}
int cp_audio_queue_put(cp_audio_queue *q,uint32_t timestamp,const void *au,
                       size_t length,uint64_t now)
{
    unsigned i;
    if (!q || !au || !length || length>CP_AUDIO_AU_MAX) return -1;
    if (q->started && distance(timestamp+q->geometry.frames,q->cursor)<=0) return 0;
    if (q->have_input && distance(timestamp+q->geometry.frames,q->input_end)<=0) return 0;
    for (i=0;i<q->count;++i) if (q->au[q->order[i]].timestamp==timestamp) return 0;
    /* A bounded discontinuity/overflow recovery clears codec history too.
     * Never retain seconds of old audio while accepting newer AUs. */
    if (q->count==AU_COUNT || ((q->started || q->have_input) &&
        distance(timestamp,q->cursor)>(int32_t)(2*q->geometry.rate))) cp_audio_queue_reset(q);
    uint8_t slot=q->order[q->count];
    for (i=q->count;i && distance(timestamp,q->au[q->order[i-1]].timestamp)<0;--i)
        q->order[i]=q->order[i-1];
    q->order[i]=slot;
    q->au[slot].timestamp=timestamp; q->au[slot].length=(unsigned)length;
    memcpy(q->au[slot].data,au,length); ++q->count;
    if (!q->deadline) q->deadline=now+(uint64_t)q->latency_ms*1000000u+1;
    return 0;
}
int cp_audio_queue_read(cp_audio_queue *q,int16_t *out,size_t samples,uint64_t now)
{
    size_t remaining;
    unsigned channels;
    if (!q || !out || samples>65536 || samples%q->geometry.channels) return -1;
    memset(out,0,samples*sizeof(*out));
    channels=q->geometry.channels; remaining=samples/channels;
    if (!q->started) {
        if (!q->count || now<q->deadline) return 0;
        q->cursor=q->au[q->order[0]].timestamp;
        q->started=1;
    }
    while (remaining) {
        unsigned n;
        if (q->pcm_offset<q->pcm_frames) {
            int32_t gap=distance(q->pcm_stamp+q->pcm_offset,q->cursor);
            n=q->pcm_frames-q->pcm_offset;
            if(gap<0) {
                unsigned skip=0u-(uint32_t)gap;if(skip>n)skip=n;
                q->pcm_offset+=skip;continue;
            }
            if(gap>0) {
                n=(uint32_t)gap;if(n>remaining)n=(unsigned)remaining;
                q->cursor+=n;out+=n*channels;remaining-=n;continue;
            }
            if (n>remaining) n=(unsigned)remaining;
            memcpy(out,q->pcm+q->pcm_offset*channels,n*channels*sizeof(*out));
            q->pcm_offset+=n; q->cursor+=n;
        } else if (!q->count) {
            /* Rebuffer without letting silence advance the source timeline. */
            q->started=0; q->deadline=0; return 0;
        } else {
            struct au *a=&q->au[q->order[0]];
            unsigned frames=0;
            /* Never feed a retransmitted/overlapping AU to a predictive
             * decoder after a later source timestamp. */
            if(q->have_input && distance(a->timestamp,q->input_end)<0) { pop(q);continue; }
            q->pcm_stamp=a->timestamp;
            q->have_input=1;q->input_end=a->timestamp+q->geometry.frames;
            int err=cp_audio_codec_decode(q->codec,a->data,a->length,q->pcm,
                                          CP_AUDIO_SAMPLES_MAX,&frames);
            pop(q);
            if(err || (frames && frames!=q->geometry.frames)) {
                cp_audio_codec_reset(q->codec);continue;
            }
            /* The native AAC canary suppresses the first priming AU. An
             * independent FFmpeg decode confirms the next output belongs
             * to the CURRENT AU, not to the suppressed predecessor. Keep
             * its own RTP timestamp; any missing interval stays silence. */
            q->pcm_frames=frames;q->pcm_offset=0;
            continue;
        }
        out+=n*channels; remaining-=n;
    }
    return 0;
}
