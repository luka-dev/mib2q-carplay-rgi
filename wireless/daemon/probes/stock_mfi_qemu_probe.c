/* QEMU-only ABI check for the exact MU1316 stock libairplay MFi exports. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*copy_certificate_fn)(uint8_t **, size_t *);
typedef int (*create_signature_fn)(const uint8_t *, size_t, uint8_t **, size_t *);

int main(void)
{
    static const uint8_t challenge[20] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19
    };
    void *lib = dlopen("/proc/boot/libairplay.stock.so", RTLD_NOW | RTLD_LOCAL);
    copy_certificate_fn copy;
    create_signature_fn sign;
    uint8_t *data = NULL;
    size_t length = 0;
    int result;
    if (!lib) {
        fprintf(stderr, "[stock-mfi-qemu] dlopen failed: %s\n", dlerror());
        return 1;
    }
    copy = (copy_certificate_fn)dlsym(lib, "APSMFiPlatform_CopyCertificate");
    sign = (create_signature_fn)dlsym(lib, "APSMFiPlatform_CreateSignature");
    if (!copy || !sign) {
        fprintf(stderr, "[stock-mfi-qemu] MFi exports absent\n");
        return 2;
    }
    result = copy(&data, &length);
    if (result || !data || length != 4 || memcmp(data, "CERT", 4) != 0) {
        fprintf(stderr, "[stock-mfi-qemu] certificate failed status=%d length=%u\n",
                result, (unsigned)length);
        free(data);
        return 3;
    }
    free(data);
    data = NULL;
    length = 0;
    puts("[stock-mfi-qemu] certificate=4 CERT");
    result = sign(challenge, sizeof(challenge), &data, &length);
    if (result || !data || length != 20) {
        fprintf(stderr, "[stock-mfi-qemu] signature failed status=%d length=%u\n",
                result, (unsigned)length);
        free(data);
        return 4;
    }
    for (size_t index = 0; index < length; ++index) {
        if (data[index] != 0x5a) {
            fprintf(stderr, "[stock-mfi-qemu] unexpected signature data\n");
            free(data);
            return 5;
        }
    }
    free(data);
    puts("[stock-mfi-qemu] signature=20 synthetic");
    /* This short-lived helper leaves the stock library mapped until process
     * exit, as the P5092 reference helper does. */
    return 0;
}
