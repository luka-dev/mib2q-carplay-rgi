/* Compile-only check of the hand-written production shim against the QNX BSP
 * header. The Docker SDK omits hw/i2c.h; the local QEMU BSP supplies it.
 * This file is not linked into the daemon or shipped to the head unit. */
#include "../src/qnx_i2c.c"
#include <hw/i2c.h>
#include <stddef.h>

_Static_assert(I2C_FORMAT_7BIT == I2C_ADDRFMT_7BIT, "I2C address format");
_Static_assert(I2C_SEND == DCMD_I2C_SEND, "I2C send command");
_Static_assert(I2C_RECV == DCMD_I2C_RECV, "I2C receive command");
_Static_assert(I2C_SENDRECV == DCMD_I2C_SENDRECV, "I2C combined command");
_Static_assert(I2C_SENDRECV == 0xC0140507u,
               "MU1316 stock libairplay combined I2C command");
_Static_assert(I2C_LOCK == DCMD_I2C_LOCK, "I2C lock command");
_Static_assert(I2C_UNLOCK == DCMD_I2C_UNLOCK, "I2C unlock command");
_Static_assert(sizeof(struct i2c_header) == sizeof(i2c_send_t),
               "I2C send header size");
_Static_assert(sizeof(struct i2c_header) == sizeof(i2c_recv_t),
               "I2C receive header size");
_Static_assert(sizeof(struct i2c_sendrecv_header) == sizeof(i2c_sendrecv_t),
               "I2C combined header size");
_Static_assert(offsetof(struct i2c_header, address) ==
               offsetof(i2c_send_t, slave.addr), "I2C address offset");
_Static_assert(offsetof(struct i2c_header, format) ==
               offsetof(i2c_send_t, slave.fmt), "I2C format offset");
_Static_assert(offsetof(struct i2c_header, length) ==
               offsetof(i2c_send_t, len), "I2C length offset");
_Static_assert(offsetof(struct i2c_header, stop) ==
               offsetof(i2c_send_t, stop), "I2C stop offset");
_Static_assert(offsetof(struct i2c_sendrecv_header, send_length) ==
               offsetof(i2c_sendrecv_t, send_len), "I2C combined send offset");
_Static_assert(offsetof(struct i2c_sendrecv_header, receive_length) ==
               offsetof(i2c_sendrecv_t, recv_len), "I2C combined receive offset");
_Static_assert(offsetof(struct i2c_sendrecv_header, stop) ==
               offsetof(i2c_sendrecv_t, stop), "I2C combined stop offset");
