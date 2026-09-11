/* Copyright (c) 2026, original project author. */
#ifndef EXCERPT_AVCC_H
#define EXCERPT_AVCC_H
#include <stdint.h>

/* Convert length-prefixed H.264 NAL units to Annex-B start codes.
 * Input must already be accessible as plain bytes. length_size is 1..4;
 * other values use 4. No container or session parsing is performed here.
 * With out == NULL, returns the required output size for the valid prefix.
 * With out != NULL, writes complete NAL units up to out_capacity.
 * A malformed/truncated suffix stops conversion; the valid prefix is kept.
 * in must address in_length bytes; out must address out_capacity bytes.
 * Input and output must not overlap. The result is a byte count, not a
 * validation status. The caller decides whether partial conversion is useful.
 */
uint32_t avcc_to_annexb(const uint8_t *in, uint32_t in_length,
                       uint8_t length_size, uint8_t *out,
                       uint32_t out_capacity);
#endif
