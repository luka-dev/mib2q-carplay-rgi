/* Small QNX 6.5 I2C ABI shim for the Rust MFi state machine. */
#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

struct i2c_header {
    uint32_t address;
    uint32_t format;
    uint32_t length;
    uint32_t stop;
};

struct i2c_sendrecv_header {
    uint32_t address;
    uint32_t format;
    uint32_t send_length;
    uint32_t receive_length;
    uint32_t stop;
};

#define I2C_FORMAT_7BIT 2u
#define I2C_SEND __DIOT(_DCMD_MISC, 5, struct i2c_header)
#define I2C_RECV __DIOTF(_DCMD_MISC, 6, struct i2c_header)
#define I2C_SENDRECV __DIOTF(_DCMD_MISC, 7, struct i2c_sendrecv_header)
#define I2C_LOCK __DION(_DCMD_MISC, 8)
#define I2C_UNLOCK __DION(_DCMD_MISC, 9)

_Static_assert(sizeof(struct i2c_header) == 16, "QNX I2C header ABI");
_Static_assert(sizeof(struct i2c_sendrecv_header) == 20, "QNX I2C combined header ABI");

static int monotonic_ms(uint64_t *out) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return -errno;
    *out = (uint64_t)time.tv_sec * 1000u + (uint64_t)time.tv_nsec / 1000000u;
    return 0;
}

/* The auth IC may NACK while processing. The stock QNX code retries writes
 * and combined register-select/data reads at 5 ms intervals with a 2 s
 * deadline for each operation. This helper handles the write command. */
static int transfer_until(int fd, int command, int send_parts, int receive_parts,
                          struct iovec *send, struct iovec *receive,
                          struct i2c_header *header, unsigned expected,
                          int check_count,
                          uint64_t deadline) {
    const struct i2c_header original = *header;
    for (;;) {
        int transferred = 0;
        int result;
        int clock_result;
        uint64_t now;
        *header = original;
        header->length = expected;
        result = devctlv(fd, command, send_parts, receive_parts,
                         send, receive, &transferred);
        if (result == 0 && (!check_count || transferred == (int)expected)) return 0;
        if (result == 0) result = EIO;
        clock_result = monotonic_ms(&now);
        if (clock_result != 0) return clock_result;
        if (now >= deadline) return -result;
        usleep(5000);
    }
}

int mhi2q_i2c_open(const char *path) {
    int fd = open(path, O_RDWR);
    return fd < 0 ? -errno : fd;
}

int mhi2q_i2c_close(int fd) { return close(fd) == 0 ? 0 : -errno; }

int mhi2q_i2c_lock(int fd) {
    int result = devctl(fd, I2C_LOCK, 0, 0, 0);
    return result == 0 ? 0 : -result;
}

int mhi2q_i2c_unlock(int fd) {
    int result = devctl(fd, I2C_UNLOCK, 0, 0, 0);
    return result == 0 ? 0 : -result;
}

int mhi2q_i2c_write(int fd, unsigned address, unsigned char reg,
                    const unsigned char *data, unsigned length) {
    struct i2c_header header = {address, I2C_FORMAT_7BIT, length + 1u, 1u};
    struct iovec send[3], receive[1];
    uint64_t now;
    int result = monotonic_ms(&now);
    if (result != 0) return result;
    const uint64_t deadline = now + 2000u;
    SETIOV(&send[0], &header, sizeof(header));
    SETIOV(&send[1], &reg, 1);
    SETIOV(&send[2], data, length);
    SETIOV(&receive[0], &header, sizeof(header));
    return transfer_until(fd, I2C_SEND, 3, 1, send, receive,
                          &header, length + 1u, 1, deadline);
}

int mhi2q_i2c_read(int fd, unsigned address, unsigned char reg,
                   unsigned char *data, unsigned length) {
    struct i2c_sendrecv_header header = {
        address, I2C_FORMAT_7BIT, 1u, length, 1u
    };
    struct iovec send[2], receive[2];
    uint64_t now;
    int result = monotonic_ms(&now);
    if (result != 0) return result;
    const uint64_t deadline = now + 2000u;
    SETIOV(&send[0], &header, sizeof(header));
    SETIOV(&send[1], &reg, 1);
    SETIOV(&receive[0], &header, sizeof(header));
    SETIOV(&receive[1], data, length);
    for (;;) {
        int transferred = 0;
        header = (struct i2c_sendrecv_header){
            address, I2C_FORMAT_7BIT, 1u, length, 1u
        };
        result = devctlv(fd, I2C_SENDRECV, 2, 2, send, receive, &transferred);
        if (result == 0 && transferred == (int)length) return 0;
        if (result == 0) result = EIO;
        int clock_result = monotonic_ms(&now);
        if (clock_result != 0) return clock_result;
        if (now >= deadline) return -result;
        usleep(5000);
    }
}
