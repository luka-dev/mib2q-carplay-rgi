/* One in-flight OMX frame. Its callback owns the source until completion.
 * Only the compositor touches GL; no decoder callback migrates its context. */
#ifndef ALTSCREEN_PRISM_QUEUE_H
#define ALTSCREEN_PRISM_QUEUE_H
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int state; /* 0 empty, 1 queued, 2 rendering, 3 completed */
    int result, loader_requested;
    unsigned int source;
    uint64_t arrival_ns;
} prism_queue_t;

static inline int prism_queue_init(prism_queue_t *q) {
    if(pthread_mutex_init(&q->lock,NULL)) return -1;
    if(pthread_cond_init(&q->cond,NULL)) { pthread_mutex_destroy(&q->lock); return -1; }
    q->state=0; q->loader_requested=0;
    return 0;
}
static inline void prism_queue_destroy(prism_queue_t *q) {
    pthread_cond_destroy(&q->cond); pthread_mutex_destroy(&q->lock);
}
static inline void prism_queue_loader(prism_queue_t *q) {
    pthread_mutex_lock(&q->lock); q->loader_requested=1;
    pthread_cond_broadcast(&q->cond); pthread_mutex_unlock(&q->lock);
}
/* Returns -2 on timeout/error: source ownership remains live, so the isolated
 * renderer must exit rather than recycle the OMX buffer or free GPU storage. */
static inline int prism_queue_submit(prism_queue_t *q,unsigned int source,
                                      uint64_t arrival_ns,int timeout_ms) {
    struct timespec deadline;
    int rc=0;
    if(clock_gettime(CLOCK_REALTIME,&deadline)) return -2;
    deadline.tv_sec+=timeout_ms/1000;
    deadline.tv_nsec+=(timeout_ms%1000)*1000000L;
    if(deadline.tv_nsec>=1000000000L) { deadline.tv_nsec-=1000000000L; deadline.tv_sec++; }
    pthread_mutex_lock(&q->lock);
    while(q->state && !rc) rc=pthread_cond_timedwait(&q->cond,&q->lock,&deadline);
    if(rc) { pthread_mutex_unlock(&q->lock); return -2; }
    q->source=source; q->arrival_ns=arrival_ns; q->state=1;
    pthread_cond_broadcast(&q->cond);
    while(q->state!=3 && !rc) rc=pthread_cond_timedwait(&q->cond,&q->lock,&deadline);
    if(rc) { pthread_mutex_unlock(&q->lock); return -2; }
    rc=q->result; q->state=0;
    pthread_cond_broadcast(&q->cond); pthread_mutex_unlock(&q->lock);
    return rc;
}
static inline int prism_queue_take(prism_queue_t *q,unsigned int *source,
                                    uint64_t *arrival_ns,int *loader_requested) {
    int ready;
    pthread_mutex_lock(&q->lock);
    *loader_requested=q->loader_requested; q->loader_requested=0;
    ready=q->state==1;
    if(ready) { *source=q->source; *arrival_ns=q->arrival_ns; q->state=2; }
    pthread_mutex_unlock(&q->lock);
    return ready;
}
static inline void prism_queue_complete(prism_queue_t *q,int result) {
    pthread_mutex_lock(&q->lock);
    q->result=result; q->state=3;
    pthread_cond_broadcast(&q->cond); pthread_mutex_unlock(&q->lock);
}
/* A settled stream needs no repeated GPU work. Wake for new input or return;
 * the short timeout lets the compositor observe shutdown without another lock. */
static inline int prism_queue_idle(prism_queue_t *q) {
    struct timespec deadline;
    int rc=0;
    if(clock_gettime(CLOCK_REALTIME,&deadline)) return 1;
    deadline.tv_nsec+=30000000L;
    if(deadline.tv_nsec>=1000000000L) { deadline.tv_nsec-=1000000000L; deadline.tv_sec++; }
    pthread_mutex_lock(&q->lock);
    while(q->state!=1 && !q->loader_requested && !rc)
        rc=pthread_cond_timedwait(&q->cond,&q->lock,&deadline);
    rc=q->state==1 || q->loader_requested;
    pthread_mutex_unlock(&q->lock);
    return rc;
}
#endif
