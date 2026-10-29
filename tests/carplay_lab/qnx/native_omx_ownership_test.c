/* Regression for the actual fake OMX/Screen double-free, plus bounded
 * registration exhaustion and concurrent repeated buffer lifetimes. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
struct video_omx_prefix {
    uint32_t size, version;
    uint8_t *buffer;
    uint32_t allocated, filled, offset;
};
#include "../../../wireless/receiver_bridge/native_fake_omx.h"

static uint32_t fake_use(void *handle, void **out, uint32_t port, void *context,
                         uint32_t size, uint8_t *bytes)
{
    struct video_omx_prefix *b = calloc(1, sizeof(*b));
    (void)handle; (void)port; (void)context;
    assert(b); b->buffer = bytes; b->allocated = size; *out = b;
    return 0;
}
static uint32_t fake_free(void *handle, uint32_t port, void *header)
{
    struct video_omx_prefix *b = header;
    (void)handle; (void)port;
    /* This is the fake backend's incorrect ownership behavior. */
    if (b) { free(b->buffer); free(b); }
    return 0;
}
static void *cycle(void *handle)
{
    unsigned i;
    for (i = 0; i < 1000; ++i) {
        uint8_t *screen = malloc(64);
        void *header = NULL;
        assert(screen); memset(screen, 0x5a, 64);
        assert(!video_omx_use_buffer_tap(handle, &header, 1, NULL, 64, screen));
        assert(header);
        assert(!video_omx_free_buffer_tap(handle, 1, header));
        assert(screen[0] == 0x5a); /* ASan detects an early fake-OMX free here. */
        screen[63] = 1;
        free(screen); /* Screen remains the sole storage owner. */
    }
    return NULL;
}
int main(void)
{
    void *headers[32]; uint8_t *storage[33]; unsigned i;
    pthread_t threads[4];
    video_omx_use_buffer = fake_use; video_omx_free_buffer = fake_free;
    for (i = 0; i < 33; ++i) {
        storage[i] = malloc(64); assert(storage[i]);
        if (i < 32) assert(!video_omx_use_buffer_tap((void *)1, &headers[i], 1, NULL, 64, storage[i]));
        else {
            void *overflow = (void *)1;
            assert(video_omx_use_buffer_tap((void *)1, &overflow, 1, NULL, 64, storage[i]) == 0x80001000u);
            assert(!overflow); storage[i][0] = 42; free(storage[i]);
        }
    }
    for (i = 0; i < 32; ++i) {
        assert(!video_omx_free_buffer_tap((void *)1, 1, headers[i]));
        storage[i][0] = 42; free(storage[i]);
    }
    /* Separate OMX headers may alias one Screen allocation. */
    {
        void *first = NULL, *second = NULL;
        uint8_t *shared = malloc(64); assert(shared);
        assert(!video_omx_use_buffer_tap((void *)1, &first, 1, NULL, 64, shared));
        assert(!video_omx_use_buffer_tap((void *)2, &second, 1, NULL, 64, shared));
        assert(!video_omx_free_buffer_tap((void *)2, 1, second));
        assert(!video_omx_free_buffer_tap((void *)1, 1, first));
        shared[0] = 42; free(shared);
    }
    /* AllocateBuffer storage is owned by OMX and must still be freed. */
    {
        void *owned = NULL;
        assert(!fake_use((void *)1, &owned, 0, NULL, 64, malloc(64)));
        assert(!video_omx_free_buffer_tap((void *)1, 0, owned));
    }
    for (i = 0; i < 4; ++i) assert(!pthread_create(&threads[i], NULL, cycle, (void *)(uintptr_t)(i + 1)));
    for (i = 0; i < 4; ++i) assert(!pthread_join(threads[i], NULL));
    assert(video_omx_borrowed_count == 0);
    puts("fake OMX ownership: 4000 concurrent lifetimes, overflow and owned storage passed");
    return 0;
}
