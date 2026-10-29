#ifndef CP_WIRELESS_PEER_HEALTH_H
#define CP_WIRELESS_PEER_HEALTH_H
#include <stdint.h>
#include <string.h>

/* Ten seconds spans at least three native 2..3 s timing-probe intervals.
 * No media-frame rate assumption. Clock failure/regression never closes. */
#define CP_PEER_SILENT_MS UINT64_C(10000)
/* An authenticated SETUP must progress to RECORD within a bounded window,
 * even if timing remains alive. Media activity cannot extend this deadline. */
#define CP_PEER_SETUP_MS UINT64_C(30000)
struct cp_peer_health {
    uintptr_t session;
    uint64_t last_ms;
    uint32_t rx, activity, timing, timing_large_rtt;
    int armed, expired, started;
};
static inline int cp_peer_health_sample(struct cp_peer_health *h, uintptr_t session,
    int started, uint32_t rx, uint32_t activity, uint32_t timing, uint32_t timing_large_rtt, uint64_t now)
{
    if (!session || !now) { memset(h,0,sizeof(*h)); return 0; }
    if (!started) {
        if (!h->armed || h->session!=session || h->started || now<h->last_ms) {
            if (!h->armed || h->session!=session)h->expired=0;
            h->session=session;h->last_ms=now;h->armed=1;h->started=0;
        }
        if (!h->expired && now-h->last_ms>=CP_PEER_SETUP_MS) {
            h->expired=1;return 1;
        }
        return 0;
    }
    if (!h->armed || !h->started || h->session!=session || now<h->last_ms ||
        rx!=h->rx || activity!=h->activity || timing!=h->timing || timing_large_rtt!=h->timing_large_rtt) {
        if(!h->armed || h->session!=session)h->expired=0;
        h->session=session; h->last_ms=now; h->rx=rx;
        h->activity=activity; h->timing=timing; h->timing_large_rtt=timing_large_rtt; h->armed=1; h->started=1;
        /* Once shutdown was requested, activity cannot authorize a second
         * close or revive this epoch. The native finalizer owns teardown. */
        return 0;
    }
    if (!h->expired && now-h->last_ms>=CP_PEER_SILENT_MS) {
        h->expired=1; return 1;
    }
    return 0;
}
#endif
