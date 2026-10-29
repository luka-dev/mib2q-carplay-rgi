#ifndef WIFI_LATENCY_RUNTIME_H
#define WIFI_LATENCY_RUNTIME_H
#include <stdint.h>

/* Pinned MU1316 module and io-pkt only. All pointer arithmetic is ARM32.
 * Static QNX PTHREAD_RMUTEX_INITIALIZER, verified against the 6.5 SDK, supplies
 * recursive priority inheritance. The module is pinned before hooks enroll.
 */
uintptr_t wl_native_base(void);
uint32_t wl_native_ticks(void);
int wl_runtime_prepare(void);
int wl_lock(void);
void wl_unlock(void);
uint32_t wl_runtime_probe(uint32_t *out);

int wl_orig_attach(void *parent, void *softc, void *args);
int wl_orig_detach(void *softc);
int wl_tx_runtime_attach(void *parent, void *softc, void *args);
int wl_tx_runtime_detach(void *softc);
uint32_t wl_tx_runtime_gate(void *handle, void *ifnet, void *owner);
int wl_tx_runtime_request(void *function, void *argument, void *owner);
#endif
