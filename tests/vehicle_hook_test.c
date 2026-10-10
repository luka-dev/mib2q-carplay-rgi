/* vehicle_hook: $PASCD equals the MFi R32 41.3.3 examples byte for byte, VehicleStatusUpdate
 * and the Identify components follow MFi R29 97-19..97-21 / 97-161..97-162, and the module
 * flow (start/stop, 1 Hz limit, batching, stock-PASCD and rejection fallbacks) runs on stubs. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vehicle/vehicle_hook.h"

void veh_test_set_clock(uint64_t (*fn)(void));
void veh_test_set_config(bool status_on, bool speed_on, uint8_t engine);

/* ---- framework stubs ---- */
static bus_handler_t cmd_handler;
hook_result_t bus_on(uint16_t t, bus_handler_t h, void* c) { (void)c; if (t == CMD_VEHICLE) cmd_handler = h; return HOOK_OK; }
void bus_off(uint16_t t) { (void)t; }
static uint16_t sent_msg[16];
static uint8_t sent_payload[16][256];
static size_t sent_len[16];
static int sent_count;
hook_result_t hook_inject_message(uint16_t m, const uint8_t* p, size_t n) {
    assert(sent_count < 16 && n <= 256);
    sent_msg[sent_count] = m; sent_len[sent_count] = n; memcpy(sent_payload[sent_count], p, n);
    sent_count++;
    return HOOK_OK;
}
static uint8_t big[1024];
static const uint8_t* grow_src;
/* The real NmeArray is 32-bit ARM { base@+0, len@+4, cap@+8 }; the host test only checks
 * that the module grows through the framework and patches the grown copy. */
uint8_t* hook_identify_grow(hook_context_t* ctx, size_t len, size_t new_len, size_t* cap) {
    (void)ctx;
    if (new_len > sizeof(big)) return NULL;
    memcpy(big, grow_src, len);
    *cap = sizeof(big);
    return big;
}
static uint64_t now_ms = 5000;
static uint64_t clock_stub(void) { return now_ms; }

static void cmd(uint16_t speed_cms, uint8_t gear, uint16_t range, int16_t temp_dc, bool warn, uint8_t valid) {
    uint8_t p[VEH_CMD_LEN] = { 1, valid, (uint8_t)speed_cms, (uint8_t)(speed_cms >> 8), gear,
                               (uint8_t)range, (uint8_t)(range >> 8), (uint8_t)temp_dc,
                               (uint8_t)((uint16_t)temp_dc >> 8), warn ? 1 : 0, 0, 0, 0, 0 };
    cmd_handler(CMD_VEHICLE, BUS_FLAG_BINARY, p, sizeof(p), NULL);
}

static void deliver(hook_context_t* ctx, msg_direction_t dir, uint16_t id, const uint8_t* p, size_t n) {
    iap2_frame_t f;
    memset(&f, 0, sizeof(f));
    f.msgid = id; f.payload = n ? p : NULL; f.payload_len = n;
    ctx->direction = dir;
    (void)vehicle_module_def.on_message(ctx, &f);
}

