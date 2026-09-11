/* Copyright (c) 2026, original project author. */
#include "avcc.h"
#include <string.h>

uint32_t avcc_to_annexb(const uint8_t *in, uint32_t inlen, uint8_t ls,
                               uint8_t *out, uint32_t outcap)
{
    uint32_t i = 0, o = 0;
    if (ls < 1 || ls > 4) ls = 4;
    while (inlen - i >= ls) {                            /* i <= inlen invariant */
        uint32_t nal = 0, k;
        for (k = 0; k < ls; k++) nal = (nal << 8) | in[i + k];
        i += ls;                                         /* i <= inlen (checked above) */
        if (nal == 0 || nal > inlen - i) break;          /* truncated / garbage → stop */
        if (o > 0xFFFFFFFFu - 4 - nal) break;            /* output size overflow */
        if (out) {
            if (o + 4 + nal > outcap) break;             /* output capacity exhausted */
            out[o] = 0; out[o + 1] = 0; out[o + 2] = 0; out[o + 3] = 1;
            memcpy(out + o + 4, in + i, nal);
        }
        o += 4 + nal;
        i += nal;
    }
    return o;
}
