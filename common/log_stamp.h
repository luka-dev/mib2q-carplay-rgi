/* Wall-clock "HH:MM:SS.mmm" prefix, the same format as the hook log and the
 * Java log, so renderer milestones line up with them in one startup timeline. */
#ifndef LOG_STAMP_H
#define LOG_STAMP_H
#include <time.h>
#include <stdio.h>

static inline const char *log_stamp(void) {
    static char buf[16];
    struct timespec ts;
    struct tm tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min,
             tm.tm_sec, (int)(ts.tv_nsec / 1000000));
    return buf;
}
#endif
