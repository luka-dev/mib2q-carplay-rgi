/* Disposable QNX fixture: unpack an unchanged zlib payload onto a writable
 * filesystem. This is a data transporter, not an ELF editor or guest patcher.
 * The host checks SHA-256; the guest checks size/CRC and byte-exact readback. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#define LIMIT (8UL * 1024 * 1024)

static int unpack(char **argv) {
    unsigned char *packed = NULL, *plain = NULL;
    int in = -1, out = -1, created = 0, result = 1;
    char *end;
    unsigned long expected, checksum;
    struct stat st;
    errno = 0;
    expected = strtoul(argv[3], &end, 10);
    if (errno || *end || !expected || expected > LIMIT) return 2;
    checksum = strtoul(argv[4], &end, 16);
    if (errno || *end || checksum > UINT32_MAX) return 2;
    in = open(argv[1], O_RDONLY);
    if (in < 0 || fstat(in, &st) || st.st_size <= 0 || st.st_size > (off_t)LIMIT) goto done;
    packed = malloc((size_t)st.st_size);
    plain = malloc(expected);
    if (!packed || !plain) goto done;
    size_t n = 0;
    while (n < (size_t)st.st_size) {
        ssize_t got = read(in, packed + n, (size_t)st.st_size - n);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) goto done;
        n += (size_t)got;
    }
    uLongf size = expected;
    if (uncompress(plain, &size, packed, n) != Z_OK || size != expected ||
        crc32(0, plain, size) != checksum) goto done;
    out = open(argv[2], O_RDWR | O_CREAT | O_EXCL, 0700);
    if (out < 0) goto done;
    created = 1;
    n = 0;
    while (n < size) {
        ssize_t wrote = write(out, plain + n, size - n);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) goto done;
        n += (size_t)wrote;
    }
    if (lseek(out, 0, SEEK_SET) < 0) goto done;
    unsigned char check[4096];
    n = 0;
    while (n < size) {
        size_t count = size - n;
        if (count > sizeof(check)) count = sizeof(check);
        ssize_t got = read(out, check, count);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0 || memcmp(check, plain + n, (size_t)got)) goto done;
        n += (size_t)got;
    }
    if (fstat(out, &st) || st.st_size != (off_t)size || fchmod(out, 0755)) goto done;
    printf("SI-PAYLOAD: verified size=%lu crc32=%08lx path=%s\n",
           (unsigned long)size, checksum, argv[2]);
    result = 0;
done:
    if (result) fprintf(stderr, "SI-PAYLOAD: FAIL path=%s errno=%d\n", argv[2], errno);
    if (in >= 0) close(in);
    if (out >= 0 && close(out)) result = 1;
    if (result && created) unlink(argv[2]);
    free(packed);
    free(plain);
    return result;
}

int main(int argc, char **argv) {
    if (argc < 5 || (argc - 1) % 4) return 2;
    for (int i = 1; i < argc; i += 4) {
        int result = unpack(argv + i - 1);
        if (result) return result;
    }
    return 0;
}
