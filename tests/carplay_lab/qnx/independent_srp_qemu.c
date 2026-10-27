/* QEMU-only independent /pair-setup M1-M6 and /pair-verify M1-M4 client.
 * Implements SRP and protocol framing itself; uses QNX OpenSSL for hashes,
 * and calls only the receiver library's raw Ed25519/ChaCha primitives.
 * Fixed ephemeral secret is acceptable only for this isolated test image.
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/bn.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define N_BYTES 384
#define MAX_BODY 8192
#define MAX_TLV_TYPES 16

static const char group_hex[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74"
    "020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437"
    "4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF05"
    "98DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB"
    "9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183"
    "995497CEA956AE515D2261898FA051015728E5A8AAAC42DAD33170D04507A33A"
    "85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7AB"
    "F5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6BF12FFA06D98A0864D8"
    "7602733EC86A64521F2B18177B200CBBE117577A615D6C770988C0BAD946E208"
    "E24FA074E5AB3143DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF";

struct tlv_value {
    uint8_t data[512];
    size_t length;
};

static int write_all(int fd, const void *buffer, size_t length)
{
    const uint8_t *p = buffer;
    while (length) {
        ssize_t count = write(fd, p, length);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        p += count;
        length -= (size_t)count;
    }
    return 0;
}

static int read_all(int fd, void *buffer, size_t length)
{
    uint8_t *p = buffer;
    while (length) {
        ssize_t count = read(fd, p, length);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
#ifdef MHI2Q_REPEAT_MEDIA_QEMU
            char line[112];
            int used = snprintf(line, sizeof(line),
                "[independent-srp-qemu] read failed fd=%d count=%ld errno=%d remaining=%zu\n",
                fd, (long)count, count < 0 ? errno : 0, length);
            if (used > 0 && (size_t)used < sizeof(line))
                (void)write(2, line, (size_t)used);
#endif
            return -1;
        }
        p += count;
        length -= (size_t)count;
    }
    return 0;
}

static void sha_parts(uint8_t out[64], const void *a, size_t alen,
                      const void *b, size_t blen, const void *c, size_t clen)
{
    SHA512_CTX hash;
    SHA512_Init(&hash);
    if (alen) SHA512_Update(&hash, a, alen);
    if (blen) SHA512_Update(&hash, b, blen);
    if (clen) SHA512_Update(&hash, c, clen);
    SHA512_Final(out, &hash);
}

static int padded_bn(const BIGNUM *number, uint8_t out[N_BYTES])
{
    int length = BN_num_bytes(number);
    if (length < 0 || length > N_BYTES) return -1;
    memset(out, 0, N_BYTES);
    BN_bn2bin(number, out + N_BYTES - length);
    return 0;
}

static int append_tlv(uint8_t *buffer, size_t capacity, size_t *used,
                      uint8_t type, const void *value, size_t length)
{
    const uint8_t *p = value;
    do {
        size_t chunk = length > 255 ? 255 : length;
        if (*used + 2 + chunk > capacity) return -1;
        buffer[(*used)++] = type;
        buffer[(*used)++] = (uint8_t)chunk;
        if (chunk) memcpy(buffer + *used, p, chunk);
        *used += chunk;
        p += chunk;
        length -= chunk;
    } while (length);
    return 0;
}

static int parse_tlv(const uint8_t *body, size_t length,
                     struct tlv_value values[MAX_TLV_TYPES])
{
    size_t offset = 0;
    memset(values, 0, MAX_TLV_TYPES * sizeof(*values));
    while (offset < length) {
        uint8_t type, size;
        if (length - offset < 2) return -1;
        type = body[offset++];
        size = body[offset++];
        if (type >= MAX_TLV_TYPES || size > length - offset ||
            values[type].length + size > sizeof(values[type].data)) return -1;
        memcpy(values[type].data + values[type].length, body + offset, size);
        values[type].length += size;
        offset += size;
    }
    return 0;
}

static int post_pairing(int fd, const char *path, const uint8_t *body, size_t body_length,
                        struct tlv_value values[MAX_TLV_TYPES])
{
    char request[256];
    uint8_t response[MAX_BODY];
    size_t used = 0, header_length = 0, content_length = 0, i;
    int request_length = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\nHost: 127.0.0.1\r\nX-Apple-HKP: 0\r\n"
        "Content-Type: application/pairing+tlv8\r\nContent-Length: %zu\r\n"
        "Connection: keep-alive\r\n\r\n", path, body_length);
    if (request_length < 0 || (size_t)request_length >= sizeof(request) ||
        write_all(fd, request, (size_t)request_length) ||
        write_all(fd, body, body_length)) return -1;
    while (used < sizeof(response) - 1) {
        ssize_t count = read(fd, response + used, sizeof(response) - 1 - used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -2;
        used += (size_t)count;
        if (!header_length) {
            for (i = 0; i + 3 < used; ++i) {
                if (memcmp(response + i, "\r\n\r\n", 4) == 0) {
                    header_length = i + 4;
                    break;
                }
            }
            if (header_length) {
                char *field;
                uint8_t saved = response[header_length];
                response[header_length] = 0;
                if (strncmp((char *)response, "HTTP/1.1 200", 12) ||
                    !strstr((char *)response, "Content-Type: application/pairing+tlv8")) {
                    fprintf(stderr, "[independent-srp-qemu] %s response %.40s\n", path, response);
                    return -3;
                }
                field = strstr((char *)response, "Content-Length: ");
                if (!field || sscanf(field, "Content-Length: %zu", &content_length) != 1 ||
                    content_length > sizeof(response) - header_length) return -4;
                response[header_length] = saved;
            }
        }
        if (header_length && used >= header_length + content_length) {
            int status = parse_tlv(response + header_length, content_length, values);
            if (!status && values[7].length)
                fprintf(stderr, "[independent-srp-qemu] %s TLV error=%u state=%u\n",
                        path, values[7].data[0], values[6].length ? values[6].data[0] : 0);
            return status;
        }
    }
    return -5;
}

static int open_receiver(void)
{
    struct sockaddr_in address;
#ifdef MHI2Q_PAIRING_HOOK_QEMU
    struct timeval timeout = { 30, 0 }; /* CPU-loaded QEMU: SRP is software ARM. */
#else
    struct timeval timeout = { 5, 0 };
#endif
    int attempt;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#ifndef MHI2Q_PAIRING_TEST_PORT
#define MHI2Q_PAIRING_TEST_PORT 7000
#endif
    address.sin_port = htons(MHI2Q_PAIRING_TEST_PORT);
    for (attempt = 0; attempt < 200; ++attempt) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0) return fd;
        close(fd);
        usleep(200000);
    }
    return -1;
}

static int verify_srp(const struct tlv_value values[MAX_TLV_TYPES],
                      uint8_t client_a[N_BYTES], uint8_t proof[64],
                      uint8_t expected_server[64], uint8_t shared[64])
{
    static const uint8_t secret[32] = {
        0x73, 0x1a, 0x96, 0xa4, 0x42, 0xb9, 0x33, 0x0c,
        0xa1, 0x68, 0x5d, 0xde, 0xc4, 0x77, 0x0a, 0x31,
        0xa5, 0xeb, 0x82, 0x4c, 0x97, 0x15, 0x36, 0x0f,
        0x48, 0xdb, 0xe1, 0x20, 0x6f, 0x39, 0x54, 0x7a
    };
    static const char username[] = "Pair-Setup";
    static const char credentials[] = "Pair-Setup:3939";
    BN_CTX *ctx = BN_CTX_new();
    BIGNUM *N = NULL, *g = BN_new(), *a = BN_bin2bn(secret, sizeof(secret), NULL);
    BIGNUM *A = BN_new(), *B = NULL, *u = NULL, *k = NULL, *x = NULL;
    BIGNUM *gx = BN_new(), *kgx = BN_new(), *base = BN_new();
    BIGNUM *ux = BN_new(), *exponent = BN_new(), *S = BN_new();
    uint8_t padded_g[N_BYTES], padded_b[N_BYTES], hN[64], hg[64], hI[64];
    uint8_t inner[64], xhash[64], digest[64], xor_hash[64];
    uint8_t sbytes[N_BYTES];
    SHA512_CTX hash;
    int slen, i, status = -1;
    if (!ctx || !g || !a || !A || !gx || !kgx || !base || !ux ||
        !exponent || !S || values[2].length != 16 || values[3].length != N_BYTES ||
        !BN_hex2bn(&N, group_hex) || !BN_set_word(g, 5)) goto exit;
    B = BN_bin2bn(values[3].data, N_BYTES, NULL);
    if (!B || BN_is_zero(B) || BN_cmp(B, N) >= 0 ||
        !BN_mod_exp(A, g, a, N, ctx) || padded_bn(A, client_a) ||
        padded_bn(g, padded_g) || padded_bn(B, padded_b)) goto exit;
    sha_parts(digest, client_a, N_BYTES, padded_b, N_BYTES, NULL, 0);
    u = BN_bin2bn(digest, sizeof(digest), NULL);
    {
        uint8_t nbytes[N_BYTES];
        if (padded_bn(N, nbytes)) goto exit;
        sha_parts(digest, nbytes, N_BYTES, padded_g, N_BYTES, NULL, 0);
        sha_parts(hN, nbytes, N_BYTES, NULL, 0, NULL, 0);
    }
    k = BN_bin2bn(digest, sizeof(digest), NULL);
    sha_parts(inner, credentials, sizeof(credentials) - 1, NULL, 0, NULL, 0);
    sha_parts(xhash, values[2].data, values[2].length,
              inner, sizeof(inner), NULL, 0);
    x = BN_bin2bn(xhash, sizeof(xhash), NULL);
    if (!u || !k || !x || !BN_mod_exp(gx, g, x, N, ctx) ||
        !BN_mod_mul(kgx, k, gx, N, ctx) ||
        !BN_mod_sub(base, B, kgx, N, ctx) ||
        !BN_mul(ux, u, x, ctx) || !BN_add(exponent, a, ux) ||
        !BN_mod_exp(S, base, exponent, N, ctx)) goto exit;
    slen = BN_num_bytes(S);
    if (slen <= 0 || slen > N_BYTES) goto exit;
    BN_bn2bin(S, sbytes);
    sha_parts(shared, sbytes, (size_t)slen, NULL, 0, NULL, 0);
    sha_parts(hg, (const uint8_t *)"\x05", 1, NULL, 0, NULL, 0);
    sha_parts(hI, username, sizeof(username) - 1, NULL, 0, NULL, 0);
    for (i = 0; i < 64; ++i) xor_hash[i] = hN[i] ^ hg[i];
    SHA512_Init(&hash);
    SHA512_Update(&hash, xor_hash, sizeof(xor_hash));
    SHA512_Update(&hash, hI, sizeof(hI));
    SHA512_Update(&hash, values[2].data, values[2].length);
    SHA512_Update(&hash, client_a, N_BYTES);
    SHA512_Update(&hash, padded_b, N_BYTES);
    SHA512_Update(&hash, shared, 64);
    SHA512_Final(proof, &hash);
    sha_parts(expected_server, client_a, N_BYTES, proof, 64, shared, 64);
    status = 0;
exit:
    BN_free(N); BN_free(g); BN_free(a); BN_free(A); BN_free(B);
    BN_free(u); BN_free(k); BN_free(x); BN_free(gx); BN_free(kgx);
    BN_free(base); BN_free(ux); BN_free(exponent); BN_free(S);
    BN_CTX_free(ctx);
    return status;
}

