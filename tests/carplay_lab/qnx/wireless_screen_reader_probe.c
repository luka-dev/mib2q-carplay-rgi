/* Production wireless 110 wrapper and real private SDK AEAD on ARM/QNX.
 * The NetSocket source is an in-memory exact-read fixture, not a phone/decoder. */
#define ENABLE_LOGGING 0
#include "../../../hook/wireless/media.c"
#include <assert.h>
#include <stdio.h>
#include <time.h>

enum { BODY=64, RECORD=128+BODY+16, FRAMES=3 };
struct probe {
    pthread_mutex_t lock;
    pthread_cond_t done;
    int complete;
    unsigned offset, calls;
    uint8_t wire[FRAMES*RECORD], plain[BODY];
};
static int transport(void *net,size_t min,size_t max,void *out,size_t *got,int flags,int timeout)
{
    struct probe *p=net;
    assert(min==max && flags==7 && timeout==9);
    assert(!pthread_mutex_trylock(&screen_readers_lock));
    pthread_mutex_unlock(&screen_readers_lock);
    assert(max<=sizeof(p->wire)-p->offset);
    memcpy(out,p->wire+p->offset,max);*got=max;p->offset+=(unsigned)max;++p->calls;
    return 0;
}
static void *read_frames(void *net)
{
    struct probe *p=net;
    for(unsigned i=0;i<FRAMES;++i) {
        uint8_t header[128],body[BODY];size_t got=0;
        assert(!screen_read(net,sizeof(header),sizeof(header),header,&got,7,9));
        assert(got==sizeof(header) && cp210_get(header,0)==BODY);
        int err=screen_read(net,sizeof(body),sizeof(body),body,&got,7,9);
        if(i==FRAMES-1) {
            assert(err==kAuthenticationErr && !got);
            for(unsigned j=0;j<BODY;++j)assert(!body[j]);
        } else assert(!err && got==sizeof(body) && !memcmp(body,p->plain,got));
    }
    pthread_mutex_lock(&p->lock);p->complete=1;
    pthread_cond_signal(&p->done);pthread_mutex_unlock(&p->lock);
    return NULL;
}
int main(void)
{
    struct probe p={.lock=PTHREAD_MUTEX_INITIALIZER,.done=PTHREAD_COND_INITIALIZER};
    struct screen_reader reader={.net=&p,.real=transport};
    pthread_t worker;
    struct timespec deadline;
    int complete;
    for(unsigned i=0;i<32;++i)reader.key[i]=(uint8_t)(i*7+1);
    for(unsigned i=0;i<BODY;++i)p.plain[i]=(uint8_t)(i*13+5);
    for(unsigned i=0;i<FRAMES;++i) {
        uint8_t *record=p.wire+i*RECORD,nonce[8];
        cp210_set(record,0,BODY+16);nonce_bytes(i,nonce);
        cp_pair_seal(reader.key,nonce,record,128,p.plain,BODY,record+128,record+128+BODY);
    }
    p.wire[sizeof(p.wire)-1]^=1; /* Authenticity remains mandatory. */
    readers[0]=&reader;
    pthread_mutex_lock(&media_lock); /* Represents stalled audio/control I/O. */
    assert(!pthread_create(&worker,NULL,read_frames,&p));
    assert(!clock_gettime(CLOCK_REALTIME,&deadline));deadline.tv_sec+=3;
    pthread_mutex_lock(&p.lock);
    while(!p.complete) {
        int err=pthread_cond_timedwait(&p.done,&p.lock,&deadline);
        if(err==ETIMEDOUT)break;
        assert(!err);
    }
    complete=p.complete;pthread_mutex_unlock(&p.lock);
    pthread_mutex_unlock(&media_lock);
    assert(!pthread_join(worker,NULL));readers[0]=NULL;
    assert(complete && reader.nonce==2 && p.calls==9 && p.offset==sizeof(p.wire));
    pthread_cond_destroy(&p.done);pthread_mutex_destroy(&p.lock);
    puts("WIRELESS_SCREEN_READER_PASS real SDK AEAD, 2 frames independent of held media lock, bad tag rejected/erased");
    return 0;
}
