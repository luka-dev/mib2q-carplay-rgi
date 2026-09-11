/* Caller owns lock. A timeout NEVER cancels a worker or releases its state. */
#ifndef SPA_RECEIVER_WAIT_H
#define SPA_RECEIVER_WAIT_H
#include <pthread.h>
#include <time.h>

static inline int spa_receiver_workers_alive(int created, int exited,
                                             int control_created, int control_exited)
{
    return (created && !exited) || (control_created && !control_exited);
}

static inline int spa_receiver_wait_workers(pthread_cond_t *cond, pthread_mutex_t *lock,
                                             int created, const int *exited,
                                             int control_created, const int *control_exited,
                                             const struct timespec *deadline)
{
    int result = 0;
    while (spa_receiver_workers_alive(created, *exited, control_created, *control_exited)
           && !result)
        result = pthread_cond_timedwait(cond, lock, deadline);
    return result;
}
#endif
