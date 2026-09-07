/*
 * Bounded JPEG metadata helpers used by the cover-art receiver.
 * Kept independent from stb/QNX so malformed-input regressions can run on host.
 */
#ifndef COVERART_JPEG_SAFETY_H
#define COVERART_JPEG_SAFETY_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define COVERART_HIDDEN __attribute__((visibility("hidden")))
#else
#define COVERART_HIDDEN
#endif

/* Returns EXIF orientation 1..8, or zero for absent/malformed metadata. */
COVERART_HIDDEN int coverart_jpeg_orientation(const uint8_t *data, size_t len);

/* Reject dimensions before stb allocates the decoded pixel buffer. */
COVERART_HIDDEN int coverart_image_dimensions_safe(int width, int height,
                                                    int max_dimension,
                                                    size_t max_pixels);

#endif /* COVERART_JPEG_SAFETY_H */
