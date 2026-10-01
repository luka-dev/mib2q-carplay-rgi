/* Host check: UiConfig payload (VideoConfiguration field 11) and the per-view 0x8009 UpdateUiConfigRequest. */
#include <stdarg.h>
void probe_log(const char *fmt, ...) { (void)fmt; }
void live_rotary_video_channel(unsigned ch) { (void)ch; }
void live_msg_watch_start(void) { }
#include "../src/uiconfig.c"
#include <assert.h>
static void dump(const unsigned char *p, unsigned n) { for (unsigned i = 0; i < n; i++) printf("%02x", p[i]); printf(" (%u)\n", n); }
int main(void)
{
    unsigned char p[64]; unsigned n;
    /* 1080p defaults: top 90 bottom 60 left 420 right 420 + dark theme */
    ui_num = 3; ui_den = 2;
    ui_top = UI_TOP * 3 / 2; ui_bottom = UI_BOTTOM * 3 / 2; ui_left = UI_LEFT * 3 / 2; ui_right = UI_RIGHT * 3 / 2;
    n = payload(p, 32); dump(p, n);
    assert(n >= 16 && ui_theme_sent && p[0] == 0x5a && p[2] == 0x12 && p[n - 2] == 0x20 && p[n - 1] == 2);
    /* exact default bytes change with owner tuning (run 88: top 110, right 170); structure checked above */
    /* 15-byte fallback drops the theme */
    ui_top = 150; ui_bottom = 240; ui_left = 420; ui_right = 420;
    n = payload(p, 15); assert(n == 0 || !ui_theme_sent);
    n = payload(p, 32); assert(n == 18 && ui_theme_sent);
    /* zero fields omitted */
    ui_top = 150; ui_bottom = 0; ui_left = 0; ui_right = 1050;
    n = payload(p, 32); dump(p, n); assert(n == 12 && p[4] == 0x08 && p[7] == 0x20);
    /* relayout: full view = the connect-time insets (already scaled) */
    ui_top = 90; ui_bottom = 60; ui_left = 420; ui_right = 420;
    n = live_relayout_message(p, sizeof(p), 0); dump(p, n);
    assert(n == 18 && p[0] == 0x80 && p[1] == 0x09 && p[2] == 0x0a && p[3] == 14 && p[4] == 0x12);
    assert(!memcmp(p + 4, "\x12\x0a\x08\x5a\x10\x3c\x18\xa4\x03\x20\xa4\x03\x20\x02", 14));
    /* relayout: small view = run 108 Sport area (SM_* in 720p-frame units) scaled x1.5 at 1080p */
    n = live_relayout_message(p, sizeof(p), 1); dump(p, n);
    assert(n == 20 && p[0] == 0x80 && p[1] == 0x09 && p[2] == 0x0a && p[3] == 16 && p[4] == 0x12 && p[5] == 12);
#define V2(v) (unsigned char)(0x80 | ((v) & 0x7f)), (unsigned char)((v) >> 7)
    {
        const unsigned char want[] = { 0x08, V2(SM_TOP * 3 / 2), 0x10, V2(SM_BOTTOM * 3 / 2), 0x18, V2(SM_LEFT * 3 / 2),
                                       0x20, V2(SM_RIGHT * 3 / 2), 0x20, 2 };
        assert(!memcmp(p + 6, want, sizeof(want)));
    }
    /* 720p scaling (x1) */
    ui_num = 1; ui_den = 1;
    n = live_relayout_message(p, sizeof(p), 1); assert(n > 0 && p[12] == 0x18 && p[13] == (0x80 | (SM_LEFT & 0x7f)));
    /* 1080p viewport mode (1440x540, 1:1): one preset per cockpit state L / C / S, sent in FULL-frame
     * coordinates (run 117: the phone measures insets from the 1920x1080 frame) = preset + (240 x, 270 y) */
    vp_mode = 1;
    {
        /* run 149: the owner's map-zoom step -1 baked into all three presets */
        static const unsigned char want_s[] = { 0x08, V2(78 + 270), 0x10, V2(146 + 270), 0x18, V2(484 + 240),
                                                0x20, V2(394 + 240), 0x20, 2 };
        static const unsigned char want_c[] = { 0x08, V2(97 + 270), 0x10, V2(167 + 270), 0x18, V2(510 + 240),
                                                0x20, V2(510 + 240), 0x20, 2 };
        static const unsigned char want_l[] = { 0x08, V2(97 + 270), 0x10, V2(167 + 270), 0x18, V2(350 + 240),
                                                0x20, V2(370 + 240), 0x20, 2 };
        n = live_relayout_message(p, sizeof(p), 2); dump(p, n);
        assert(p[0] == 0x80 && p[1] == 0x09 && !memcmp(p + 6, want_s, sizeof(want_s)));
        n = live_relayout_message(p, sizeof(p), 1);
        assert(!memcmp(p + 6, want_c, sizeof(want_c)));
        n = live_relayout_message(p, sizeof(p), 0);
        assert(!memcmp(p + 6, want_l, sizeof(want_l)));
    }
    /* map zoom step +1: Large - only the height grows, around its centre (car + card stay put) */
    {
        uint32_t t = 347, b = 416, l = 590, r = 610, t0 = t, b0 = b;
        mz_step = 1; mz_apply(&t, &b, &l, &r, 0); mz_step = 0;
        assert(l == 590 && r == 610 && t < t0 && b < b0 && (1080 - b) - t > (1080 - b0 - t0) * 110 / 100);
        assert((int)t0 - (int)t - ((int)b0 - (int)b) <= 1 && (int)t0 - (int)t - ((int)b0 - (int)b) >= -1);
    }
    /* run 149: L/C presets = the old ones at map-zoom step -1; run 153: Sport = the original at step +3 */
    {
        uint32_t t = 77 + 270, b = 146 + 270, l = 350 + 240, r = 370 + 240;
        mz_step = -1; mz_apply(&t, &b, &l, &r, 0);
        assert(t == vp_preset[0][0] + VP_OY && b == vp_preset[0][1] + VP_OY && l == vp_preset[0][2] + VP_OX && r == vp_preset[0][3] + VP_OX);
        t = 78 + 270; b = 146 + 270; l = 580 + 240; r = 490 + 240;
        mz_step = 3; mz_apply(&t, &b, &l, &r, 2); mz_step = 0;   /* run 153: Sport = the original at step +3 */
        assert(t == vp_preset[2][0] + VP_OY && b == vp_preset[2][1] + VP_OY && l == vp_preset[2][2] + VP_OX && r == vp_preset[2][3] + VP_OX);
    }
    /* Sport step -2: only the width shrinks, around its centre */
    {
        uint32_t t = 348, b = 416, l = 820, r = 730, l0 = l, r0 = r;
        mz_step = -2; mz_apply(&t, &b, &l, &r, 2); mz_step = 0;
        assert(t == 348 && b == 416 && l > l0 && r > r0);
        assert(((int)l - (int)l0) - ((int)r - (int)r0) <= 1 && ((int)l - (int)l0) - ((int)r - (int)r0) >= -1);
    }
    /* run 162: the theme is always Dark (Light / Automatic crashed Android Auto on the cluster display) */
    n = live_relayout_message(p, sizeof(p), 0); assert(p[n - 2] == 0x20 && p[n - 1] == 2);
    vp_mode = 0;
    /* too small an output buffer is refused */
    assert(live_relayout_message(p, 10, 1) == 0);
    puts("PASS: uiconfig payload (defaults, theme fallback, zero fields omitted) and 0x8009 UpdateUiConfigRequest full/small");
    return 0;
}
