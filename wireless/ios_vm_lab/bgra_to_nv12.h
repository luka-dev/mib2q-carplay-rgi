/* Full-range BT.601 conversion for the VM's native 420f writeback pool.
 * Rows are independent; alpha and row padding never contribute to chroma.
 */
#ifndef MHI2_BGRA_TO_NV12_H
#define MHI2_BGRA_TO_NV12_H

static unsigned char nv12_clip(int value)
{
    return value < 0 ? 0 : value > 255 ? 255 : (unsigned char)value;
}

static int bgra_to_nv12(const unsigned char *src, size_t src_row, size_t src_bytes,
                       unsigned char *y_plane, size_t y_row, size_t y_bytes,
                       unsigned char *uv_plane, size_t uv_row, size_t uv_bytes,
                       size_t width, size_t height)
{
    if (!src || !y_plane || !uv_plane || !width || !height ||
            width > 800 || height > 480 || width % 2 || height % 2 ||
            src_row < width*4 || y_row < width || uv_row < width ||
            src_row > 16384 || y_row > 16384 || uv_row > 16384 ||
            height > src_bytes/src_row || height > y_bytes/y_row ||
            height/2 > uv_bytes/uv_row) return 1;
    for (size_t y = 0; y < height; ++y)
        for (size_t x = 0; x < width; ++x) {
            const unsigned char *p = src + y*src_row + x*4;
            y_plane[y*y_row+x] = nv12_clip((19595*p[2]+38470*p[1]+7471*p[0]+32768)/65536);
        }
    for (size_t y = 0; y < height; y += 2)
        for (size_t x = 0; x < width; x += 2) {
            int r = 0, g = 0, b = 0;
            for (size_t dy = 0; dy < 2; ++dy)
                for (size_t dx = 0; dx < 2; ++dx) {
                    const unsigned char *p = src + (y+dy)*src_row + (x+dx)*4;
                    b += p[0]; g += p[1]; r += p[2];
                }
            r = (r+2)/4; g = (g+2)/4; b = (b+2)/4;
            uv_plane[(y/2)*uv_row+x] = nv12_clip((-11059*r-21709*g+32768*b+8421376)/65536);
            uv_plane[(y/2)*uv_row+x+1] = nv12_clip((32768*r-27439*g-5329*b+8421376)/65536);
        }
    return 0;
}
#endif
