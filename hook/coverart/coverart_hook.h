/*
 * CarPlay Cover Art Hook Module
 *
 * Reassembles artwork from the stock Cinemo NmeTransport::Recv stream,
 * decodes JPEG/PNG off the iAP2 receive thread, resizes to 256x256, and
 * notifies Java over the bus.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef COVERART_HOOK_H
#define COVERART_HOOK_H

#include "../framework/hook_framework.h"

/* Registered by the framework from hook_module_table (hook/main.c).  Neither
 * lifecycle entry is an LD_PRELOAD constructor: the receive tap goes live only
 * after the first real Cinemo call enters hook_framework_init(). */
extern const hook_module_def_t coverart_module_def;

#endif /* COVERART_HOOK_H */
