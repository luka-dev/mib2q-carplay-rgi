#ifndef MHI2Q_SI_NATIVE_H
#define MHI2Q_SI_NATIVE_H
#include "si_controller.h"
struct si_native_context { uint32_t controller, impl; };
extern const struct si_native_ops si_native_operations;
/* Entry handlers: nonzero consumes the call; zero executes native USB code. */
int32_t si_native_prepare(void *handler, uint32_t retry, uint32_t unused);
int32_t si_native_cleanup(void *handler, uint32_t error, uint32_t port_reset);
int32_t si_native_connect(void *controller, uint32_t device, uint32_t app);
int32_t si_native_carplay_device(void *data_proxy, uint32_t app, uint32_t device);
/* 0 native USB, 1 native WLAN, -1 inconsistent/unknown CarPlay context. */
int si_native_child_transport(void);
#endif
