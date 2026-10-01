/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SQ5_CLUSTER_ENCODED_RING_H
#define SQ5_CLUSTER_ENCODED_RING_H
#include <stdint.h>
#include <stddef.h>
#define CL_MAGIC 0x53434c31u
#define CL_VERSION 1u
#define CL_SLOTS 8u
#define CL_PACKET_MAX (1024u*1024u)
#define CL_KEYFRAME 1u
/* One producer (GAL callback, protected in transport.c), one decoder consumer.
 * Producer never overwrites an unread slot. Counters are atomic aligned 32-bit
 * accesses; full fences publish/consume packet contents on ARMv7. Each GAL PID
 * owns a DIFFERENT object; a retry cannot resize or reset an old reader's map. */
typedef struct { uint32_t size, epoch, flags; unsigned char data[CL_PACKET_MAX]; } cl_packet;
typedef struct {
    uint32_t magic, version, owner, bytes;
    volatile uint32_t write_count, read_count, stopped, drops;
    uint32_t reserved[8];
    cl_packet packets[CL_SLOTS];
} cl_ring;
static inline unsigned cl_pending(const cl_ring *r) {
    return r->write_count-r->read_count;
}
static inline cl_packet *cl_write_slot(cl_ring *r) {
    uint32_t read=r->read_count; __sync_synchronize();
    if (r->write_count-read>=CL_SLOTS) return NULL;
    return &r->packets[r->write_count%CL_SLOTS];
}
static inline void cl_publish(cl_ring *r) {
    __sync_synchronize(); ++r->write_count;
}
static inline cl_packet *cl_read_slot(cl_ring *r) {
    uint32_t write=r->write_count; __sync_synchronize();
    if (write==r->read_count || write-r->read_count>CL_SLOTS) return NULL;
    return &r->packets[r->read_count%CL_SLOTS];
}
static inline void cl_release(cl_ring *r) {
    __sync_synchronize(); ++r->read_count;
}
#endif
