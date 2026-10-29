/* Numeric stream-111 sender diagnostics using the receiver's own CFLite ABI. */
#ifndef ALT_SENDER_TELEMETRY_H
#define ALT_SENDER_TELEMETRY_H

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALT_SENDER_FIELDS 14
#define ALT_SENDER_PLIST_MAX 16384u

static const char *const alt_sender_fields[ALT_SENDER_FIELDS] = {
    "sentFramesAvg", "queuedFramesAvg", "encoderCurrentFPS", "rttAvg",
    "txUsageAvg", "txCapacityAvg", "lossAvg", "submitSurfaceFPS",
    "beforeEncoderFPS", "sinkOverflowDropFPS", "encoderDropFPS",
    "encoderQueueDropFPS", "idleEncoderFPS", "idleDropFPS"
};

typedef struct {
    void *(*data_create)(void *, const uint8_t *, long);
    void *(*plist_create)(void *, void *, unsigned long, long *, void **);
    void *(*string_create)(void *, const char *, unsigned);
    void *(*dict_get)(void *, const void *);
    unsigned long (*type_id)(const void *);
    unsigned long (*dict_type)(void);
    unsigned long (*number_type)(void);
    unsigned char (*is_float)(void *);
    unsigned char (*number_get)(void *, int, void *);
    void (*release)(void *);
    void *keys[ALT_SENDER_FIELDS];
} alt_sender_parser_t;

typedef struct {
    uint32_t valid;
    double value[ALT_SENDER_FIELDS];
} alt_sender_sample_t;

static inline void alt_sender_parser_close(alt_sender_parser_t *p)
{
    unsigned i;
    if (p->release)
        for (i = 0; i < ALT_SENDER_FIELDS; ++i)
            if (p->keys[i]) p->release(p->keys[i]);
    memset(p, 0, sizeof(*p));
}

static inline int alt_sender_parser_init(alt_sender_parser_t *p, void *scope)
{
    unsigned i;
    memset(p, 0, sizeof(*p));
#define ALT_SENDER_RESOLVE(member, symbol) \
    do { p->member = (__typeof__(p->member))dlsym(scope, symbol); if (!p->member) goto fail; } while (0)
    ALT_SENDER_RESOLVE(data_create, "CFDataCreate");
    ALT_SENDER_RESOLVE(plist_create, "CFPropertyListCreateWithData");
    ALT_SENDER_RESOLVE(string_create, "CFStringCreateWithCString");
    ALT_SENDER_RESOLVE(dict_get, "CFDictionaryGetValue");
    ALT_SENDER_RESOLVE(type_id, "CFGetTypeID");
    ALT_SENDER_RESOLVE(dict_type, "CFDictionaryGetTypeID");
    ALT_SENDER_RESOLVE(number_type, "CFNumberGetTypeID");
    ALT_SENDER_RESOLVE(is_float, "CFLNumberIsFloatType");
    ALT_SENDER_RESOLVE(number_get, "CFNumberGetValue");
    ALT_SENDER_RESOLVE(release, "CFRelease");
#undef ALT_SENDER_RESOLVE
    for (i = 0; i < ALT_SENDER_FIELDS; ++i) {
        p->keys[i] = p->string_create(NULL, alt_sender_fields[i], 0x08000100);
        if (!p->keys[i]) goto fail;
    }
    return 0;
fail:
    alt_sender_parser_close(p);
    return -1;
}

/* No configuration, queue or rate is changed. Missing/invalid values stay
 * absent, distinct from a measured zero; strings and booleans are not numbers.
 * CFPropertyListCreateWithData's five-argument ABI is verified in stock-210
 * libairplay @0x4dcb8 (including the fifth stack argument at entry+24). */
static inline int alt_sender_parse(alt_sender_parser_t *p, const uint8_t *bytes,
                                   uint32_t size, alt_sender_sample_t *out)
{
    void *data, *plist, *error = NULL;
    unsigned i;
    memset(out, 0, sizeof(*out));
    if (!p->data_create || !p->keys[ALT_SENDER_FIELDS - 1] ||
        size < 40 || size > ALT_SENDER_PLIST_MAX ||
        memcmp(bytes, "bplist00", 8)) return -1;
    data = p->data_create(NULL, bytes, (long)size);
    if (!data) return -1;
    plist = p->plist_create(NULL, data, 0, NULL, &error);
    p->release(data);
    if (error) p->release(error);
    if (!plist) return -1;
    if (p->type_id(plist) != p->dict_type()) {
        p->release(plist);
        return -1;
    }
    for (i = 0; i < ALT_SENDER_FIELDS; ++i) {
        void *number = p->dict_get(plist, p->keys[i]);
        double value;
        if (!number || p->type_id(number) != p->number_type()) continue;
        /* Read the stored numeric family first, avoiding implicit CFGetDouble
         * integer conversion through the legacy shared-library EABI runtime.
         * Types 4 and 13 are kCFNumberSInt64Type and kCFNumberDoubleType. */
        if (p->is_float(number)) {
            if (!p->number_get(number, 13, &value)) continue;
        } else {
            int64_t integer;
            if (!p->number_get(number, 4, &integer)) continue;
            value = (double)integer;
        }
        /* Ordered comparisons reject NaN and infinities without the QNX
         * math.h isfinite macro's incompatible long-double runtime helper. */
        if (!(value >= 0 && value <= 1e12)) continue;
        out->value[i] = value;
        out->valid |= UINT32_C(1) << i;
    }
    p->release(plist);
    return 0;
}

static inline void alt_sender_format(const alt_sender_sample_t *s, char *out, size_t cap)
{
    unsigned i;
    size_t used = 0;
    if (!cap) return;
    out[0] = 0;
    for (i = 0; i < ALT_SENDER_FIELDS; ++i) {
        int n;
        if (s->valid & (UINT32_C(1) << i))
            n = snprintf(out + used, cap - used, "%s%s=%.3f", i ? " " : "",
                         alt_sender_fields[i], s->value[i]);
        else
            n = snprintf(out + used, cap - used, "%s%s=NA", i ? " " : "", alt_sender_fields[i]);
        if (n < 0 || (size_t)n >= cap - used) break;
        used += (size_t)n;
    }
}

#endif
