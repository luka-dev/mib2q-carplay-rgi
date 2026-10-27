#define _GNU_SOURCE
#include "stock210_abi.h"
#include "stock210_fingerprint.h"
#include "pairing.h"
#include <dlfcn.h>
#include <errno.h>
static uintptr_t verified_base;

int cp210_callsite(void *caller, uintptr_t offset)
{
    return verified_base && (uintptr_t)caller == verified_base + offset;
}

int cp210_matches(void *create, void *request)
{
    Dl_info info;
    if (sizeof(void *) != 4 || !dladdr(create, &info) || !info.dli_fbase) return 0;
    uintptr_t base = (uintptr_t)info.dli_fbase;
    if ((uintptr_t)create - base != 0x6c324u ||
        (uintptr_t)request - base != 0x302e0u) return 0;
    size_t i;
    for (i = 0; i < sizeof(cp210_fingerprints) / sizeof(cp210_fingerprints[0]); ++i)
        if (memcmp((void *)(base + cp210_fingerprints[i].address),
                   cp210_fingerprints[i].bytes, 16)) return 0;
    verified_base = base;
    return 1;
}

static int read_header(cp_pair_connection *pair, uint8_t *message)
{
    typedef int (*parse_fn)(void *);
    parse_fn parse = (parse_fn)dlsym(RTLD_NEXT, "HTTPHeader_Parse");
    uint8_t *header = message + CP210_MSG_HEADER;
    size_t used = cp210_get(message, CP210_MSG_HEADER_LENGTH), i, got;
    if (!parse || used > 8192) return kSizeErr;
    for (;;) {
        size_t end = 0;
        if (used >= 4 && header[0] == '$') end = 4;
        for (i = 0; !end && i + 1 < used; ++i) {
            if (header[i] == '\n' && header[i + 1] == '\n') end = i + 2;
            else if (i + 2 < used && header[i] == '\n' &&
                     header[i + 1] == '\r' && header[i + 2] == '\n') end = i + 3;
        }
        if (end) {
            cp210_set(message, CP210_MSG_HEADER_LENGTH, (uint32_t)end);
            int err = parse(header);
            if (err) return err;
            cp210_set(message, CP210_MSG_EXTRA_PTR, (uint32_t)(uintptr_t)(header + end));
            cp210_set(message, CP210_MSG_EXTRA_LENGTH, (uint32_t)(used - end));
            return 0;
        }
        if (used == 8192) return kSizeErr;
        size_t extra = cp210_get(message, CP210_MSG_EXTRA_LENGTH);
        if (extra) {
            if (extra > 8192 - used) return kSizeErr;
            memmove(header + used, cp210_ptr(message, CP210_MSG_EXTRA_PTR), extra);
            cp210_set(message, CP210_MSG_EXTRA_LENGTH, 0);
            got = extra;
        } else {
            got = 0;
            int err = cp_pair_read(pair, header + used, 8192 - used, &got);
            if (err) return err;
            if (!got) return kConnectionErr;
        }
        used += got;
        cp210_set(message, CP210_MSG_HEADER_LENGTH, (uint32_t)used);
    }
}

int cp210_read_message(cp_pair_connection *pair, void *opaque)
{
    typedef int (*length_fn)(void *, size_t);
    uint8_t *message = opaque;
    size_t size, used, extra, got = 0;
    int err;
    if (!message[CP210_MSG_HEADER_READ]) {
        err = read_header(pair, message);
        if (err) return err;
        size = cp210_get(message, CP210_MSG_CONTENT_LENGTH);
        if (cp210_get(message, CP210_MSG_CONTENT_LENGTH + 4) ||
            size > cp210_get(message, CP210_MSG_BODY_LIMIT)) return kSizeErr;
        length_fn set_length = (length_fn)dlsym(RTLD_NEXT, "HTTPMessageSetBodyLength");
        if (!set_length) return kBadReferenceErr;
        err = set_length(message, size);
        if (err) return err;
        message[CP210_MSG_HEADER_READ] = 1;
    }
    size = cp210_get(message, CP210_MSG_BODY_LENGTH);
    used = cp210_get(message, CP210_MSG_BODY_OFFSET);
    if (used > size) return kSizeErr;
    extra = cp210_get(message, CP210_MSG_EXTRA_LENGTH);
    if (extra) {
        size_t take = extra < size - used ? extra : size - used;
        uint8_t *p = cp210_ptr(message, CP210_MSG_EXTRA_PTR);
        memmove((uint8_t *)cp210_ptr(message, CP210_MSG_BODY_PTR) + used, p, take);
        used += take;
        cp210_set(message, CP210_MSG_EXTRA_PTR, (uint32_t)(uintptr_t)(p + take));
        cp210_set(message, CP210_MSG_EXTRA_LENGTH, (uint32_t)(extra - take));
        cp210_set(message, CP210_MSG_BODY_OFFSET, (uint32_t)used);
    }
    if (used == size) return 0;
    err = cp_pair_read(pair, (uint8_t *)cp210_ptr(message, CP210_MSG_BODY_PTR) + used, size - used, &got);
    used += got;
    cp210_set(message, CP210_MSG_BODY_OFFSET, (uint32_t)used);
    return err ? err : (used == size ? 0 : EWOULDBLOCK);
}
