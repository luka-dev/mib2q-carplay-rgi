/* LOCAL TEST ONLY: actual dynamic interposition + concurrent first use. */
#include "interpose_example.h"
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t start_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
static int start;

static void *run_calls(void *unused)
{
    int marker = 17;
    (void)unused;
    pthread_mutex_lock(&start_lock);
    while (!start) pthread_cond_wait(&start_cond, &start_lock);
    pthread_mutex_unlock(&start_lock);
    for (int i=0; i<64; ++i) {
        uint8_t byte=(uint8_t)(i&1);
        errno=EDOM;
        int result=demo_video_submit(&marker,&byte,1);
        assert(result==(byte ? 23 : 0));
        assert(errno==(byte ? EIO : EAGAIN));
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[4];
    void (*get_stats)(demo_hook_stats *) = NULL;
    void *address = dlsym(RTLD_DEFAULT,"demo_hook_get_stats");
    assert(address && sizeof(address)==sizeof(get_stats));
    memcpy(&get_stats,&address,sizeof(get_stats));
    for (int i=0;i<4;++i) assert(pthread_create(&threads[i],NULL,run_calls,NULL)==0);
    pthread_mutex_lock(&start_lock);
    start=1;
    pthread_cond_broadcast(&start_cond);
    pthread_mutex_unlock(&start_lock);
    for (int i=0;i<4;++i) assert(pthread_join(threads[i],NULL)==0);
    demo_hook_stats snapshot;
    errno=EDOM;
    get_stats(&snapshot);
    assert(errno==EDOM);
    assert(snapshot.calls==256 && snapshot.successes==128 && snapshot.failures==128);
    puts("Dynamic hook: 256 calls, original arguments/status/errno preserved, exact concurrent counters.");
    return 0;
}
