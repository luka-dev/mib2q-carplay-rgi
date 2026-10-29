#define _POSIX_C_SOURCE 200809L
#include "../receiver_bridge/si_guard.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    struct mhi2q_si_binding b={0}; uint8_t context[16]={0};
    assert(mhi2q_si_authorize(b,99,2,1,3,context));
    assert(!mhi2q_si_authorize(b,99,3,1,3,context));
    setenv("MHI2Q_CP_SI_EPOCH","123456789abcdef0",1);
    b=mhi2q_si_environment(); assert(b.enabled && !b.valid);
    setenv("MHI2Q_CP_SI_SESSION","7",1);setenv("MHI2Q_CP_SI_LINK","3",1);
    setenv("MHI2Q_CP_SI_PARENT_PID","1234",1);
    b=mhi2q_si_environment(); assert(b.valid);
    for(unsigned i=0;i<8;++i) context[i]=(uint8_t)(b.epoch>>(8*i));
    context[8]=7;
    assert(mhi2q_si_authorize(b,1234,3,1,3,context));
    for(unsigned op=6;op<=10;++op) {
        assert(mhi2q_si_authorize(b,1234,3,op,3,context));
        assert(!mhi2q_si_authorize(b,1234,2,op,3,context));
        assert(!mhi2q_si_authorize(b,1234,3,op,4,context));
        assert(!mhi2q_si_authorize(b,1235,3,op,3,context));
        context[0]^=1;assert(!mhi2q_si_authorize(b,1234,3,op,3,context));context[0]^=1;
    }
    assert(!mhi2q_si_authorize(b,1234,2,1,3,context));
    assert(!mhi2q_si_authorize(b,1235,3,1,3,context));
    assert(!mhi2q_si_authorize(b,1234,3,1,4,context));
    context[0]^=1;assert(!mhi2q_si_authorize(b,1234,3,1,3,context));context[0]^=1;
    context[8]=8;assert(!mhi2q_si_authorize(b,1234,3,1,3,context));context[8]=7;
    context[15]=1;assert(!mhi2q_si_authorize(b,1234,3,1,3,context));context[15]=0;
    assert(mhi2q_si_authorize(b,1,2,3,0,context)); /* orphan can be quiesced, never started */
    assert(mhi2q_si_authorize(b,1,2,4,0,context));
    const char *bad[]={" 7","+7","-7","0","4294967296","7x",""};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        setenv("MHI2Q_CP_SI_SESSION",bad[i],1);assert(!mhi2q_si_environment().valid);
    }
    puts("Receiver SI binding: epoch/session/link/parent, legacy rejection and orphan drain PASS");
}
