#ifndef SQ5_CAPTURE_H
#define SQ5_CAPTURE_H
#include <stddef.h>
void probe_log(const char *fmt, ...);
int capture_init(const char *directory);
void capture_codec(const void *data, size_t size);
void capture_frame(const void *data, size_t size);
void capture_finish(const char *reason);
void capture_join(void);
unsigned capture_nals(const void *data, size_t size);
#endif
