/* SI child context is immutable across this process. CPRX v3 carries the
 * admitted epoch/session as well as the RFCOMM generation. Legacy requests
 * cannot start an SI-owned receiver. No PID file is an authority. */
#ifndef MHI2Q_RBR_SI_GUARD_H
#define MHI2Q_RBR_SI_GUARD_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
struct mhi2q_si_binding { uint64_t epoch; uint32_t session,link,parent; int enabled,valid; };
static inline uint64_t mhi2q_si_number(const char *s,int base,uint64_t max) {
    char *end; unsigned long long n;
    if (!s || !*s || *s=='-' || *s=='+') return 0;
    for(const char *p=s;*p;++p)
        if (!((*p>='0' && *p<='9') ||
              (base==16 && ((*p>='a' && *p<='f') || (*p>='A' && *p<='F'))))) return 0;
    errno=0; n=strtoull(s,&end,base);
    return errno || *end || n>max ? 0:(uint64_t)n;
}
static inline struct mhi2q_si_binding mhi2q_si_environment(void) {
    struct mhi2q_si_binding b; memset(&b,0,sizeof(b));
    const char *epoch=getenv("MHI2Q_CP_SI_EPOCH");
    const char *session=getenv("MHI2Q_CP_SI_SESSION");
    const char *link=getenv("MHI2Q_CP_SI_LINK");
    const char *parent=getenv("MHI2Q_CP_SI_PARENT_PID");
    b.enabled=epoch || session || link || parent;
    b.epoch=mhi2q_si_number(epoch,16,UINT64_MAX);
    b.session=(uint32_t)mhi2q_si_number(session,10,UINT32_MAX);
    b.link=(uint32_t)mhi2q_si_number(link,10,UINT32_MAX);
    b.parent=(uint32_t)mhi2q_si_number(parent,10,INT32_MAX);
    b.valid=b.epoch && b.session && b.link && b.parent>1;
    return b;
}
static inline int mhi2q_si_parent_ended(struct mhi2q_si_binding b,uint32_t parent) {
    return b.enabled && b.valid && parent!=b.parent;
}
static inline int mhi2q_si_authorize(struct mhi2q_si_binding b,uint32_t parent,
                                    unsigned version,unsigned operation,uint32_t link,
                                    const uint8_t context[16]) {
    if (!b.enabled) return version==2;
    if (!b.valid) return 0;
    /* Native owner-lock handoff must still be able to drain an orphan.
     * WIRED_DRAIN independently verifies the wired-priority kernel lock;
     * QUIESCE only stops an idle receiver. Neither admits a media start. */
    if (operation==3 || operation==4) return version==2 && link==0;
    if (parent!=b.parent) return 0;
    if (version!=3 || !context || link!=b.link) return 0;
    uint64_t epoch=0; uint32_t session=0;
    for(unsigned i=0;i<8;++i) epoch|=(uint64_t)context[i]<<(8*i);
    for(unsigned i=0;i<4;++i) session|=(uint32_t)context[8+i]<<(8*i);
    return epoch==b.epoch && session==b.session &&
        !context[12] && !context[13] && !context[14] && !context[15];
}
#endif
