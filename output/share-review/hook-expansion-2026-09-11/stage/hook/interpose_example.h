/* Copyright (c) 2026, original project author. */
#ifndef EXCERPT_INTERPOSE_EXAMPLE_H
#define EXCERPT_INTERPOSE_EXAMPLE_H
#include <stddef.h>
#include <stdint.h>

/* Fictional API for demonstrating a dynamic-library interposer.
 * This name and signature do not describe a vehicle or phone API.
 * A real provider and its caller must share this exact declaration.
 * Status 0 means success for this example. Missing provider: -1/ENOSYS.
 */
int demo_video_submit(void *context, const uint8_t *data, size_t size);

typedef struct {
    uint64_t calls;
    uint64_t successes;
    uint64_t failures;
} demo_hook_stats;

/* A snapshot of completed calls; counters wrap modulo 2^64.
 * No payload bytes, addresses or caller identifiers are retained.
 */
void demo_hook_get_stats(demo_hook_stats *out);
#endif
