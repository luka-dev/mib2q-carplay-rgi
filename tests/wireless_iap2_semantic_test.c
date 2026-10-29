/* Real framework boundary and file parser. Module callbacks stand in for
 * their independently tested RGI/vehicle/decoder consumers. No USB framing. */
#define HOOK_WIRELESS 1
#define ENABLE_LOGGING 0
#include "../hook/framework/hook_framework.c"
#include "../hook/coverart/coverart_stream.h"
#include <assert.h>
static unsigned messages,files;
static coverart_stream_t artwork;
static void identified(hook_context_t *ctx) {
    ctx->identify_patched=true;ctx->rgd_component_valid=true;ctx->rgd_component_id=16;
}
static size_t identify(hook_context_t *ctx,uint8_t *p,size_t n,size_t cap) {
    (void)p;(void)cap;identified(ctx);return n;
}
static bool received(hook_context_t *ctx,const iap2_frame_t *frame) {
    assert(ctx->direction==MSG_DIR_INCOMING&&ctx->raw_len==frame->frame_len);++messages;return false;
}
static void file(const uint8_t *p,size_t n) {
    uint8_t *image=NULL;size_t length=0;
    coverart_stream_file(&artwork,p,n,&image,&length);
    if(image){assert(length==9&&!memcmp(image,"\x89PNG\r\n\x1a\n!",9));++files;free(image);}
}
static const hook_module_def_t module={.name="boundary-oracle",.on_identify=identify,.on_message=received,.on_file_transfer=file};
const hook_module_def_t *const hook_module_table[]={&module};
const size_t hook_module_table_count=1;
hook_result_t bus_init(void){return HOOK_OK;}
void bus_shutdown(void){}
static void nmea(char *out,const char *body) {
    unsigned checksum=0;for(const char *p=body;*p;++p)checksum^=(unsigned char)*p;
    snprintf(out,161,"$%s*%02X",body,checksum);
}
int main(void) {
    uint8_t begin[]={0x40,0x40,0,16,0x1d,1,0,10,0,30,0,6,0,0,0,16};
    uint8_t location[]={0x40,0x40,0,10,0xff,0xfa,0,4,0,1};
    uint8_t stop[]={0x40,0x40,0,6,0xff,0xfc},out[512];
    setenv("MHI2Q_CP_SI_EPOCH","1",1);setenv("MHI2Q_DIO_OWNER_MODE","wired",1);
    assert(mhi2q_wireless_iap2_receive(0,begin,sizeof(begin))<0);
    setenv("MHI2Q_DIO_OWNER_MODE","wireless",1);
    assert(mhi2q_wireless_iap2_receive(1,location,sizeof(location))<0);
    assert(mhi2q_wireless_iap2_receive(0,begin,sizeof(begin))==1&&hook_is_ready());
    assert(mhi2q_wireless_iap2_receive(1,location,sizeof(location))==1&&messages==1);
    char sentence[161];nmea(sentence,"GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    assert(mhi2q_wireless_iap2_nmea(sentence,strlen(sentence))==1);
    int size=mhi2q_wireless_iap2_outgoing(out,sizeof(out));iap2_frame_t parsed;iap2_tlv_t text;
    assert(size>0&&iap2_find_frame(out,(size_t)size,&parsed)&&parsed.msgid==0xfffb);
    assert(iap2_find_tlv(parsed.payload,parsed.payload_len,0,&text)&&!strcmp((const char *)text.value,sentence));
    sentence[strlen(sentence)-1]^=1;assert(mhi2q_wireless_iap2_nmea(sentence,strlen(sentence))<0);
    nmea(sentence,"GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W");
    assert(!mhi2q_wireless_iap2_nmea(sentence,strlen(sentence))); /* not subscribed */
    assert(mhi2q_wireless_iap2_receive(1,stop,sizeof(stop))==1);
    nmea(sentence,"GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    assert(!mhi2q_wireless_iap2_nmea(sentence,strlen(sentence)));
    uint8_t setup[10]={7,4},first[]={7,0x80,137,80,78,71},last[]={7,0x40,13,10,26,10,33};
    setup[9]=9;
    assert(mhi2q_wireless_iap2_receive(2,setup,sizeof(setup))==1);
    assert(mhi2q_wireless_iap2_receive(2,first,sizeof(first))==1&&!files);
    assert(mhi2q_wireless_iap2_receive(2,last,sizeof(last))==1&&files==1);
    uint8_t request[]={0x40,0x40,0,6,0x52,0};
    for(unsigned i=0;i<16;++i)assert(hook_inject_frame(request,sizeof(request))==HOOK_OK);
    assert(hook_inject_frame(request,sizeof(request))==HOOK_ERR_IO);
    for(unsigned i=0;i<16;++i)assert(mhi2q_wireless_iap2_outgoing(out,sizeof(out))==6);
    assert(!mhi2q_wireless_iap2_outgoing(out,sizeof(out)));
    assert(mhi2q_wireless_iap2_receive(0,begin,sizeof(begin))==1);
    size=mhi2q_wireless_iap2_outgoing(out,sizeof(out));
    assert(size==150&&iap2_find_frame(out,(size_t)size,&parsed)&&parsed.msgid==0x5000);
    assert(!mhi2q_wireless_iap2_outgoing(out,sizeof(out)));
    unsigned random=47;
    for(unsigned i=0;i<20000;++i) {
        uint8_t mutation[sizeof(begin)];memcpy(mutation,begin,sizeof(begin));
        random=random*1664525+1013904223;unsigned index=random%sizeof(begin);
        random=random*1664525+1013904223;mutation[index]^=(uint8_t)(1+random%255);
        (void)mhi2q_wireless_iap2_receive(1,mutation,sizeof(mutation));
    }
    coverart_stream_dispose(&artwork);
    puts("wireless semantic boundary: admission, CSM routing, FTS reassembly, real NMEA/checksum/subscription, bounded output and 20000 mutations PASS");
}
