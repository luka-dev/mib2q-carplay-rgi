/* Private DIO -> SI state envelope. No pointers or untagged enum cross
 * this boundary. SIW2 daemon commands retain their original wire ABI. */
#ifndef SI_CHILD_STATE_H
#define SI_CHILD_STATE_H
#include <stdint.h>
#include <stddef.h>
#define SI_CHILD_STATE_SIZE 192u
enum { SI_ASI_STATE=0,SI_ASI_WATCHDOG,SI_ASI_FLUSH,SI_ASI_TEARDOWN,SI_ASI_RESTART };
struct si_child_state { uint64_t epoch; uint32_t child,session,link,sequence,state,kind,cookie; };
static inline uint32_t si_cs_get(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static inline void si_cs_put(unsigned char *p,uint32_t v) {
    for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));
}
static inline void si_child_state_encode(const struct si_child_state *s,unsigned char *p) {
    for(unsigned i=0;i<SI_CHILD_STATE_SIZE;++i)p[i]=0;
    p[0]='S';p[1]='I';p[2]='A';p[3]='3';p[4]=2;
    si_cs_put(p+8,SI_CHILD_STATE_SIZE);si_cs_put(p+12,s->sequence);
    si_cs_put(p+16,(uint32_t)s->epoch);si_cs_put(p+20,(uint32_t)(s->epoch>>32));
    si_cs_put(p+24,s->child);si_cs_put(p+28,s->link);
    si_cs_put(p+32,s->kind);si_cs_put(p+36,s->cookie);
    si_cs_put(p+160,s->session);si_cs_put(p+164,s->state);
}
static inline int si_child_state_decode(const unsigned char *p,size_t n,struct si_child_state *s) {
    if(n!=SI_CHILD_STATE_SIZE||p[0]!='S'||p[1]!='I'||p[2]!='A'||p[3]!='3'||p[4]!=2||
       p[5]||p[6]||p[7]||si_cs_get(p+8)!=SI_CHILD_STATE_SIZE)return 0;
    for(unsigned i=40;i<160;++i)if(p[i])return 0;
    for(unsigned i=168;i<SI_CHILD_STATE_SIZE;++i)if(p[i])return 0;
    s->epoch=si_cs_get(p+16)|((uint64_t)si_cs_get(p+20)<<32);
    s->sequence=si_cs_get(p+12);s->child=si_cs_get(p+24);s->link=si_cs_get(p+28);
    s->session=si_cs_get(p+160);s->state=si_cs_get(p+164);
    s->kind=si_cs_get(p+32);s->cookie=si_cs_get(p+36);
    int valid=s->kind==SI_ASI_STATE?s->state<=6&&!s->cookie:
        s->kind==SI_ASI_WATCHDOG?s->state<=3&&s->cookie<=UINT16_MAX:
        s->kind==SI_ASI_FLUSH?s->state<=UINT8_MAX&&!s->cookie:
        (s->kind==SI_ASI_TEARDOWN||s->kind==SI_ASI_RESTART)?!s->state&&!s->cookie:0;
    return valid&&s->epoch&&s->sequence&&s->child>1&&s->child<=INT32_MAX&&s->link&&s->session;
}
#endif