static int hkdf32(const uint8_t *secret, size_t secret_length, const char *salt,
                  const char *info, uint8_t out[32])
{
    uint8_t prk[64], digest[64], message[128];
    unsigned int length = 0;
    size_t info_length = strlen(info);
    if (info_length + 1 > sizeof(message)) return -1;
    if (!HMAC(EVP_sha512(), salt, (int)strlen(salt), secret, secret_length,
              prk, &length) || length != 64) return -1;
    memcpy(message, info, info_length);
    message[info_length] = 1;
    if (!HMAC(EVP_sha512(), prk, 64, message, info_length + 1,
              digest, &length) || length != 64) return -1;
    memcpy(out, digest, 32);
    return 0;
}

typedef void (*ed_make_fn)(uint8_t[32], uint8_t[32]);
typedef void (*ed_sign_fn)(uint8_t[64], const void *, size_t,
                           const uint8_t[32], const uint8_t[32]);
typedef int (*ed_verify_fn)(const void *, size_t,
                            const uint8_t[64], const uint8_t[32]);
typedef void (*seal_fn)(const uint8_t[32], const uint8_t[8],
                        const void *, size_t, const void *, size_t,
                        void *, uint8_t[16]);
typedef int (*open_fn)(const uint8_t[32], const uint8_t[8],
                       const void *, size_t, const void *, size_t,
                       void *, const uint8_t[16]);
typedef void (*curve_fn)(uint8_t[32], const uint8_t[32], const uint8_t[32]);

static const char controller_id[] = "qemu-independent";
struct pairing_identity {
    uint8_t controller_pk[32], controller_sk[32], accessory_pk[32];
    uint8_t accessory_id[512];
    size_t accessory_id_length;
};

#ifdef MHI2Q_PAIRING_HOOK_QEMU
static int hook_bad_signature;
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
static int hook_media_run;
#endif
#define HOOK_TRACE(s) ((void)write_all(2, s, sizeof(s) - 1))

/* Consume two independently authenticated HTTP responses. The two requests
 * share one incoming plaintext record, exercising stock Reset's extra bytes. */
#ifndef MHI2Q_WIRELESS_MEDIA_QEMU
static int hook_control_regression(int fd, const uint8_t shared[32], seal_fn seal,
                                   open_fn open_sealed, unsigned read_counter)
{
    static const char requests[] =
        "GET /info HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\nabc"
        "GET /info HTTP/1.1\r\nHost: localhost\r\n\r\n";
    uint8_t write_key[32], read_key[32], nonce[8] = {3}, frame[512], response[8192];
    size_t length = sizeof(requests) - 1, used = 0;
    unsigned complete = 0, iteration;
    if (hkdf32(shared, 32, "Control-Salt", "Control-Write-Encryption-Key", write_key) ||
        hkdf32(shared, 32, "Control-Salt", "Control-Read-Encryption-Key", read_key)) return -1;
    frame[0] = (uint8_t)length; frame[1] = (uint8_t)(length >> 8);
    seal(write_key, nonce, frame, 2, requests, length, frame + 2, frame + 2 + length);
    if (write_all(fd, frame, length + 18)) return -2;
    for (iteration = 0; iteration < 16 && complete < 2; ++iteration) {
        uint8_t header[2], cipher[1024], tag[16];
        size_t size, i;
        if (read_all(fd, header, 2)) return -3;
        size = header[0] | ((size_t)header[1] << 8);
        if (size > sizeof(cipher) || used + size >= sizeof(response) ||
            read_all(fd, cipher, size) || read_all(fd, tag, 16)) return -4;
        memset(nonce, 0, sizeof(nonce)); nonce[0] = (uint8_t)read_counter++;
        if (open_sealed(read_key, nonce, header, 2, cipher, size, response + used, tag)) return -5;
        used += size; response[used] = 0;
        for (;;) {
            size_t end = 0, body = 0;
            char *field;
            for (i = 0; i + 3 < used; ++i)
                if (!memcmp(response + i, "\r\n\r\n", 4)) { end = i + 4; break; }
            if (!end) break;
            /* Invalid plist body deliberately gets 400; the following clean
             * request must still parse and return 200 on the same socket. */
            if (strncmp((char *)response, complete ? "HTTP/1.1 200" : "HTTP/1.1 400", 12)) return -6;
            field = strstr((char *)response, "Content-Length: ");
            if (!field || field >= (char *)response + end ||
                sscanf(field, "Content-Length: %zu", &body) != 1) return -7;
            if (body > sizeof(response) - end) return -8;
            if (used < end + body) break;
            used -= end + body;
            memmove(response, response + end + body, used);
            response[used] = 0;
            ++complete;
        }
    }
    if (complete != 2 || used) return -9;
    /* Correct next nonce, deliberately invalid tag: no plaintext fallback. */
    memset(nonce, 0, sizeof(nonce)); nonce[0] = 4;
    seal(write_key, nonce, frame, 2, requests, length, frame + 2, frame + 2 + length);
    frame[2 + length] ^= 1;
    if (write_all(fd, frame, length + 18)) return -10;
    {
        ssize_t n = read(fd, response, sizeof(response));
        if (n != 0 && !(n < 0 && errno == ECONNRESET)) return -11;
    }
    HOOK_TRACE("[independent-srp-qemu] pipelined body and bad-tag rejection verified\n");
    return 0;
}
#endif
#endif

#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
#include "stock210_lifecycle_phone_qemu.c"
#endif

static int probe_encrypted_info(int fd, const uint8_t shared[32],
                                seal_fn seal, open_fn open_sealed,
                                unsigned *next_read_counter)
{
    static const char request[] = "GET /info HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
    uint8_t write_key[32], read_key[32], nonce[8] = {0};
    uint8_t frame[2 + sizeof(request) - 1 + 16], response[4096];
    size_t used = 0, header_length = 0, expected_length = 0, index;
    int round;
    if (hkdf32(shared, 32, "Control-Salt", "Control-Write-Encryption-Key",
               write_key) ||
        hkdf32(shared, 32, "Control-Salt", "Control-Read-Encryption-Key",
               read_key)) return -1;
    frame[0] = (uint8_t)((sizeof(request) - 1) & 255);
    frame[1] = (uint8_t)((sizeof(request) - 1) >> 8);
    seal(write_key, nonce, frame, 2, request, sizeof(request) - 1,
         frame + 2, frame + 2 + sizeof(request) - 1);
#if defined(MHI2Q_PAIRING_HOOK_QEMU) && !defined(MHI2Q_WIRELESS_MEDIA_QEMU)
    {
        size_t offset = 0;
        unsigned part;
        for (part = 0; part < 3; ++part) {
            size_t n = part == 2 ? sizeof(request) - 1 - offset : 9;
            nonce[0] = (uint8_t)part;
            frame[0] = (uint8_t)n; frame[1] = 0;
            seal(write_key, nonce, frame, 2, request + offset, n, frame + 2, frame + 2 + n);
            /* Separate TCP deliveries split the length field, ciphertext and tag. */
            if (write_all(fd, frame, 1)) return -2;
            usleep(20000);
            if (write_all(fd, frame + 1, n + 17)) return -2;
            offset += n;
        }
    }
#else
    if (write_all(fd, frame, sizeof(frame))) return -2;
#endif
    for (round = 0; round < 4; ++round) {
        uint8_t header[2], ciphertext[1024], tag[16];
        size_t frame_length;
        if (read_all(fd, header, sizeof(header))) return -3;
        frame_length = (size_t)header[0] | ((size_t)header[1] << 8);
        if (frame_length > sizeof(ciphertext) ||
            used + frame_length >= sizeof(response)) return -4;
        if (read_all(fd, ciphertext, frame_length) ||
            read_all(fd, tag, sizeof(tag))) return -5;
        nonce[0] = (uint8_t)round;
        if (open_sealed(read_key, nonce, header, sizeof(header), ciphertext,
                        frame_length, response + used, tag)) return -6;
        used += frame_length;
        if (!header_length) {
            for (index = 0; index + 3 < used; ++index) {
                if (memcmp(response + index, "\r\n\r\n", 4) == 0) {
                    header_length = index + 4;
                    break;
                }
            }
            if (header_length) {
                char *field;
                uint8_t saved = response[header_length];
                response[header_length] = 0;
                if (strncmp((char *)response, "HTTP/1.1 200", 12)) return -7;
                field = strstr((char *)response, "Content-Length: ");
                if (!field || sscanf(field, "Content-Length: %zu",
                                     &expected_length) != 1) return -8;
                response[header_length] = saved;
            }
        }
        if (header_length && used >= header_length + expected_length &&
            expected_length > 0) {
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
            lifecycle_dump_info(response+header_length,expected_length);
#endif
            *next_read_counter = (unsigned)round + 1;
            return 0;
        }
    }
    return -9;
}

