/* Host check: 0x8001 InputReport rotary encoding (field numbers from this libautoreceiver). */
#include <stdarg.h>
void probe_log(const char *fmt, ...) { (void)fmt; }
int live_map_zoom(int delta) { (void)delta; return 0; }
#include "../src/rotary.c"
#include <assert.h>
static void dump(const unsigned char *p, unsigned n) { for (unsigned i = 0; i < n; i++) printf("%02x", p[i]); printf(" (%u)\n", n); }
int main(void)
{
    unsigned char m[64]; unsigned n;
    n = live_rotary_message(m, sizeof(m), 1000, 1, 0); dump(m, n);
    /* 80 01 | 08 e8 07 | 32 08 | 0a 06 | 08 80 80 04 | 10 01 */
    assert(n == 15 && !memcmp(m, "\x80\x01\x08\xe8\x07\x32\x08\x0a\x06\x08\x80\x80\x04\x10\x01", 15));
    n = live_rotary_message(m, sizeof(m), 1000, -1, 0); dump(m, n);
    /* int32 -1 -> 10-byte varint ff..ff 01 */
    assert(m[5] == 0x32 && m[6] == 17 && m[7] == 0x0a && m[8] == 15 && m[13] == 0x10);
    assert(!memcmp(m + 14, "\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10) && n == 24);
    assert(live_rotary_message(m, 20, 1000, 1, 0) == 0);
    n = live_rotary_message(m, sizeof(m), 1000, 1, 64); dump(m, n);
    /* 80 01 | 08 e8 07 | 10 40 (disp_channel_id 64) | 32 08 ... */
    assert(n == 17 && m[5] == 0x10 && m[6] == 64 && m[7] == 0x32);
    puts("PASS: rotary InputReport (keycode 65536, +1 / -1, timestamp varint, small buffer refused)");
    return 0;
}
