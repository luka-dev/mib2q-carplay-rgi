#ifndef SPA_MEDIA_TRANSPORT_H
#define SPA_MEDIA_TRANSPORT_H
/* Protected by sink.lock. Separate teardown cancellation from the failure
 * latch: ordinary 103 teardown may resume 100, transport EOF may not. */
typedef struct {
    int stopping;
    int await_media_restart;
    int invalidating;
} spa_media_transport_t;

static inline int spa_media_transport_blocked(const spa_media_transport_t *transport)
{
    return transport->stopping || transport->await_media_restart || transport->invalidating;
}
static inline void spa_media_transport_fail_begin(spa_media_transport_t *transport)
{
    transport->invalidating = 1;
    transport->await_media_restart = 1;
}
static inline void spa_media_transport_fail_end(spa_media_transport_t *transport)
{
    transport->invalidating = 0;
}
static inline void spa_media_transport_media_prepare(spa_media_transport_t *transport)
{
    /* Even a fresh 100 must wait until invalidation has retired the old ring. */
    transport->await_media_restart = 0;
}
static inline void spa_media_transport_setup(spa_media_transport_t *transport)
{
    /* Only after preceding workers really exit and are joined. */
    transport->stopping = 0;
    transport->await_media_restart = 0;
    transport->invalidating = 0;
}
#endif