#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
#include "stock210_media_phone_qemu.c"
#endif

#ifdef MHI2Q_INDEPENDENT_CONTROL_SETUP
#ifdef MHI2Q_INDEPENDENT_SCREEN_SETUP
struct timing_responder {
    int fd;
    volatile unsigned stop;
    volatile unsigned responses;
};

static void *respond_timing(void *argument)
{
    struct timing_responder *responder = argument;
    while (!__sync_val_compare_and_swap(&responder->stop, 0, 0)) {
        uint8_t packet[32];
        struct sockaddr_in peer;
        socklen_t peer_length = sizeof(peer);
        ssize_t count = recvfrom(responder->fd, packet, sizeof(packet), 0,
                                 (struct sockaddr *)&peer, &peer_length);
        if (count != (ssize_t)sizeof(packet) || packet[0] != 0x80 ||
            packet[1] != 210) continue;
        packet[1] = 211;
        memcpy(packet + 8, packet + 24, 8);
        memcpy(packet + 16, packet + 24, 8);
        if (sendto(responder->fd, packet, sizeof(packet), 0,
                   (struct sockaddr *)&peer, peer_length) == sizeof(packet))
            __sync_add_and_fetch(&responder->responses, 1);
    }
    return NULL;
}

static int start_timing_responder(struct timing_responder *responder,
                                  pthread_t *thread)
{
    struct sockaddr_in address;
    struct timeval timeout = { 0, 200000 };
    memset(responder, 0, sizeof(*responder));
    responder->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (responder->fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(6002);
    (void)setsockopt(responder->fd, SOL_SOCKET, SO_RCVTIMEO,
                     &timeout, sizeof(timeout));
    if (bind(responder->fd, (struct sockaddr *)&address, sizeof(address)) ||
        pthread_create(thread, NULL, respond_timing, responder)) {
        close(responder->fd);
        return -1;
    }
    return 0;
}

#ifdef MHI2Q_SCREEN_DATA_QEMU
/* Minimal binary-plist reader for the SETUP response's streams[0].dataPort.
 * It intentionally accepts only small ASCII-key dictionaries and arrays. */
static uint64_t plist_be(const uint8_t *data, size_t width)
{
    uint64_t value = 0;
    size_t i;
    for (i = 0; i < width; ++i) value = (value << 8) | data[i];
    return value;
}

struct plist_view {
    const uint8_t *data;
    size_t length, table, objects, offset_width, ref_width;
};

static int plist_object(const struct plist_view *view, size_t reference,
                        const uint8_t **object)
{
    size_t position;
    uint64_t offset;
    if (reference >= view->objects || reference > (SIZE_MAX - view->table) /
        view->offset_width) return -1;
    position = view->table + reference * view->offset_width;
    if (position > view->length || view->offset_width > view->length - position)
        return -1;
    offset = plist_be(view->data + position, view->offset_width);
    if (offset >= view->length - 32) return -1;
    *object = view->data + (size_t)offset;
    return 0;
}

static int plist_dict_get(const struct plist_view *view, size_t reference,
                          const char *name, size_t *value_reference)
{
    const uint8_t *dict, *key;
    size_t count, offset, i, key_reference;
    if (plist_object(view, reference, &dict) || (*dict >> 4) != 0xd ||
        (*dict & 15) == 15) return -1;
    count = *dict & 15;
    offset = (size_t)(dict - view->data);
    if (offset + 1 + 2 * count * view->ref_width > view->length - 32)
        return -1;
    for (i = 0; i < count; ++i) {
        key_reference = (size_t)plist_be(dict + 1 + i * view->ref_width,
                                         view->ref_width);
        if (plist_object(view, key_reference, &key) || (*key >> 4) != 5 ||
            (*key & 15) != strlen(name) ||
            (size_t)(key - view->data) + 1 + strlen(name) > view->length - 32)
            continue;
        if (memcmp(key + 1, name, strlen(name)) == 0) {
            *value_reference = (size_t)plist_be(
                dict + 1 + (count + i) * view->ref_width, view->ref_width);
            return 0;
        }
    }
    return -1;
}

static int stream_data_port(const uint8_t *data, size_t length,
                            size_t stream_index)
{
    struct plist_view view;
    const uint8_t *array, *number;
    size_t streams_ref, stream_ref, port_ref, top;
    size_t width;
    uint64_t port;
    if (length < 40 || memcmp(data, "bplist00", 8)) return -1;
    view.data = data;
    view.length = length;
    view.offset_width = data[length - 26];
    view.ref_width = data[length - 25];
    view.objects = (size_t)plist_be(data + length - 24, 8);
    top = (size_t)plist_be(data + length - 16, 8);
    view.table = (size_t)plist_be(data + length - 8, 8);
    if (!view.offset_width || view.offset_width > 8 || !view.ref_width ||
        view.ref_width > 8 || view.table >= length - 32 ||
        plist_dict_get(&view, top, "streams", &streams_ref) ||
        plist_object(&view, streams_ref, &array) || (*array >> 4) != 0xa ||
        (*array & 15) == 0 || (*array & 15) == 15 ||
        stream_index >= (*array & 15) ||
        (size_t)(array - data) + 1 + (stream_index + 1) * view.ref_width >
            length - 32)
        return -1;
    stream_ref = (size_t)plist_be(array + 1 + stream_index * view.ref_width,
                                  view.ref_width);
    if (plist_dict_get(&view, stream_ref, "dataPort", &port_ref) ||
        plist_object(&view, port_ref, &number) || (*number >> 4) != 1 ||
        (*number & 15) > 3) return -1;
    width = (size_t)1 << (*number & 15);
    if ((size_t)(number - data) + 1 + width > length - 32) return -1;
    port = plist_be(number + 1, width);
    return port > 0 && port <= 65535 ? (int)port : -1;
}

static int screen_event_port(const uint8_t *data, size_t length)
{
    struct plist_view view;
    const uint8_t *number;
    size_t top, port_ref, width;
    uint64_t port;
    if (length < 40 || memcmp(data, "bplist00", 8)) return -1;
    view.data = data;
    view.length = length;
    view.offset_width = data[length - 26];
    view.ref_width = data[length - 25];
    view.objects = (size_t)plist_be(data + length - 24, 8);
    top = (size_t)plist_be(data + length - 16, 8);
    view.table = (size_t)plist_be(data + length - 8, 8);
    if (!view.offset_width || view.offset_width > 8 || !view.ref_width ||
        view.ref_width > 8 || view.table >= length - 32 ||
        plist_dict_get(&view, top, "eventPort", &port_ref) ||
        plist_object(&view, port_ref, &number) || (*number >> 4) != 1 ||
        (*number & 15) > 3) return -1;
    width = (size_t)1 << (*number & 15);
    if ((size_t)(number - data) + 1 + width > length - 32) return -1;
    port = plist_be(number + 1, width);
    return port > 0 && port <= 65535 ? (int)port : -1;
}

static int open_screen_port(int port)
{
    struct sockaddr_in address;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)port);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address))) {
        close(fd);
        return -1;
    }
    return fd;
}

