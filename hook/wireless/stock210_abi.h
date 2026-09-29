#ifndef CARPLAY_WIRELESS_STOCK210_ABI_H
#define CARPLAY_WIRELESS_STOCK210_ABI_H
#include <stdint.h>
#include <string.h>

/* Byte offsets, taken from the pinned ARM32 binary, never from 280 headers. */
enum {
    CP210_CNX_FD = 56, CP210_CNX_REQUEST = 196, CP210_CNX_RESPONSE = 200,
    CP210_MSG_HEADER = 16, CP210_MSG_HEADER_LENGTH = 8208,
    CP210_MSG_EXTRA_PTR = 8212, CP210_MSG_EXTRA_LENGTH = 8216,
    CP210_MSG_METHOD_PTR = 8220, CP210_MSG_METHOD_LENGTH = 8224,
    CP210_MSG_PATH_PTR = 8268, CP210_MSG_PATH_LENGTH = 8272,
    CP210_MSG_CONTENT_LENGTH = 8328, CP210_MSG_HEADER_READ = 8344,
    CP210_MSG_BODY_PTR = 8348, CP210_MSG_BODY_LENGTH = 8352,
    CP210_MSG_BODY_LIMIT = 8356, CP210_MSG_BODY_OFFSET = 8360
};
static inline uint32_t cp210_get(const void *p, size_t offset)
{
    uint32_t value;
    memcpy(&value, (const uint8_t *)p + offset, sizeof(value));
    return value;
}
static inline void cp210_set(void *p, size_t offset, uint32_t value)
{
    memcpy((uint8_t *)p + offset, &value, sizeof(value));
}
static inline void *cp210_ptr(const void *p, size_t offset)
{
    return (void *)(uintptr_t)cp210_get(p, offset);
}
int cp210_matches(void *create, void *request);
int cp210_callsite(void *caller, uintptr_t offset);
/* Called only on a known encrypted connection. */
struct cp_pair_connection;
int cp210_read_message(struct cp_pair_connection *pair, void *message);
#endif
