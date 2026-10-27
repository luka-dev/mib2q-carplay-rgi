/*
 * CarPlay AltScreen (instrument-cluster stream 111) — MHI2Q.
 *
 * Shipping path: LD_PRELOAD over stock libairplay 210.81. This module advertises and
 * negotiates altScreen, owns the type-111 TCP receiver, derives the stream AES key,
 * decrypts AirPlay screen packets, converts AVCC to Annex-B, and forwards H.264 to
 * altscreen_render on 127.0.0.1:19831. No from-source libairplay is loaded or required.
 *
 * QNX: NO __thread (gcc 4.4 emutls trap) — globals + pthread_mutex throughout.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef CARPLAY_ALTSCREEN_HOOK_H
#define CARPLAY_ALTSCREEN_HOOK_H

#include "../framework/hook_framework.h"

/* Registered by the framework from hook_module_table (hook/main.c). */
extern const hook_module_def_t altscreen_module_def;

#endif /* CARPLAY_ALTSCREEN_HOOK_H */