#ifdef MHI2Q_SCREEN_AUDIO_QEMU
static int probe_audio_packets(const uint8_t *plist, size_t length,
                               const uint8_t shared[32], seal_fn seal)
{
    enum { samples_per_packet = 352, packet_count = 80 };
    struct sockaddr_in address;
    uint8_t pcm_be[samples_per_packet * 4];
    uint8_t packet[12 + sizeof(pcm_be) + 16 + 8];
    uint8_t key[32];
    int port = stream_data_port(plist, length, 1);
    int fd, size, i;
    char marker[112];
    if (port < 0 || hkdf32(shared, 32, "DataStream-Salt2",
                          "DataStream-Output-Encryption-Key", key)) return -1;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -2;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)port);
    for (i = 0; i < (int)sizeof(pcm_be); i += 2) {
        pcm_be[i] = 0x20;
        pcm_be[i + 1] = 0x00;
    }
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
    for (i = -2; i < packet_count; ++i) {
#else
    for (i = 0; i < packet_count; ++i) {
#endif
        unsigned seq = (unsigned)i + 1;
        unsigned timestamp = (unsigned)i * samples_per_packet;
        uint8_t nonce[8] = {0};
        memset(packet, 0, sizeof(packet));
        packet[0] = 0x80; /* RTP v2. */
        packet[1] = 100;  /* MainAudio payload type. */
        packet[2] = (uint8_t)(seq >> 8);
        packet[3] = (uint8_t)seq;
        packet[4] = (uint8_t)(timestamp >> 24);
        packet[5] = (uint8_t)(timestamp >> 16);
        packet[6] = (uint8_t)(timestamp >> 8);
        packet[7] = (uint8_t)timestamp;
        packet[11] = 1; /* SSRC. */
        nonce[0] = (uint8_t)i;
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
        seal(key, nonce, packet + 4, 8, pcm_be, sizeof(pcm_be),
#else
        seal(key, nonce, packet, 12, pcm_be, sizeof(pcm_be),
#endif
             packet + 12, packet + 12 + sizeof(pcm_be));
        memcpy(packet + sizeof(packet) - sizeof(nonce), nonce, sizeof(nonce));
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
        if (i == -2) packet[12 + sizeof(pcm_be)] ^= 1; /* invalid tag */
        if (i == -1) packet[4] ^= 1; /* invalid timestamp AAD */
#endif
        if (sendto(fd, packet, sizeof(packet), 0,
                   (struct sockaddr *)&address, sizeof(address)) !=
            (ssize_t)sizeof(packet)) {
            close(fd);
            return -3;
        }
        usleep(8000);
    }
    close(fd);
    usleep(150000);
    size = snprintf(marker, sizeof(marker),
                    "[independent-srp-qemu] encrypted PCM RTP packets=%d port=%d\n",
                    packet_count, port);
    if (size > 0 && (size_t)size < sizeof(marker))
        (void)write_all(1, marker, (size_t)size);
    return port;
}
#endif

static int probe_screen_data(const uint8_t *plist, size_t length, int *out_fd,
                             const uint8_t shared[32], seal_fn seal)
{
    uint8_t keepalive[128] = {0};
    int port = stream_data_port(plist, length, 0);
    int fd;
    if (port < 0) return -1;
    fd = open_screen_port(port);
    if (fd < 0) return -2;
    keepalive[4] = 2; /* kAirPlayScreenOpCode_KeepAlive; bodySize = 0. */
    if (write_all(fd, keepalive, sizeof(keepalive))) {
        close(fd);
        return -4;
    }
#ifdef MHI2Q_SCREEN_FRAME_QEMU
    {
        static const uint8_t avcc[] = {
            0x01,0x64,0xc0,0x28,0xff,0xe1,0x00,0x11,0x67,0x64,0xc0,0x28,
            0xac,0x56,0x20,0x08,0xc1,0xef,0x9e,0xe6,0xe0,0x40,0x40,0x40,
            0x40,0x01,0x00,0x05,0x28,0xee,0x05,0x72,0xc0
        };
        static const uint8_t avcc_frame[] = {
            0x00,0x00,0x00,0x04,0x65,0x88,0x84,0x21
        };
        uint8_t header[128] = {0};
        uint8_t key[32], nonce[8] = {0};
        uint8_t ciphertext[sizeof(avcc_frame)], tag[16];
        uint32_t body_size = sizeof(avcc);
        float width = 800.0f, height = 480.0f;
        memcpy(header, &body_size, 4);
        header[4] = 1; /* VideoConfig. */
        memcpy(header + 16, &width, sizeof(width));
        memcpy(header + 20, &height, sizeof(height));
        memcpy(header + 40, &width, sizeof(width));
        memcpy(header + 44, &height, sizeof(height));
        if (write_all(fd, header, sizeof(header)) ||
            write_all(fd, avcc, sizeof(avcc))) {
            close(fd);
            return -5;
        }
        if (hkdf32(shared, 32, "DataStream-Salt1",
                   "DataStream-Output-Encryption-Key", key)) {
            close(fd);
            return -6;
        }
        memset(header, 0, sizeof(header));
        body_size = sizeof(avcc_frame) + sizeof(tag);
        memcpy(header, &body_size, 4);
        header[4] = 0; /* VideoFrame. */
        seal(key, nonce, header, sizeof(header), avcc_frame, sizeof(avcc_frame),
             ciphertext, tag);
        if (write_all(fd, header, sizeof(header)) ||
            write_all(fd, ciphertext, sizeof(ciphertext)) ||
            write_all(fd, tag, sizeof(tag))) {
            close(fd);
            return -7;
        }
        {
            static const char marker[] =
                "[independent-srp-qemu] screen AVCC config+encrypted frame sent\n";
            (void)write_all(1, marker, sizeof(marker) - 1);
        }
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
        usleep(150000); /* Let the valid frame reach stock OMX first. */
        nonce[0] = 1;
        seal(key, nonce, header, sizeof(header), avcc_frame, sizeof(avcc_frame), ciphertext, tag);
        tag[0] ^= 1;
        if (write_all(fd, header, sizeof(header)) || write_all(fd, ciphertext, sizeof(ciphertext)) ||
            write_all(fd, tag, sizeof(tag))) { close(fd); return -8; }
        HOOK_TRACE("[stock210-phone] corrupted screen tag sent\n");
#endif
    }
#else
    (void)shared;
    (void)seal;
#endif
#ifndef MHI2Q_SCREEN_AUDIO_QEMU
    usleep(300000);
#endif
    *out_fd = fd;
    return port;
}
#endif

static int probe_screen_command(int fd, const char *request,
                                const uint8_t write_key[32],
                                const uint8_t read_key[32],
                                unsigned write_counter, unsigned read_counter,
                                seal_fn seal, open_fn open_sealed)
{
    uint8_t frame[2 + 256 + 16];
    uint8_t prefix[2], ciphertext[1024], tag[16], response[1025];
    uint8_t nonce[8] = {0};
    size_t length, request_length = strlen(request);
    int status;
    if (request_length > 256) return -1;
    frame[0] = (uint8_t)(request_length & 255);
    frame[1] = (uint8_t)(request_length >> 8);
    nonce[0] = (uint8_t)write_counter;
    seal(write_key, nonce, frame, 2, request, request_length,
         frame + 2, frame + 2 + request_length);
    if (write_all(fd, frame, 2 + request_length + 16)) return -1;
    if (read_all(fd, prefix, 2)) return -2;
    length = (size_t)prefix[0] | ((size_t)prefix[1] << 8);
    if (length > sizeof(ciphertext)) return -3;
    if (read_all(fd, ciphertext, length) || read_all(fd, tag, 16)) return -4;
    memset(nonce, 0, sizeof(nonce));
    nonce[0] = (uint8_t)read_counter;
    if (open_sealed(read_key, nonce, prefix, 2, ciphertext, length,
                    response, tag)) return -5;
    response[length] = 0;
    if (sscanf((char *)response, "RTSP/1.0 %d", &status) != 1 &&
        sscanf((char *)response, "HTTP/1.1 %d", &status) != 1) return -6;
    return status;
}

#if !defined(MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU) && !defined(MHI2Q_ACTIVE_WIRED_DRAIN_QEMU) && !defined(MHI2Q_ACTIVE_OWNER_WIRED_QEMU)
static int probe_screen_teardown(int fd, const uint8_t write_key[32],
                                 const uint8_t read_key[32],
                                 unsigned write_counter, unsigned read_counter,
                                 seal_fn seal, open_fn open_sealed)
{
    static const char request2[] = "TEARDOWN /stream RTSP/1.0\r\n"
        "CSeq: 2\r\nContent-Length: 0\r\n\r\n";
    static const char request3[] = "TEARDOWN /stream RTSP/1.0\r\n"
        "CSeq: 3\r\nContent-Length: 0\r\n\r\n";
    return probe_screen_command(fd, write_counter == 2 ? request2 : request3,
                                write_key, read_key,
                                write_counter, read_counter, seal, open_sealed);
}
#endif
#ifdef MHI2Q_ACTIVE_SESSION_QUIESCE_QEMU
/* The first media session in the repeat fixture leaves admission open. */
static unsigned repeat_media_cycle = 2;

static int probe_bridge_request(uint8_t operation, uint32_t generation)
{
    uint8_t request[16] = {
        'C', 'P', 'R', 'X', 2, 0, 0, 0, 0, 0, 0, 0, 0x58, 0x1b, 0, 0
    };
    uint8_t response[8];
    struct sockaddr_in address;
#ifdef MHI2Q_PAIRING_HOOK_QEMU
    struct timeval timeout = { 30, 0 }; /* CPU-loaded QEMU: SRP is software ARM. */
#else
    struct timeval timeout = { 5, 0 };
#endif
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int status = -1;
    if (fd < 0) return -1;
    request[5] = operation;
    request[8] = (uint8_t)generation;
    request[9] = (uint8_t)(generation >> 8);
    request[10] = (uint8_t)(generation >> 16);
    request[11] = (uint8_t)(generation >> 24);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(57343);
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0 &&
        write_all(fd, request, sizeof(request)) == 0 &&
        read_all(fd, response, sizeof(response)) == 0 &&
        memcmp(response, "CPRA\002", 5) == 0)
        status = response[5];
    close(fd);
    return status;
}

static int probe_active_session_quiesce(void)
{
    return probe_bridge_request(3, 0);
}
#if defined(MHI2Q_ACTIVE_WIRED_DRAIN_QEMU) || defined(MHI2Q_ACTIVE_OWNER_WIRED_QEMU)
static int probe_active_wired_drain(void)
{
    return probe_bridge_request(4, 0);
}
#endif
#ifdef MHI2Q_ACTIVE_OWNER_WIRED_QEMU
static int probe_wired_owner_takeover(void)
{
    pid_t child;
    extern char **environ;
    char *const args[] = {
        "dio_owner_exec", "wired", "/proc/boot/wired-confirm-qemu", NULL
    };
    int spawn_error;
    setenv("MHI2Q_DIO_OWNER_LOCK", "/mnt/owner/owner.lock", 1);
    setenv("MHI2Q_DIO_WIRED_PRIORITY_LOCK", "/mnt/owner/priority.lock", 1);
    unsetenv("LD_PRELOAD");
    spawn_error = posix_spawn(&child, "/mnt/owner/dio_owner_exec", NULL,
                              NULL, args, environ);
    if (spawn_error != 0) {
        char line[96];
        int size = snprintf(line, sizeof(line),
            "[independent-srp-qemu] wired spawn error=%d\n", spawn_error);
        if (size > 0 && (size_t)size < sizeof(line))
            (void)write_all(1, line, (size_t)size);
        return -1;
    }
    for (unsigned tick = 0; tick < 120; ++tick) {
        int status = 0;
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -2;
        if (waited < 0) return -3;
        usleep(100000);
    }
    kill(child, SIGTERM);
    waitpid(child, NULL, 0);
    return -4;
}
#endif
#endif
#endif
/* Diagnostic only: the default plist has no streams entry; the screen variant
 * requests type 110. Both use the next nonce on the verified connection. */
