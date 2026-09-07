#include "coverart/jpeg_safety.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static size_t make_exif(uint8_t out[40], int little_endian, uint32_t offset,
                        uint16_t orientation)
{
    uint8_t *tiff;

    memset(out, 0, 40);
    out[0] = 0xff;
    out[1] = 0xd8;
    out[2] = 0xff;
    out[3] = 0xe1;
    out[4] = 0x00;
    out[5] = 0x22;
    memcpy(out + 6, "Exif\0\0", 6);
    tiff = out + 12;
    if (little_endian) {
        tiff[0] = 'I'; tiff[1] = 'I'; tiff[2] = 0x2a;
        tiff[4] = (uint8_t)offset;
        tiff[5] = (uint8_t)(offset >> 8);
        tiff[6] = (uint8_t)(offset >> 16);
        tiff[7] = (uint8_t)(offset >> 24);
        tiff[8] = 1;
        tiff[10] = 0x12; tiff[11] = 0x01;
        tiff[12] = 3;
        tiff[14] = 1;
        tiff[18] = (uint8_t)orientation;
        tiff[19] = (uint8_t)(orientation >> 8);
    } else {
        tiff[0] = 'M'; tiff[1] = 'M'; tiff[3] = 0x2a;
        tiff[4] = (uint8_t)(offset >> 24);
        tiff[5] = (uint8_t)(offset >> 16);
        tiff[6] = (uint8_t)(offset >> 8);
        tiff[7] = (uint8_t)offset;
        tiff[9] = 1;
        tiff[10] = 0x01; tiff[11] = 0x12;
        tiff[13] = 3;
        tiff[17] = 1;
        tiff[18] = (uint8_t)(orientation >> 8);
        tiff[19] = (uint8_t)orientation;
    }
    out[38] = 0xff;
    out[39] = 0xd9;
    return 40;
}

int main(void)
{
    uint8_t jpeg[40];
    uint8_t fuzz[256];
    size_t len;
    uint32_t state = UINT32_C(0x7f4a7c15);
    unsigned iteration;

    len = make_exif(jpeg, 1, 8, 6);
    assert(coverart_jpeg_orientation(jpeg, len) == 6);
    len = make_exif(jpeg, 0, 8, 4);
    assert(coverart_jpeg_orientation(jpeg, len) == 4);

    /* The old `ifd_offset + 2` check wrapped and indexed outside `jpeg`. */
    len = make_exif(jpeg, 1, UINT32_MAX, 1);
    assert(coverart_jpeg_orientation(jpeg, len) == 0);
    len = make_exif(jpeg, 0, UINT32_MAX, 1);
    assert(coverart_jpeg_orientation(jpeg, len) == 0);
    assert(coverart_jpeg_orientation(jpeg, len - 5) == 0);

    assert(coverart_image_dimensions_safe(1200, 1200, 4096,
                                           8u * 1024u * 1024u));
    assert(!coverart_image_dimensions_safe(4097, 100, 4096,
                                            8u * 1024u * 1024u));
    assert(!coverart_image_dimensions_safe(3000, 3000, 4096,
                                            8u * 1024u * 1024u));
    assert(!coverart_image_dimensions_safe(-1, 100, 4096,
                                            8u * 1024u * 1024u));

    /* Deterministic malformed-input sweep. Under ASan/UBSan this exercises
     * segment lengths, byte orders, offsets and truncated IFD entry tables. */
    for (iteration = 0; iteration < 50000; iteration++) {
        size_t index;
        int orientation;

        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        len = (size_t)(state % (sizeof(fuzz) + 1));
        for (index = 0; index < len; index++) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            fuzz[index] = (uint8_t)(state >> 24);
        }
        if (len >= 2) { fuzz[0] = 0xff; fuzz[1] = 0xd8; }
        if (len >= 12 && (iteration & 3u) == 0) {
            fuzz[2] = 0xff; fuzz[3] = 0xe1;
            memcpy(fuzz + 6, "Exif\0\0", 6);
        }
        orientation = coverart_jpeg_orientation(fuzz, len);
        assert(orientation >= 0 && orientation <= 8);
    }

    puts("OK  bounded EXIF parser, 50000 malformed inputs, pre-decode image budget");
    return 0;
}
