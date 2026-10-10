/* tel_hook: the StartListUpdates bytes equal xPaiiN's car-verified MST2 request,
 * Hold/MuteStatusUpdate follow iAP2 R29 (param 0), and ListUpdate parsing matches the
 * three 0x4171 frames captured from an iPhone on a MU1102 car
 * (V54.3D8 DEVICE_EVIDENCE tel-listupdate.hex; numbers and the contact name replaced
 * with placeholders of the same byte length).  Bus and injection are stubbed. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "telephony/tel_hook.h"

/* ---- stubs for the framework seams tel_hook.c links against ---- */
void bus_set_sticky(uint16_t t, bool s) { (void)t; (void)s; }
static bus_handler_t cmd_handler;
hook_result_t bus_on(uint16_t t, bus_handler_t h, void* c) { (void)c; if (t == CMD_TEL) cmd_handler = h; return HOOK_OK; }
void bus_off(uint16_t t) { (void)t; }
hook_result_t bus_text_begin_heap(bus_text_builder_t* b, const char* o, uint32_t c) {
    (void)b; (void)o; (void)c; return HOOK_ERR_IO;
}
void bus_text_int(bus_text_builder_t* b, const char* k, int64_t v) { (void)b; (void)k; (void)v; }
void bus_text_uint(bus_text_builder_t* b, const char* k, uint64_t v) { (void)b; (void)k; (void)v; }
void bus_text_str(bus_text_builder_t* b, const char* k, const char* v) { (void)b; (void)k; (void)v; }
void bus_text_bool(bus_text_builder_t* b, const char* k, bool v) { (void)b; (void)k; (void)v; }
hook_result_t bus_send_text(uint16_t t, uint8_t f, bus_text_builder_t* b) { (void)t; (void)f; (void)b; return HOOK_OK; }
static uint16_t sent_msg;
static uint8_t sent_payload[32];
static size_t sent_len;
static int sent_count;
hook_result_t hook_inject_message(uint16_t m, const uint8_t* p, size_t n) {
    sent_msg = m; sent_len = n < sizeof(sent_payload) ? n : sizeof(sent_payload);
    memcpy(sent_payload, p, sent_len); sent_count++;
    return HOOK_OK;
}

/* CMD_TEL [action][arg] from Java -> the iAP2 message and a single u8 param 0. */
static void check_cmd(uint8_t action, uint8_t arg, uint16_t want_msg, uint8_t want_value) {
    const uint8_t cmd[2] = { action, arg };
    const uint8_t want[5] = { 0x00, 0x05, 0x00, 0x00, want_value };
    int before = sent_count;
    cmd_handler(CMD_TEL, 0, cmd, 2, NULL);
    assert(sent_count == before + 1 && sent_msg == want_msg);
    assert(sent_len == 5 && memcmp(sent_payload, want, 5) == 0);
}

/* xPaiiN cpvc-tel-lib send_start_list_updates_probe (recents max 25, favorites max 0). */
static const uint8_t verified_start[] =
    "\x00\x2c\x00\x01"
    "\x00\x04\x00\x00\x00\x04\x00\x01\x00\x04\x00\x02\x00\x04\x00\x03\x00\x04\x00\x04"
    "\x00\x04\x00\x05\x00\x04\x00\x06\x00\x04\x00\x07\x00\x04\x00\x08\x00\x04\x00\x09"
    "\x00\x06\x00\x03\x00\x19"
    "\x00\x05\x00\x04\x01"
    "\x00\x1c\x00\x06"
    "\x00\x04\x00\x00\x00\x04\x00\x01\x00\x04\x00\x02\x00\x04\x00\x03\x00\x04\x00\x04\x00\x04\x00\x05"
    "\x00\x06\x00\x08\x00\x00";

/* Captured frames, including the 6-byte 40 40 len 41 71 header. */
static const uint8_t cap_avail[] =
    "\x40\x40\x00\x11\x41\x71\x00\x05\x00\x00\x01\x00\x06\x00\x02\x00"
    "\x03";
