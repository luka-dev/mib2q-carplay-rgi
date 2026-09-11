/* Copyright (c) 2026, original project author. */
#ifndef EXCERPT_FRAME_IO_H
#define EXCERPT_FRAME_IO_H
#include <stdint.h>
#include <stddef.h>

/* Blocking POSIX descriptor reader: 0 success, -1 EOF/error.
 * The caller supplies descriptor lifetime and cancellation policy.
 * A descriptor must not be reused concurrently for a different connection.
 */
int frame_read_exact(int fd, void *buf, size_t length);

/* Read [u32 big-endian length][payload]. Start with *buf=NULL, *capacity=0.
 * Reuses/grows malloc storage; the caller eventually frees *buf.
 * All pointer arguments must be valid. max_payload is a caller policy.
 * Returns 0 success, -1 EOF/I/O error, -2 size/allocation error.
 * A zero-length record succeeds with *out_length=0.
 * On error, out_length is unspecified; discard any partially read record.
 */
int frame_read(int fd, uint8_t **buf, uint32_t *capacity,
               uint32_t *out_length, uint32_t max_payload);
#endif
