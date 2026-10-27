/* Host ASan/UBSan checks for the actual QEMU I2C fixture decoder. */
#include "../wireless/daemon/probes/software_mfi_fixture.h"
#include <assert.h>
#include <stdio.h>

static size_t make_fixture(uint8_t *data)
{
    const char cert[] = "NOT_A_REAL_MFI_IDENTITY";
    const size_t n = sizeof(cert) - 1;
    const uint8_t header[] = {'M','F','I','Q',1,0,0,0,0,(uint8_t)n,0,2};
    memcpy(data, header, sizeof(header));
    memcpy(data + 12, cert, n);
    memset(data + 12 + n, 0x12, 20);
    memset(data + 32 + n, 0x5a, 128);
    memset(data + 160 + n, 0x34, 20);
    memset(data + 180 + n, 0xa5, 128);
    return 12 + n + 296;
}
int main(void)
{
    uint8_t data[MFIQ_MAX_FILE + 1], copy[MFIQ_MAX_FILE + 1];
    struct mfiq_fixture fixture;
    uint8_t digest[20], signature[128];
    size_t i, j, n = make_fixture(data);
    uint32_t random = 17;
    const size_t cert_len = sizeof("NOT_A_REAL_MFI_IDENTITY") - 1;
    assert(mfiq_decode(&fixture, data, n) == 0);
    assert(fixture.case_count == 2 && fixture.certificate_length == cert_len);
    memset(digest, 0x12, 20);
    assert(mfiq_signature(&fixture, digest, signature) == 0 && signature[127] == 0x5a);
    memset(digest, 0x34, 20);
    assert(mfiq_signature(&fixture, digest, signature) == 0 && signature[127] == 0xa5);
    digest[0] ^= 1;
    memset(signature, 0xcc, sizeof(signature));
    assert(mfiq_signature(&fixture, digest, signature) != 0 && signature[127] == 0xcc);
    for (i = 0; i < n; ++i) assert(mfiq_decode(&fixture, data, i) != 0);
    assert(mfiq_decode(&fixture, data, n + 1) != 0);
    assert(mfiq_decode(NULL, data, n) != 0);
    assert(mfiq_decode(&fixture, NULL, n) != 0);
    for (i = 0; i < 12; ++i) {
        memcpy(copy, data, n); copy[i] ^= 0xff;
        assert(mfiq_decode(&fixture, copy, n) != 0);
    }
    memcpy(copy, data, n);
    memset(copy + 12, 'x', cert_len);
    assert(mfiq_decode(&fixture, copy, n) != 0);
    memcpy(copy, data, n);
    memcpy(copy + 160 + cert_len, copy + 12 + cert_len, 20);
    assert(mfiq_decode(&fixture, copy, n) != 0);
    for (i = 0; i < 100000; ++i) {
        size_t size = i % sizeof(data);
        for (j = 0; j < size; ++j) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            copy[j] = (uint8_t)random;
        }
        (void)mfiq_decode(&fixture, copy, size);
    }
    puts("software MFi fixture bounds, lookup and 100000 fuzz inputs passed");
    return 0;
}