static const uint8_t cap_entries[] =
    "\x40\x40\x01\x4b\x41\x71\x00\x7b\x00\x01\x00\x06\x00\x00\x00\x00"
    "\x00\x13\x00\x01\x2b\x38\x36\x31\x33\x38\x30\x30\x30\x30\x30\x30"
    "\x30\x31\x00\x00\x0f\x00\x02\x41\x6c\x69\x63\x65\x20\x54\x65\x73"
    "\x74\x00\x00\x0b\x00\x03\xe6\x89\x8b\xe6\x9c\xba\x00\x00\x29\x00"
    "\x04\x37\x44\x31\x39\x31\x37\x30\x41\x2d\x39\x35\x39\x43\x2d\x34"
    "\x32\x30\x34\x2d\x41\x43\x32\x41\x2d\x36\x42\x45\x33\x38\x36\x34"
    "\x44\x43\x43\x42\x43\x00\x00\x05\x00\x05\x03\x00\x05\x00\x06\x02"
    "\x00\x0c\x00\x07\x00\x00\x00\x00\x69\xec\x67\x54\x00\x05\x00\x09"
    "\x02\x00\x65\x00\x01\x00\x06\x00\x00\x00\x01\x00\x13\x00\x01\x2b"
    "\x38\x36\x31\x33\x38\x30\x30\x30\x30\x30\x30\x30\x32\x00\x00\x16"
    "\x00\x02\x2b\x38\x36\x20\x31\x33\x38\x20\x30\x30\x30\x30\x20\x30"
    "\x30\x30\x32\x00\x00\x12\x00\x03\xe6\xb5\x99\xe6\xb1\x9f\x20\xe6"
    "\x9d\xad\xe5\xb7\x9e\x00\x00\x05\x00\x04\x00\x00\x05\x00\x05\x03"
    "\x00\x05\x00\x06\x02\x00\x0c\x00\x07\x00\x00\x00\x00\x69\x45\x7b"
    "\x8f\x00\x05\x00\x09\x02\x00\x65\x00\x01\x00\x06\x00\x00\x00\x02"
    "\x00\x13\x00\x01\x2b\x38\x36\x31\x33\x38\x30\x30\x30\x30\x30\x30"
    "\x30\x32\x00\x00\x16\x00\x02\x2b\x38\x36\x20\x31\x33\x38\x20\x30"
    "\x30\x30\x30\x20\x30\x30\x30\x32\x00\x00\x12\x00\x03\xe6\xb5\x99"
    "\xe6\xb1\x9f\x20\xe6\x9d\xad\xe5\xb7\x9e\x00\x00\x05\x00\x04\x00"
    "\x00\x05\x00\x05\x00\x00\x05\x00\x06\x02\x00\x0c\x00\x07\x00\x00"
    "\x00\x00\x69\x45\x75\x38\x00\x05\x00\x09\x07";
static const uint8_t cap_fav[] =
    "\x40\x40\x00\x11\x41\x71\x00\x05\x00\x05\x01\x00\x06\x00\x07\x00"
    "\x00";

/* 0x4158 CommunicationsUpdate from the 2026-08-21 accessoryd packet log (our car):
 * signal 3, registration 4 (home), airplane off, carrier "AT&T", cellular 1. */
static const uint8_t cap_comm[] =
    "\x40\x40\x00\x23\x41\x58\x00\x05\x00\x00\x03\x00\x05\x00\x01\x04"
    "\x00\x05\x00\x02\x00\x00\x09\x00\x04\x41\x54\x26\x54\x00\x00\x05"
    "\x00\x05\x01";

static void parse(const uint8_t* frame, size_t n, tel_lists_t* l) {
    assert(frame[4] == 0x41 && frame[5] == 0x71 && (size_t)((frame[2] << 8) | frame[3]) == n);
    assert(tel_parse_list_update(frame + 6, n - 6, l));
}

