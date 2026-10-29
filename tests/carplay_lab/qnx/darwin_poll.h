/* Private QEMU laboratory: retain sub-millisecond Darwin wait deadlines. */
#include <poll.h>
#include <sys/event.h>
#include <limits.h>

static int fps_darwin_poll_ns(GPollFD *fds, guint nfds, int64_t timeout)
{
    struct kevent changes[128], event;
    struct timespec duration;
    unsigned count = 0, i;
    int ready, queue, waited, saved_errno;
    int64_t milliseconds = timeout < 0 ? -1 : timeout / 1000000 + !!(timeout % 1000000);
    int poll_timeout = milliseconds > INT_MAX ? INT_MAX : milliseconds;
    _Static_assert(sizeof(GPollFD) == sizeof(struct pollfd), "pollfd size");
    _Static_assert(offsetof(GPollFD, fd) == offsetof(struct pollfd, fd), "pollfd fd");
    _Static_assert(offsetof(GPollFD, events) == offsetof(struct pollfd, events), "pollfd events");
    _Static_assert(offsetof(GPollFD, revents) == offsetof(struct pollfd, revents), "pollfd revents");
    if (timeout <= 0 || timeout > 1000000 || nfds > 64) {
        return poll((struct pollfd *)fds, nfds, poll_timeout);
    }
    ready = poll((struct pollfd *)fds, nfds, 0);
    if (ready != 0) {
        return ready;
    }
    for (i = 0; i < nfds; i++) {
        if (fds[i].fd < 0) {
            continue;
        }
        /* Urgent/extended poll events retain native poll's exact semantics. */
        if (fds[i].events & ~(POLLIN | POLLOUT | POLLERR | POLLHUP)) {
            return poll((struct pollfd *)fds, nfds, poll_timeout);
        }
        if ((fds[i].events & POLLIN) || !(fds[i].events & POLLOUT)) {
            EV_SET(&changes[count++], fds[i].fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        }
        if (fds[i].events & POLLOUT) {
            EV_SET(&changes[count++], fds[i].fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
        }
    }
    queue = kqueue();
    if (queue < 0) {
        return poll((struct pollfd *)fds, nfds, poll_timeout);
    }
    duration.tv_sec = 0;
    duration.tv_nsec = timeout;
    waited = kevent(queue, changes, count, &event, 1, &duration);
    saved_errno = errno;
    close(queue);
    if (waited < 0) {
        errno = saved_errno;
        return -1;
    }
    if (waited && (event.flags & EV_ERROR)) {
        return poll((struct pollfd *)fds, nfds, poll_timeout);
    }
    /* Native poll supplies all revents, HUP/ERR/NVAL and ready-fd counts. */
    return poll((struct pollfd *)fds, nfds, 0);
}
