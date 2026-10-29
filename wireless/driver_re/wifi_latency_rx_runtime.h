#ifndef WIFI_LATENCY_RX_RUNTIME_H
#define WIFI_LATENCY_RX_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
/* Pinned MU1316 driver + io-pkt runtime ABI, not a generic binary hook.
 * Root supplies full-function entry gateways: replay the first 4-byte PUSH, then
 * branch to original entry +4; never copy the PC-relative second instruction.
 */
uintptr_t wl_native_base(void);
int wl_lock(void);
void wl_unlock(void);
uint32_t wl_native_ticks(void);
int wl_orig_rx(void *, uint32_t, uint32_t, const void *, uint32_t, void *);
int wl_orig_create(void *, void *, const void *);
void wl_orig_delete(void *, void *);
void wl_orig_flush(void *);

int wl_rx_runtime_receive(void *, uint32_t, uint32_t, const void *, uint32_t, void *);
int wl_rx_runtime_create(void *, void *, const void *);
void wl_rx_runtime_delete(void *, void *);
void wl_rx_runtime_flush(void *);
/* ARM inline tail hook0x47748: table=r5 oldstart=r6 adapter=r9.
 * 0 -> replayoriginal ldrb at47748 thenresume4774c;
 * 1 -> skipnative timer tail andresume47790; r8 result preserved by glue.
 */
uint32_t wl_rx_runtime_tail(void *, uint32_t, void *);
/* Diagnostic for managed RX table/cookie drain, not an unload permission.
 * Driver detach retires tables through wrapped native delete. The complete
 * payload remains pinned RTLD_NODELETE through process lifetime even if quiet.
 */
int wl_rx_runtime_quiescent(void);
struct wl_rx_runtime_stats {
    uint32_t enroll, fallback, pool_exhausted, stale_callback, callbacks,
             arm, cancel_queued, cancel_captured, forced_flush, invalid;
};
void wl_rx_runtime_stats(struct wl_rx_runtime_stats *);
#endif
