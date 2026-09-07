#include "jpeg_safety.h"

#include <string.h>

static uint16_t read_u16(const uint8_t *p, int little_endian)
{
    if (little_endian)
        return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t read_u32(const uint8_t *p, int little_endian)
{
    if (little_endian) {
        return (uint32_t)p[0] |
               ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) |
               ((uint32_t)p[3] << 24);
    }
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

int coverart_jpeg_orientation(const uint8_t *data, size_t len)
{
    size_t pos;

    if (!data || len < 12 || data[0] != 0xff || data[1] != 0xd8)
        return 0;

    pos = 2;
    while (pos < len && len - pos >= 2) {
        uint8_t marker;
        uint16_t segment_length;
        size_t segment_size;

        if (data[pos] != 0xff) {
            pos++;
            continue;
        }
        marker = data[pos + 1];
        if (marker == 0xff) {
            pos++;
            continue;
        }
        if (marker == 0xd9 || marker == 0xda)
            break;
        if ((marker >= 0xd0 && marker <= 0xd7) ||
            marker == 0xd8 || marker == 0x01) {
            pos += 2;
            continue;
        }
        if (len - pos < 4)
            break;

        segment_length = (uint16_t)(((uint16_t)data[pos + 2] << 8) |
                                    (uint16_t)data[pos + 3]);
        if (segment_length < 2 ||
            (size_t)segment_length > len - pos - 2)
            break;
        segment_size = (size_t)segment_length - 2;

        if (marker == 0xe1 && segment_size >= 14) {
            const uint8_t *segment = data + pos + 4;

            if (memcmp(segment, "Exif\0\0", 6) == 0) {
                const uint8_t *tiff = segment + 6;
                size_t tiff_size = segment_size - 6;
                int little_endian;
                uint32_t ifd_offset;
                uint16_t entry_count;
                size_t entry_offset;
                size_t index;

                if (tiff_size < 8)
                    goto next_segment;
                if (tiff[0] == 'I' && tiff[1] == 'I')
                    little_endian = 1;
                else if (tiff[0] == 'M' && tiff[1] == 'M')
                    little_endian = 0;
                else
                    goto next_segment;
                if (read_u16(tiff + 2, little_endian) != UINT16_C(0x002a))
                    goto next_segment;

                ifd_offset = read_u32(tiff + 4, little_endian);
                /* Subtraction form is intentional: ifd_offset is attacker-
                 * controlled and adding two to UINT32_MAX used to wrap. */
                if ((size_t)ifd_offset > tiff_size ||
                    tiff_size - (size_t)ifd_offset < 2)
                    goto next_segment;

                entry_count = read_u16(tiff + (size_t)ifd_offset,
                                       little_endian);
                entry_offset = (size_t)ifd_offset + 2;
                for (index = 0; index < (size_t)entry_count; index++) {
                    const uint8_t *entry;
                    uint16_t orientation;

                    if (entry_offset > tiff_size ||
                        tiff_size - entry_offset < 12)
                        break;
                    entry = tiff + entry_offset;
                    if (read_u16(entry, little_endian) == UINT16_C(0x0112) &&
                        read_u16(entry + 2, little_endian) == 3 &&
                        read_u32(entry + 4, little_endian) == 1) {
                        orientation = read_u16(entry + 8, little_endian);
                        return orientation >= 1 && orientation <= 8 ?
                               (int)orientation : 0;
                    }
                    entry_offset += 12;
                }
            }
        }

next_segment:
        pos += 2 + (size_t)segment_length;
    }
    return 0;
}

int coverart_image_dimensions_safe(int width, int height,
                                   int max_dimension, size_t max_pixels)
{
    size_t w, h;

    if (width <= 0 || height <= 0 || max_dimension <= 0 || !max_pixels)
        return 0;
    if (width > max_dimension || height > max_dimension)
        return 0;
    w = (size_t)width;
    h = (size_t)height;
    return h <= max_pixels / w;
}
