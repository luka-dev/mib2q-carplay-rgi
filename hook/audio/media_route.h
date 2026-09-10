/*
 * media_route.h -- stock-main priority and buffered CarPlay route ownership.
 *
 * Stock type-100 releases the Pioneer Media route after its QSA worker has
 * stopped.  If type-103 keeps the QSA handle alive, the hook must request the
 * route again, but only after that stock release has had time to be queued.
 * Keeping the decision pure makes the stock -> detached ownership edge
 * regression-testable without loading dio_manager on the host.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef MEDIA_ROUTE_H
#define MEDIA_ROUTE_H

#include <stdint.h>
#include <stddef.h>

#define SPA_MEDIA_ROUTE_REACQUIRE_DELAY_NS UINT64_C(50000000)
#define SPA_MEDIA_ROUTE_RETRY_DELAY_NS     UINT64_C(1000000000)

/* Stock owns the global main-audio route from BEFORE Prepare's type callback
 * until AFTER Stop's teardown callback. Track objects, not just the latest
 * audioType: an old media Stop must not unblock a newer phone/Siri Prepare.
 * All access, including the corresponding DIO callback, is serialized by the
 * sink ioLock. Never hold that lock across a stock Prepare/Stop (HMI wait and
 * worker joins re-enter the QSA interposers).
 *
 * Eight concurrent main objects exceeds the stock main-stream capacity. An
 * overflow disables hook route claims fail-closed for the process lifetime;
 * it never prevents stock telephony from proceeding. No heap or audio-thread
 * allocation, and no stale entry after an ordinary Stop. */
#define SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS 8u
typedef struct {
    const void *main_streams[SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS];
    int faulted;
    int hook_owned;
    uint64_t reacquire_after_ns;
} spa_media_route_t;

static inline int spa_media_route_blocked(const spa_media_route_t *route)
{
    size_t i;
    if (route->faulted) return 1;
    for (i = 0; i < SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS; ++i)
        if (route->main_streams[i]) return 1;
    return 0;
}

/* 1 = newly tracked, 0 = already tracked, -1 = fail-closed overflow. */
static inline int spa_media_route_stock_begin(spa_media_route_t *route,
                                               const void *stream)
{
    size_t i, empty = SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS;
    if (!stream) return 0;
    route->hook_owned = 0; /* transfer, NOT a global tornDown */
    route->reacquire_after_ns = 0;
    for (i = 0; i < SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS; ++i) {
        if (route->main_streams[i] == stream) return 0;
        if (!route->main_streams[i]) empty = i;
    }
    if (empty == SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS) {
        route->faulted = 1;
        return -1;
    }
    route->main_streams[empty] = stream;
    return 1;
}

static inline int spa_media_route_stock_end(spa_media_route_t *route,
                                             const void *stream,
                                             uint64_t now_ns)
{
    size_t i;
    if (!stream) return 0;
    for (i = 0; i < SPA_MEDIA_ROUTE_MAX_MAIN_STREAMS; ++i) {
        if (route->main_streams[i] != stream) continue;
        route->main_streams[i] = NULL;
        if (!spa_media_route_blocked(route))
            route->reacquire_after_ns = now_ns + SPA_MEDIA_ROUTE_REACQUIRE_DELAY_NS;
        return 1;
    }
    return 0; /* repeated Stop cannot postpone resume or release a newer owner */
}

static inline int spa_media_route_can_request(const spa_media_route_t *route,
                                               uint64_t now_ns)
{
    return !spa_media_route_blocked(route) &&
           (!route->reacquire_after_ns ||
            (now_ns && now_ns >= route->reacquire_after_ns));
}

/* Called immediately before the global tornDown, under the SAME ioLock as
 * stock_begin. Clearing a flag and calling tornDown after unlocking is unsafe. */
static inline int spa_media_route_take_release(spa_media_route_t *route)
{
    int release = route->hook_owned && !spa_media_route_blocked(route);
    route->hook_owned = 0;
    return release;
}

/* pending is the renderer's PLAYING + primed snapshot, not receiver lifetime.
 * A paused receiver with queued PCM must pass zero here. */
static inline int spa_media_route_buffered_ready(const spa_media_route_t *route,
                                                   size_t pending,
                                                   size_t period,
                                                   uint64_t now_ns)
{
    return period && pending >= period && spa_media_route_can_request(route, now_ns);
}

#endif /* MEDIA_ROUTE_H */
