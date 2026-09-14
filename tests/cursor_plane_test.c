#include "../cursor_render/session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static cursor_session session;
static uint64_t test_now = 1000;
static int is_open, draws, managed = 1, fail_open;
static uint8_t reply[CURSOR_ACK_SIZE];
static uint32_t reply_seq;
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
int cursor_plane_open(unsigned w, unsigned h) {
    assert(w == 1024 && h == 480);
    if (fail_open) return -1;
    is_open = 1; return 0;
}
int cursor_plane_managed(void) { return managed; }
int cursor_plane_draw(const cursor_frame *f) {
    assert(is_open && managed && (f->mode == CURSOR_LOADER || (f->alpha && f->x < f->vw && f->y < f->vh)));
    draws++; return 0;
}
void cursor_plane_close(void) { is_open = 0; }
static void tick(void) {
    test_now += 10;
    if (cursor_session_tick(&session, test_now, reply)) reply_seq = get32(reply);
}
static void emit(uint8_t *p, unsigned mode, uint32_t seq, uint32_t token) {
    int result;
    p[1] = mode; cursor_put32(p + 4, seq); cursor_put32(p + 8, token);
    result = cursor_session_receive(&session, p, CURSOR_FRAME_SIZE, test_now, reply);
    assert(result >= 0);
    if (result) reply_seq = get32(reply);
}
static uint32_t wait_ack(uint32_t seq, int status) {
    int i;
    for (i = 0; i < 100; i++) {
        if (reply_seq == seq) { assert((int32_t)get32(reply + 4) == status); return get32(reply + 8); }
        tick();
    }
    assert(!"missing cursor ACK"); return 0;
}
static void wait_open(int expected) {
    int i;
    for (i = 0; i < 100; i++) { if (is_open == expected) return; tick(); }
    assert(!"cursor window lifecycle timeout");
}
static int draw_count(void) { return draws; }

int main(int argc, char **argv) {
    uint8_t wire[CURSOR_FRAME_SIZE], bad[CURSOR_FRAME_SIZE];
    cursor_frame f;
    uint32_t token, old_token;
    FILE *input;
    int i, n;
    assert(argc == 2); input = fopen(argv[1], "rb"); assert(input);
    assert(fread(wire, 1, CURSOR_FRAME_SIZE, input) == CURSOR_FRAME_SIZE); fclose(input);
    assert(cursor_decode(&f, wire, CURSOR_FRAME_SIZE) == 0);
    assert(f.seq == 0x12345678 && f.token == 0x76543210 && f.vx == 100 && f.vy == 20
        && f.vw == 800 && f.vh == 440 && f.x == 399 && f.y == 219 && f.flags == CURSOR_INPUT_TOUCH && f.activity == 7 && f.revision == 9);
    for (i = 0; i < CURSOR_FRAME_SIZE; i++) assert(cursor_decode(&f, wire, i) != 0);
    memcpy(bad, wire, CURSOR_FRAME_SIZE); bad[3] = 16; assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) != 0);
    memcpy(bad, wire, CURSOR_FRAME_SIZE); bad[12] = 255; assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) != 0);
    memcpy(bad, wire, CURSOR_FRAME_SIZE); bad[16] = 255; assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) != 0);
    memcpy(bad, wire, CURSOR_FRAME_SIZE); bad[24] = 255; assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) != 0);
    memcpy(bad, wire, CURSOR_FRAME_SIZE); bad[28] = 5; assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) != 0);
    memcpy(bad, wire, CURSOR_FRAME_SIZE); bad[31] = 1; assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) != 0);
    assert(cursor_decode(&f, wire, CURSOR_FRAME_SIZE) == 0);
    memcpy(bad, wire, CURSOR_FRAME_SIZE); bad[1] = CURSOR_LOADER; bad[24] = 255; bad[28] = 0;
    assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) == 0);
    bad[1] = 4; assert(cursor_decode(&f, bad, CURSOR_FRAME_SIZE) != 0);

    cursor_session_init(&session, 12345);
    emit(wire, CURSOR_PROBE, 1, 0); token = wait_ack(1, 1); assert(token); wait_open(0);
    old_token = token;
    emit(wire, CURSOR_SHOW, 2, token); token = wait_ack(2, 0); wait_open(1); n = draw_count();
    emit(wire, CURSOR_SHOW, 3, old_token); wait_ack(3, -2); assert(draw_count() == n);
    wait_open(0); // native lease expires with no Java/video callbacks
    emit(wire, CURSOR_SHOW, 4, token); wait_ack(4, -2); wait_open(0);
    emit(wire, CURSOR_PROBE, 5, 0); token = wait_ack(5, 1);
    emit(wire, CURSOR_SHOW, 6, token); token = wait_ack(6, 0); wait_open(1);
    emit(wire, CURSOR_HIDE, 7, 0); wait_ack(7, 0); wait_open(0);
    emit(wire, CURSOR_SHOW, 8, token); wait_ack(8, -2); wait_open(0);

    emit(wire, CURSOR_PROBE, 9, 0); token = wait_ack(9, 1);
    emit(wire, CURSOR_SHOW, 10, token); token = wait_ack(10, 0);
    cursor_session_reset(&session); wait_open(0);
    emit(wire, CURSOR_SHOW, 11, token); wait_ack(11, -2);

    emit(wire, CURSOR_PROBE, 12, 0); token = wait_ack(12, 1);
    managed = 0;
    n = draw_count(); emit(wire, CURSOR_SHOW, 13, token); wait_open(1);
    cursor_session_reset(&session);
    wait_open(0);
    managed = 1;
    assert(draw_count() == n); // disconnect during adoption never publishes coloured pixels
    emit(wire, CURSOR_SHOW, 14, token); wait_ack(14, -2);

    // Native keeps rendering between packets, and heartbeat traffic never keeps it visible.
    emit(wire, CURSOR_PROBE, 100, 0); token = wait_ack(100, 1);
    emit(wire, CURSOR_SHOW, 101, token); token = wait_ack(101, 0);
    assert(get32(reply+16)==CURSOR_SHOW && get32(reply+24)>0);
    assert(reply[20]==1 && reply[21]==143 && reply[22]==0 && reply[23]==219);
    n=draw_count(); for(i=0;i<10;i++) tick(); assert(draw_count()>n);
    for(i=0;i<14;i++) {
        for(int j=0;j<10;j++) tick();
        emit(wire,CURSOR_SHOW,102+i,token); token=wait_ack(102+i,0);
    }
    assert(!is_open && get32(reply+16)==CURSOR_HIDE && get32(reply+24)==0);
    cursor_put32(wire+32,8); cursor_put32(wire+36,10);
    emit(wire,CURSOR_SHOW,120,token); token=wait_ack(120,0); assert(is_open);
    cursor_session_reset(&session);

    emit(wire, CURSOR_PROBE, 15, 0); token = wait_ack(15, 1);
    fail_open = 1;
    emit(wire, CURSOR_SHOW, 16, token); wait_ack(16, -1); wait_open(0);
    cursor_session_reset(&session);
    puts("cursor_plane_test: Java/C wire, autonomous redraw, leases, stale commands, disconnect/adoption and failure PASS");
    return 0;
}
