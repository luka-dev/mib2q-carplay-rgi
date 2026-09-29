/* Real signer provided by the pinned stock MU1316 library, never a dummy.
 * Both operations acquire/release the QNX I2C bus lock inside stock code. */
#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>

int MFiPlatform_CreateSignature(const void *digest, size_t length, uint8_t **out, size_t *out_len)
{
    typedef int (*sign_fn)(const void *, size_t, uint8_t **, size_t *);
    sign_fn fn = (sign_fn)dlsym(RTLD_NEXT, "APSMFiPlatform_CreateSignature");
    if (!out || !out_len || !digest || length != 20) return EINVAL;
    *out = NULL; *out_len = 0;
    return fn ? fn(digest, length, out, out_len) : ENOSYS;
}

int MFiPlatform_CopyCertificate(uint8_t **out, size_t *out_len)
{
    typedef int (*copy_fn)(uint8_t **, size_t *);
    copy_fn fn = (copy_fn)dlsym(RTLD_NEXT, "APSMFiPlatform_CopyCertificate");
    if (!out || !out_len) return EINVAL;
    *out = NULL; *out_len = 0;
    return fn ? fn(out, out_len) : ENOSYS;
}