static int probe_control_setup(int fd, const uint8_t shared[32],
                               seal_fn seal, open_fn open_sealed,
                               unsigned read_counter)
{
#ifndef MHI2Q_INDEPENDENT_SCREEN_SETUP
    static const uint8_t empty_plist[] = {
        0x62,0x70,0x6c,0x69,0x73,0x74,0x30,0x30,0xd0,0x08,0x00,0x00,
        0x00,0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x09
    };
#endif
#ifdef MHI2Q_INDEPENDENT_SCREEN_SETUP
#ifndef MHI2Q_SCREEN_AUDIO_QEMU
    /* plistlib binary plist: {streams: [{type: 110, streamConnectionID: 1}]} */
    static const uint8_t screen_plist[] = {
        0x62,0x70,0x6c,0x69,0x73,0x74,0x30,0x30,0xd1,0x01,0x02,0x57,
        0x73,0x74,0x72,0x65,0x61,0x6d,0x73,0xa1,0x03,0xd2,0x04,0x05,
        0x06,0x07,0x5f,0x10,0x12,0x73,0x74,0x72,0x65,0x61,0x6d,0x43,
        0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x49,0x44,0x54,
        0x74,0x79,0x70,0x65,0x10,0x01,0x10,0x6e,0x08,0x0b,0x13,0x15,
        0x1a,0x2f,0x34,0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38
    };
#endif
#ifdef MHI2Q_SCREEN_AUDIO_QEMU
    /* screen 110 plus main PCM audio 100; generated with Python plistlib. */
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
    /* Include the phone's feature proposal to catch unsupported altScreen
     * negotiation under the production wrapper's enabled display flags. */
    static const uint8_t screen_audio_plist[] = {
        0x62,0x70,0x6c,0x69,0x73,0x74,0x30,0x30,0xd3,0x01,0x02,0x03,0x04,0x05,0x08,0x52,
        0x65,0x74,0x58,0x66,0x65,0x61,0x74,0x75,0x72,0x65,0x73,0x57,0x73,0x74,0x72,0x65,
        0x61,0x6d,0x73,0x10,0x20,0xa2,0x06,0x07,0x59,0x61,0x6c,0x74,0x53,0x63,0x72,0x65,
        0x65,0x6e,0x59,0x76,0x69,0x65,0x77,0x41,0x72,0x65,0x61,0x73,0xa2,0x09,0x0e,0xd2,
        0x0a,0x0b,0x0c,0x0d,0x5f,0x10,0x12,0x73,0x74,0x72,0x65,0x61,0x6d,0x43,0x6f,0x6e,
        0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x49,0x44,0x54,0x74,0x79,0x70,0x65,0x10,0x01,
        0x10,0x6e,0xd5,0x0f,0x10,0x11,0x0a,0x0b,0x12,0x13,0x14,0x15,0x16,0x5b,0x61,0x75,
        0x64,0x69,0x6f,0x46,0x6f,0x72,0x6d,0x61,0x74,0x58,0x64,0x61,0x74,0x61,0x50,0x6f,
        0x72,0x74,0x55,0x69,0x6e,0x70,0x75,0x74,0x11,0x08,0x00,0x11,0x17,0x7b,0x09,0x10,
        0x02,0x10,0x64,0x08,0x0f,0x12,0x1b,0x23,0x25,0x28,0x32,0x3c,0x3f,0x44,0x59,0x5e,
        0x60,0x62,0x6d,0x79,0x82,0x88,0x8b,0x8e,0x8f,0x91,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x17,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x93,
    };
#else
    static const uint8_t screen_audio_plist[] = {
        0x62,0x70,0x6c,0x69,0x73,0x74,0x30,0x30,0xd1,0x01,0x02,0x57,
        0x73,0x74,0x72,0x65,0x61,0x6d,0x73,0xa2,0x03,0x08,0xd2,0x04,
        0x05,0x06,0x07,0x5f,0x10,0x12,0x73,0x74,0x72,0x65,0x61,0x6d,
        0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x49,0x44,
        0x54,0x74,0x79,0x70,0x65,0x10,0x01,0x10,0x6e,0xd3,0x09,0x04,
        0x05,0x0a,0x0b,0x0c,0x5b,0x61,0x75,0x64,0x69,0x6f,0x46,0x6f,
        0x72,0x6d,0x61,0x74,0x11,0x08,0x00,0x10,0x02,0x10,0x64,0x08,
        0x0b,0x13,0x16,0x1b,0x30,0x35,0x37,0x39,0x40,0x4c,0x4f,0x51,
        0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x53
    };
#endif
    const uint8_t *plist = screen_audio_plist;
    const size_t plist_length = sizeof(screen_audio_plist);
#else
    const uint8_t *plist = screen_plist;
    const size_t plist_length = sizeof(screen_plist);
#endif
#else
    const uint8_t *plist = empty_plist;
    const size_t plist_length = sizeof(empty_plist);
#endif
    uint8_t write_key[32], read_key[32], nonce[8] = {0};
    uint8_t request[512], frame[2 + sizeof(request) + 16];
    uint8_t response[4096];
    int header_length, status = 0, attempt;
    size_t request_length, used = 0, index, header_end = 0, content_length = 0;
#ifdef MHI2Q_INDEPENDENT_SCREEN_SETUP
    struct timing_responder timing;
    pthread_t timing_thread;
    int timing_started = 0;
#ifdef MHI2Q_SCREEN_DATA_QEMU
    int screen_data_fd = -1;
    int screen_event_fd = -1;
#endif
#endif
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
    int mic_fd = media_mic_socket();
    if (mic_fd < 0) return -30;
#endif
    if (hkdf32(shared, 32, "Control-Salt", "Control-Write-Encryption-Key",
               write_key) ||
        hkdf32(shared, 32, "Control-Salt", "Control-Read-Encryption-Key",
               read_key)) return -1;
    header_length = snprintf((char *)request, sizeof(request),
        "SETUP /stream RTSP/1.0\r\nCSeq: 1\r\n"
        "Content-Type: application/x-apple-binary-plist\r\n"
        "Content-Length: %zu\r\n\r\n", plist_length);
    if (header_length < 0 ||
        (size_t)header_length + plist_length > sizeof(request)) return -2;
    memcpy(request + header_length, plist, plist_length);
    request_length = (size_t)header_length + plist_length;
    frame[0] = (uint8_t)(request_length & 255);
    frame[1] = (uint8_t)(request_length >> 8);
    nonce[0] = 1;
    seal(write_key, nonce, frame, 2, request, request_length,
         frame + 2, frame + 2 + request_length);
#ifdef MHI2Q_INDEPENDENT_SCREEN_SETUP
    if (start_timing_responder(&timing, &timing_thread)) return -11;
    timing_started = 1;
#endif
    if (write_all(fd, frame, 2 + request_length + 16)) return -3;
    for (attempt = 0; attempt < 4; ++attempt) {
        uint8_t prefix[2], ciphertext[1024], tag[16];
        size_t length;
        if (read_all(fd, prefix, 2)) return -4;
        length = (size_t)prefix[0] | ((size_t)prefix[1] << 8);
        if (length > sizeof(ciphertext) || used + length >= sizeof(response))
            return -5;
        if (read_all(fd, ciphertext, length) || read_all(fd, tag, 16)) return -6;
        memset(nonce, 0, sizeof(nonce));
        nonce[0] = (uint8_t)(read_counter + (unsigned)attempt);
        if (open_sealed(read_key, nonce, prefix, 2, ciphertext, length,
                        response + used, tag)) return -7;
        used += length;
        if (!header_end) {
            for (index = 0; index + 3 < used; ++index) {
                if (memcmp(response + index, "\r\n\r\n", 4) == 0) {
                    char *field;
                    uint8_t saved;
                    header_end = index + 4;
                    saved = response[header_end];
                    response[header_end] = 0;
                    if (sscanf((char *)response, "RTSP/1.0 %d", &status) != 1 &&
                        sscanf((char *)response, "HTTP/1.1 %d", &status) != 1)
                        return -8;
                    field = strstr((char *)response, "Content-Length: ");
                    if (!field || sscanf(field, "Content-Length: %zu",
                                         &content_length) != 1 ||
                        content_length > sizeof(response) - header_end)
                        return -9;
                    response[header_end] = saved;
                    break;
                }
            }
        }
        if (header_end && used >= header_end + content_length) {
#ifdef MHI2Q_INDEPENDENT_SCREEN_SETUP
            static const char digits[] = "0123456789abcdef";
            char line[sizeof(response) * 2 + 64];
            static const char prefix[] = "[independent-srp-qemu] screen-setup-body-hex=";
#ifndef MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU
            int teardown_status;
#endif
            unsigned next_response = read_counter + (unsigned)attempt + 1;
#if !defined(MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU) && !defined(MHI2Q_ACTIVE_WIRED_DRAIN_QEMU) && !defined(MHI2Q_ACTIVE_OWNER_WIRED_QEMU)
            unsigned teardown_nonce = 2;
#endif
            if (content_length * 2 + sizeof(prefix) < sizeof(line)) {
                memcpy(line, prefix, sizeof(prefix) - 1);
                for (index = 0; index < content_length; ++index) {
                    uint8_t byte = response[header_end + index];
                    line[sizeof(prefix) - 1 + index * 2] = digits[byte >> 4];
                    line[sizeof(prefix) + index * 2] = digits[byte & 15];
                }
                line[sizeof(prefix) - 1 + content_length * 2] = '\n';
                (void)write_all(1, line, sizeof(prefix) + content_length * 2);
            }
#ifdef MHI2Q_SCREEN_DATA_QEMU
            {
                static const char record[] = "RECORD /stream RTSP/1.0\r\n"
                    "CSeq: 2\r\nContent-Length: 0\r\n\r\n";
                int event_port = screen_event_port(response + header_end,
                                                   content_length);
                int record_status = -1;
                int port = -1;
                if (event_port > 0)
                    screen_event_fd = open_screen_port(event_port);
                if (screen_event_fd >= 0)
                    record_status = probe_screen_command(
                        fd, record, write_key, read_key, 2, next_response,
                        seal, open_sealed);
#if !defined(MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU) && !defined(MHI2Q_ACTIVE_WIRED_DRAIN_QEMU) && !defined(MHI2Q_ACTIVE_OWNER_WIRED_QEMU)
                teardown_nonce = 3;
#endif
                ++next_response;
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
                if (record_status == 200) {
                    int checked = media_event_mic_check(screen_event_fd, mic_fd, shared, seal, open_sealed);
                    close(mic_fd);
                    if (checked) { char msg[80]; int n = snprintf(msg, sizeof(msg), "[stock210-phone] media check=%d\n", checked); (void)write_all(2, msg, (size_t)n); return -31; }
                }
#endif
                if (record_status == 200)
                    port = probe_screen_data(response + header_end,
                                             content_length, &screen_data_fd,
                                             shared, seal);
#ifdef MHI2Q_SCREEN_AUDIO_QEMU
                if (port > 0)
                    (void)probe_audio_packets(response + header_end,
                                              content_length, shared, seal);
#endif
                {
                    int size = snprintf(line, sizeof(line),
                        "[independent-srp-qemu] screen event port=%d RECORD status=%d\n",
                        event_port, record_status);
                    if (size > 0 && (size_t)size < sizeof(line))
                        (void)write_all(1, line, (size_t)size);
                }
                int size = snprintf(line, sizeof(line),
                    "[independent-srp-qemu] screen data keepalive port=%d\n", port);
                if (size > 0 && (size_t)size < sizeof(line))
                    (void)write_all(1, line, (size_t)size);
                if (screen_data_fd >= 0) {
                    close(screen_data_fd);
                    screen_data_fd = -1;
                }
            }
#endif
#if defined(MHI2Q_ACTIVE_SESSION_QUIESCE_QEMU) && !defined(MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU)
            if (repeat_media_cycle != 1) {
                teardown_status = probe_active_session_quiesce();
                {
                char status_line[96];
                int size = snprintf(status_line, sizeof(status_line),
                    "[independent-srp-qemu] active session QUIESCE=%d\n",
                    teardown_status);
                if (size > 0 && (size_t)size < sizeof(status_line))
                    (void)write_all(1, status_line, (size_t)size);
                }
                if (teardown_status != 3) return -12;
            }
#ifdef MHI2Q_ACTIVE_OWNER_WIRED_QEMU
            teardown_status = probe_active_wired_drain();
            {
                char status_line[96];
                int size = snprintf(status_line, sizeof(status_line),
                    "[independent-srp-qemu] unowned wired drain=%d\n",
                    teardown_status);
                if (size > 0 && (size_t)size < sizeof(status_line))
                    (void)write_all(1, status_line, (size_t)size);
            }
            if (teardown_status != 1) return -13;
            teardown_status = probe_wired_owner_takeover();
            {
                char status_line[96];
                int size = snprintf(status_line, sizeof(status_line),
                    "[independent-srp-qemu] wired helper exit=%d\n",
                    teardown_status);
                if (size > 0 && (size_t)size < sizeof(status_line))
                    (void)write_all(1, status_line, (size_t)size);
            }
            if (teardown_status != 0) return -13;
#elif defined(MHI2Q_ACTIVE_WIRED_DRAIN_QEMU)
            teardown_status = probe_active_wired_drain();
            {
                char status_line[96];
                int size = snprintf(status_line, sizeof(status_line),
                    "[independent-srp-qemu] active wired drain=%d\n",
                    teardown_status);
                if (size > 0 && (size_t)size < sizeof(status_line))
                    (void)write_all(1, status_line, (size_t)size);
            }
            if (teardown_status != 0) return -13;
#else
            teardown_status = probe_screen_teardown(
                fd, write_key, read_key, teardown_nonce, next_response,
                seal, open_sealed);
            {
                char status_line[96];
                int size = snprintf(status_line, sizeof(status_line),
                    "[independent-srp-qemu] screen TEARDOWN status=%d\n",
                    teardown_status);
                if (size > 0 && (size_t)size < sizeof(status_line))
                    (void)write_all(1, status_line, (size_t)size);
            }
            if (teardown_status != 200) return -13;
#endif
            if (repeat_media_cycle != 1) {
#ifndef MHI2Q_ACTIVE_OWNER_WIRED_QEMU
                unsigned retry;
                for (retry = 0; retry < 20; ++retry) {
                    teardown_status = probe_active_session_quiesce();
                    if (teardown_status == 0) break;
                    usleep(100000);
                }
                {
                    char status_line[104];
#ifdef MHI2Q_ACTIVE_WIRED_DRAIN_QEMU
                    const char *label = "post-drain QUIESCE";
#else
                    const char *label = "post-TEARDOWN QUIESCE";
#endif
                    int size = snprintf(status_line, sizeof(status_line),
                        "[independent-srp-qemu] %s=%d\n", label,
                        teardown_status);
                    if (size > 0 && (size_t)size < sizeof(status_line))
                        (void)write_all(1, status_line, (size_t)size);
                }
                if (teardown_status != 0) return -14;
#endif
            }
#elif defined(MHI2Q_ACTIVE_SESSION_QUIESCE_QEMU) && defined(MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU)
            {
                int quiesce_status = probe_active_session_quiesce();
                char status_line[96];
                int size = snprintf(status_line, sizeof(status_line),
                    "[independent-srp-qemu] active session QUIESCE=%d\n",
                    quiesce_status);
                if (size > 0 && (size_t)size < sizeof(status_line))
                    (void)write_all(1, status_line, (size_t)size);
                if (quiesce_status != 3) return -12;
            }
            {
                static const char marker[] =
                    "[independent-srp-qemu] screen abrupt close without TEARDOWN\n";
                (void)write_all(1, marker, sizeof(marker) - 1);
            }
#elif defined(MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU)
            {
                static const char marker[] =
                    "[independent-srp-qemu] screen abrupt close without TEARDOWN\n";
                (void)write_all(1, marker, sizeof(marker) - 1);
            }
#else
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
            if(lifecycle_cycle==2) {
                if(lifecycle_wired())return -32;
                static const char marker[] =
                    "[stock210-lifecycle] media ended by wired drain\n";
                (void)write_all(1,marker,sizeof(marker)-1);
            } else {
#endif
            teardown_status = probe_screen_teardown(
                fd, write_key, read_key, teardown_nonce, next_response,
                seal, open_sealed);
            {
                char status_line[90];
                int size = snprintf(status_line, sizeof(status_line),
                    "[independent-srp-qemu] screen TEARDOWN status=%d\n",
                    teardown_status);
                if (size > 0 && (size_t)size < sizeof(status_line))
                    (void)write_all(1, status_line, (size_t)size);
            }
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
            }
#endif
#endif
            if (timing_started) {
                char count_line[90];
                int size;
                __sync_lock_test_and_set(&timing.stop, 1);
                pthread_join(timing_thread, NULL);
                close(timing.fd);
                size = snprintf(count_line, sizeof(count_line),
                    "[independent-srp-qemu] timing responses=%u\n",
                    (unsigned)timing.responses);
                if (size > 0 && (size_t)size < sizeof(count_line))
                    (void)write_all(1, count_line, (size_t)size);
            }
#ifdef MHI2Q_SCREEN_DATA_QEMU
            if (screen_data_fd >= 0) close(screen_data_fd);
            if (screen_event_fd >= 0) close(screen_event_fd);
#endif
#endif
            return status;
        }
    }
    return -10;
}
#endif

