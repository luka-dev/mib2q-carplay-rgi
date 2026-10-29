/* Owned headless OMX test backend only. OMX_UseBuffer borrows its storage;
 * the fake FreeBuffer must release only the header. Callers provide the
 * ABI prefix and the two pinned fake component methods before use. */
#ifndef MHI2Q_NATIVE_FAKE_OMX_H
#define MHI2Q_NATIVE_FAKE_OMX_H
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
static pthread_mutex_t video_omx_borrowed_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t (*video_omx_use_buffer)(void *, void **, uint32_t, void *, uint32_t, uint8_t *);
static uint32_t (*video_omx_free_buffer)(void *, uint32_t, void *);
static struct { void *handle, *header; } video_omx_borrowed[32];
static unsigned video_omx_borrowed_count;
static uint32_t video_omx_use_buffer_tap(void *handle, void **header, uint32_t port,
                                       void *context, uint32_t size, uint8_t *bytes)
{
    uint32_t result = video_omx_use_buffer(handle, header, port, context, size, bytes);
    if (!result && header && *header) {
        pthread_mutex_lock(&video_omx_borrowed_lock);
        if (video_omx_borrowed_count < 32) {
            unsigned at = video_omx_borrowed_count++;
            video_omx_borrowed[at].handle = handle;
            video_omx_borrowed[at].header = *header;
            pthread_mutex_unlock(&video_omx_borrowed_lock);
        } else {
            pthread_mutex_unlock(&video_omx_borrowed_lock);
            /* The fake component owns only this newly allocated header. */
            ((struct video_omx_prefix *)*header)->buffer = NULL;
            video_omx_free_buffer(handle, port, *header);
            *header = NULL;
            return 0x80001000u;
        }
    }
    return result;
}
static uint32_t video_omx_free_buffer_tap(void *handle, uint32_t port, void *header)
{
    unsigned at;
    pthread_mutex_lock(&video_omx_borrowed_lock);
    for (at = 0; at < video_omx_borrowed_count; ++at) {
        if (video_omx_borrowed[at].handle == handle && video_omx_borrowed[at].header == header) {
            /* OMX_UseBuffer borrows Screen's allocation. The pinned fake
             * FreeBuffer unconditionally frees pBuffer: suppress that second
             * owner while still freeing the component's header normally. */
            ((struct video_omx_prefix *)header)->buffer = NULL;
            video_omx_borrowed[at] = video_omx_borrowed[--video_omx_borrowed_count];
            break;
        }
    }
    pthread_mutex_unlock(&video_omx_borrowed_lock);
    return video_omx_free_buffer(handle, port, header);
}

#endif
