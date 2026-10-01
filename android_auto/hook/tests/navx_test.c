/* Host test for src/navxlate.c: hand-encoded 0x8006/0x8007 bodies (aa-proxy-rs schema) -> legacy 0x8004/0x8005,
 * and 0x8006 steps[0].lanes -> the /tmp/sq5_aa_lanes record. */
#define NAVX_HOST_TEST
#include "../src/navxlate.c"
#include <assert.h>
#include <stdio.h>
void probe_log(const char *fmt, ...) { (void)fmt; }

typedef struct { unsigned char b[512]; unsigned n; } buf;
static void v(buf *o, uint64_t x) { o->n += put_varint(o->b + o->n, x); }
static void tagv(buf *o, unsigned f, uint64_t x) { v(o, (uint64_t)f << 3); v(o, x); }
static void tagb(buf *o, unsigned f, const buf *in) { v(o, ((uint64_t)f << 3) | 2); v(o, in->n); memcpy(o->b + o->n, in->b, in->n); o->n += in->n; }
static void tags(buf *o, unsigned f, const char *s) { buf t = {{0}, 0}; memcpy(t.b, s, strlen(s)); t.n = (unsigned)strlen(s); tagb(o, f, &t); }

/* decoded legacy fields */
typedef struct { char road[96]; long f[8]; int has[8]; } leg;
static leg parse(const unsigned char *p, int n)
{
    leg L; pb b = { p, p + n }, s; unsigned f, w; uint64_t x;
    memset(&L, 0, sizeof L);
    while (pb_next(&b, &f, &w, &x, &s)) {
        if (w == 2 && f == 1) { unsigned k = (unsigned)(s.e - s.p); memcpy(L.road, s.p, k); L.road[k] = 0; L.has[1] = 1; }
        else if (w == 0 && f < 8) { L.f[f] = (long)x; L.has[f] = 1; }
    }
    return L;
}
static int state(unsigned type, const char *road, int exitn, int angle, unsigned char *out, const char *fallback)
{
    buf m = {{0}, 0}, r = {{0}, 0}, cue = {{0}, 0}, lane = {{0}, 0}, ld = {{0}, 0}, step = {{0}, 0}, st = {{0}, 0};
    tagv(&m, 1, type); if (exitn >= 0) tagv(&m, 2, (uint64_t)exitn); if (angle >= 0) tagv(&m, 3, (uint64_t)angle);
    if (road) tags(&r, 1, road);
    tags(&cue, 1, "Turn left onto something long enough to be realistic");
    tagv(&ld, 1, 3); tagv(&ld, 2, 1); tagb(&lane, 1, &ld);
    tagb(&step, 1, &m); if (road) tagb(&step, 2, &r); tagb(&step, 3, &lane); tagb(&step, 4, &cue);
    tagb(&st, 1, &step);
    { buf d = {{0}, 0}; tags(&d, 1, "123 Main St"); tagb(&st, 2, &d); }
    return nav_state_to_legacy(st.b, st.n, out, 512, fallback);
}
/* one NavigationLane: pairs of (shape, highlighted) */
static void lane(buf *step, const int *d, int k)
{
    buf l = {{0}, 0}; int i;
    for (i = 0; i < k; i += 2) { buf ld = {{0}, 0}; tagv(&ld, 1, (uint64_t)d[i]); if (d[i + 1] >= 0) tagv(&ld, 2, (uint64_t)d[i + 1]); tagb(&l, 1, &ld); }
    tagb(step, 3, &l);
}
static int lanes_of(const buf *step, char *text)
{
    buf st = {{0}, 0}; tagb(&st, 1, step);
    { buf d = {{0}, 0}; tags(&d, 1, "123 Main St"); tagb(&st, 2, &d); }
    return nav_state_lanes(st.b, st.n, text);
}
static void lanes(void)
{
    char text[NAV_LANES_BODY], rec[NAV_LANES_REC]; unsigned char out[512]; int n, i; leg L;
    /* real 0x8006 from the car (after the 80 06 id): one step, no lanes; translation unchanged */
    static const char real[] = "\x0a\x30\x0a\x02\x08\x01\x12\x14\x0a\x12toward Goodyear St\x22\x14\x0a\x12toward Goodyear St"
                               "\x12\x1a\x0a\x18" "5751 Pacific Center Blvd";
    assert(sizeof(real) - 1 == 2 + 0x30 + 2 + 0x1a);
    assert(nav_state_lanes((const unsigned char *)real, sizeof(real) - 1, text) == 0 && text[0] == 0);
    n = nav_state_to_legacy((const unsigned char *)real, sizeof(real) - 1, out, 512, NULL); assert(n > 0); L = parse(out, n);
    assert(!strcmp(L.road, "toward Goodyear St") && L.f[3] == 1 && L.f[2] == 3);
    /* the state() fixture: one lane, SLIGHT_RIGHT highlighted */
    { unsigned char in[512]; buf m = {{0}, 0}, step = {{0}, 0}, lz = {{0}, 0}, ld = {{0}, 0}, st = {{0}, 0};
      tagv(&m, 1, 7); tagv(&ld, 1, 3); tagv(&ld, 2, 1); tagb(&lz, 1, &ld);
      tagb(&step, 1, &m); tagb(&step, 3, &lz); tagb(&st, 1, &step); memcpy(in, st.b, st.n);
      assert(nav_state_lanes(in, st.n, text) == 1 && !strcmp(text, "3,1")); }
    /* 3 lanes: straight/left, straight (highlighted), right (highlighted)/straight; is_highlighted absent = 0 */
    { buf step = {{0}, 0}, m = {{0}, 0}; int a[] = {1, 0, 4, -1}, b2[] = {1, 1}, c[] = {5, 1, 1, 0};
      tagv(&m, 1, 8); tagb(&step, 1, &m); lane(&step, a, 4); lane(&step, b2, 2); lane(&step, c, 4);
      assert(lanes_of(&step, text) == 3 && !strcmp(text, "1,0,4,0|1,1|5,1,1,0")); }
    /* U-turns, sharp, unknown shape 12 -> 0, empty lane kept */
    { buf step = {{0}, 0}; int a[] = {8, 1, 9, 0, 6, 0, 7, 0}, b2[] = {12, 1};
      lane(&step, a, 8); lane(&step, b2, 2); { buf l = {{0}, 0}; tagb(&step, 3, &l); }
      assert(lanes_of(&step, text) == 3 && !strcmp(text, "8,1,9,0,6,0,7,0|0,1|")); }
    /* more than 4 directions: first 4 kept */
    { buf step = {{0}, 0}; int a[] = {1, 0, 2, 0, 3, 0, 4, 0, 5, 1};
      lane(&step, a, 10); assert(lanes_of(&step, text) == 1 && !strcmp(text, "1,0,2,0,3,0,4,0")); }
    /* 8 lanes ok, 9 lanes -> none */
    { buf step = {{0}, 0}; int a[] = {1, 1};
      for (i = 0; i < 8; i++) lane(&step, a, 2);
      assert(lanes_of(&step, text) == 8 && strlen(text) == 8 * 3 + 7);
      lane(&step, a, 2); assert(lanes_of(&step, text) == 0 && text[0] == 0); }
    /* malformed lane direction (truncated inner length) -> none */
    { unsigned char bad[] = { 0x0a, 0x06, 0x1a, 0x04, 0x0a, 0x05, 0x08, 0x01 };
      assert(nav_state_lanes(bad, sizeof bad, text) == 0 && text[0] == 0); }
    { unsigned char junk[4] = { 0xff, 0xff, 0xff, 0xff }; assert(nav_state_lanes(junk, 4, text) == 0); }
    assert(nav_state_lanes(NULL, 0, text) == 0 && text[0] == 0);
    /* record: fixed length, seq at both ends, padded, '\n' last; worst case fits */
    assert(nav_lanes_record(7, 3, "1,0,4,0|1,1|5,1,1,0", rec) == NAV_LANES_REC);
    { static const char want[] = "SQ5L1 7\nlanes=3\n1,0,4,0|1,1|5,1,1,0\nend=7\n";
      assert(!memcmp(rec, want, sizeof(want) - 1) && rec[NAV_LANES_REC - 1] == '\n');
      for (i = (int)sizeof(want) - 1; i < NAV_LANES_REC - 1; i++) assert(rec[i] == ' '); }
    assert(nav_lanes_record(1, 0, "", rec) == NAV_LANES_REC && !memcmp(rec, "SQ5L1 1\nlanes=0\n\nend=1\n ", 24));
    { char big[NAV_LANES_BODY]; memset(big, '9', 8 * 15 + 7); big[8 * 15 + 7] = 0;
      assert(nav_lanes_record(4294967295u, 8, big, rec) == NAV_LANES_REC); }
}
int main(void)
{
    unsigned char out[512]; char road[96]; int n; leg L;
    /* 1. TURN_NORMAL_LEFT onto "W Alma Ave" */
    n = state(7, "W Alma Ave", -1, -1, out, NULL); assert(n > 0); L = parse(out, n);
    assert(!strcmp(L.road, "W Alma Ave") && L.f[2] == 1 && L.f[3] == 4 && !L.has[5] && !L.has[6]);
    /* 2. roundabout CCW with angle (right-hand traffic): exit 2 at 180 deg */
    n = state(35, "Roundabout Rd", 2, 180, out, NULL); assert(n > 0); L = parse(out, n);
    assert(L.f[3] == 13 && L.f[2] == 2 && L.f[5] == 2 && L.f[6] == 180);
    /* 3. position: 15 m, "50" FEET(6), 4 s, current road "Palm St" */
    {
        buf d = {{0}, 0}, sd = {{0}, 0}, cr = {{0}, 0}, pos = {{0}, 0};
        tagv(&d, 1, 15); tags(&d, 2, "50"); tagv(&d, 3, 6);
        tagb(&sd, 1, &d); tagv(&sd, 2, 4);
        tags(&cr, 1, "Palm St");
        tagb(&pos, 1, &sd); tagb(&pos, 3, &cr);
        n = nav_position_to_legacy(pos.b, pos.n, out, 512, road, sizeof road); assert(n > 0); L = parse(out, n);
        assert(L.f[1] == 15 && L.f[2] == 4 && L.f[3] == 50000 && L.f[4] == 6 && !strcmp(road, "Palm St"));
        /* "0.3" miles */
        memset(&d, 0, sizeof d); memset(&sd, 0, sizeof sd); memset(&pos, 0, sizeof pos);
        tagv(&d, 1, 483); tags(&d, 2, "0.3"); tagv(&d, 3, 4); tagb(&sd, 1, &d); tagv(&sd, 2, 40); tagb(&pos, 1, &sd);
        n = nav_position_to_legacy(pos.b, pos.n, out, 512, road, sizeof road); L = parse(out, n);
        assert(L.f[1] == 483 && L.f[3] == 300 && L.f[4] == 4);
    }
    /* 4. step without road -> fallback current road; DEPART */
    n = state(1, NULL, -1, -1, out, "Palm St"); assert(n > 0); L = parse(out, n);
    assert(!strcmp(L.road, "Palm St") && L.f[3] == 1 && L.f[2] == 3);
    /* 5. DESTINATION_RIGHT -> DESTINATION(19), RIGHT */
    n = state(42, "Home", -1, -1, out, NULL); L = parse(out, n); assert(L.f[3] == 19 && L.f[2] == 2);
    /* 6. legacy is always shorter than the new body it replaces (in-place rewrite) */
    {
        unsigned char in2[512]; buf m = {{0}, 0}, r = {{0}, 0}, step = {{0}, 0}, st = {{0}, 0};
        tagv(&m, 1, 8); tags(&r, 1, "A"); tagb(&step, 1, &m); tagb(&step, 2, &r); tagb(&st, 1, &step);
        memcpy(in2, st.b, st.n);
        n = nav_state_to_legacy(in2, st.n, out, 512, NULL); assert(n > 0 && (unsigned)n <= st.n + 2);
    }
    /* 7. garbage / empty -> 0 (left untouched) */
    { unsigned char junk[4] = { 0xff, 0xff, 0xff, 0xff }; assert(nav_state_to_legacy(junk, 4, out, 512, NULL) == 0);
      assert(nav_position_to_legacy(junk, 0, out, 512, road, sizeof road) == 0); }
    lanes();
    puts("PASS: navxlate 0x8006->0x8004 (turn, roundabout exit/angle, depart fallback road, destination side), "
         "0x8007->0x8005 (meters, seconds, display e3, units), garbage rejected; "
         "lanes (real sample, 3 lanes, >8 lanes, >4 directions, bad shape, malformed, record)");
    return 0;
}