static int exchange_identity(int fd, const uint8_t shared[64],
                             struct tlv_value values[MAX_TLV_TYPES],
                             struct pairing_identity *identity)
{
    void *crypto = NULL;
    ed_make_fn make_key;
    ed_sign_fn sign;
    ed_verify_fn verify;
    seal_fn seal;
    open_fn open_sealed;
    uint8_t public_key[32], private_key[32], signature[64];
    uint8_t sign_key[32], encrypt_key[32], signed_bytes[512];
    uint8_t sub_tlv[512], ciphertext[528], request[1024], plaintext[512];
    struct tlv_value subvalues[MAX_TLV_TYPES];
    size_t signed_length, sub_length = 0, request_length = 0, payload_length;
    uint8_t state = 5;
    int result = -1;
#ifndef MHI2Q_PAIRING_TEST_CRYPTO
#define MHI2Q_PAIRING_TEST_CRYPTO "/proc/boot/libairplay.so"
#endif
    crypto = dlopen(MHI2Q_PAIRING_TEST_CRYPTO, RTLD_NOW | RTLD_LOCAL);
    if (!crypto) goto exit;
    make_key = (ed_make_fn)dlsym(crypto, "ed25519_make_key_pair_ref");
    sign = (ed_sign_fn)dlsym(crypto, "ed25519_sign_ref");
    verify = (ed_verify_fn)dlsym(crypto, "ed25519_verify_ref");
    seal = (seal_fn)dlsym(crypto, "chacha20_poly1305_encrypt_all_64x64");
    open_sealed = (open_fn)dlsym(crypto, "chacha20_poly1305_decrypt_all_64x64");
    if (!make_key || !sign || !verify || !seal || !open_sealed) goto exit;
    make_key(public_key, private_key);
    if (hkdf32(shared, 64, "Pair-Setup-Controller-Sign-Salt",
               "Pair-Setup-Controller-Sign-Info", sign_key) ||
        hkdf32(shared, 64, "Pair-Setup-Encrypt-Salt",
               "Pair-Setup-Encrypt-Info", encrypt_key)) goto exit;
    signed_length = 32 + sizeof(controller_id) - 1 + 32;
    memcpy(signed_bytes, sign_key, 32);
    memcpy(signed_bytes + 32, controller_id, sizeof(controller_id) - 1);
    memcpy(signed_bytes + 32 + sizeof(controller_id) - 1, public_key, 32);
    sign(signature, signed_bytes, signed_length, public_key, private_key);
    if (append_tlv(sub_tlv, sizeof(sub_tlv), &sub_length, 1,
                   controller_id, sizeof(controller_id) - 1) ||
        append_tlv(sub_tlv, sizeof(sub_tlv), &sub_length, 3,
                   public_key, sizeof(public_key)) ||
        append_tlv(sub_tlv, sizeof(sub_tlv), &sub_length, 10,
                   signature, sizeof(signature))) goto exit;
    seal(encrypt_key, (const uint8_t *)"PS-Msg05", NULL, 0,
         sub_tlv, sub_length, ciphertext, ciphertext + sub_length);
    if (append_tlv(request, sizeof(request), &request_length, 5,
                   ciphertext, sub_length + 16) ||
        append_tlv(request, sizeof(request), &request_length, 6,
                   &state, 1) ||
        post_pairing(fd, "/pair-setup", request, request_length, values) ||
        values[6].length != 1 || values[6].data[0] != 6 ||
        values[7].length || values[5].length <= 16) goto exit;
    payload_length = values[5].length - 16;
    if (payload_length > sizeof(plaintext) ||
        open_sealed(encrypt_key, (const uint8_t *)"PS-Msg06", NULL, 0,
                    values[5].data, payload_length, plaintext,
                    values[5].data + payload_length) ||
        parse_tlv(plaintext, payload_length, subvalues) ||
        !subvalues[1].length || subvalues[3].length != 32 ||
        subvalues[10].length != 64 ||
        hkdf32(shared, 64, "Pair-Setup-Accessory-Sign-Salt",
               "Pair-Setup-Accessory-Sign-Info", sign_key)) goto exit;
    signed_length = 32 + subvalues[1].length + 32;
    if (signed_length > sizeof(signed_bytes)) goto exit;
    memcpy(signed_bytes, sign_key, 32);
    memcpy(signed_bytes + 32, subvalues[1].data, subvalues[1].length);
    memcpy(signed_bytes + 32 + subvalues[1].length, subvalues[3].data, 32);
    if (verify(signed_bytes, signed_length, subvalues[10].data,
               subvalues[3].data)) goto exit;
    memcpy(identity->controller_pk, public_key, 32);
    memcpy(identity->controller_sk, private_key, 32);
    memcpy(identity->accessory_pk, subvalues[3].data, 32);
    memcpy(identity->accessory_id, subvalues[1].data, subvalues[1].length);
    identity->accessory_id_length = subvalues[1].length;
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
    if(strlen(lifecycle_identity)!=identity->accessory_id_length ||
       memcmp(lifecycle_identity,identity->accessory_id,identity->accessory_id_length))goto exit;
    puts("[stock210-lifecycle] iAP2 pi matches signed PairSetup identity");fflush(stdout);
#endif
    result = 0;
exit:
    if (crypto) dlclose(crypto);
    return result;
}

