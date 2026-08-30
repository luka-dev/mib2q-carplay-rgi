/*
 * CarPlay Cover Art Hook Module
 *
 * Reassembles artwork from the stock Cinemo NmeTransport::Recv stream,
 * decodes JPEG/PNG off the iAP2 receive thread, resizes to 256x256, and
 * notifies Java over the bus.
 */

#ifndef COVERART_HOOK_H
#define COVERART_HOOK_H

#include "../framework/common.h"
#include "../framework/logging.h"

/* Called by the hook framework's lazy lifecycle.  Neither function is an
 * LD_PRELOAD constructor: the receive sink is installed only after the first
 * real Cinemo call enters hook_framework_init(). */
void coverart_runtime_init(void);
void coverart_runtime_shutdown(void);

#endif /* COVERART_HOOK_H */
