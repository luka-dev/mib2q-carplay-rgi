/* QEMU-only LD_PRELOAD fake for the ordinary daemon's QNX I2C calls. */
#define _DEFAULT_SOURCE
#include <devctl.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>
#include "software_mfi_fixture.h"

struct i2c_header {
    uint32_t address, format, length, stop;
};
struct i2c_sendrecv_header {
    uint32_t address, format, send_length, receive_length, stop;
};
#define I2C_SEND __DIOT(_DCMD_MISC, 5, struct i2c_header)
#define I2C_RECV __DIOTF(_DCMD_MISC, 6, struct i2c_header)
#define I2C_SENDRECV __DIOTF(_DCMD_MISC, 7, struct i2c_sendrecv_header)
#define I2C_LOCK __DION(_DCMD_MISC, 8)
#define I2C_UNLOCK __DION(_DCMD_MISC, 9)

static int fake_fd = -1;
static uint8_t selected_register;
static int signed_challenge;
static int reported_combined_read;
static int bus_locked;
static int injected_nack;
static int injected_short;
static int fail_first_certificate_state;
static struct timespec fail_first_started;
static struct mfiq_fixture software_fixture;
static int software_active;
static uint8_t challenge_digest[20], software_signature[128];
static int challenge_ready;

static int load_software_fixture(const char *path)
{
    uint8_t data[MFIQ_MAX_FILE + 1];
    size_t size;
    int error;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size = fread(data, 1, sizeof(data), file);
    error = ferror(file);
    fclose(file);
    if (error || mfiq_decode(&software_fixture, data, size)) return -1;
    software_active = 1;
    fprintf(stderr, "[mfi-qemu] software fixture loaded certificate=%u cases=%u\n",
            (unsigned)software_fixture.certificate_length,
            (unsigned)software_fixture.case_count);
    return 0;
}

__attribute__((constructor)) static void stock_mfi_probe_loaded(void)
{
    if (getenv("MHI2Q_STOCK_MFI_QEMU")) {
        static const char marker[] = "[stock-mfi-qemu] I2C mock preloaded\n";
        (void)write(2, marker, sizeof(marker) - 1);
    }
}

void *dlopen(const char *path, int flags)
{
    static void *(*real_dlopen)(const char *, int);
    if (!real_dlopen)
        real_dlopen = (void *(*)(const char *, int))dlsym(RTLD_NEXT, "dlopen");
    if (!real_dlopen) return NULL;
    if (path && strcmp(path, "libdns_sd.so.1") == 0 &&
        getenv("MHI2Q_PHONE_QEMU_CONTROL")) {
        fprintf(stderr, "[dns-qemu] daemon DNS-SD mock loaded\n");
        return real_dlopen("libqemu_dns_sd.so", flags);
    }
    return real_dlopen(path, flags);
}

unsigned int if_nametoindex(const char *name)
{
    static unsigned int (*real_if_nametoindex)(const char *);
    if (!real_if_nametoindex)
        real_if_nametoindex = (unsigned int (*)(const char *))
            dlsym(RTLD_NEXT, "if_nametoindex");
    if (!real_if_nametoindex) return 0;
    if (name && strcmp(name, "uap0") == 0 &&
        getenv("MHI2Q_PHONE_QEMU_CONTROL"))
        return real_if_nametoindex("lo0");
    return real_if_nametoindex(name);
}

int open(const char *path, int flags, ...)
{
    static int (*real_open)(const char *, int, ...);
    if (!real_open)
        real_open = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    if (!real_open) {
        errno = ENOSYS;
        return -1;
    }
    if (path && strcmp(path,"/dev/urandom")==0 && getenv("MHI2Q_PHONE_QEMU_INTEGRATED"))
        return real_open(getenv("MHI2Q_INTEGRATED_DAEMON_ITER") && !strcmp(getenv("MHI2Q_INTEGRATED_DAEMON_ITER"),"1")?"/proc/boot/integrated-entropy-next":"/proc/boot/integrated-entropy",O_RDONLY);
    if (path && (strcmp(path, "/dev/shmem/qemu_mfi") == 0 ||
                 (strcmp(path, "/dev/i2c8") == 0 && getenv("MHI2Q_STOCK_MFI_QEMU")))) {
        const char *fixture = getenv("MHI2Q_MFI_QEMU_FIXTURE");
        if (fixture && !software_active && load_software_fixture(fixture)) {
            errno = EINVAL;
            return -1; /* Never fall back to dummy authentication. */
        }
        if (getenv("MHI2Q_STOCK_MFI_QEMU")) {
            static const char marker[] = "[stock-mfi-qemu] intercepted open\n";
            (void)write(2, marker, sizeof(marker) - 1);
        }
        int fd = real_open("/dev/null", O_RDWR);
        fake_fd = fd;
        return fd;
    }
    if (flags & O_CREAT) {
        va_list args;
        int mode;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
        return real_open(path, flags, mode);
    }
    return real_open(path, flags);
}

