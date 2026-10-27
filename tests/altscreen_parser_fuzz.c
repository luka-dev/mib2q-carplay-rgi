#define ENABLE_LOGGING 0
#include "../hook/altscreen/altscreen_av.c"
#include "../hook/altscreen/avcc_config.h"
#include <assert.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t out[8192], owned[2048], header[ALT_WIRE_HEADER_SIZE];
    uint32_t n = 0, len = 0;
    int idr = 0;
    alt_frame_meta_t meta, roundtrip;
    alt_wire_state_t state = {0};
    if (size <= 2048 && size) {
        for (uint8_t ls = 1; ls <= 4; ++ls) {
            n = avcc_to_annexb(data, (uint32_t)size, ls, NULL, 0, &idr);
            alt_avcc_frame_t prepared;
            memcpy(owned, data, size);
            int valid = alt_avcc_prepare(owned, (uint32_t)size, ls, ls == 4, &prepared) == 0;
            assert(valid == (n != 0));
            if (valid) {
                assert(prepared.annexb_len == n && prepared.idr == idr);
            }

            if (n && n <= sizeof(out)) {
                assert(avcc_to_annexb(data, (uint32_t)size, ls, out, sizeof(out), NULL) == n);
                if (ls == 4)
                    assert(!memcmp(owned, out, n));
                assert(!avcc_to_annexb(data, (uint32_t)size, ls, out, n - 1, NULL));
            }
        }
        (void)alt_video_config_to_annexb(data, (uint32_t)size, out, sizeof(out), &n);
    }
    if (size >= ALT_WIRE_HEADER_SIZE && !alt_wire_decode(data, ALT_WIRE_HEADER_SIZE, &len, &meta)) {
        alt_wire_encode(header, len, &meta);
        assert(!alt_wire_decode(header, sizeof(header), &n, &roundtrip));
        assert(n == len && !memcmp(header, data, sizeof(header)));
        (void)alt_wire_accept(&state, len, &meta);
        for (unsigned i = 0; i < 16; ++i) {
            alt_wire_state_t previous = state;
            roundtrip = meta;
            roundtrip.generation ^= i;
            roundtrip.sequence += i;
            if (alt_wire_accept(&state, len, &roundtrip) != 0)
                assert(!memcmp(&state, &previous, sizeof(state)));
        }
    }
    {
        alt_recovery_policy_t policy;
        uint64_t now = 1000000;
        alt_recovery_reset(&policy, 1);
        for (size_t i = 0; i < size && i < 128; ++i) {
            now += (uint64_t)data[i] * 10000u;
            switch (data[i] % 6u) {
            case 0:
                alt_recovery_fault(&policy, now, 1);
                assert(policy.connect_due_us <= now + 1000000u);
                break;
            case 1:
                if (alt_recovery_request(&policy, 1, ALT_KEYFRAME_RECOVERY, now))
                    assert(policy.request_due_us >= now + 2000000u);
                break;
            case 2:
                (void)alt_recovery_request(&policy, 1, ALT_KEYFRAME_START, now);
                break;
            case 3:
                (void)alt_recovery_request(&policy, 1, ALT_KEYFRAME_UI, now);
                break;
            case 4:
                alt_recovery_health(&policy, now, 0);
                break;
            default:
                alt_recovery_health(&policy, now, 1);
                break;
            }
            assert(!alt_recovery_request(&policy, 2, ALT_KEYFRAME_RECOVERY, now));
        }
    }
    return 0;
}
