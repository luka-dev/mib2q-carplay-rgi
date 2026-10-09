/* Test-phone-only independent verifier using factory QNX OpenSSL libcrypto. */
#include <dlfcn.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int mfiq_verify(const uint8_t *certificate, unsigned certificate_length,
                const uint8_t *digest, unsigned digest_length,
                const uint8_t *signature, unsigned signature_length)
{
    static void *library;
    const unsigned char *cursor = certificate;
    X509 *cert = NULL;
    EVP_PKEY *key = NULL;
    RSA *rsa = NULL;
    int result = -1;
    uint8_t signature_copy[128];
    __typeof__(&d2i_X509) decode;
    __typeof__(&X509_get_pubkey) public_key;
    __typeof__(&EVP_PKEY_get1_RSA) get_rsa;
    __typeof__(&RSA_verify) verify;
    __typeof__(&RSA_size) rsa_size;
    __typeof__(&X509_free) free_cert;
    __typeof__(&EVP_PKEY_free) free_key;
    __typeof__(&RSA_free) free_rsa;
    if (!certificate || !digest || !signature || !certificate_length ||
        certificate_length > 1280 || digest_length != 20 || signature_length != 128) return -1;
    if (!library) library = dlopen("/proc/boot/libcrypto.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!library) return -1;
#define LOAD(destination, symbol) destination = (__typeof__(destination))dlsym(library, #symbol)
    LOAD(decode, d2i_X509); LOAD(public_key, X509_get_pubkey);
    LOAD(get_rsa, EVP_PKEY_get1_RSA); LOAD(verify, RSA_verify); LOAD(rsa_size, RSA_size);
    LOAD(free_cert, X509_free); LOAD(free_key, EVP_PKEY_free); LOAD(free_rsa, RSA_free);
#undef LOAD
    if (!decode || !public_key || !get_rsa || !verify || !rsa_size ||
        !free_cert || !free_key || !free_rsa) return -1;
    cert = decode(NULL, &cursor, certificate_length);
    if (!cert || cursor != certificate + certificate_length) goto done;
    key = public_key(cert);
    if (!key) goto done;
    rsa = get_rsa(key);
    if (!rsa || rsa_size(rsa) != 128) goto done;
    /* The old SDK declares sigbuf mutable. Never pass Rust's shared slice
     * through that API, even though this library normally leaves it untouched. */
    memcpy(signature_copy, signature, sizeof(signature_copy));
    result = verify(NID_sha1, digest, digest_length, signature_copy, signature_length, rsa) == 1;
done:
    if (rsa) free_rsa(rsa);
    if (key) free_key(key);
    if (cert) free_cert(cert);
    return result;
}
