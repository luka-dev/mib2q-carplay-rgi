/* QEMU-only AirPlay 280 HTTP pairing client. Never shipped on the head. */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

typedef int (*pair_create_fn)(void **, const void *, uint32_t);
typedef int (*pair_identifier_fn)(void *, const void *, size_t);
typedef int (*pair_code_fn)(void *, const void *, size_t);
typedef int (*pair_exchange_fn)(void *, const void *, size_t,
                                uint8_t **, size_t *, uint8_t *);
typedef int (*pair_derive_fn)(void *, const void *, size_t, const void *,
                              size_t, size_t, uint8_t *);
typedef void (*encrypt_fn)(const uint8_t *, const uint8_t *, const void *,
                            size_t, const void *, size_t, void *, uint8_t *);
typedef int (*decrypt_fn)(const uint8_t *, const uint8_t *, const void *,
                           size_t, const void *, size_t, void *, const uint8_t *);
typedef void (*cf_release_fn)(const void *);

static int write_all(int fd, const void *bytes, size_t length)
{
    const uint8_t *ptr = bytes;
    while (length) {
        ssize_t count = write(fd, ptr, length);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        ptr += (size_t)count;
        length -= (size_t)count;
    }
    return 0;
}

static int read_all(int fd, void *bytes, size_t length)
{
    uint8_t *ptr = bytes;
    while (length) {
        ssize_t count = read(fd, ptr, length);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        ptr += (size_t)count;
        length -= (size_t)count;
    }
    return 0;
}

static int probe_encrypted_info(int fd, void *session)
{
    static const char request[] = "GET /info HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
    static const char salt[] = "Control-Salt";
    static const char read_info[] = "Control-Read-Encryption-Key";
    static const char write_info[] = "Control-Write-Encryption-Key";
    pair_derive_fn derive = (pair_derive_fn)dlsym(RTLD_DEFAULT, "PairingSessionDeriveKey");
    encrypt_fn encrypt = (encrypt_fn)dlsym(RTLD_DEFAULT, "chacha20_poly1305_encrypt_all_64x64");
    decrypt_fn decrypt = (decrypt_fn)dlsym(RTLD_DEFAULT, "chacha20_poly1305_decrypt_all_64x64");
    uint8_t write_key[32], read_key[32], nonce[8] = {0};
    uint8_t frame[2 + sizeof(request) - 1 + 16];
    uint8_t response[4096];
    size_t used = 0, header_length = 0, expected_length = 0, index;
    int status, round;
    if (!derive || !encrypt || !decrypt) return -30;
    status = derive(session, salt, sizeof(salt) - 1,
                    write_info, sizeof(write_info) - 1, sizeof(write_key), write_key);
    if (status) return status;
    status = derive(session, salt, sizeof(salt) - 1,
                    read_info, sizeof(read_info) - 1, sizeof(read_key), read_key);
    if (status) return status;

    frame[0] = (uint8_t)((sizeof(request) - 1) & 255);
    frame[1] = (uint8_t)((sizeof(request) - 1) >> 8);
    encrypt(write_key, nonce, frame, 2, request, sizeof(request) - 1,
            frame + 2, frame + 2 + sizeof(request) - 1);
    if (write_all(fd, frame, sizeof(frame)) != 0) return -31;
    for (round = 0; round < 4; ++round) {
        uint8_t header[2], ciphertext[1024], tag[16];
        size_t frame_length;
        if (read_all(fd, header, 2) != 0) return -32;
        frame_length = (size_t)header[0] | ((size_t)header[1] << 8);
        if (frame_length > sizeof(ciphertext) ||
            used + frame_length > sizeof(response) - 1) return -33;
        if (read_all(fd, ciphertext, frame_length) != 0 ||
            read_all(fd, tag, sizeof(tag)) != 0) return -34;
        nonce[0] = (uint8_t)round;
        status = decrypt(read_key, nonce, header, 2, ciphertext, frame_length,
                         response + used, tag);
        if (status) return status;
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
                uint8_t saved = used > header_length ? response[header_length] : 0;
                response[header_length] = 0;
                if (strncmp((const char *)response, "HTTP/1.1 200", 12) != 0)
                    return -35;
                field = strstr((char *)response, "Content-Length: ");
                if (!field || sscanf(field, "Content-Length: %zu", &expected_length) != 1)
                    return -36;
                response[header_length] = saved;
            }
        }
        if (header_length && used >= header_length + expected_length) {
            char line[100];
            int length = snprintf(line, sizeof(line),
                                  "[pair-qemu] encrypted /info HTTP 200 body=%zu\n",
                                  expected_length);
            if (length > 0 && (size_t)length < sizeof(line))
                (void)write_all(2, line, (size_t)length);
            return 0;
        }
    }
    return -37;
}