int main(void) {
    char s[256];
    veh_sample_t v[10];
    size_t i, n;
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* MFi R32 41.3.3, examples 1 and 2: 10 Hz samples, one sentence per second. */
    for (i = 0; i < 10; i++) { v[i].t_ms = 1000001 + 100 * i; v[i].speed_mms = (uint32_t)(123 + 1000 * i); v[i].gear = VEH_GEAR_DRIVE; }
    n = veh_format_pascd(s, sizeof(s), v, 10);
    assert(n == strlen(s) && !strcmp(s, "$PASCD,1000.001,C,D,0,10,0.00,0.123,0.10,1.123,0.20,2.123,0.30,3.123,0.40,"
                                       "4.123,0.50,5.123,0.60,6.123,0.70,7.123,0.80,8.123,0.90,9.123*41"));
    v[3].gear = VEH_GEAR_REVERSE;                    /* reverse at any time in the interval -> R */
    n = veh_format_pascd(s, sizeof(s), v, 10);
    assert(!strcmp(s + n - 3, "*57") && strstr(s, ",C,R,0,10,"));
    v[0].t_ms = 86400000ull + 1500; v[0].gear = VEH_GEAR_UNKNOWN;   /* daily rollover, unknown gear */
    assert(veh_format_pascd(s, sizeof(s), v, 1) && !strncmp(s, "$PASCD,1.500,C,U,0,1,0.00,0.123*", 32));
    assert(veh_format_pascd(s, 20, v, 10) == 0 && veh_format_pascd(s, sizeof(s), v, 0) == 0);
    assert(veh_format_pascd(s, sizeof(s), v, 11) == 0);

    /* VehicleStatusUpdate: only wanted and valid fields; -0.6 C rounds to -1. */
    {
        veh_cmd_t st = { VEH_VALID_RANGE | VEH_VALID_TEMP | VEH_VALID_WARNING, 0, 0, 412, -6, true };
        static const uint8_t want_all[] = { 0x00, 0x06, 0x00, 0x03, 0x01, 0x9C,  0x00, 0x06, 0x00, 0x04, 0xFF, 0xFF,
                                            0x00, 0x05, 0x00, 0x06, 0x01 };
        uint8_t out[64];
        n = veh_build_status_update(out, sizeof(out), VEH_WANT_RANGE | VEH_WANT_TEMP | VEH_WANT_WARNING, &st);
        assert(n == sizeof(want_all) && !memcmp(out, want_all, n));
        st.valid = VEH_VALID_TEMP; st.temp_dc = 215;
        n = veh_build_status_update(out, sizeof(out), VEH_WANT_RANGE | VEH_WANT_TEMP, &st);
        assert(n == 6 && out[3] == 4 && out[5] == 22);
        assert(veh_build_status_update(out, sizeof(out), VEH_WANT_RANGE, &st) == 0);
    }
    {
        static const uint8_t start[] = { 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x06 };
        static const uint8_t loc[] = { 0x00, 0x04, 0x00, 0x01, 0x00, 0x04, 0x00, 0x02, 0x00, 0x04, 0x00, 0x04 };
        assert(veh_parse_start_status(start, sizeof(start)) == (VEH_WANT_RANGE | VEH_WANT_WARNING));
        assert(veh_location_wants_speed(loc, sizeof(loc)) && !veh_location_wants_speed(loc, 8));
    }

    /* Identify components: group 20 (id, name, engine, display name) then group 21 (id, name, 3/4/6). */
    {
        uint8_t out[192];
        iap2_tlv_t t, in;
        n = veh_build_identify_components(out, sizeof(out), true, VEH_ENGINE_DIESEL);
        assert(iap2_find_tlv(out, n, 20, &t) && iap2_find_tlv(t.value, t.value_len, 2, &in) && in.value[0] == 1);
        assert(iap2_find_tlv(t.value, t.value_len, 6, &in) && !strcmp((const char*)in.value, "Audi"));
        assert(iap2_find_tlv(out, n, 21, &t));
        assert(iap2_find_tlv(t.value, t.value_len, 3, &in) && in.value_len == 0);
        assert(iap2_find_tlv(t.value, t.value_len, 4, &in) && iap2_find_tlv(t.value, t.value_len, 6, &in));
        n = veh_build_identify_components(out, sizeof(out), false, 0);
        assert(!iap2_find_tlv(out, n, 20, &t) && iap2_find_tlv(out, n, 21, &t) && t.len == n);
    }
    {
        static const uint8_t rej_comp[] = { 0x00, 0x04, 0x00, 0x15 };
        static const uint8_t rej_msg[] = { 0x00, 0x08, 0x00, 0x07, 0x41, 0x71, 0xA1, 0x00 };
        static const uint8_t rej_other[] = { 0x00, 0x06, 0x00, 0x06, 0x41, 0x70, 0x00, 0x04, 0x00, 0x16 };
        assert(veh_rejection_is_ours(rej_comp, sizeof(rej_comp)) && veh_rejection_is_ours(rej_msg, sizeof(rej_msg)));
        assert(!veh_rejection_is_ours(rej_other, sizeof(rej_other)));
    }

    /* ---- module flow ---- */
    veh_test_set_clock(clock_stub);
    vehicle_module_def.on_init();
    veh_test_set_config(true, true, VEH_ENGINE_GASOLINE);
    assert(cmd_handler);

    /* Identify: appended behind the stock TLVs in the grown buffer, header length follows. */
    {
        uint8_t ident[64] = { 0x40, 0x40, 0x00, 0x0E, 0x1D, 0x01,
                              0x00, 0x08, 0x00, 0x16, 0x00, 0x04, 0x00, 0x14 };   /* 22 { VehicleSpeedData } */
        grow_src = ident;
        n = vehicle_module_def.on_identify(&ctx, ident, 14, 14);
        assert(n > 14 && big[2] == (uint8_t)(n >> 8) && big[3] == (uint8_t)n);
        assert(!memcmp(big, ident, 2) && !memcmp(big + 4, ident + 4, 10));
        { iap2_tlv_t t; assert(iap2_find_tlv(big + 6, n - 6, 20, &t) && iap2_find_tlv(big + 6, n - 6, 21, &t)); }
    }

    /* VehicleStatus: nothing before the phone asks; on start the snapshot goes out at once. */
    sent_count = 0;
    cmd(1000, VEH_GEAR_DRIVE, 400, 200, false, 0x1F);
    assert(sent_count == 0);
    {
        static const uint8_t start[] = { 0x00, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x04 };
        deliver(&ctx, MSG_DIR_INCOMING, 0xA100, start, sizeof(start));
    }
    assert(sent_count == 1 && sent_msg[0] == 0xA101 && sent_len[0] == 12);
    now_ms += 200; cmd(1000, VEH_GEAR_DRIVE, 400, 203, false, 0x1F);   /* same rounded degrees */
    assert(sent_count == 1);
    now_ms += 200; cmd(1000, VEH_GEAR_DRIVE, 399, 203, false, 0x1F);   /* changed, < 1 s */
    assert(sent_count == 1);
    now_ms += 700; cmd(1000, VEH_GEAR_DRIVE, 399, 203, false, 0x1F);   /* 1.1 s after the first */
    assert(sent_count == 2 && sent_msg[1] == 0xA101 && sent_payload[1][5] == 0x8F);
    /* Below zero: -0.4 C is sent as 0, -0.6 C must count as a change to -1 (same rounding). */
    now_ms += 1100; cmd(1000, VEH_GEAR_DRIVE, 399, -4, false, 0x1F);
    assert(sent_count == 3 && sent_payload[2][10] == 0x00 && sent_payload[2][11] == 0x00);
    now_ms += 1100; cmd(1000, VEH_GEAR_DRIVE, 399, -6, false, 0x1F);
    assert(sent_count == 4 && sent_payload[3][10] == 0xFF && sent_payload[3][11] == 0xFF);
    deliver(&ctx, MSG_DIR_INCOMING, 0xA102, NULL, 0);
    now_ms += 2000; cmd(1000, VEH_GEAR_DRIVE, 300, 203, false, 0x1F);
    assert(sent_count == 4);

    /* PASCD: only after the phone asks for VehicleSpeedData; 5 Hz in, one sentence per 0.8 s span. */
    {
        static const uint8_t loc[] = { 0x00, 0x04, 0x00, 0x01, 0x00, 0x04, 0x00, 0x04 };
        deliver(&ctx, MSG_DIR_INCOMING, 0xFFFA, loc, sizeof(loc));
    }
    sent_count = 0;
    for (i = 0; i < 5; i++) { now_ms += 200; cmd((uint16_t)(1389 + i), VEH_GEAR_DRIVE, 300, 203, false, 0x1F); }
    assert(sent_count == 1 && sent_msg[0] == 0xFFFB);
    {
        iap2_tlv_t t;
        const char* str;
        assert(iap2_find_tlv(sent_payload[0], sent_len[0], 0, &t));
        str = (const char*)t.value;
        assert(t.value[t.value_len - 1] == 0 && !strncmp(str, "$PASCD,", 7) && strstr(str, ",C,D,0,5,0.00,13.890,0.20,13.900,"));
        assert(strstr(str, ",0.80,13.930*"));
        /* Our own sentence echoed by the encoder is not stock's. */
        deliver(&ctx, MSG_DIR_OUTGOING, 0xFFFB, sent_payload[0], sent_len[0]);
    }
    for (i = 0; i < 5; i++) { now_ms += 200; cmd(0, VEH_GEAR_REVERSE, 300, 203, false, 0x1F); }
    assert(sent_count == 2);
    /* A gap drops the old samples instead of sending them late. */
    now_ms += 200; cmd(0, VEH_GEAR_DRIVE, 300, 203, false, 0x1F);
    now_ms += 5000; cmd(0, VEH_GEAR_DRIVE, 300, 203, false, 0x1F);
    assert(sent_count == 2);
    /* Stock PASCD seen -> ours stop. */
    {
        uint8_t p[64];
        size_t len = 0;
        static const char stock[] = "$PASCD,12.000,C,D,0,1,0.00,0.010*00";
        p[0] = 0; p[1] = (uint8_t)(4 + sizeof(stock)); p[2] = 0; p[3] = 0;
        memcpy(p + 4, stock, sizeof(stock)); len = 4 + sizeof(stock);
        deliver(&ctx, MSG_DIR_OUTGOING, 0xFFFB, p, len);
    }
    for (i = 0; i < 10; i++) { now_ms += 200; cmd(500, VEH_GEAR_DRIVE, 300, 203, false, 0x1F); }
    assert(sent_count == 2);

    /* New Identify: state resets; a rejection naming component 21 turns the patch off. */
    vehicle_module_def.on_state(&ctx, HOOK_EVENT_IDENTIFY_START, NULL);
    {
        uint8_t ident[64] = { 0x40, 0x40, 0x00, 0x06, 0x1D, 0x01 };
        static const uint8_t rej[] = { 0x00, 0x04, 0x00, 0x15 };
        assert(vehicle_module_def.on_identify(&ctx, ident, 6, sizeof(ident)) > 6);
        deliver(&ctx, MSG_DIR_INCOMING, 0x1D03, rej, sizeof(rej));
        vehicle_module_def.on_state(&ctx, HOOK_EVENT_IDENTIFY_START, NULL);
        ident[3] = 6;
        assert(vehicle_module_def.on_identify(&ctx, ident, 6, sizeof(ident)) == 6);
    }

    /* Malformed input never crashes or overruns. */
    srand(11);
    for (i = 0; i < 100000; i++) {
        uint8_t buf[96], out[64];
        size_t len = (size_t)(rand() % sizeof(buf)), j;
        veh_cmd_t c;
        for (j = 0; j < len; j++) buf[j] = (uint8_t)rand();
        (void)veh_parse_start_status(buf, len);
        (void)veh_location_wants_speed(buf, len);
        (void)veh_rejection_is_ours(buf, len);
        if (veh_parse_cmd(buf, len, &c)) assert(veh_build_status_update(out, sizeof(out), 7, &c) <= sizeof(out));
    }
    puts("vehicle_hook_test: R32 PASCD examples byte-exact, R29 VehicleStatus/Identify layouts, start/stop, 1 Hz limit, batching, gap, stock-PASCD and rejection fallbacks, 100000 malformed inputs PASS");
    return 0;
}
