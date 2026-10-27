/* Wire-format, identity preservation and moving-buffer growth contracts. */
#define ENABLE_LOGGING 0
#include "../hook/altscreen/maps_profile.h"
#include <assert.h>

static uint8_t moved[ALT_MAPS_IDENTIFY_MAX];
static uint8_t *source;
static size_t grow_calls;
static int grow_fail;

uint8_t *hook_identify_grow(hook_context_t *ctx, size_t len, size_t new_len,
                            size_t *capacity)
{
    ++grow_calls;
    assert(ctx && new_len <= sizeof(moved));
    if (grow_fail) return NULL;
    memcpy(moved, source, len);
    memset(source, 0xa5, len); /* Catch writes through the pre-growth pointer. */
    *capacity = sizeof(moved);
    return moved;
}

static void frame(uint8_t *buf, size_t len)
{
    memset(buf, 0, len);
    buf[0] = buf[1] = 0x40;
    write_be16(buf + 2, (uint16_t)len);
    write_be16(buf + 4, IAP2_MSG_IDENTIFY);
}

static void unchanged(uint8_t *buf, size_t len, size_t capacity)
{
    uint8_t before[ALT_MAPS_IDENTIFY_MAX];
    hook_context_t ctx = {0};
    size_t calls = grow_calls;
    memcpy(before, buf, len);
    assert(alt_maps_profile_identify(&ctx, buf, len, capacity) == len);
    assert(!memcmp(before, buf, len));
    assert(grow_calls == calls);
}

int main(void)
{
    static const uint8_t want[] = {
        0x00, 0x15, 0x00, 0x22,
        'b', 'f', '8', '8', '9', 'd', 'f', 'd',
        'a', 'e', 'a', '5', '4', '0', '9', '6', 0
    };
    uint8_t buf[ALT_MAPS_IDENTIFY_MAX], before[ALT_MAPS_IDENTIFY_MAX];
    hook_context_t ctx = {0};
    size_t len, i;

    /* Preserve an ordinary identity parameter and a nested parameter 34. */
    frame(buf, 20);
    write_be16(buf + 6, 6); write_be16(buf + 8, 0);
    buf[10] = 'A'; buf[11] = 0;
    write_be16(buf + 12, 8); write_be16(buf + 14, 22);
    write_be16(buf + 16, 4); write_be16(buf + 18, 34);
    memcpy(before, buf, 20);
    len = alt_maps_profile_identify(&ctx, buf, 20, sizeof(buf));
    assert(len == 41 && read_be16(buf + 2) == len);
    assert(!memcmp(buf + 6, before + 6, 14));
    assert(!memcmp(buf + 20, want, sizeof(want)));
    unchanged(buf, len, sizeof(buf)); /* Repeated application is idempotent. */

    /* Even an unknown/empty/duplicate existing PPID must be preserved. */
    frame(buf, 12);
    write_be16(buf + 6, 6); write_be16(buf + 8, 34);
    buf[10] = 'X'; buf[11] = 0;
    unchanged(buf, 12, sizeof(buf));
    frame(buf, 10);
    write_be16(buf + 6, 4); write_be16(buf + 8, 34);
    unchanged(buf, 10, sizeof(buf));
    write_be16(buf + 2, 14);
    memcpy(buf + 10, buf + 6, 4);
    unchanged(buf, 14, sizeof(buf));

    /* Validate the whole list, including malformed data after a good TLV. */
    for (i = 0; i < 4; ++i) {
        frame(buf, 10);
        write_be16(buf + 6, (uint16_t)i);
        unchanged(buf, 10, sizeof(buf));
    }
    frame(buf, 10); write_be16(buf + 6, 5);
    unchanged(buf, 10, sizeof(buf));
    frame(buf, 11); write_be16(buf + 6, 4);
    unchanged(buf, 11, sizeof(buf));
    frame(buf, 6); write_be16(buf + 2, 7);
    unchanged(buf, 6, sizeof(buf));
    frame(buf, 6); write_be16(buf + 4, IAP2_MSG_IDENTIFY_START);
    unchanged(buf, 6, sizeof(buf));
    frame(buf, 6); buf[0] = 0;
    unchanged(buf, 6, sizeof(buf));
    frame(buf, 6); unchanged(buf, 5, sizeof(buf));
    unchanged(buf, 6, 5);
    assert(alt_maps_profile_identify(&ctx, NULL, 6, 64) == 6);

    /* The real callback must append into the new allocation, not the old one. */
    frame(buf, 6); source = buf;
    len = alt_maps_profile_identify(&ctx, buf, 6, 6);
    assert(len == 27 && grow_calls == 1);
    assert(read_be16(moved + 2) == 27);
    assert(!memcmp(moved + 6, want, sizeof(want)));
    for (i = 0; i < 6; ++i) assert(buf[i] == 0xa5);
    frame(buf, 6); memcpy(before, buf, 6); grow_fail = 1;
    assert(alt_maps_profile_identify(&ctx, buf, 6, 6) == 6);
    assert(grow_calls == 2 && !memcmp(before, buf, 6));

    /* Exact size limit succeeds; the next larger frame remains untouched. */
    frame(buf, sizeof(buf) - sizeof(want));
    write_be16(buf + 6, (uint16_t)(sizeof(buf) - sizeof(want) - 6));
    len = alt_maps_profile_identify(&ctx, buf, sizeof(buf) - sizeof(want), sizeof(buf));
    assert(len == sizeof(buf));
    assert(!memcmp(buf + len - sizeof(want), want, sizeof(want)));
    frame(buf, sizeof(buf) - sizeof(want) + 1);
    write_be16(buf + 6, (uint16_t)(sizeof(buf) - sizeof(want) - 5));
    unchanged(buf, sizeof(buf) - sizeof(want) + 1, sizeof(buf));
    puts("altscreen_maps_profile_test: wire bytes, existing PPIDs, malformed input and moving growth PASS");
    return 0;
}
