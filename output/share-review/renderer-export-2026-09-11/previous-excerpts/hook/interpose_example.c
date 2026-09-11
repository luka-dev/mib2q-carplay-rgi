/* Copyright (c) 2026, original project author. */
/* Educational adaptation of a public interposition pattern, not a production hook. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "interpose_example.h"
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

typedef int (*submit_fn)(void *, const uint8_t *, size_t);
static submit_fn real_submit;
static pthread_once_t resolve_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
static demo_hook_stats stats;

static void resolve_original(void)
{
    void *address;
    /* Clear an old loader error before attributing an error to this lookup. */
    (void)dlerror();
    address = dlsym(RTLD_NEXT, "demo_video_submit");
    if (dlerror() != NULL || address == NULL) return;
    /* POSIX dynamic linking with equal-size object/function pointers is the
     * supported host model here. Do not apply an unverified target ABI cast.
     */
    if (sizeof(real_submit) == sizeof(address))
        memcpy(&real_submit, &address, sizeof(real_submit));
}

int demo_video_submit(void *context, const uint8_t *data, size_t size)
{
    int result, saved_errno;
    int entry_errno = errno;
    int init_result = pthread_once(&resolve_once, resolve_original);
    if (init_result != 0 || real_submit == NULL) {
        errno = ENOSYS;
        return -1;
    }

    /* No hook mutex is held across the provider call. The original receives
     * unchanged arguments and the errno value seen at wrapper entry.
     */
    errno = entry_errno;
    result = real_submit(context, data, size);
    saved_errno = errno;

    /* Observe both success and failure, but preserve the provider's status.
     * Keeping this small also avoids retaining objects owned by the caller.
     */
    pthread_mutex_lock(&stats_lock);
    ++stats.calls;
    if (result == 0) ++stats.successes;
    else ++stats.failures;
    pthread_mutex_unlock(&stats_lock);

    errno = saved_errno;
    return result;
}

void demo_hook_get_stats(demo_hook_stats *out)
{
    int saved_errno = errno;
    if (out != NULL) {
        pthread_mutex_lock(&stats_lock);
        *out = stats;
        pthread_mutex_unlock(&stats_lock);
    }
    errno = saved_errno;
}
