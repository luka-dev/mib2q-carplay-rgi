/* One capability source for stock210 /info and every Bonjour TXT rebuild.
 * Keep stock sourceVersion/deviceID/display identity. Private 280 CF objects
 * must never enter these dictionaries.
 * See docs/wireless-carplay/STOCK210_RECEIVER_LIFECYCLE.md. */
#include "wireless_hook.h"
#include "stock210_abi.h"
#include "advertisement.h"
#include "pairing.h"
#include "audio_stream.h"
#include "../audio/buffered_wireless.h"
DEFINE_LOG_MODULE(WADVERT);

static struct {
    void *(*string)(void *, const char *, unsigned);
    void (*release)(const void *);
    void *(*get)(const void *, const void *);
    uint32_t (*type)(const void *), (*dict_type)(void), (*array_type)(void);
    int64_t (*integer)(void *, const void *, int *);
    void (*set_integer)(void *, const void *, int64_t);
    int (*set_string)(void *, const void *, const char *, size_t);
    void (*remove)(void *, const void *);
    long (*count)(const void *);
    void *(*at)(const void *, long);
    void (*remove_at)(void *, long);
    void *(*create)(void *,long,const void *,const void *);
    void (*append)(void *,const void *);
    void *keys,*values;
    void *features, *pi, *audio, *type_key, *input, *output, *input_ext, *output_ext;
    int ready;
} cf;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static void resolve(void)
{
#define R(member,name) *(void **)(&cf.member)=dlsym(RTLD_NEXT,name); if(!cf.member)return
    R(string,"CFStringCreateWithCString"); R(release,"CFRelease");
    R(get,"CFDictionaryGetValue"); R(type,"CFGetTypeID");
    R(dict_type,"CFDictionaryGetTypeID"); R(array_type,"CFArrayGetTypeID");
    R(integer,"CFDictionaryGetInt64"); R(set_integer,"CFDictionarySetInt64");
    R(set_string,"CFDictionarySetCString"); R(remove,"CFDictionaryRemoveValue");
    R(count,"CFArrayGetCount"); R(at,"CFArrayGetValueAtIndex");
    R(remove_at,"CFArrayRemoveValueAtIndex");
    R(create,"CFDictionaryCreateMutable"); R(append,"CFArrayAppendValue");
    R(keys,"kCFLDictionaryKeyCallBacksCFLTypes"); R(values,"kCFLDictionaryValueCallBacksCFLTypes");
#undef R
#define K(member,name) cf.member=cf.string(NULL,name,0x08000100); if(!cf.member)return
    K(features,"features"); K(pi,"pi"); K(audio,"audioFormats"); K(type_key,"type");
    K(input,"audioInputFormats"); K(output,"audioOutputFormats");
    K(input_ext,"audioInputFormatsExtended"); K(output_ext,"audioOutputFormatsExtended");
#undef K
    cf.ready=1;
}
int cp_advertisement_available(void)
{ pthread_once(&once,resolve); return cf.ready; }

static uint64_t supported_features(uint64_t features)
{
    /* Retain bit26: stock MFi-SAP
     * is real and is also used by client authentication selection. */
    features &= ~((UINT64_C(1)<<20) | (UINT64_C(1)<<22));
    if(cp_wireless_audio_enabled() && (cp_audio_codec_formats()&CP_AUDIO_AAC))
        features|=UINT64_C(1)<<20;
    return features | CP_WIRELESS_FEATURES;
}

HOOK_EXPORT uint64_t AirPlayGetFeatures(void)
{
    typedef uint64_t (*fn)(void);
    fn real=(fn)dlsym(RTLD_NEXT,"AirPlayGetFeatures");
    uint64_t value=real?real():0;
    return carplay_wireless_receiver_ready() ? supported_features(value) : value;
}

/* In stock210 this exact call writes srcvers at the end of TXT construction.
 * Extending the record here covers Register, UpdateRecord AND /info's txtAirPlay
 * data, without owning DNSServiceRef lifetimes or intercepting other services. */