static int open_pairing_socket(void)
{
    struct sockaddr_in address;
    struct timeval timeout = { 5, 0 };
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(7000);
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void dump_m2_response(const uint8_t *response, size_t length)
{
    static const char prefix[] = "[dio-capture] pair-setup-m1-hex=";
    static const char digits[] = "0123456789abcdef";
    char line[sizeof(prefix) + 8192 * 2];
    size_t index;
    if (length > 8192) return;
    memcpy(line, prefix, sizeof(prefix) - 1);
    for (index = 0; index < length; ++index) {
        line[sizeof(prefix) - 1 + index * 2] = digits[response[index] >> 4];
        line[sizeof(prefix) + index * 2] = digits[response[index] & 15];
    }
    line[sizeof(prefix) - 1 + length * 2] = '\n';
    (void)write_all(2, line, sizeof(prefix) + length * 2);
}

static int post_pairing(int fd, const char *path, const uint8_t *request,
                        size_t request_length, uint8_t **body, size_t *body_length,
                        int dump_response)
{
    char header[256];
    uint8_t response[8192];
    size_t used = 0, header_length = 0, expected_length = 0, index;
    int length = snprintf(header, sizeof(header),
                          "POST %s HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                          "X-Apple-HKP: 0\r\nContent-Type: application/pairing+tlv8\r\n"
                          "Content-Length: %zu\r\nConnection: keep-alive\r\n\r\n",
                          path, request_length);
    if (length <= 0 || (size_t)length >= sizeof(header) ||
        write_all(fd, header, (size_t)length) != 0 ||
        write_all(fd, request, request_length) != 0) return -1;

    for (;;) {
        ssize_t count;
        if (used >= sizeof(response) - 1) return -2;
        count = read(fd, response + used, sizeof(response) - 1 - used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -3;
        used += (size_t)count;
        if (!header_length) {
            for (index = 0; index + 3 < used; ++index) {
                if (memcmp(response + index, "\r\n\r\n", 4) == 0) {
                    header_length = index + 4;
                    break;
                }
            }
            if (header_length) {
                char *field;
                uint8_t saved = used > header_length ? response[header_length] : 0;
                response[header_length] = 0;
                if (strncmp((const char *)response, "HTTP/1.1 200", 12) != 0) {
                    fprintf(stderr, "[pair-qemu] HTTP error: %.80s\n", response);
                    return -4;
                }
                if (!strstr((const char *)response,
                            "Content-Type: application/pairing+tlv8")) return -5;
                field = strstr((char *)response, "Content-Length: ");
                if (!field || sscanf(field, "Content-Length: %zu", &expected_length) != 1 ||
                    expected_length > sizeof(response) - header_length) return -6;
                response[header_length] = saved;
            }
        }
        if (header_length && used >= header_length + expected_length) break;
    }
    if (dump_response) dump_m2_response(response, header_length + expected_length);
    *body = malloc(expected_length ? expected_length : 1);
    if (!*body) return -7;
    memcpy(*body, response + header_length, expected_length);
    *body_length = expected_length;
    return 0;
}

static int run_pairing(const char *path, uint32_t type,
                       pair_create_fn create, pair_identifier_fn set_identifier,
                       pair_code_fn set_code, pair_exchange_fn exchange,
                       cf_release_fn release)
{
    void *session = NULL;
    uint8_t *out = NULL, *input = NULL;
    size_t out_length = 0, input_length = 0;
    uint8_t done = 0;
    int fd = -1, result, round;
    result = create(&session, NULL, type);
    if (result != 0 || !session) goto exit;
    result = set_identifier(session, "qemu-phone", 10);
    if (result != 0) goto exit;
    if (type == 1) {
        result = set_code(session, "3939", 4);
        if (result != 0) goto exit;
    }
    fd = open_pairing_socket();
    if (fd < 0) { result = -20; goto exit; }
    result = exchange(session, NULL, 0, &out, &out_length, &done);
    if (result != 0) goto exit;
    for (round = 1; round <= 3 && !done; ++round) {
        result = post_pairing(fd, path, out, out_length, &input, &input_length,
                              type == 1 && round == 1);
        free(out);
        out = NULL;
        if (result != 0) goto exit;
        {
            char line[128];
            int length = snprintf(line, sizeof(line),
                                  "[pair-qemu] %s response=%d body=%zu\n",
                                  path, round * 2, input_length);
            if (length > 0 && (size_t)length < sizeof(line))
                (void)write_all(2, line, (size_t)length);
        }
        result = exchange(session, input, input_length, &out, &out_length, &done);
        free(input);
        input = NULL;
        if (result != 0) goto exit;
    }
    if (!done) result = -21;
    if (result == 0 && type == 3) {
        int encrypted_status = probe_encrypted_info(fd, session);
        char line[100];
        int length = snprintf(line, sizeof(line),
                              "[pair-qemu] encrypted /info result=%d\n",
                              encrypted_status);
        if (length > 0 && (size_t)length < sizeof(line))
            (void)write_all(2, line, (size_t)length);
    }
exit:
    {
        char line[128];
        int length = snprintf(line, sizeof(line),
                              "[pair-qemu] %s result=%d done=%u\n", path, result, done);
        if (length > 0 && (size_t)length < sizeof(line))
            (void)write_all(2, line, (size_t)length);
    }
    free(out);
    free(input);
    if (fd >= 0) close(fd);
    if (session) release(session);
    return result;
}

void mhi2q_probe_pairing_full(void)
{
    pair_create_fn create = (pair_create_fn)dlsym(RTLD_DEFAULT, "PairingSessionCreate");
    pair_identifier_fn set_identifier =
        (pair_identifier_fn)dlsym(RTLD_DEFAULT, "PairingSessionSetIdentifier");
    pair_code_fn set_code =
        (pair_code_fn)dlsym(RTLD_DEFAULT, "PairingSessionSetSetupCode");
    pair_exchange_fn exchange =
        (pair_exchange_fn)dlsym(RTLD_DEFAULT, "PairingSessionExchange");
    cf_release_fn release = (cf_release_fn)dlsym(RTLD_DEFAULT, "CFRelease");
    if (!create || !set_identifier || !set_code || !exchange || !release) {
        fprintf(stderr, "[pair-qemu] SDK pairing symbols missing\n");
        return;
    }
    if (run_pairing("/pair-setup", 1, create, set_identifier,
                    set_code, exchange, release) == 0) {
        struct stat keychain;
        if (stat("/dev/shmem/airplay_keychains/default.keychain", &keychain) != 0 ||
            (keychain.st_mode & 0777) != 0600 || keychain.st_size == 0) {
            static const char error[] = "[pair-qemu] keychain mode or size invalid\n";
            (void)write_all(2, error, sizeof(error) - 1);
            return;
        }
        {
            char line[100];
            int length = snprintf(line, sizeof(line),
                                  "[pair-qemu] keychain mode=0600 bytes=%ld\n",
                                  (long)keychain.st_size);
            if (length > 0 && (size_t)length < sizeof(line))
                (void)write_all(2, line, (size_t)length);
        }
        (void)run_pairing("/pair-verify", 3, create, set_identifier,
                          set_code, exchange, release);
    }
}
