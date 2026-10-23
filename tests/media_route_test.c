/* Global DIO ownership regressions: no device or firmware required. */
#include "audio/media_route.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>

static const uint64_t now = UINT64_C(1000000000);
static int media, phone, siri;

/* Same eligibility predicate as both production buffered sink paths. */
static int claim(spa_media_route_t *route, size_t playing_pending, uint64_t at)
{
    if (!spa_media_route_buffered_ready(route, playing_pending, 768, at)) return 0;
    route->hook_owned = 1;
    route->reacquire_after_ns = 0;
    return 1;
}

static void music_call_resume(void)
{
    spa_media_route_t route = { 0 };
    uint64_t deadline;

    assert(claim(&route, 19200, now)); /* cold 103 */
    assert(spa_media_route_stock_begin(&route, &media) == 1);
    assert(!route.hook_owned); /* transfer, not tornDown */
    assert(!spa_media_route_take_release(&route));
    assert(!claim(&route, 19200, now));
    assert(spa_media_route_stock_end(&route, &media, now));
    deadline = route.reacquire_after_ns;
    assert(!claim(&route, 19200, deadline - 1));

    /* Phone Prepare arrives before the old 50-ms media timer expires. */
    assert(spa_media_route_stock_begin(&route, &phone) == 1);
    assert(!claim(&route, 19200, deadline));
    assert(!claim(&route, 19200, deadline + UINT64_C(60000000000)));
    assert(!spa_media_route_take_release(&route)); /* teardown103 during call */
    assert(!claim(&route, 0, deadline)); /* rate=0 receiver is still alive */

    /* Stop's callback has run; only now may the main object be removed. */
    assert(spa_media_route_stock_end(&route, &phone, deadline));
    deadline = route.reacquire_after_ns;
    assert(!claim(&route, 19200, deadline - 1));
    assert(!claim(&route, 0, deadline)); /* hangup does not force playback */
    assert(!claim(&route, 767, deadline));
    assert(claim(&route, 768, deadline));
    assert(spa_media_route_take_release(&route));
    assert(!spa_media_route_take_release(&route)); /* exactly one release */
}

static void overlap_and_failures(void)
{
    spa_media_route_t route = { 0 };
    int added;

    assert(spa_media_route_stock_begin(&route, &media) == 1);
    assert(spa_media_route_stock_begin(&route, &phone) == 1);
    assert(spa_media_route_stock_end(&route, &media, now));
    assert(spa_media_route_blocked(&route)); /* old Stop cannot release phone */
    assert(!claim(&route, 19200, now + SPA_MEDIA_ROUTE_RETRY_DELAY_NS));
    assert(spa_media_route_stock_begin(&route, &siri) == 1);
    assert(spa_media_route_stock_end(&route, &phone, now));
    assert(spa_media_route_blocked(&route)); /* Siri still owns stock main */

    /* Duplicate Prepare fails stock's already-prepared check. The wrapper
     * rolls back ONLY a newly added object, not an existing active one. */
    added = spa_media_route_stock_begin(&route, &siri);
    assert(added == 0);
    if (added == 1) (void)spa_media_route_stock_end(&route, &siri, now);
    assert(spa_media_route_blocked(&route));
    assert(spa_media_route_stock_end(&route, &siri, now));
    assert(!spa_media_route_stock_end(&route, &phone, now));
    assert(claim(&route, 768, now + SPA_MEDIA_ROUTE_REACQUIRE_DELAY_NS));

    /* New Prepare fails (or incoming call is cancelled before start): no
     * permanent block and no stale hook-owned teardown of the phone route. */
    added = spa_media_route_stock_begin(&route, &phone);
    assert(added == 1);
    assert(!spa_media_route_take_release(&route));
    if (added == 1) assert(spa_media_route_stock_end(&route, &phone, now));
    assert(!claim(&route, 768, now));
    assert(claim(&route, 768, now + SPA_MEDIA_ROUTE_REACQUIRE_DELAY_NS));

    /* Cold 103 during a call has no media handle but uses the same guard. */
    assert(spa_media_route_stock_begin(&route, &phone) == 1);
    assert(!claim(&route, 19200, now + SPA_MEDIA_ROUTE_RETRY_DELAY_NS));
    assert(!spa_media_route_take_release(&route));
    assert(spa_media_route_stock_end(&route, &phone, now));
    assert(claim(&route, 19200, now + SPA_MEDIA_ROUTE_REACQUIRE_DELAY_NS));
}

static void capacity_fails_closed(void)
{
    spa_media_route_t route = { 0 };
    int streams[SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS + 1] = { 0 }; /* only the addresses matter; gcc -Werror=maybe-uninitialized */
    size_t i;
    for (i = 0; i < SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS; ++i)
        assert(spa_media_route_stock_begin(&route, &streams[i]) == 1);
    assert(spa_media_route_stock_begin(&route, &streams[i]) == -1);
    for (i = 0; i < SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS; ++i)
        assert(spa_media_route_stock_end(&route, &streams[i], now));
    assert(spa_media_route_blocked(&route));
    assert(!claim(&route, 19200, now + SPA_MEDIA_ROUTE_RETRY_DELAY_NS));
    route.hook_owned = 1; /* even an inconsistent flag cannot release voice */
    assert(!spa_media_route_take_release(&route));
}

/* Exercise claim/release vs stock priority with the production lock contract.
 * The lock covers the modeled callback as well as the state transition. */
static pthread_mutex_t route_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pcm_io_lock = PTHREAD_MUTEX_INITIALIZER;
static spa_media_route_t concurrent_route;
static int stock_active;

