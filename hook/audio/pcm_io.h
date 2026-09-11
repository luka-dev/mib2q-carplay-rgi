/* Bounded nonblocking PCM transfer; callbacks make device faults host-testable. */
#ifndef SPA_PCM_IO_H
#define SPA_PCM_IO_H
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#define SPA_PCM_IO_TIMEOUT_MS 250u
#define SPA_PCM_IO_SLICE_MS 10u
typedef struct {
    uint64_t (*now_ms)(void *);
    int (*cancelled)(void *);
    int (*wait_ready)(void *, unsigned);
    int64_t (*write)(void *, const void *, size_t);
} spa_pcm_io_ops_t;

/* Partial success always wins over an error so the caller commits exactly the
 * accepted prefix. stop_error preserves the positive errno independently of
 * that byte count (zero means full success). wait_ready must actually wait.
 * A single write is <= one device period and MUST be nonblocking. */
static inline int64_t spa_pcm_io_write(const spa_pcm_io_ops_t *ops, void *ctx,
                                       const void *data, size_t bytes,
                                       size_t frame_bytes, size_t period_bytes,
                                       int *stop_error)
{
    size_t done = 0;
    uint64_t start = ops->now_ms(ctx);
    int error = -ETIMEDOUT;
    if (stop_error) *stop_error = 0;
    if (!data || !frame_bytes || !period_bytes || bytes % frame_bytes ||
        period_bytes % frame_bytes) {
        if (stop_error) *stop_error = EINVAL;
        return -EINVAL;
    }
    if (bytes && !start) {
        if (stop_error) *stop_error = EIO;
        return -EIO; /* unavailable monotonic clock: fail closed */
    }
    while (done < bytes) {
        uint64_t elapsed = ops->now_ms(ctx) - start;
        unsigned wait_ms;
        size_t chunk;
        int ready;
        int64_t written;
        if (ops->cancelled(ctx)) { error = -ECANCELED; break; }
        if (elapsed >= SPA_PCM_IO_TIMEOUT_MS) break;
        wait_ms = SPA_PCM_IO_TIMEOUT_MS - (unsigned)elapsed;
        if (wait_ms > SPA_PCM_IO_SLICE_MS) wait_ms = SPA_PCM_IO_SLICE_MS;
        ready = ops->wait_ready(ctx, wait_ms);
        if (ready < 0) {
            if (ready == -EINTR) continue;
            error = ready;
            break;
        }
        if (!ready) continue;
        if (ops->cancelled(ctx)) { error = -ECANCELED; break; }
        if (ops->now_ms(ctx) - start >= SPA_PCM_IO_TIMEOUT_MS) break;
        chunk = bytes - done;
        if (chunk > period_bytes) chunk = period_bytes;
        written = ops->write(ctx, (const uint8_t *)data + done, chunk);
        if (written == 0 || written == -EAGAIN || written == -EWOULDBLOCK ||
            written == -EINTR) continue;
        if (written < 0) { error = (int)written; break; }
        if ((uint64_t)written > chunk || (size_t)written % frame_bytes) {
            error = -EIO;
            break;
        }
        done += (size_t)written;
    }
    if (stop_error && done < bytes) *stop_error = -error;
    return done ? (int64_t)done : (bytes ? error : 0);
}

static inline int spa_pcm_expanded_size(size_t size, unsigned source_channels,
                                        unsigned output_channels, size_t capacity,
                                        size_t *output_bytes)
{
    size_t source_frame, output_frame, frames;
    if (!source_channels || source_channels > 8 ||
        !output_channels || output_channels > 8) return 0;
    source_frame = source_channels * sizeof(int16_t);
    output_frame = output_channels * sizeof(int16_t);
    if (size % source_frame) return 0;
    frames = size / source_frame;
    if (frames > capacity / output_frame) return 0;
    *output_bytes = frames * output_frame;
    return 1;
}
#endif