int main(void) {
    uint8_t buf[256];
    size_t n;
    tel_lists_t l, last;

    n = tel_build_status(buf, sizeof(buf), true);
    assert(n == 5 && memcmp(buf, "\x00\x05\x00\x00\x01", 5) == 0);

    /* CMD_TEL: mute/hold status, AcceptCall/EndCall action 0 without CallUUID. */
    tel_module_def.on_init();
    assert(cmd_handler);
    check_cmd(1, 1, 0x4160, 1);        /* MuteStatusUpdate on */
    check_cmd(2, 0, 0x415F, 0);        /* HoldStatusUpdate off */
    check_cmd(3, 0, 0x415B, 0);        /* AcceptCall: accept the incoming call */
    check_cmd(4, 0, 0x415C, 0);        /* EndCall: end the frontmost call */
    {
        const uint8_t swap[2] = { 5, 0 };
        int before = sent_count;
        cmd_handler(CMD_TEL, 0, swap, 2, NULL);
        assert(sent_count == before + 1 && sent_msg == 0x415D && sent_len == 0);  /* SwapCalls, no params */
    }
    {
        const uint8_t bad[2] = { 9, 0 };
        int before = sent_count;
        cmd_handler(CMD_TEL, 0, bad, 2, NULL);
        cmd_handler(CMD_TEL, 0, bad, 1, NULL);
        assert(sent_count == before);  /* unknown or short command: nothing sent */
    }

    n = tel_build_start_list_updates(buf, sizeof(buf), 25, 0);
    assert(n == sizeof(verified_start) - 1 && memcmp(buf, verified_start, n) == 0);
    assert(tel_build_start_list_updates(buf, 60, 25, 0) == 0);          /* too small */

    /* The three captured frames, folded like the hook does. */
    memset(&last, 0, sizeof(last));
    parse(cap_avail, sizeof(cap_avail) - 1, &l);           /* recents available, count 3 */
    assert(l.recents.count_set && l.recents.count == 3 && l.recents.n == 0 && !l.favorites.count_set);
    tel_merge_lists(&last, &l);
    assert(last.recents.n == 3);

    parse(cap_entries, sizeof(cap_entries) - 1, &l);       /* the three entries */
    assert(!l.recents.count_set && l.recents.n == 3 && l.favorites.n == 0);
    assert(strcmp(l.recents.e[0].a, "+8613800000001") == 0);
    assert(strcmp(l.recents.e[0].b, "Alice Test") == 0);
    assert(l.recents.e[0].type == 2 && l.recents.e[0].timestamp == 0x69EC6754ULL);
    assert(l.recents.e[0].occurrences == 2 && l.recents.e[2].occurrences == 7);
    assert(strcmp(l.recents.e[1].a, "+8613800000002") == 0 && strcmp(l.recents.e[1].b, "+86 138 0000 0002") == 0);
    assert(l.recents.e[2].index == 2 && l.recents.e[2].timestamp == 0x69457538ULL);
    tel_merge_lists(&last, &l);
    assert(last.recents.n == 3 && strcmp(last.recents.e[2].a, "+8613800000002") == 0);

    parse(cap_fav, sizeof(cap_fav) - 1, &l);               /* favorites available, count 0 */
    assert(l.favorites.count_set && l.favorites.count == 0 && !l.recents.count_set);
    tel_merge_lists(&last, &l);
    assert(last.recents.n == 3 && last.favorites.n == 0);  /* recents untouched */

    /* A list split over two messages: the second carries entries only (index 2),
     * and it must not replace entries 0..1. */
    memset(&l, 0, sizeof(l));
    l.recents.count_set = true; l.recents.count = 3;
    l.recents.n = 2; l.recents.e[0].index = 0; l.recents.e[1].index = 1;
    strcpy(l.recents.e[0].a, "100"); strcpy(l.recents.e[1].a, "101");
    tel_merge_lists(&last, &l);
    memset(&l, 0, sizeof(l));
    l.recents.n = 1; l.recents.e[0].index = 2; strcpy(l.recents.e[0].a, "102");
    tel_merge_lists(&last, &l);
    assert(last.recents.n == 3 && strcmp(last.recents.e[0].a, "100") == 0 &&
           strcmp(last.recents.e[1].a, "101") == 0 && strcmp(last.recents.e[2].a, "102") == 0);

    /* The list shrinks, and an out-of-range index is ignored. */
    memset(&l, 0, sizeof(l));
    l.recents.count_set = true; l.recents.count = 1;
    l.recents.n = 1; l.recents.e[0].index = TEL_LIST_MAX;
    tel_merge_lists(&last, &l);
    assert(last.recents.n == 1 && strcmp(last.recents.e[0].a, "100") == 0);


    /* CommunicationsUpdate: full captured frame, then a signal-only update merges. */
    {
        tel_comm_t c, cl;
        memset(&cl, 0, sizeof(cl));
        assert(tel_parse_comm_update(cap_comm + 6, sizeof(cap_comm) - 1 - 6, &c));
        assert(c.has_signal && c.signal == 3 && c.has_registration && c.registration == 4);
        assert(c.has_airplane && !c.airplane && c.has_carrier && strcmp(c.carrier, "AT&T") == 0);
        tel_merge_comm(&cl, &c);
        memset(&c, 0, sizeof(c)); c.has_signal = true; c.signal = 5;
        tel_merge_comm(&cl, &c);
        assert(cl.signal == 5 && cl.registration == 4 && strcmp(cl.carrier, "AT&T") == 0);
        assert(!tel_parse_comm_update(cap_comm + 6, sizeof(cap_comm) - 1 - 6 - 2, &c));
    }

    /* PowerUpdate (MFi R32 95.15.2): level u16 param 6, charging state enum param 5,
     * other params (MaximumCurrentDrawn 0) skipped; level clamped to 100. */
    {
        static const uint8_t pu[] = { 0x00, 0x06, 0x00, 0x00, 0x05, 0xDC,   /* 1500 mA */
                                      0x00, 0x05, 0x00, 0x05, 0x01,         /* charging */
                                      0x00, 0x06, 0x00, 0x06, 0x00, 0x4D }; /* 77 % */
        static const uint8_t over[] = { 0x00, 0x06, 0x00, 0x06, 0x01, 0x2C };   /* 300 */
        tel_power_t pw, last;
        memset(&last, 0, sizeof(last));
        assert(tel_parse_power_update(pu, sizeof(pu), &pw));
        assert(pw.has_level && pw.level == 77 && pw.has_state && pw.state == 1);
        tel_merge_power(&last, &pw);
        assert(tel_parse_power_update(over, sizeof(over), &pw) && pw.level == 100 && !pw.has_state);
        tel_merge_power(&last, &pw);
        assert(last.level == 100 && last.has_state && last.state == 1);
        assert(!tel_parse_power_update(pu, sizeof(pu) - 1, &pw));
    }
    /* StartPowerUpdates union: stock params kept in order, 5 and 6 appended as voids;
     * nothing to send when stock already asks for both. */
    {
        static const uint8_t stock[] = { 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x04, 0x00, 0x04 };
        static const uint8_t want[] = { 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x04, 0x00, 0x04,
                                        0x00, 0x04, 0x00, 0x05, 0x00, 0x04, 0x00, 0x06 };
        static const uint8_t both[] = { 0x00, 0x04, 0x00, 0x06, 0x00, 0x04, 0x00, 0x05 };
        static const uint8_t level_only[] = { 0x00, 0x04, 0x00, 0x06 };
        uint8_t out[64];
        size_t n = tel_build_power_union(stock, sizeof(stock), out, sizeof(out));
        assert(n == sizeof(want) && memcmp(out, want, n) == 0);
        assert(tel_build_power_union(both, sizeof(both), out, sizeof(out)) == 0);
        n = tel_build_power_union(level_only, sizeof(level_only), out, sizeof(out));
        assert(n == 8 && out[7] == 0x05);
        assert(tel_build_power_union(NULL, 0, out, sizeof(out)) == 8);
        assert(tel_build_power_union(stock, sizeof(stock) - 1, out, sizeof(out)) == 0);
        assert(tel_build_power_union(stock, sizeof(stock), out, 15) == 0);
    }

    /* Ordering: stock's StartPowerUpdates (Encode, not yet on the wire) only arms the
     * extension; it goes out after the phone's first PowerUpdate, exactly once per Identify. */
    {
        static const uint8_t stock[] = { 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04 };
        static const uint8_t pu[] = { 0x00, 0x06, 0x00, 0x06, 0x00, 0x32 };
        hook_context_t ctx;
        iap2_frame_t f;
        int before;
        memset(&ctx, 0, sizeof(ctx));
        memset(&f, 0, sizeof(f));
        tel_module_def.on_init();                       /* IAP2_PHONE_BATTERY unset = on */
        before = sent_count;
        ctx.direction = MSG_DIR_OUTGOING; f.msgid = 0xAE00; f.payload = stock; f.payload_len = sizeof(stock);
        tel_module_def.on_message(&ctx, &f);
        assert(sent_count == before);
        ctx.direction = MSG_DIR_INCOMING; f.msgid = 0xAE01; f.payload = pu; f.payload_len = sizeof(pu);
        tel_module_def.on_message(&ctx, &f);
        assert(sent_count == before + 1 && sent_msg == 0xAE00 && sent_len == sizeof(stock) + 8);
        tel_module_def.on_message(&ctx, &f);
        ctx.direction = MSG_DIR_OUTGOING; f.msgid = 0xAE00; f.payload = stock; f.payload_len = sizeof(stock);
        tel_module_def.on_message(&ctx, &f);           /* stock repeats: once per Identify */
        ctx.direction = MSG_DIR_INCOMING; f.msgid = 0xAE01; f.payload = pu; f.payload_len = sizeof(pu);
        tel_module_def.on_message(&ctx, &f);
        assert(sent_count == before + 1);
        tel_module_def.on_state(&ctx, HOOK_EVENT_IDENTIFY_START, NULL);
        ctx.direction = MSG_DIR_OUTGOING; f.msgid = 0xAE00; f.payload = stock; f.payload_len = sizeof(stock);
        tel_module_def.on_message(&ctx, &f);
        ctx.direction = MSG_DIR_INCOMING; f.msgid = 0xAE01; f.payload = pu; f.payload_len = sizeof(pu);
        tel_module_def.on_message(&ctx, &f);
        assert(sent_count == before + 2);
    }

    /* Truncated / garbage: never reads past len, reports failure. */
    assert(!tel_parse_list_update(cap_entries + 6, sizeof(cap_entries) - 1 - 6 - 3, &l));
    srand(7);
    for (int i = 0; i < 100000; i++) {
        size_t len = (size_t)(rand() % sizeof(buf));
        for (size_t j = 0; j < len; j++) buf[j] = (uint8_t)rand();
        (void)tel_parse_list_update(buf, len, &l);
        tel_merge_lists(&last, &l);
        { tel_comm_t c; (void)tel_parse_comm_update(buf, len, &c); assert(strlen(c.carrier) < TEL_TEXT_MAX); }
        { tel_power_t pw; (void)tel_parse_power_update(buf, len, &pw); assert(pw.level <= 100); }
        { uint8_t o[80]; size_t n = tel_build_power_union(buf, len, o, sizeof(o)); assert(n <= sizeof(o)); }
        assert(l.recents.n <= TEL_LIST_MAX && l.favorites.n <= TEL_LIST_MAX);
        assert(last.recents.n <= TEL_LIST_MAX && last.favorites.n <= TEL_LIST_MAX);
    }
    puts("tel_hook_test: verified 0x4170 bytes, R29 hold/mute, AcceptCall/EndCall/SwapCalls commands, 3 captured 0x4171 frames, split/shrink merge, captured 0x4158, PowerUpdate battery + StartPowerUpdates union, 100000 malformed inputs PASS");
    return 0;
}