static void *stock_thread(void *unused)
{
    unsigned i;
    (void)unused;
    for (i = 0; i < 20000; ++i) {
        assert(!pthread_mutex_lock(&route_lock));
        assert(spa_media_route_stock_begin(&concurrent_route, &phone) == 1);
        stock_active = 1; /* stock Telephony callback */
        assert(!pthread_mutex_unlock(&route_lock));
        assert(!pthread_mutex_lock(&route_lock));
        stock_active = 0; /* stock tornDown callback */
        assert(spa_media_route_stock_end(&concurrent_route, &phone, now));
        assert(!pthread_mutex_unlock(&route_lock));
    }
    return NULL;
}

static void *buffered_thread(void *unused)
{
    unsigned i;
    (void)unused;
    for (i = 0; i < 20000; ++i) {
        assert(!pthread_mutex_lock(&pcm_io_lock));
        assert(!pthread_mutex_lock(&route_lock));
        if (claim(&concurrent_route, 19200, now + SPA_MEDIA_ROUTE_RETRY_DELAY_NS))
            assert(!stock_active); /* modeled Media callback */
        if (spa_media_route_take_release(&concurrent_route))
            assert(!stock_active); /* modeled global tornDown callback */
        assert(!pthread_mutex_unlock(&route_lock));
        assert(!pthread_mutex_unlock(&pcm_io_lock));
    }
    return NULL;
}

static void feedback_ownership(void)
{
    spa_media_route_t route = { 0 };
    uint64_t epoch;
    int nonmedia;
    assert(!spa_media_route_has_media(&route)); /* cold103: no cacheable stock media */
    assert(spa_media_route_stock_begin_type(&route, &media, 1) == 1);
    epoch = route.epoch;
    assert(spa_media_route_has_media(&route));
    assert(spa_media_route_feedback_allowed(&route, epoch, 0));
    assert(spa_media_route_stock_begin_type(&route, &phone, 3) == 1);
    assert(!spa_media_route_feedback_allowed(&route, epoch, 0));
    nonmedia = spa_media_route_nonmedia(&route);
    epoch = route.epoch;
    /* Telephony never supplies a music clock, regardless of 16/24/48 kHz. */
    assert(!spa_media_route_feedback_allowed(&route, epoch, nonmedia));
    assert(spa_media_route_stock_end(&route, &phone, now));
    assert(!spa_media_route_feedback_allowed(&route, epoch, nonmedia));
    epoch = route.epoch;
    /* Even a complete call inside a slow Control query invalidates the response. */
    assert(spa_media_route_stock_begin_type(&route, &phone, 3) == 1);
    assert(spa_media_route_stock_end(&route, &phone, now));
    assert(!spa_media_route_feedback_allowed(&route, epoch, 0));
    assert(spa_media_route_feedback_allowed(&route, route.epoch, 0));
    epoch = route.epoch;
    assert(spa_media_route_stock_begin_type(&route, &media, 4) == 0);
    assert(!spa_media_route_feedback_allowed(&route, epoch, 0));
    assert(!spa_media_route_has_media(&route));
    assert(spa_media_route_nonmedia(&route));
    assert(spa_media_route_stock_end(&route, &media, now));
    assert(spa_media_route_stock_begin_type(&route, &siri, 0) == 1);
    assert(spa_media_route_nonmedia(&route)); /* unknown purpose fails closed */
}

static void realtime_upmix_policy(void)
{
    spa_media_route_t route = { 0 };
    int alert = 0;

    /* nothing tracked yet: plain L/R */
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 1));
    assert(spa_media_route_stock_begin_type(&route, &media, 1) == 1);
    assert(spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 1));
    /* never inside a type-103 pause/shortage, never for non-2->6, never unready */
    assert(!spa_media_route_realtime_upmix_allowed(&route, 0, 2, 6, 1));
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 1, 6, 1));
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 2, 8, 1));
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 0));
    /* a phone/Siri/alert owner anywhere wins, even beside live media */
    assert(spa_media_route_stock_begin_type(&route, &phone, 3) == 1);
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 1));
    assert(spa_media_route_stock_end(&route, &phone, now));
    assert(spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 1));
    assert(spa_media_route_stock_begin_type(&route, &alert, 0) == 1);
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 1));
    assert(spa_media_route_stock_end(&route, &alert, now));
    /* media stopped: back to plain L/R; a faulted table never upmixes */
    assert(spa_media_route_stock_end(&route, &media, now));
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 1));
    route.faulted = 1;
    assert(spa_media_route_stock_begin_type(&route, &media, 1) == 1);
    assert(!spa_media_route_realtime_upmix_allowed(&route, 1, 2, 6, 1));
}

int main(void)
{
    pthread_t stock, buffered;
    music_call_resume();
    overlap_and_failures();
    capacity_fails_closed();
    feedback_ownership();
    realtime_upmix_policy();
    /* A stalled media writer cannot delay stock priority publication. */
    assert(!pthread_mutex_lock(&pcm_io_lock));
    assert(!pthread_create(&stock, NULL, stock_thread, NULL));
    assert(!pthread_join(stock, NULL));
    assert(!pthread_mutex_unlock(&pcm_io_lock));
    assert(!pthread_create(&stock, NULL, stock_thread, NULL));
    assert(!pthread_create(&buffered, NULL, buffered_thread, NULL));
    assert(!pthread_join(stock, NULL));
    assert(!pthread_join(buffered, NULL));
    assert(!spa_media_route_blocked(&concurrent_route));
    puts("media_route_test: OK (call/Siri priority, paused103, overlap, failure, cold/resume, release race, feedback epochs)");
    return 0;
}
