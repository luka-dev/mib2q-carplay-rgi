/*
 * media_route.h -- tiny state predicate for the buffered CarPlay route owner.
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

#define SPA_MEDIA_ROUTE_REACQUIRE_DELAY_NS UINT64_C(50000000)
#define SPA_MEDIA_ROUTE_RETRY_DELAY_NS     UINT64_C(1000000000)

static inline int spa_media_route_reacquire_due(int detached,
                                                 int hook_owned,
                                                 uint64_t deadline_ns,
                                                 uint64_t now_ns)
{
    return detached && !hook_owned && deadline_ns && now_ns &&
           now_ns >= deadline_ns;
}

#endif /* MEDIA_ROUTE_H */
