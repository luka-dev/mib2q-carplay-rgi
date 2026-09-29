/* This translation unit and the SDK archive are symbol-namespaced at build
 * time. The 280 PairingSession/CFLite state never enters stock AirPlay 210. */
#include "pairing.h"
#include "PairingUtils.h"
#include "NetTransportChaCha20Poly1305.h"
#include "RandomNumberUtils.h"
#include "SHAUtils.h"
#include "ChaCha20Poly1305.h"
#include CF_HEADER
#include ED25519_HEADER
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef O_NOFOLLOW
/* QNX 6.5 has no O_NOFOLLOW; lstat/fstat below verify regular file identity. */
#define O_NOFOLLOW 0
#endif

/* Disk format is fixed-width bytes, independent of host endianness/ABI.
 * Identity: magic[8], id_length:u16LE, ID[252], EdPK[32], EdSK[32].
 * Peer: the same prefix and ID followed by EdPK[32], without a secret key. */
#define STORE_PREFIX 262
#define IDENTITY_SIZE (STORE_PREFIX + 64)
#define PEER_SIZE (STORE_PREFIX + 32)
ulog_define(CPPairingPrivate, kLogLevelOff, kLogFlags_None, "CPP", NULL);
struct cp_pair_store {
    char directory[768];
    int lock_fd;
    char id[CP_PAIR_ID_MAX + 1];
    uint8_t pk[32], sk[32];
};
struct cp_pair_connection {
    cp_pair_store *store;
    PairingSessionRef setup, verify;
    NetTransportDelegate transport;
    int verify_done, encrypted, failed;
};

static void erase(void *p, size_t n)
{
    volatile uint8_t *b = p;
    while (n--) *b++ = 0;
}

static int path_for(cp_pair_store *s, const char *name, char out[1024])
{
    int n = snprintf(out, 1024, "%s/%s", s->directory, name);
    return n > 0 && n < 1024 ? 0 : EINVAL;
}

static int read_record(cp_pair_store *s, const char *name, uint8_t *data, size_t size)
{
    char path[1024];
    struct stat st, opened;
    size_t offset = 0;
    int fd, err = path_for(s, name, path);
    if (err) return err;
    /* The directory is private and locked by this store owner. */
    if (lstat(path, &st) != 0) return errno;
    if (!S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & 077) || st.st_size != (off_t)size) return EINVAL;
    fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return errno;
    if (fstat(fd, &opened) != 0 || !S_ISREG(opened.st_mode) ||
        opened.st_dev != st.st_dev || opened.st_ino != st.st_ino) {
        close(fd); return EINVAL;
    }
    while (offset < size) {
        ssize_t n = read(fd, data + offset, size - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { err = n < 0 ? errno : EIO; break; }
        offset += (size_t)n;
    }
    close(fd);
    return err;
}

