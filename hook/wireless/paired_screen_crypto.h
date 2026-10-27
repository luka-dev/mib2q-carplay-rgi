#ifndef MHI2Q_PAIRED_SCREEN_CRYPTO_H
#define MHI2Q_PAIRED_SCREEN_CRYPTO_H
#include <stdint.h>
#include <stddef.h>
#include "../framework/ap_status.h"
typedef int (*cp_screen_open_fn)(const uint8_t *, const uint8_t *, const void *, size_t,
                                const void *, size_t, const uint8_t *, void *);
/* One fresh TCP stream consumes one claimed key; failed streams never resume.
 * The caller uses the same authenticated SDK primitive as paired stream 110.
 */
typedef struct { uint64_t nonce; int failed; } cp_screen_crypto;
static inline int cp_screen_open(cp_screen_crypto *state, const uint8_t key[32],
                                 const uint8_t header[128], uint8_t *body,
                                 uint32_t *length, cp_screen_open_fn open)
{
    uint8_t nonce[8];
    uint32_t wire_length;
    unsigned i;
    int err;
    if (!state) return kParamErr;
    if (state->failed) return kAuthenticationErr;
    if (!key || !header || !body || !length || !open || state->nonce == UINT64_MAX) goto invalid;
    wire_length=(uint32_t)header[0] | (uint32_t)header[1]<<8 |
        (uint32_t)header[2]<<16 | (uint32_t)header[3]<<24;
    if (header[4] != 0 || *length != wire_length || *length <= 16 || *length > 16u*1024u*1024u) goto invalid;
    for (i=0;i<8;++i) nonce[i]=(uint8_t)(state->nonce>>(i*8));
    err=open(key,nonce,header,128,body,*length-16,body+*length-16,body);
    if (err) { state->failed=1; return err; }
    ++state->nonce;
    *length-=16;
    return 0;
invalid:
    state->failed=1;
    return kParamErr;
}
#endif