static int verify_pairing(const struct pairing_identity *identity)
{
    static const uint8_t ephemeral_seed[32] = {
        0x86, 0x5a, 0x3e, 0x1c, 0x92, 0x70, 0xf4, 0x4b,
        0x2d, 0x0f, 0xa1, 0x69, 0x3b, 0x27, 0x4e, 0xc8,
        0x51, 0xd0, 0x7f, 0x98, 0x24, 0xba, 0x6c, 0xe5,
        0x12, 0x43, 0x9d, 0x08, 0x77, 0x6e, 0x30, 0x55
    };
    void *crypto = NULL;
    curve_fn curve;
    ed_sign_fn sign;
    ed_verify_fn verify;
    seal_fn seal;
    open_fn open_sealed;
    uint8_t private_key[32], public_key[32], peer_key[32], shared[32];
    uint8_t encrypt_key[32], signature[64], signed_bytes[1024];
    uint8_t request[1024], sub_tlv[512], ciphertext[528], plaintext[512];
    struct tlv_value values[MAX_TLV_TYPES], subvalues[MAX_TLV_TYPES];
    size_t used = 0, sub_length = 0, payload_length, signed_length;
    uint8_t state = 1;
    unsigned next_read_counter = 0;
    int fd = -1, result = -1;
    crypto = dlopen(MHI2Q_PAIRING_TEST_CRYPTO, RTLD_NOW | RTLD_LOCAL);
    if (!crypto) goto exit;
    curve = (curve_fn)dlsym(crypto, "curve25519_donna");
    sign = (ed_sign_fn)dlsym(crypto, "ed25519_sign_ref");
    verify = (ed_verify_fn)dlsym(crypto, "ed25519_verify_ref");
    seal = (seal_fn)dlsym(crypto, "chacha20_poly1305_encrypt_all_64x64");
    open_sealed = (open_fn)dlsym(crypto, "chacha20_poly1305_decrypt_all_64x64");
    if (!curve || !sign || !verify || !seal || !open_sealed) goto exit;
    if (hkdf32(ephemeral_seed, sizeof(ephemeral_seed), "Pair-Verify-ECDH-Salt",
               "Pair-Verify-ECDH-Info", private_key)) goto exit;
    curve(public_key, private_key, NULL);
    fd = open_receiver();
    if (fd < 0) goto exit;
    if (append_tlv(request, sizeof(request), &used, 6, &state, 1) ||
        append_tlv(request, sizeof(request), &used, 3, public_key, 32) ||
        post_pairing(fd, "/pair-verify", request, used, values) ||
        values[6].length != 1 || values[6].data[0] != 2 ||
        values[7].length || values[3].length != 32 || values[5].length <= 16)
        goto exit;
    memcpy(peer_key, values[3].data, 32);
    curve(shared, private_key, peer_key);
    if (hkdf32(shared, sizeof(shared), "Pair-Verify-Encrypt-Salt",
               "Pair-Verify-Encrypt-Info", encrypt_key)) goto exit;
    payload_length = values[5].length - 16;
    if (payload_length > sizeof(plaintext) ||
        open_sealed(encrypt_key, (const uint8_t *)"PV-Msg02", NULL, 0,
                    values[5].data, payload_length, plaintext,
                    values[5].data + payload_length) ||
        parse_tlv(plaintext, payload_length, subvalues) ||
        subvalues[1].length != identity->accessory_id_length ||
        memcmp(subvalues[1].data, identity->accessory_id,
               identity->accessory_id_length) ||
        subvalues[10].length != 64) goto exit;
    signed_length = 32 + identity->accessory_id_length + 32;
    if (signed_length > sizeof(signed_bytes)) goto exit;
    memcpy(signed_bytes, peer_key, 32);
    memcpy(signed_bytes + 32, identity->accessory_id,
           identity->accessory_id_length);
    memcpy(signed_bytes + 32 + identity->accessory_id_length, public_key, 32);
    if (verify(signed_bytes, signed_length, subvalues[10].data,
               identity->accessory_pk)) goto exit;
    signed_length = 32 + sizeof(controller_id) - 1 + 32;
    memcpy(signed_bytes, public_key, 32);
    memcpy(signed_bytes + 32, controller_id, sizeof(controller_id) - 1);
    memcpy(signed_bytes + 32 + sizeof(controller_id) - 1, peer_key, 32);
    sign(signature, signed_bytes, signed_length,
         identity->controller_pk, identity->controller_sk);
#ifdef MHI2Q_PAIRING_HOOK_QEMU
    if (hook_bad_signature) signature[0] ^= 1;
#endif
    if (append_tlv(sub_tlv, sizeof(sub_tlv), &sub_length, 1,
                   controller_id, sizeof(controller_id) - 1) ||
        append_tlv(sub_tlv, sizeof(sub_tlv), &sub_length, 10,
                   signature, sizeof(signature))) goto exit;
    seal(encrypt_key, (const uint8_t *)"PV-Msg03", NULL, 0,
         sub_tlv, sub_length, ciphertext, ciphertext + sub_length);
    used = 0;
    state = 3;
    if (append_tlv(request, sizeof(request), &used, 5,
                   ciphertext, sub_length + 16) ||
        append_tlv(request, sizeof(request), &used, 6, &state, 1) ||
        post_pairing(fd, "/pair-verify", request, used, values) ||
        values[6].length != 1 || values[6].data[0] != 4) goto exit;
#ifdef MHI2Q_PAIRING_HOOK_QEMU
    if (hook_bad_signature) {
        if (values[7].length != 1 || values[7].data[0] != 2) goto exit;
        HOOK_TRACE("[independent-srp-qemu] bad-signature rejection verified\n");
        result = 0; goto exit;
    }
#endif
    if (values[7].length) goto exit;
    {
        static const char verified[] =
            "[independent-srp-qemu] pair-verify M1-M4 verified\n";
        (void)write_all(1, verified, sizeof(verified) - 1);
    }
    {
        int info_status = probe_encrypted_info(fd, shared, seal, open_sealed,
                                               &next_read_counter);
#ifdef MHI2Q_REPEAT_MEDIA_QEMU
        char line[96];
        int size = snprintf(line, sizeof(line),
            "[independent-srp-qemu] encrypted info cycle=%u status=%d\n",
            repeat_media_cycle, info_status);
        if (size > 0 && (size_t)size < sizeof(line))
            (void)write_all(1, line, (size_t)size);
#endif
        if (info_status) goto exit;
    }
#if defined(MHI2Q_PAIRING_HOOK_QEMU) && !defined(MHI2Q_WIRELESS_MEDIA_QEMU)
    {
        int check = hook_control_regression(fd, shared, seal, open_sealed, next_read_counter);
        if (check) {
            fprintf(stderr, "[independent-srp-qemu] control regression=%d\n", check);
            goto exit;
        }
    }
#endif
#ifdef MHI2Q_INDEPENDENT_CONTROL_SETUP
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
    if (hook_media_run)
#endif
    {
        int setup_status = probe_control_setup(fd, shared, seal, open_sealed,
                                               next_read_counter);
        char line[100];
        int length = snprintf(line, sizeof(line),
            "[independent-srp-qemu] control SETUP status=%d\n", setup_status);
        if (length > 0 && (size_t)length < sizeof(line))
            (void)write_all(1, line, (size_t)length);
        if (setup_status != 200) goto exit;
#ifdef MHI2Q_SCREEN_ABRUPT_CLOSE_QEMU
        if (setup_status == 200) {
            struct linger reset = {1, 0};
            if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)) != 0)
                goto exit;