static int write_record(cp_pair_store *s, const char *name, const uint8_t *data, size_t size)
{
    char path[1024], temporary[1056];
    size_t offset = 0;
    int fd, err = path_for(s, name, path);
    if (err) return err;
    snprintf(temporary, sizeof(temporary), "%s/.write.XXXXXX", s->directory);
    fd = mkstemp(temporary);
    if (fd < 0) return errno;
    if (fchmod(fd, 0600) != 0) err = errno;
    while (!err && offset < size) {
        ssize_t n = write(fd, data + offset, size - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { err = n < 0 ? errno : EIO; break; }
        offset += (size_t)n;
    }
    if (!err && fsync(fd) != 0) err = errno;
    if (close(fd) != 0 && !err) err = errno;
    if (!err && rename(temporary, path) != 0) err = errno;
    if (err) unlink(temporary);
    return err;
}

static int unpack(const uint8_t *record, const char magic[8], const void *id, size_t id_len)
{
    size_t n = (size_t)record[8] | ((size_t)record[9] << 8);
    if (memcmp(record, magic, 8) || !n || n > CP_PAIR_ID_MAX) return EINVAL;
    if (id && (id_len != n || memcmp(record + 10, id, n))) return EINVAL;
    return 0;
}

static void pack(uint8_t *record, size_t size, const char magic[8], const void *id, size_t n)
{
    memset(record, 0, size);
    memcpy(record, magic, 8);
    record[8] = (uint8_t)n;
    record[9] = (uint8_t)(n >> 8);
    memcpy(record + 10, id, n);
}

int cp_pair_store_open(const char *directory, cp_pair_store **out)
{
    cp_pair_store *s;
    char path[1024];
    uint8_t record[IDENTITY_SIZE], uuid[16];
    struct stat st, opened;
    struct flock lock;
    int err;
    if (!directory || !out || directory[0] != '/' || strlen(directory) >= 768) return EINVAL;
    *out = NULL;
    s = calloc(1, sizeof(*s));
    if (!s) return ENOMEM;
    s->lock_fd = -1;
    strcpy(s->directory, directory);
    if (mkdir(directory, 0700) != 0 && errno != EEXIST) { err = errno; goto fail; }
    if (lstat(directory, &st) != 0) { err = errno; goto fail; }
    if (!S_ISDIR(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 077)) { err = EACCES; goto fail; }
    path_for(s, ".owner", path);
    s->lock_fd = open(path, O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    if (s->lock_fd < 0) { err = errno; goto fail; }
    if (lstat(path, &st) || fstat(s->lock_fd, &opened) ||
        !S_ISREG(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 077) ||
        st.st_dev != opened.st_dev || st.st_ino != opened.st_ino) {
        err = EACCES; goto fail;
    }
    if (fcntl(s->lock_fd, F_SETFD, FD_CLOEXEC) != 0) { err = errno; goto fail; }
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(s->lock_fd, F_SETLK, &lock) != 0) { err = errno; goto fail; }
    err = read_record(s, "identity.v1", record, sizeof(record));
    if (err == ENOENT) {
        err = RandomBytes(uuid, sizeof(uuid));
        if (err) goto fail;
        uuid[6] = (uuid[6] & 15) | 64;
        uuid[8] = (uuid[8] & 63) | 128;
        snprintf(s->id, sizeof(s->id),
                 "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                 uuid[0],uuid[1],uuid[2],uuid[3],uuid[4],uuid[5],uuid[6],uuid[7],
                 uuid[8],uuid[9],uuid[10],uuid[11],uuid[12],uuid[13],uuid[14],uuid[15]);
        Ed25519_make_key_pair(s->pk, s->sk);
        pack(record, sizeof(record), "CPIDENT1", s->id, strlen(s->id));
        memcpy(record + STORE_PREFIX, s->pk, 32);
        memcpy(record + STORE_PREFIX + 32, s->sk, 32);
        err = write_record(s, "identity.v1", record, sizeof(record));
    } else if (!err) {
        err = unpack(record, "CPIDENT1", NULL, 0);
        if (!err) {
            size_t n = record[8] | ((size_t)record[9] << 8);
            memcpy(s->id, record + 10, n);
            if (memchr(s->id, 0, n)) { err = EINVAL; goto fail; }
            memcpy(s->pk, record + STORE_PREFIX, 32);
            memcpy(s->sk, record + STORE_PREFIX + 32, 32);
        }
    }
    if (err) goto fail;
    erase(record, sizeof(record));
    *out = s;
    return 0;
fail:
    erase(record, sizeof(record));
    cp_pair_store_close(s);
    return err;
}

void cp_pair_store_close(cp_pair_store *s)
{
    if (!s) return;
    if (s->lock_fd >= 0) close(s->lock_fd);
    erase(s, sizeof(*s));
    free(s);
}

int cp_pair_identity(cp_pair_store *s, char id[CP_PAIR_ID_MAX + 1], uint8_t pk[32])
{
    if (!s) return EINVAL;
    if (id) strcpy(id, s->id);
    if (pk) memcpy(pk, s->pk, 32);
    return 0;
}

static OSStatus copy_identity(Boolean create, char **id, uint8_t *pk, uint8_t *sk, void *context)
{
    cp_pair_store *s = context;
    (void)create;
    if (id && !(*id = strdup(s->id))) return kNoMemoryErr;
    if (pk) memcpy(pk, s->pk, 32);
    if (sk) memcpy(sk, s->sk, 32);
    return 0;
}

static int peer_name(const void *id, size_t n, char name[72])
{
    uint8_t digest[64];
    size_t i;
    if (!id || !n || n > CP_PAIR_ID_MAX) return kSizeErr;
    SHA512(id, n, digest);
    memcpy(name, "peer-", 5);
    /* QNX4 allows 48-byte names. 160 digest bits + prefix fit in 45 bytes;
     * the full identifier is checked inside the record as well. */
    for (i = 0; i < 20; ++i) snprintf(name + 5 + i * 2, 3, "%02x", digest[i]);
    return 0;
}

static OSStatus find_peer(const void *id, size_t n, uint8_t pk[32], void *context)
{
    char name[72];
    uint8_t record[PEER_SIZE];
    int err = peer_name(id, n, name);
    if (!err) err = read_record(context, name, record, sizeof(record));
    if (!err) err = unpack(record, "CPPEERS1", id, n);
    if (!err) memcpy(pk, record + STORE_PREFIX, 32);
    return err == ENOENT ? kNotFoundErr : err;
}

static OSStatus save_peer(const void *id, size_t n, const uint8_t pk[32], void *context)
{
    char name[72];
    uint8_t record[PEER_SIZE];
    int err = peer_name(id, n, name);
    if (err) return err;
    pack(record, sizeof(record), "CPPEERS1", id, n);
    memcpy(record + STORE_PREFIX, pk, 32);
    return write_record(context, name, record, sizeof(record));
}

int cp_pair_connection_create(cp_pair_store *store, cp_pair_connection **out)
{
    if (!store || !out) return EINVAL;
    *out = calloc(1, sizeof(**out));
    if (!*out) return ENOMEM;
    (*out)->store = store;
    return 0;
}

void cp_pair_connection_free(cp_pair_connection *c)
{
    if (!c) return;
    if (c->setup) CFRelease(c->setup);
    if (c->verify) CFRelease(c->verify);
    if (c->transport.finalize_f) c->transport.finalize_f(c->transport.context);
    erase(c, sizeof(*c));
    free(c);
}

int cp_pair_exchange(cp_pair_connection *c, int verify, const void *input, size_t n,
                     uint8_t **output, size_t *length, int *done)
{
    PairingSessionRef *session;
    PairingDelegate delegate;
    Boolean finished = false;
    int err;
    if (!c || !input || !n || n > CP_PAIR_BODY_MAX || !output || !length || !done ||
        c->encrypted || c->verify_done || c->failed) return kStateErr;
    *output = NULL; *length = 0; *done = 0;
    session = verify ? &c->verify : &c->setup;
    if (!*session) {
        PairingDelegateInit(&delegate);
        delegate.context = c->store;
        delegate.copyIdentity_f = copy_identity;
        delegate.findPeer_f = find_peer;
        delegate.savePeer_f = save_peer;
        err = PairingSessionCreate(session, &delegate,
                 verify ? kPairingSessionType_VerifyServer : kPairingSessionType_SetupServer);
        if (err) return err;
        PairingSessionSetLogging(*session, &log_category_from_name(CPPairingPrivate));
        if (!verify && (err = PairingSessionSetSetupCode(*session, "3939", 4))) return err;
    }
    err = PairingSessionExchange(*session, input, n, output, length, &finished);
    if (err) { c->failed = 1; return err; }
    *done = finished;
    if (finished && verify) c->verify_done = 1;
    if (finished && !verify) { CFRelease(c->setup); c->setup = NULL; }
    return 0;
}

int cp_pair_derive(cp_pair_connection *c, const char *salt, const char *info, uint8_t key[32])
{
    if (!c || !c->verify_done || c->failed || !salt || !info || !key) return kStateErr;
    return PairingSessionDeriveKey(c->verify, salt, strlen(salt), info, strlen(info), 32, key);
}

int cp_pair_commit(cp_pair_connection *c, int fd)
{
    uint8_t read_key[32] = {0}, write_key[32] = {0};
    int err;
    if (!c || c->encrypted || c->failed || fd < 0) return kStateErr;
    err = cp_pair_derive(c, "Control-Salt", "Control-Write-Encryption-Key", read_key);
    if (!err) err = cp_pair_derive(c, "Control-Salt", "Control-Read-Encryption-Key", write_key);
    if (!err) err = NetTransportChaCha20Poly1305Configure(&c->transport, NULL,
                                                        read_key, NULL, write_key, NULL);
    erase(read_key, sizeof(read_key)); erase(write_key, sizeof(write_key));
    if (!err) err = c->transport.initialize_f(fd, c->transport.context);
    if (err) c->failed = 1;
    else c->encrypted = 1;
    return err;
}

int cp_pair_is_encrypted(const cp_pair_connection *c) { return c && c->encrypted; }
int cp_pair_transport_create(int fd, const uint8_t read_key[32],
                             const uint8_t write_key[32], cp_pair_connection **out)
{
    int err;
    cp_pair_connection *c;
    if (fd < 0 || !read_key || !write_key || !out) return EINVAL;
    *out = NULL;
    c = calloc(1, sizeof(*c));
    if (!c) return ENOMEM;
    err = NetTransportChaCha20Poly1305Configure(&c->transport, NULL,
                                               read_key, NULL, write_key, NULL);
    if (!err) err = c->transport.initialize_f(fd, c->transport.context);
    if (err) { cp_pair_connection_free(c); return err; }
    c->encrypted = 1;
    *out = c;
    return 0;
}

int cp_pair_open(const uint8_t key[32], const uint8_t nonce[8],
                 const void *aad, size_t aad_len, const void *cipher, size_t length,
                 const uint8_t tag[16], void *plain)
{
    int err = chacha20_poly1305_decrypt_all_64x64(key, nonce, aad, aad_len,
                                                cipher, length, plain, tag);
    if (err) erase(plain, length);
    return err;
}

void cp_pair_seal(const uint8_t key[32], const uint8_t nonce[8],
                  const void *aad, size_t aad_len, const void *plain, size_t length,
                  void *cipher, uint8_t tag[16])
{
    chacha20_poly1305_encrypt_all_64x64(key, nonce, aad, aad_len, plain, length, cipher, tag);
}
int cp_pair_read(cp_pair_connection *c, void *buffer, size_t capacity, size_t *length)
{
    int err;
    if (!cp_pair_is_encrypted(c) || c->failed) return kStateErr;
    err = c->transport.read_f(buffer, capacity, length, c->transport.context);
    if (err && err != EWOULDBLOCK && err != EAGAIN) c->failed = 1;
    return err;
}
int cp_pair_writev(cp_pair_connection *c, struct iovec **vectors, int *count)
{
    int err;
    if (!cp_pair_is_encrypted(c) || c->failed) return kStateErr;
    err = c->transport.writev_f(vectors, count, c->transport.context);
    if (err && err != EWOULDBLOCK && err != EAGAIN) c->failed = 1;
    return err;
}