int open64(const char *path, int flags, ...)
{
    static int (*real_open64)(const char *, int, ...);
    if (path && strcmp(path, "/dev/i2c8") == 0 &&
        getenv("MHI2Q_STOCK_MFI_QEMU")) {
        static const char marker[] = "[stock-mfi-qemu] intercepted open64\n";
        (void)write(2, marker, sizeof(marker) - 1);
        return open(path, flags);
    }
    if (!real_open64)
        real_open64 = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open64");
    if (!real_open64) {
        errno = ENOSYS;
        return -1;
    }
    if (flags & O_CREAT) {
        va_list args;
        int mode;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
        return real_open64(path, flags, mode);
    }
    return real_open64(path, flags);
}

int close(int fd)
{
    static int (*real_close)(int);
    if (!real_close)
        real_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
    if (fd == fake_fd)
        fake_fd = -1;
    return real_close ? real_close(fd) : -1;
}

int devctl(int fd, int command, void *data, size_t bytes, int *info)
{
    static int (*real_devctl)(int, int, void *, size_t, int *);
    if (fd == fake_fd && command == I2C_LOCK) {
        if (bus_locked) return EBUSY;
        bus_locked = 1;
        return 0;
    }
    if (fd == fake_fd && command == I2C_UNLOCK) {
        if (!bus_locked) return EINVAL;
        bus_locked = 0;
        if (fail_first_certificate_state == 1) {
            struct timespec finished;
            long elapsed_ms = -1;
            if (clock_gettime(CLOCK_MONOTONIC, &finished) == 0)
                elapsed_ms = (finished.tv_sec - fail_first_started.tv_sec) * 1000L +
                    (finished.tv_nsec - fail_first_started.tv_nsec) / 1000000L;
            fail_first_certificate_state = 2;
            fprintf(stderr, "[mfi-qemu] persistent NACK released lock elapsed_ms=%ld\n",
                    elapsed_ms);
        }
        return 0;
    }
    if (!real_devctl)
        real_devctl = (int (*)(int, int, void *, size_t, int *))
            dlsym(RTLD_NEXT, "devctl");
    return real_devctl ? real_devctl(fd, command, data, bytes, info) : ENOSYS;
}

