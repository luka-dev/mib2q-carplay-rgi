/* SPDX-License-Identifier: GPL-3.0-or-later
 * AAP 1.7 + cluster display (live runs 3-4, wired and adapter): the phone sends a stock service (channel 12)
 * a message this 2016 libautoreceiver does not handle. gal answers with
 * MessageRouter::sendUnexpectedMessage(channel) ("Sending unexpected message on channel 12", 6x), and
 * < 1 s later the phone resets the USB link. At AAP 1.2 there is no error, but the phone then treats the
 * second sink as another MAIN display (owner photos 2026-09-29: the AA main UI, not the cluster map).
 * So stay at 1.7 and do not send that reply: log it and drop it. An unsupported message left unanswered
 * is ordinary for the phone; an explicit "unexpected" is what it tears the link down over (hypothesis
 * under test). SD flag sq5_cluster_unexpected_pass = send the stock reply as before.
 */
#include "capture.h"
#include <dlfcn.h>
#include <unistd.h>

typedef void (*unexpected_fn)(void *, unsigned char);

void _ZN13MessageRouter21sendUnexpectedMessageEh(void *router, unsigned char channel)
{
    static unexpected_fn real;
    static unsigned count;
    int pass = access("/fs/sda0/sq5_cluster_unexpected_pass", F_OK) == 0;
    if (!real) real = (unexpected_fn)dlsym(RTLD_NEXT, "_ZN13MessageRouter21sendUnexpectedMessageEh");
    if (count++ < 64)
        probe_log("unexpected.%s channel=%u n=%u", pass || !real ? "sent" : "suppressed", channel, count);
    if ((pass || !real) && real) real(router, channel);
    if (!real) probe_log("unexpected.real_missing channel=%u", channel);
}
