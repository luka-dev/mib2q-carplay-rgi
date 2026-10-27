#include "../common/alt_control_wire.h"
#include <assert.h>
typedef struct {
    alt_control_record_t records[256];
    unsigned count;
    int failed;
    alt_control_stream_t tail;
} trace_t;
static trace_t parse(const uint8_t *data, size_t size, int chunks) {
    trace_t trace = {0};
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = chunks ? 1u + data[offset] % 31u : 1u;
        if (chunk > size - offset)
            chunk = size - offset;
        size_t end = offset + chunk;
        while (offset < end) {
            size_t consumed = 0;
            alt_control_record_t record;
            int result =
                alt_control_feed(&trace.tail, data + offset, end - offset, &consumed, &record);
            offset += consumed;
            if (result < 0) {
                trace.failed = 1;
                return trace;
            }
            assert(consumed);
            if (result == 1) {
                assert(trace.count < 256);
                trace.records[trace.count++] = record;
            }
        }
    }
    return trace;
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 2048)
        return 0;
    trace_t bytes = parse(data, size, 0), chunks = parse(data, size, 1);
    assert(bytes.failed == chunks.failed && bytes.count == chunks.count);
    assert(!memcmp(bytes.records, chunks.records, bytes.count * sizeof(bytes.records[0])));
    assert(bytes.tail.used == chunks.tail.used && bytes.tail.failed == chunks.tail.failed);
    assert(!memcmp(bytes.tail.bytes, chunks.tail.bytes, bytes.tail.used));
    alt_control_record_t r, out;
    uint8_t encoded[12];
    if (!alt_control_decode(data, size, &r)) {
        unsigned n = alt_control_encode(encoded, sizeof(encoded), &r);
        assert(n == size && !memcmp(data, encoded, n));
        assert(!alt_control_decode(encoded, n, &out));
    }
    return 0;
}