int devctlv(int fd, int command, int send_count, int receive_count,
            const struct iovec *send, const struct iovec *receive,
            int *transferred)
{
    static int (*real_devctlv)(int, int, int, int, const struct iovec *,
                               const struct iovec *, int *);
    const struct i2c_header *header;
    uint8_t *out;
    size_t length;
    if (fd != fake_fd) {
        if (!real_devctlv)
            real_devctlv = (int (*)(int, int, int, int, const struct iovec *,
                                    const struct iovec *, int *))
                dlsym(RTLD_NEXT, "devctlv");
        return real_devctlv ? real_devctlv(fd, command, send_count,
                                           receive_count, send, receive,
                                           transferred) : ENOSYS;
    }
    if (!bus_locked) return EACCES;
    if ((unsigned)command == I2C_SENDRECV) {
        const struct i2c_sendrecv_header *combined;
        if (!send || !receive || send_count != 2 || receive_count != 2 ||
            send[0].iov_len != sizeof(*combined) ||
            receive[0].iov_len != sizeof(*combined) ||
            send[1].iov_len != 1)
            return EINVAL;
        combined = (const struct i2c_sendrecv_header *)send[0].iov_base;
        if (combined->address != 0x11 || combined->format != 2 ||
            combined->send_length != 1 || combined->stop != 1 ||
            combined->receive_length != receive[1].iov_len)
            return EINVAL;
        selected_register = *(const uint8_t *)send[1].iov_base;
        if (getenv("MHI2Q_MFI_QEMU_FAIL_FIRST") &&
            selected_register == 0x30 && fail_first_certificate_state != 2) {
            if (fail_first_certificate_state == 0) {
                fail_first_certificate_state = 1;
                (void)clock_gettime(CLOCK_MONOTONIC, &fail_first_started);
                fprintf(stderr, "[mfi-qemu] persistent NACK began\n");
            }
            return EAGAIN;
        }
        if (getenv("MHI2Q_MFI_QEMU_RETRY") &&
            selected_register == 0x30 && !injected_nack) {
            injected_nack = 1;
            fprintf(stderr, "[mfi-qemu] injected transient NACK on combined read\n");
            return EAGAIN;
        }
        if (getenv("MHI2Q_MFI_QEMU_RETRY") &&
            selected_register == 0x31 && !injected_short) {
            injected_short = 1;
            if (transferred) *transferred = 0;
            fprintf(stderr, "[mfi-qemu] injected short combined read\n");
            return 0;
        }
        if (!reported_combined_read) {
            fprintf(stderr, "[mfi-qemu] combined SENDRECV command=0x%08x\n",
                    (unsigned)command);
            reported_combined_read = 1;
        }
        out = (uint8_t *)receive[1].iov_base;
        length = receive[1].iov_len;
        goto read_response;
    }
    if (!send || !receive || send_count < 1 ||
        send[0].iov_len != sizeof(struct i2c_header))
        return EINVAL;
    header = (const struct i2c_header *)send[0].iov_base;
    if (header->address != 0x11 || header->format != 2 || header->stop != 1)
        return EINVAL;
    if ((unsigned)command == I2C_SEND && send_count >= 2 && receive_count == 1 &&
        send[1].iov_len == 1) {
        selected_register = *(const uint8_t *)send[1].iov_base;
        if (send_count == 3) {
            if (selected_register == 0x20) {
                const uint8_t *challenge = (const uint8_t *)send[2].iov_base;
                if (send[2].iov_len != 22 || challenge[0] != 0 ||
                    challenge[1] != 20)
                    return EINVAL;
                memcpy(challenge_digest, challenge + 2, 20);
                challenge_ready = 1;
                signed_challenge = 0;
            } else if (selected_register == 0x10) {
                if (send[2].iov_len != 1 ||
                    *(const uint8_t *)send[2].iov_base != 1)
                    return EINVAL;
                if (software_active && (!challenge_ready ||
                    mfiq_signature(&software_fixture, challenge_digest, software_signature)))
                    return EINVAL;
                signed_challenge = 1;
            } else {
                return EINVAL;
            }
        }
        if (transferred)
            *transferred = (int)header->length;
        return 0;
    }
    if ((unsigned)command != I2C_RECV || send_count != 1 || receive_count != 2)
        return EINVAL;
    out = (uint8_t *)receive[1].iov_base;
    length = receive[1].iov_len;
    if (length != header->length)
        return EINVAL;
read_response:
    if (software_active && selected_register >= 0x31 && selected_register <= 0x3a) {
        size_t offset = (size_t)(selected_register - 0x31) * 128u;
        size_t available;
        if (offset >= software_fixture.certificate_length) return EINVAL;
        available = software_fixture.certificate_length - offset;
        if (available > 128u) available = 128u;
        if (length != available) return EINVAL;
        memcpy(out, software_fixture.certificate + offset, length);
        if (offset + length == software_fixture.certificate_length)
            fprintf(stderr, "[mfi-qemu] software certificate read=%u\n",
                    (unsigned)software_fixture.certificate_length);
        if (transferred) *transferred = (int)length;
        return 0;
    }
    switch (selected_register) {
    case 0x30:
        if (length != 2) return EINVAL;
        out[0] = software_active ? (uint8_t)(software_fixture.certificate_length >> 8) : 0;
        out[1] = software_active ? (uint8_t)software_fixture.certificate_length : 4;
        break;
    case 0x31:
        if (length != 4) return EINVAL;
        memcpy(out, "CERT", 4);
        fprintf(stderr, "[mfi-qemu] certificate read=4\n");
        break;
    case 0x10:
        if (length != 1 || !signed_challenge) return EINVAL;
        out[0] = 0;
        break;
    case 0x11:
        if (length != 2 || !signed_challenge) return EINVAL;
        out[0] = 0; out[1] = software_active ? 128 : 20;
        break;
    case 0x12:
        if (length != (software_active ? 128u : 20u) || !signed_challenge) return EINVAL;
        if (software_active) {
            memcpy(out, software_signature, 128);
            if (getenv("MHI2Q_MFI_QEMU_TAMPER")) out[0] ^= 1;
            fprintf(stderr, "[mfi-qemu] software signature read=128\n");
        } else {
            memset(out, 0x5a, 20);
            fprintf(stderr, "[mfi-qemu] signature read=20\n");
        }
        break;
    default:
        return EINVAL;
    }
    if (transferred)
        *transferred = (int)length;
    return 0;
}
