/* Bounded playback of a local known waveform via the public AVAudioSession /
 * AVAudioPlayer APIs. Observes the real route; never forces a fake AirPlay
 * destination or changes an Apple daemon. */
typedef void *Object;
typedef void *Selector;
extern Object objc_getClass(const char *);
extern Selector sel_registerName(const char *);
extern void objc_msgSend(void);
extern void *dlopen(const char *,int);
extern void *dlsym(void *,const char *);
extern int printf(const char *,...);
extern int fflush(void *);
extern unsigned alarm(unsigned);
extern int atoi(const char *);
extern int strcmp(const char *,const char *);
extern int CFRunLoopRunInMode(Object,double,unsigned char);
extern Object kCFRunLoopDefaultMode;
static Object call0(Object o,const char *s)
{ return ((Object (*)(Object,Selector))objc_msgSend)(o,sel_registerName(s)); }
static Object string(const char *s)
{ return ((Object (*)(Object,Selector,const char *))objc_msgSend)(objc_getClass("NSString"),sel_registerName("stringWithUTF8String:"),s); }
static void set(Object o,const char *selector,Object value)
{ ((void (*)(Object,Selector,Object))objc_msgSend)(o,sel_registerName(selector),value); }
static const char *utf8(Object o)
{ const char *s=o?((const char *(*)(Object,Selector))objc_msgSend)(o,sel_registerName("UTF8String")):0;return s?s:"(none)"; }
static void route(Object session,unsigned second)
{
    Object outputs=call0(call0(session,"currentRoute"),"outputs");
    unsigned long count=((unsigned long (*)(Object,Selector))objc_msgSend)(outputs,sel_registerName("count"));
    long actual=((long (*)(Object,Selector))objc_msgSend)(session,sel_registerName("outputNumberOfChannels"));
    long maximum=((long (*)(Object,Selector))objc_msgSend)(session,sel_registerName("maximumOutputNumberOfChannels"));
    printf("AUDIO_ROUTE second=%u count=%lu channels=%ld maximum=%ld",second,count,actual,maximum);
    for(unsigned long i=0;i<count && i<8;++i) {
        Object port=((Object (*)(Object,Selector,unsigned long))objc_msgSend)(outputs,sel_registerName("objectAtIndex:"),i);
        printf(" type=%s name=%s",utf8(call0(port,"portType")),utf8(call0(port,"portName")));
    }
    printf("\n");fflush((void *)0);
}
int main(int argc,char **argv)
{
    unsigned seconds=argc==5?(unsigned)atoi(argv[2]):45;
    unsigned channels=argc==5?(unsigned)atoi(argv[3]):2;
    if(argc!=5 || seconds<10 || seconds>120 || (channels!=2&&channels!=6))return 2;
    int avplayer=!strcmp(argv[4],"avplayer");
    alarm(seconds+20);
    if(!dlopen("/System/Library/Frameworks/Foundation.framework/Foundation",2) ||
       !dlopen("/System/Library/Frameworks/AVFAudio.framework/AVFAudio",2))return 3;
    Object pool=call0(objc_getClass("NSAutoreleasePool"),"new");
    Object session=call0(objc_getClass("AVAudioSession"),"sharedInstance"),error=0;
    unsigned char category=((unsigned char (*)(Object,Selector,Object,Object,unsigned long,unsigned long,Object *))objc_msgSend)(
        session,sel_registerName("setCategory:mode:routeSharingPolicy:options:error:"),
        string("AVAudioSessionCategoryPlayback"),string("AVAudioSessionModeDefault"),1,0,&error);
    printf("AUDIO_CATEGORY ok=%u error=%s\n",category,utf8(call0(error,"description")));
    if(!category)return 4;
    if(channels>2) {
        error=0;
        unsigned char okay=((unsigned char (*)(Object,Selector,unsigned char,Object *))objc_msgSend)(session,
            sel_registerName("setSupportsMultichannelContent:error:"),1,&error);
        printf("AUDIO_MULTICHANNEL ok=%u error=%s\n",okay,utf8(call0(error,"description")));
        if(!okay)return 8;
    }
    error=0;
    unsigned char active=((unsigned char (*)(Object,Selector,unsigned char,Object *))objc_msgSend)(session,sel_registerName("setActive:error:"),1,&error);
    printf("AUDIO_ACTIVE ok=%u error=%s\n",active,utf8(call0(error,"description")));
    if(!active)return 5;
    if(channels>2) {
        error=0;
        unsigned char okay=((unsigned char (*)(Object,Selector,long,Object *))objc_msgSend)(session,
            sel_registerName("setPreferredOutputNumberOfChannels:error:"),(long)channels,&error);
        printf("AUDIO_PREFERRED_CHANNELS requested=%u ok=%u error=%s\n",channels,okay,utf8(call0(error,"description")));
    }
    Object url=((Object (*)(Object,Selector,Object))objc_msgSend)(objc_getClass("NSURL"),sel_registerName("fileURLWithPath:"),string(argv[1]));
    Object player;
    if(avplayer) {
        if(!dlopen("/System/Library/Frameworks/AVFoundation.framework/AVFoundation",2))return 9;
        Object items=call0(objc_getClass("NSMutableArray"),"new");
        for(unsigned i=0;i<seconds;++i) {
            Object item=((Object (*)(Object,Selector,Object))objc_msgSend)(objc_getClass("AVPlayerItem"),sel_registerName("playerItemWithURL:"),url);
            ((void (*)(Object,Selector,Object))objc_msgSend)(items,sel_registerName("addObject:"),item);
        }
        player=((Object (*)(Object,Selector,Object))objc_msgSend)(objc_getClass("AVQueuePlayer"),sel_registerName("queuePlayerWithItems:"),items);
        call0(player,"retain");
        call0(items,"release");
    } else player=((Object (*)(Object,Selector,Object,Object *))objc_msgSend)(call0(objc_getClass("AVAudioPlayer"),"alloc"),sel_registerName("initWithContentsOfURL:error:"),url,&error);
    if(!player){printf("AUDIO_PLAYER error=%s\n",utf8(call0(error,"description")));return 6;}
    if(!avplayer)((void (*)(Object,Selector,long))objc_msgSend)(player,sel_registerName("setNumberOfLoops:"),-1);
    ((void (*)(Object,Selector,float))objc_msgSend)(player,sel_registerName("setVolume:"),0.1f);
    unsigned char playing=1;
    if(avplayer)((void (*)(Object,Selector))objc_msgSend)(player,sel_registerName("play"));
    else playing=((unsigned char (*)(Object,Selector))objc_msgSend)(player,sel_registerName("play"));
    printf("AUDIO_PLAY ok=%u engine=%s\n",playing,argv[4]);fflush((void *)0);
    if(!playing)return 7;
    void *media=dlopen("/System/Library/Frameworks/MediaPlayer.framework/MediaPlayer",2);
    Object center=0;
    if(media) {
        Object *key=dlsym(media,"MPMediaItemPropertyTitle");
        if(key) {
            Object info=call0(objc_getClass("NSMutableDictionary"),"new");
            ((void (*)(Object,Selector,Object,Object))objc_msgSend)(info,sel_registerName("setObject:forKey:"),string("Wireless audio validation"),*key);
            center=call0(objc_getClass("MPNowPlayingInfoCenter"),"defaultCenter");
            set(center,"setNowPlayingInfo:",info);call0(info,"release");
        }
    }
    for(unsigned i=0;i<seconds;++i) {
        if(i%5==0) {
            route(session,i);
            if(avplayer) {
                long status=((long (*)(Object,Selector))objc_msgSend)(player,sel_registerName("status"));
                printf("AUDIO_AVPLAYER status=%ld error=%s\n",status,utf8(call0(call0(player,"error"),"description")));
            }
        }
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,1,0);
    }
    call0(player,avplayer?"pause":"stop");
    if(center)set(center,"setNowPlayingInfo:",0);
    ((unsigned char (*)(Object,Selector,unsigned char,Object *))objc_msgSend)(session,sel_registerName("setActive:error:"),0,&error);
    call0(player,"release");call0(pool,"drain");
    printf("AUDIO_DONE\n");fflush((void *)0);return 0;
}
