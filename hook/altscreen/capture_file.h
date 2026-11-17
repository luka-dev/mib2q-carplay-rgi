/* Size-capped raw capture file: the renderer's cap111 writer
 * (altscreen_render/main.c cap111_*), ported so a hook-side receiver can use it.
 * Never blocks or retries: EAGAIN/EINTR skip one payload, any other error, a
 * short write or the cap ends the capture.  The cap counts the file's real size,
 * so appending across sessions never grows it past the limit.  No logging and
 * no marker handling here; callers own both.  C99, no __thread. */
#ifndef ALTSCREEN_CAPTURE_FILE_H
#define ALTSCREEN_CAPTURE_FILE_H
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    int fd;
    uint32_t written, skipped;
    uint64_t size;      /* fstat size at open plus every byte appended since */
    uint64_t limit;
} alt_capture_t;

static inline void alt_capture_close(alt_capture_t *c)
{
    if (c->fd < 0) return;
    close(c->fd);
    c->fd = -1;
}

/* 0 open, 1 already at the limit (closed), -1 open failed (errno kept),
 * -2 not a regular file (closed).  Never follows a symlink, also where
 * O_NOFOLLOW does not exist (QNX 6.5): an existing path must be a regular file
 * per lstat and the opened file must be that same file; a missing one is created
 * with O_EXCL, which fails on any existing name, a symlink included. */
static inline int alt_capture_open(alt_capture_t *c, const char *path, uint64_t limit)
{
    struct stat st, lst;
    int flags = O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK, existed = 0;
    memset(c, 0, sizeof(*c));
    c->fd = -1;
    c->limit = limit;
    if (!path) { errno = EINVAL; return -1; }
    if (lstat(path, &lst) == 0) {
        if (!S_ISREG(lst.st_mode)) return -2;
        existed = 1;
    } else if (errno == ENOENT) {
        flags |= O_EXCL;
    } else {
        return -1;
    }
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    c->fd = open(path, flags, 0600);     /* map imagery: owner only */
    if (c->fd < 0) return -1;
    if (fstat(c->fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        (existed && (st.st_dev != lst.st_dev || st.st_ino != lst.st_ino))) {
        alt_capture_close(c);
        return -2;
    }
    c->size = (uint64_t)st.st_size;
    if (c->size >= c->limit) {
        alt_capture_close(c);
        return 1;
    }
    return 0;
}

/* 0 written, 1 skipped (EAGAIN/EWOULDBLOCK/EINTR), -1 closed (limit, short
 * write, error, or already closed). */
static inline int alt_capture_append(alt_capture_t *c, const void *p, uint32_t n)
{
    ssize_t w;
    if (c->fd < 0) return -1;
    if (c->size + n > c->limit) {
        alt_capture_close(c);
        return -1;
    }
    w = write(c->fd, p, n);
    if (w == (ssize_t)n) {
        c->written += n;
        c->size += n;
        return 0;
    }
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        ++c->skipped;
        return 1;
    }
    if (w > 0) {
        c->written += (uint32_t)w;
        c->size += (uint64_t)w;
    }
    alt_capture_close(c);
    return -1;
}
#endif
