#define _POSIX_C_SOURCE 200809L
#include "audio/receiver_wait.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int exited, control_exited;
static struct timespec deadline_ms(long ms)
{
    struct timespec ts;
    assert(!clock_gettime(CLOCK_REALTIME, &ts));
    ts.tv_nsec += ms * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    return ts;
}
static void *exit_worker(void *unused)
{
    (void)unused;
    assert(!pthread_mutex_lock(&lock));
    exited = 1;
    assert(!pthread_cond_broadcast(&cond));
    assert(!pthread_mutex_unlock(&lock));
    return NULL;
}
int main(void)
{
    struct timespec deadline;
    pthread_t worker;
    assert(!pthread_mutex_lock(&lock));
    deadline = deadline_ms(20);
    assert(spa_receiver_wait_workers(&cond, &lock, 1, &exited,
                                     0, &control_exited, &deadline) == ETIMEDOUT);
    assert(spa_receiver_workers_alive(1, exited, 0, control_exited));
    assert(!exited); /* timeout does NOT pretend the live worker has exited */
    assert(!pthread_create(&worker, NULL, exit_worker, NULL));
    deadline = deadline_ms(1000);
    assert(!spa_receiver_wait_workers(&cond, &lock, 1, &exited,
                                      0, &control_exited, &deadline));
    assert(!spa_receiver_workers_alive(1, exited, 0, control_exited));
    assert(!pthread_mutex_unlock(&lock));
    assert(!pthread_join(worker, NULL)); /* only now is freeing/reuse safe */
    assert(!pthread_mutex_lock(&lock));
    deadline = deadline_ms(20);
    assert(spa_receiver_wait_workers(&cond, &lock, 1, &exited,
                                     1, &control_exited, &deadline) == ETIMEDOUT);
    assert(spa_receiver_workers_alive(1, exited, 1, control_exited));
    control_exited = 1;
    assert(!spa_receiver_wait_workers(&cond, &lock, 1, &exited,
                                      1, &control_exited, &deadline));
    assert(!spa_receiver_workers_alive(1, exited, 1, control_exited));
    assert(!spa_receiver_workers_alive(0, 0, 0, 0));
    assert(!pthread_mutex_unlock(&lock));
    puts("receiver_wait_test: OK (bounded stop, quarantine until real exit, control worker)");
    return 0;
}