HOOK_EXPORT int TXTRecordSetValue(void *record,const char *name,uint8_t length,const void *value)
{
    typedef int (*fn)(void *,const char *,uint8_t,const void *);
    fn real=(fn)dlsym(RTLD_NEXT,"TXTRecordSetValue");
    int result=real?real(record,name,length,value):-65537;
    if(!result && cp210_callsite(__builtin_return_address(0),0x211cc) &&
       name && !strcmp(name,"srcvers") && length==6 && value &&
       !memcmp(value,"210.81",6) && carplay_wireless_receiver_ready()) {
        char id[CP_PAIR_ID_MAX+1];
        result=carplay_wireless_copy_pairing_identity(id,sizeof(id),NULL);
        if(!result)result=real(record,"pi",(uint8_t)strlen(id),id);
        if(result)cp_wireless_advertisement_failed();
    }
    return result;
}

void cp_advertisement_info(void *session,void *info)
{
    char id[CP_PAIR_ID_MAX+1];
    void *audio;
    uint64_t codecs=0;
    int err=0;
    long i;
    if(!carplay_wireless_receiver_ready())return;
    if(cf.type(info)!=cf.dict_type() ||
       carplay_wireless_copy_pairing_identity(id,sizeof(id),NULL))goto failed;
    audio=cf.get(info,cf.audio);
    if(!audio || cf.type(audio)!=cf.array_type())goto failed;
    if(cp_wireless_audio_enabled()) {
        codecs=cp_audio_codec_formats();
        LOG_INFO(LOG_MODULE,"wireless audio codecs=%#llx",(unsigned long long)codecs);
        /* Wireless main-media needs AAC-LC independently of the voice codec.
         * A linked symbol or a format string is not an available decoder. */
        if(!(codecs&CP_AUDIO_AAC))goto failed;
    }
    /* Preserve the audio module's negotiated buffered descriptor verbatim:
     * extended indices, APAC/AAC selection and PCM6 are shared with wired. */
    for(i=cf.count(audio);i-- > 0;) {
        void *entry=cf.at(audio,i);
        uint64_t input=0, output=0;
        if(!entry || cf.type(entry)!=cf.dict_type())goto failed;
        int64_t type=cf.integer(entry,cf.type_key,&err);
        if(!err && type==103 && spa_buffered_wireless_offer &&
           spa_buffered_wireless_offer(session))continue;
        if(err || (type!=100 && type!=101)) { cf.remove_at(audio,i);continue; }
        /* /info may reuse its mutable capability objects. Preserve our own
         * already-normalized Opus masks on a repeated final pass. */
        uint64_t voice_source=CP_WIRELESS_PCM16|(codecs&CP_AUDIO_OPUS);
        if(cf.get(entry,cf.input))input=(uint64_t)cf.integer(entry,cf.input,NULL)&voice_source;
        if(cf.get(entry,cf.output))output=(uint64_t)cf.integer(entry,cf.output,NULL)&voice_source;
        if(cp_wireless_audio_enabled()) {
            /* iOS's wireless filter excludes the legacy PCM bits. Advertise
             * one compression family for voice, separately from media. */
            input=input?(codecs&CP_AUDIO_OPUS):0;
            output=output?(codecs&CP_AUDIO_OPUS):0;
        }
        if(!input && !output) { cf.remove_at(audio,i);continue; }
        if(input)cf.set_integer(entry,cf.input,(int64_t)input);else cf.remove(entry,cf.input);
        if(output)cf.set_integer(entry,cf.output,(int64_t)output);else cf.remove(entry,cf.output);
        cf.remove(entry,cf.input_ext); cf.remove(entry,cf.output_ext);
    }
    if(codecs) {
        if(!cf.count(audio))goto failed;
        void *media=cf.create(NULL,0,cf.keys,cf.values);
        if(!media)goto failed;
        /* Wireless realtime media is wire102/MainHigh, aliased to native100.
         * Buffered103 and telephony keep their own independent contracts. */
        cf.set_integer(media,cf.type_key,102);
        cf.set_integer(media,cf.output,(int64_t)(codecs&CP_AUDIO_AAC));
        cf.remove(media,cf.input);
        cf.append(audio,media);cf.release(media);
    }
    if(!cf.count(audio) || cf.set_string(info,cf.pi,id,strlen(id)))goto failed;
    cf.set_integer(info,cf.features,(int64_t)supported_features((uint64_t)cf.integer(info,cf.features,NULL)));
    return;
failed:
    cp_wireless_advertisement_failed();
    cf.remove(info,cf.pi);
    cf.set_integer(info,cf.features,(int64_t)((uint64_t)cf.integer(info,cf.features,NULL)&~CP_WIRELESS_FEATURES));
}
