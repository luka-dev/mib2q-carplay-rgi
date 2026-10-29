#include "si_wire.h"
#include <string.h>

static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put32(unsigned char *p, uint32_t v) {
    for (unsigned i=0; i<4; ++i) p[i]=(unsigned char)(v>>(8*i));
}
/* Validate Unicode scalar values; reject truncated, overlong and surrogate UTF-8. */
static int text(const unsigned char *s, size_t capacity, int key) {
    size_t i=0;
    while (i<capacity && s[i]) {
        unsigned char c=s[i++];
        if (key) {
            if (!((c>='a' && c<='z') || (c>='A' && c<='Z') ||
                  (c>='0' && c<='9') || c==':' || c=='-' || c=='_')) return 0;
            continue;
        }
        if (c<0x20 || c==0x7f) return 0;
        if (c<0x80) continue;
        unsigned n; uint32_t v, min;
        if (c>=0xc2 && c<=0xdf) { n=1; v=c&31; min=0x80; }
        else if (c>=0xe0 && c<=0xef) { n=2; v=c&15; min=0x800; }
        else if (c>=0xf0 && c<=0xf4) { n=3; v=c&7; min=0x10000; }
        else return 0;
        if (i+n>=capacity) return 0;
        while (n--) { c=s[i++]; if ((c&0xc0)!=0x80) return 0; v=(v<<6)|(c&63); }
        if (v<min || v>0x10ffff || (v>=0xd800 && v<=0xdfff)) return 0;
    }
    if (!i || i==capacity) return 0;
    for (; i<capacity; ++i) if (s[i]) return 0;
    return 1;
}

int si_wire_decode(const unsigned char f[SI_WIRE_SIZE], struct si_request *out) {
    if (!f || !out || memcmp(f,"SIW2",4) || f[4]!=SI_WIRE_VERSION ||
        f[5]<SI_HELLO || f[5]>SI_DETACH_PEER || f[6] || f[7] ||
        get32(f+8)!=SI_WIRE_SIZE || !get32(f+12) || !get32(f+24)) return 0;
    for (size_t i=164; i<SI_WIRE_SIZE; ++i) if (f[i]) return 0;
    if (!text(f+32,SI_PHONE_KEY_SIZE,1) || !text(f+64,SI_PHONE_NAME_SIZE,0)) return 0;
    memset(out,0,sizeof(*out));
    out->operation=f[5]; out->request_id=get32(f+12);
    out->si_epoch=get32(f+16)|((uint64_t)get32(f+20)<<32);
    out->daemon_epoch=get32(f+24); out->link_generation=get32(f+28);
    out->session_generation=get32(f+160);
    memcpy(out->phone_key,f+32,SI_PHONE_KEY_SIZE);
    memcpy(out->name,f+64,SI_PHONE_NAME_SIZE);
    return out->operation==SI_HELLO || (out->si_epoch && out->link_generation);
}

void si_wire_reply(const struct si_reply *r, unsigned char f[SI_WIRE_SIZE]) {
    memset(f,0,SI_WIRE_SIZE); memcpy(f,"SIR2",4);
    f[4]=SI_WIRE_VERSION; f[5]=r->operation;
    put32(f+8,SI_WIRE_SIZE); put32(f+12,r->request_id);
    put32(f+16,(uint32_t)r->si_epoch); put32(f+20,(uint32_t)(r->si_epoch>>32));
    put32(f+32,r->result); put32(f+36,r->device_id); put32(f+40,r->session_generation);
    put32(f+44,(uint32_t)r->app); put32(f+48,(uint32_t)r->app_state);
    put32(f+52,r->active_device); put32(f+56,r->child_pid); put32(f+60,r->present);
}
