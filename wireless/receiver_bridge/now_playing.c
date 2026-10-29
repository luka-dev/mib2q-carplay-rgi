/* MU1316 native client-event queue. Capture only on a native lifecycle
 * callback; the IPC worker uses an immutable value copy of its registration.
 * Native helpers copy Unicode fields and transfer event ownership to mediator.
 */
#include "now_playing.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static uint32_t registration[6];
static int ready;
static struct np_update retained;
int mhi2q_now_playing_capture(void *manager) {
    static const uint32_t track_code[]={0xe92d4070,0xe1a05001,0xe1a06000,0xebfffe8f};
    static const uint32_t playback_code[]={0xe92d4070,0xe1a05001,0xe1a06000,0xebfffe88};
    static const uint32_t elapsed_code[]={0xe92d4070,0xe1a04000,0xe1a05001,0xebfffe78};
    if(memcmp((const void *)0x129c00,track_code,sizeof(track_code)) ||
       memcmp((const void *)0x129bac,playback_code,sizeof(playback_code)) ||
       memcmp((const void *)0x129b7c,elapsed_code,sizeof(elapsed_code)))return 0;
    /* Manager ctor 0x15a064 registers CIAP2ClientEventListener(manager+8)
     * through 0x12a334. Its token lives at listener+8 (manager+16).
     * manager+232 instead contains a JOB listener token: it belongs to a
     * different map and must never be used to route client EVENTS.
     * Resolve the token under the native listener-map mutex (0x12a254) and
     * require this exact manager's listener base before publishing a snapshot.
     * No registration is manufactured and no native object is modified. */
    uint32_t copy[6]={0};
    if(!manager)return 0;
    memcpy(copy+4,(char *)manager+16,8);
    if(!copy[4])return 0;
    /* Registered listener must resolve to DIO controller, never broadcast.
     * Native 0x132274 decodes the destination AO from routing high word. */
    unsigned target=copy[5]?(copy[5]&255):copy[2];
    if(target!=1)return 0;
    copy[2]=target;
    void *listener=((void *(*)(uint32_t,uint16_t))(uintptr_t)0x12a254)(copy[4],(uint16_t)copy[5]);
    if(listener!=(char *)manager+8)return 0;
    pthread_mutex_lock(&lock);
    int published=!ready;
    if(published){memcpy(registration,copy,sizeof(copy));ready=1;}
    pthread_mutex_unlock(&lock);
    if(published)fputs("[dio-si] native Now Playing listener captured\n",stderr);
    return 1;
}
int mhi2q_now_playing_submit(const uint8_t *frame,size_t length) {
    struct np_update update;
    if(np_decode(frame,length,&update))return -1;
    typedef void (*ctor_fn)(void *);
    typedef void (*set_fn)(void *,const char *,int,unsigned,int);
    ctor_fn ctor=(ctor_fn)dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString8C1Ev");
    ctor_fn destroy=(ctor_fn)dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString8D1Ev");
    set_fn set_string=(set_fn)dlsym(RTLD_DEFAULT,"_ZN4util14UnicodeString89setStringEPKcNS_20UnicodeString_common8EncodingEjb");
    if(!ctor||!destroy||!set_string)return 0;
    pthread_mutex_lock(&lock);
    if(!ready){pthread_mutex_unlock(&lock);return 0;}
    struct np_update next=retained;
    for(unsigned i=0;i<NP_FIELDS;++i)if(update.mask&(1u<<i)) {
        next.mask|=1u<<i;next.number[i]=update.number[i];
        memcpy(next.text[i],update.text[i],sizeof(next.text[i]));
    }
    const unsigned track_mask=(1u<<NP_STATUS)-1;
    const unsigned playback_mask=(1u<<NP_STATUS)|(1u<<NP_SHUFFLE)|(1u<<NP_REPEAT)|(1u<<NP_APP);
    int result=1;
    if(update.mask&track_mask) {
        union {uint64_t alignment;unsigned char bytes[228];} info;
        memset(&info,0,sizeof(info));
        const unsigned fields[]={NP_TITLE,NP_ALBUM,NP_ARTIST,NP_GENRE,NP_COMPOSER};
        const unsigned offsets[]={4,56,100,144,188};
        for(unsigned i=0;i<5;++i) {
            ctor(info.bytes+offsets[i]);
            if(update.mask&(1u<<fields[i])) {
                info.bytes[offsets[i]-4]=1;
                set_string(info.bytes+offsets[i],update.text[fields[i]],3,0,0);
            }
        }
        if(update.mask&(1u<<NP_DURATION)) {
            info.bytes[44]=1;memcpy(info.bytes+48,&update.number[NP_DURATION],4);
        }
        result=((int (*)(void *,const void *))(uintptr_t)0x129c00)(registration,info.bytes)!=0;
        for(unsigned i=0;i<5;++i)destroy(info.bytes+offsets[i]);
    }
    if(result && (update.mask&playback_mask)) {
        union {uint64_t alignment;unsigned char bytes[52];} info;
        memset(&info,0,sizeof(info));
        memcpy(info.bytes,&next.number[NP_STATUS],4);
        memcpy(info.bytes+4,&next.number[NP_SHUFFLE],4);
        memcpy(info.bytes+8,&next.number[NP_REPEAT],4);
        ctor(info.bytes+12);set_string(info.bytes+12,next.text[NP_APP],3,0,0);
        result=((int (*)(void *,const void *))(uintptr_t)0x129bac)(registration,info.bytes)!=0;
        destroy(info.bytes+12);
    }
    if(result && (update.mask&(1u<<NP_ELAPSED)))
        result=((int (*)(void *,unsigned))(uintptr_t)0x129b7c)(registration,update.number[NP_ELAPSED])!=0;
    if(result)retained=next;
    pthread_mutex_unlock(&lock);
    if(result)fputs("[dio-si] Now Playing queued through native listener\n",stderr);
    return result;
}
