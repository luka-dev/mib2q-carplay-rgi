#include "../common/alt_control_wire.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    const uint8_t hello[] = {0x41, 0x56, 0x32, 0x48, 0, 0, 0, 3};
    const uint8_t ack[] = {0x41, 0x56, 0x32, 0x41, 0, 0, 0, 7, 0, 0, 0, 1};
    const uint8_t status[] = {0x41, 0x56, 0x32, 0x53, 0, 0, 0, 7, 0, 0, 0, 1};
    const uint8_t *records[] = {hello, ack, status};
    unsigned sizes[] = {8, 12, 12};
    uint8_t encoded[12], burst[32];
    alt_control_record_t r = {0}, out;
    for (unsigned k = 0; k < 3; ++k) {
        assert(!alt_control_decode(records[k], sizes[k], &r));
        assert(alt_control_encode(encoded, sizeof(encoded), &r) == sizes[k]);
        assert(!memcmp(encoded, records[k], sizes[k]));
        for (unsigned split = 1; split < sizes[k]; ++split) {
            alt_control_stream_t stream = {0};
            size_t consumed = 0;
            out = (alt_control_record_t){123, 456, 789};
            assert(!alt_control_feed(&stream, records[k], split, &consumed, &out));
            assert(consumed == split && out.magic == 123);
            assert(alt_control_feed(&stream, records[k] + split, sizes[k] - split, &consumed,
                                    &out) == 1);
            assert(!stream.used && out.magic == r.magic && out.generation == r.generation &&
                   out.value == r.value);
        }
    }
    memcpy(burst, hello, 8);
    memcpy(burst + 8, ack, 12);
    memcpy(burst + 20, status, 12);
    alt_control_stream_t stream = {0};
    size_t offset = 0, consumed = 0;
    for (unsigned i = 0; i < 3; ++i) {
        assert(alt_control_feed(&stream, burst + offset, sizeof(burst) - offset, &consumed, &out) ==
               1);
        assert(consumed == sizes[i]);
        offset += consumed;
    }
    assert(offset == sizeof(burst));
    memcpy(encoded, status, 12);
    encoded[11] = 2;
    out = (alt_control_record_t){123, 456, 789};
    assert(alt_control_decode(encoded, 12, &out) < 0 && out.magic == 123);
    assert(alt_control_feed(&stream, encoded, 12, &consumed, &out) < 0 && stream.failed);
    assert(alt_control_feed(&stream, hello, 8, &consumed, &out) < 0 && !consumed);
    alt_control_reset(&stream);
    assert(alt_control_feed(&stream, hello, 8, &consumed, &out) == 1);
    r = (alt_control_record_t){ALT_WIRE_ACK, 0, 1};
    memset(encoded, 0xaa, sizeof(encoded));
    assert(!alt_control_encode(encoded, sizeof(encoded), &r) && encoded[0] == 0xaa);
    assert(alt_control_decode(hello, 7, &out) < 0);
    puts("altscreen_control_wire_test: golden BE bytes, all splits, combined records, unchanged "
         "invalid output, error latch and reset PASS");
    return 0;
}
