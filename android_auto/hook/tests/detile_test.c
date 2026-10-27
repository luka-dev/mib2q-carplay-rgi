/* Host check: src/detile.h on a real hardware-decoded frame (omx_probe output) -> raw Y, U, V planes.
 *   detile_test frame.raw width height out.yuv [y0 y1 [x0 x1]]   (tests/detile_check.py compares with qcom_detile.py) */
#include <stdio.h>
#include <stdlib.h>
#include "../src/detile.h"
int main(int argc, char **argv)
{
    FILE *f; long n; unsigned char *src, *out; int w, h, y0 = 0, y1, x0 = 0, x1;
    if (argc < 5) return 2;
    w = atoi(argv[2]); h = atoi(argv[3]); y1 = h; x1 = w;
    if (argc >= 7) { y0 = atoi(argv[5]); y1 = atoi(argv[6]); }
    if (argc >= 9) { x0 = atoi(argv[7]); x1 = atoi(argv[8]); }
    f = fopen(argv[1], "rb"); if (!f) return 3;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    src = malloc((size_t)n); if (!src || fread(src, 1, (size_t)n, f) != (size_t)n) return 3;
    fclose(f);
    out = calloc((size_t)w * h * 3 / 2, 1); if (!out) return 4;
    dt_detile(src, (size_t)n, w, h, y0, y1, x0, x1, out, w, out + w * h, w / 2, out + w * h + (w / 2) * (h / 2), w / 2);
    f = fopen(argv[4], "wb"); if (!f) return 5;
    fwrite(out, 1, (size_t)w * h * 3 / 2, f); fclose(f);
    printf("frame_bytes=%u file=%ld\n", (unsigned)dt_frame_bytes(w, h), n);
    return 0;
}