#ifdef MHI2Q_SCREEN_RST_DELAY_QEMU
            {
                static const char marker[] =
                    "[independent-srp-qemu] timing socket closed before delayed RST\n";
                (void)write_all(1, marker, sizeof(marker) - 1);
                sleep(3);
            }
#endif
            close(fd);
            fd = -1;
#ifdef MHI2Q_ACTIVE_SESSION_QUIESCE_QEMU
            {
                unsigned retry;
                int quiesce_status = -1;
                for (retry = 0; retry < 20; ++retry) {
                    quiesce_status = probe_active_session_quiesce();
                    if (quiesce_status == 0) break;
                    usleep(100000);
                }
                int size = snprintf(line, sizeof(line),
                    "[independent-srp-qemu] post-RST QUIESCE=%d\n",
                    quiesce_status);
                if (size > 0 && (size_t)size < sizeof(line))
                    (void)write_all(1, line, (size_t)size);
                if (quiesce_status != 0) goto exit;
            }
#endif
        }
#endif
#ifdef MHI2Q_SCREEN_LINGER_QEMU
        {
            static const char start[] = "[independent-srp-qemu] screen linger before close\n";
            static const char end[] = "[independent-srp-qemu] screen linger complete\n";
            (void)write_all(1, start, sizeof(start) - 1);
            sleep(2);
            (void)write_all(1, end, sizeof(end) - 1);
        }
#endif
    }
#else
    (void)next_read_counter;
#endif
    result = 0;
exit:
    if (fd >= 0) close(fd);
    if (crypto) dlclose(crypto);
    return result;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    struct tlv_value values[MAX_TLV_TYPES];
    struct pairing_identity identity;
#ifndef MHI2Q_STOCK210_LIFECYCLE_QEMU
    struct stat keychain;
#endif
    uint8_t request[1024], state, client_a[N_BYTES], proof[64];
    uint8_t server_proof[64], shared[64];
    size_t used = 0;
    int fd = -1, result = 1;
    static const char srp_success[] = "[independent-srp-qemu] M1-M4 proof verified\n";
    static const char setup_success[] = "[independent-srp-qemu] M1-M6 identity verified\n";
    static const char info_success[] = "[independent-srp-qemu] encrypted /info HTTP 200\n";
#ifndef MHI2Q_STOCK210_LIFECYCLE_QEMU
    /* Let the capture probe finish its /info and baseline pairing first;
     * concurrent HTTP clients obscure its serial readiness trace in QEMU. */
    {
        int attempt;
        for (attempt = 0; attempt < 900; ++attempt) {
            if (stat("/dev/shmem/independent_srp_ready", &keychain) == 0) break;
            usleep(200000);
        }
        if (attempt == 900) goto exit;
    }
#endif
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
    if(lifecycle_start_phone())goto exit;
#endif
    fd = open_receiver();
    if (fd < 0) goto exit;
    state = 0;
    if (append_tlv(request, sizeof(request), &used, 0, &state, 1)) goto exit;
    state = 1;
    if (append_tlv(request, sizeof(request), &used, 6, &state, 1) ||
        post_pairing(fd, "/pair-setup", request, used, values) ||
        values[6].length != 1 || values[6].data[0] != 2 ||
        verify_srp(values, client_a, proof, server_proof, shared)) goto exit;
    used = 0;
    state = 3;
    if (append_tlv(request, sizeof(request), &used, 6, &state, 1) ||
        append_tlv(request, sizeof(request), &used, 3, client_a, sizeof(client_a)) ||
        append_tlv(request, sizeof(request), &used, 4, proof, sizeof(proof)) ||
        post_pairing(fd, "/pair-setup", request, used, values) ||
        values[6].length != 1 || values[6].data[0] != 4 ||
        values[4].length != 64 || values[7].length ||
        memcmp(values[4].data, server_proof, 64)) goto exit;
    (void)write_all(1, srp_success, sizeof(srp_success) - 1);
    if (exchange_identity(fd, shared, values, &identity)) goto exit;
    (void)write_all(1, setup_success, sizeof(setup_success) - 1);
    close(fd);
    fd = -1;
#ifdef MHI2Q_REPEAT_MEDIA_QEMU
    {
        int start_status = probe_bridge_request(1, 1);
        char line[96];
        int size = snprintf(line, sizeof(line),
            "[independent-srp-qemu] first bridge START=%d\n", start_status);
        if (size > 0 && (size_t)size < sizeof(line))
            (void)write_all(1, line, (size_t)size);
        if (start_status != 0) goto exit;
    }
    repeat_media_cycle = 1;
#endif
    if (verify_pairing(&identity)) goto exit;
#ifdef MHI2Q_PAIRING_HOOK_QEMU
    hook_bad_signature = 1;
    if (verify_pairing(&identity)) goto exit;
    hook_bad_signature = 0;
#ifdef MHI2Q_WIRELESS_MEDIA_QEMU
    hook_media_run = 1;
#endif
    if (verify_pairing(&identity)) goto exit;
    HOOK_TRACE("[independent-srp-qemu] reconnect with stored peer verified\n");
#endif
#ifdef MHI2Q_REPEAT_MEDIA_QEMU
    {
        static const char first_done[] =
            "[independent-srp-qemu] first media session complete\n";
        (void)write_all(1, first_done, sizeof(first_done) - 1);
    }
#ifndef MHI2Q_REPEAT_MEDIA_KEEP_SERVER_QEMU
    {
        int stop_status = -1;
        int start_status;
        unsigned retry;
        char line[96];
        int size;
        for (retry = 0; retry < 20; ++retry) {
            stop_status = probe_bridge_request(2, 1);
            if (stop_status != 3) break;
            usleep(100000);
        }
        size = snprintf(line, sizeof(line),
            "[independent-srp-qemu] first bridge STOP=%d\n", stop_status);
        if (size > 0 && (size_t)size < sizeof(line))
            (void)write_all(1, line, (size_t)size);
        if (stop_status != 0) goto exit;
        start_status = probe_bridge_request(1, 2);
        size = snprintf(line, sizeof(line),
            "[independent-srp-qemu] second bridge START=%d\n", start_status);
        if (size > 0 && (size_t)size < sizeof(line))
            (void)write_all(1, line, (size_t)size);
        if (start_status != 0) goto exit;
    }
#endif
    repeat_media_cycle = 2;
    if (verify_pairing(&identity)) goto exit;
    {
        static const char second_done[] =
            "[independent-srp-qemu] second media session complete\n";
        (void)write_all(1, second_done, sizeof(second_done) - 1);
    }
#endif
#ifdef MHI2Q_STOCK210_LIFECYCLE_QEMU
    if(lifecycle_wait(lifecycle_phone))goto exit;
    lifecycle_cycle=2;
    if(lifecycle_start_phone() || verify_pairing(&identity))goto exit;
    if(lifecycle_wait(lifecycle_phone))goto exit;
    lifecycle_cycle=3;hook_media_run=0;
    if(lifecycle_start_phone() || verify_pairing(&identity) || lifecycle_stop())goto exit;
    puts("[stock210-lifecycle] two media generations, USB priority, wireless resume passed");fflush(stdout);
#endif
    (void)write_all(1, info_success, sizeof(info_success) - 1);
    result = 0;
exit:
    if (result) {
        static const char failure[] = "[independent-srp-qemu] pairing failed\n";
#ifdef MHI2Q_PAIRING_HOOK_QEMU
        int log_fd = open("/tmp/carplay_hook.log", O_RDONLY);
        if (log_fd >= 0) {
            char line[1024];
            ssize_t n;
            while ((n = read(log_fd, line, sizeof(line))) > 0) (void)write_all(2, line, (size_t)n);
            close(log_fd);
        }
#endif
        (void)write_all(2, failure, sizeof(failure) - 1);
    }
    if (fd >= 0) close(fd);
    return result;
}
