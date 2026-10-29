#include "../hook/wireless/peer_health.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    struct cp_peer_health h={0};
    assert(!cp_peer_health_sample(&h,1,0,0,0,0,0,1));
    assert(!cp_peer_health_sample(&h,1,1,0,0,3,0,1000));
    /* Static screen: no new audio/video/HTTP, only real timing replies. */
    for(uint64_t now=4000;now<100000;now+=3000)
        assert(!cp_peer_health_sample(&h,1,1,0,0,(uint32_t)now,0,now));
    assert(!cp_peer_health_sample(&h,1,1,h.rx,h.activity,h.timing,0,h.last_ms+9999));
    assert(cp_peer_health_sample(&h,1,1,h.rx,h.activity,h.timing,0,h.last_ms+10000));
    assert(!cp_peer_health_sample(&h,1,1,h.rx,h.activity,h.timing,0,h.last_ms+11000));
    assert(!cp_peer_health_sample(&h,7,1,0,0,3,0,1000));
    /* Correlated replies can have RTT too large for clock adjustment.
     * They still prove the phone is reachable during congested Wi-Fi. */
    for(uint64_t now=4000;now<100000;now+=3000)
        assert(!cp_peer_health_sample(&h,7,1,0,0,3,(uint32_t)now,now));
    memset(&h,0,sizeof(h));
    assert(!cp_peer_health_sample(&h,2,1,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,10));
    assert(!cp_peer_health_sample(&h,2,1,0,0,0,0,9000)); /* Counter wrap is activity. */
    assert(!cp_peer_health_sample(&h,3,1,0,0,0,0,100000)); /* New session epoch. */
    assert(!cp_peer_health_sample(&h,3,1,0,0,0,0,1)); /* Clock regression. */
    assert(!cp_peer_health_sample(&h,3,1,0,0,0,0,0)); /* Clock failure. */
    memset(&h,0,sizeof(h));
    assert(!cp_peer_health_sample(&h,4,0,0,0,0,0,1000));
    /* Timing and partial SETUP activity do not renew a missing RECORD. */
    for(uint64_t now=2000;now<31000;now+=1000)
        assert(!cp_peer_health_sample(&h,4,0,now,now,now,now,now));
    assert(cp_peer_health_sample(&h,4,0,99,99,99,99,31000));
    assert(!cp_peer_health_sample(&h,4,0,0,0,0,0,32000));
    assert(!cp_peer_health_sample(&h,5,0,0,0,0,0,1000));
    assert(!cp_peer_health_sample(&h,5,1,0,0,0,0,30000));
    assert(!cp_peer_health_sample(&h,5,1,0,0,0,0,39999));
    assert(cp_peer_health_sample(&h,5,1,0,0,0,0,40000));
    puts("wireless peer health: static-screen timing, silence bound, single close, wrap and epochs PASS");
}
