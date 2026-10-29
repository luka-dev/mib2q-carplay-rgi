#ifndef SI_MEDIA_WATCH_H
#define SI_MEDIA_WATCH_H
#include <stdint.h>
/* Value snapshot only. No target-process/native pointers cross threads. */
struct si_media_target { uint64_t epoch; uint32_t device,session,link,child; };
int si_media_watch(const struct si_media_target *target);
#endif
