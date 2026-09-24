/* hook_framework.h inject_note_link / inject_unlink: a frame queued before the
 * stock message's own Send stays valid; an Identify boundary or a new link
 * session never lets an old frame through. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "framework/hook_framework.h"

int main(void) {
    injection_ctx_t inj;
    uint32_t queued;
    memset(&inj, 0, sizeof(inj));
    inject_note_link(&inj, 1);
    queued = inj.generation;               /* StartListUpdates queued from Encode(0x4154) */
    inject_note_link(&inj, 1);             /* ...then stock Send(0x4154) and more traffic */
    inject_note_link(&inj, 1);
    assert(inj.generation == queued);      /* worker still sends it */
    inject_note_link(&inj, 2);             /* new link session */
    assert(inj.generation != queued);
    queued = inj.generation;
    inject_unlink(&inj);                   /* Identify start */
    assert(!inj.linked && !inj.valid);
    inject_note_link(&inj, 2);             /* same session number after re-Identify */
    assert(inj.generation != queued);
    puts("inject_generation_test: PASS");
    return 0;
}
