#include "audio/media_transport.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    spa_media_transport_t transport = { 0 };
    assert(!spa_media_transport_blocked(&transport));
    transport.stopping = 1;
    assert(spa_media_transport_blocked(&transport));
    transport.stopping = 0;
    assert(!spa_media_transport_blocked(&transport)); /* ordinary teardown -> 100 */
    spa_media_transport_fail_begin(&transport); /* TCP EOF or framing failure */
    assert(spa_media_transport_blocked(&transport));
    spa_media_transport_fail_end(&transport);
    assert(spa_media_transport_blocked(&transport)); /* no stale 100 timeline */
    transport.stopping = 1;
    transport.stopping = 0;
    assert(spa_media_transport_blocked(&transport)); /* Stop/hangup isn't recovery */
    spa_media_transport_media_prepare(&transport);
    assert(!spa_media_transport_blocked(&transport));
    spa_media_transport_fail_begin(&transport);
    spa_media_transport_media_prepare(&transport); /* new 100 races ring invalidation */
    assert(spa_media_transport_blocked(&transport));
    spa_media_transport_fail_end(&transport);
    assert(!spa_media_transport_blocked(&transport));
    transport.stopping = 1;
    spa_media_transport_media_prepare(&transport);
    assert(spa_media_transport_blocked(&transport)); /* never cancel teardown */
    spa_media_transport_fail_begin(&transport);
    spa_media_transport_setup(&transport); /* permitted only after workers joined */
    assert(!spa_media_transport_blocked(&transport));
    puts("media_transport_test: OK (EOF latch, fresh timeline, teardown, invalidation race)");
    return 0;
}
