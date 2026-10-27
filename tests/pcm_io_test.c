#include "audio/pcm_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t now;
    unsigned waits, writes, cancel_at;
    int ready, wait_error, stop_error, stall_after_write;
    int64_t results[8];
    unsigned result_count;
    size_t total, largest;
    const unsigned char *base;
} mock_t;
static uint64_t now_ms(void *p) { return ((mock_t *)p)->now; }
static int cancelled(void *p)
{
    mock_t *m = p;
    return m->cancel_at && m->waits >= m->cancel_at;
}
static int ready(void *p, unsigned ms)
{
    mock_t *m = p;
    assert(ms && ms <= SPA_PCM_IO_SLICE_MS);
    ++m->waits;
    m->now += ms;
    if (m->stall_after_write && m->writes) return 0;
    return m->wait_error ? m->wait_error : m->ready;
}
static int64_t write_pcm(void *p, const void *data, size_t bytes)
{
    mock_t *m = p;
    int64_t rc;
    assert(data == m->base + m->total);
    if (bytes > m->largest) m->largest = bytes;
    rc = m->writes < m->result_count ? m->results[m->writes] : (int64_t)bytes;
    ++m->writes;
    if (rc > 0 && (uint64_t)rc <= bytes) m->total += (size_t)rc;
    return rc;
}
static const spa_pcm_io_ops_t ops = { now_ms, cancelled, ready, write_pcm };
static unsigned char pcm[120];
static mock_t fresh(void)
{
    mock_t m;
    memset(&m, 0, sizeof(m));
    m.now = 1000;
    m.ready = 1;
    m.base = pcm;
    return m;
}
static int64_t run(mock_t *m)
{
    return spa_pcm_io_write(&ops, m, pcm, sizeof(pcm), 12, 48, &m->stop_error);
}
int main(void)
{
    size_t bytes = 0;
    mock_t m = fresh();
    assert(run(&m) == 120 && m.writes == 3 && m.largest == 48);
    assert(m.stop_error == 0);
    m = fresh(); m.results[0] = 24; m.results[1] = -EIO; m.result_count = 2;
    assert(run(&m) == 24 && m.total == 24); /* commit accepted prefix only */
    assert(m.stop_error == EIO);
    m = fresh(); m.results[0] = -EAGAIN; m.results[1] = 0;
    m.results[2] = -EINTR; m.result_count = 3;
    assert(run(&m) == 120 && m.writes == 6);
    m = fresh(); m.ready = 0;
    assert(run(&m) == -ETIMEDOUT && m.writes == 0 && m.waits == 25);
    assert(m.stop_error == ETIMEDOUT);
    m = fresh(); m.wait_error = -EINTR;
    assert(run(&m) == -ETIMEDOUT && m.waits == 25);
    m = fresh(); m.ready = 0; m.cancel_at = 2;
    assert(run(&m) == -ECANCELED && m.writes == 0 && m.waits == 2);
    m = fresh(); m.cancel_at = 2;
    assert(run(&m) == 48 && m.writes == 1); /* don't write after wake/cancel */
    assert(m.stop_error == ECANCELED);
    m = fresh(); m.results[0] = 24; m.result_count = 1;
    m.cancel_at = 2;
    assert(run(&m) == 24 && m.stop_error == ECANCELED);
    m = fresh(); m.results[0] = 24; m.result_count = 1; m.stall_after_write = 1;
    assert(run(&m) == 24 && m.stop_error == ETIMEDOUT);
    m = fresh(); m.wait_error = -EIO;
    assert(run(&m) == -EIO && m.writes == 0);
    m = fresh(); m.results[0] = 11; m.result_count = 1;
    assert(run(&m) == -EIO); /* malformed device accepted count */
    m = fresh(); m.results[0] = 60; m.result_count = 1;
    assert(run(&m) == -EIO);
    m = fresh(); m.now = 0;
    assert(run(&m) == -EIO && m.writes == 0);
    m = fresh();
    assert(spa_pcm_io_write(&ops, &m, pcm, 1, 12, 48, &m.stop_error) == -EINVAL);
    assert(m.stop_error == EINVAL);
    assert(spa_pcm_io_write(&ops, &m, pcm, 120, 12, 47, NULL) == -EINVAL);
    assert(spa_pcm_io_write(&ops, &m, NULL, 120, 12, 48, NULL) == -EINVAL);
    assert(spa_pcm_io_write(&ops, &m, pcm, 0, 12, 48, &m.stop_error) == 0);
    assert(m.stop_error == 0);
    assert(m.writes == 0);
    assert(spa_pcm_expanded_size(3072, 2, 6, 1048576, &bytes) && bytes == 9216);
    assert(spa_pcm_expanded_size(0, 2, 6, 0, &bytes) && bytes == 0);
    assert(!spa_pcm_expanded_size(3073, 2, 6, 1048576, &bytes));
    assert(!spa_pcm_expanded_size(3072, 2, 6, 9215, &bytes)); /* allocation too small */
    assert(!spa_pcm_expanded_size(3072, 0, 6, 1048576, &bytes));
    assert(!spa_pcm_expanded_size(3072, 2, 9, 1048576, &bytes));
    assert(!spa_pcm_expanded_size(SIZE_MAX - 3, 2, 6, SIZE_MAX, &bytes));
    puts("pcm_io_test: OK (bounded writes, partial commit, cancellation, device errors, geometry)");
    return 0;
}
