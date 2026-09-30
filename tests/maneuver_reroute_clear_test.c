/*
 * Field bug: a CarPlay reroute on a very short turn sends CMD_CLEAR followed
 * by a (typically IDENTICAL) CMD_MANEUVER, and the cluster pill stays open but
 * completely black.
 *
 * This test runs the REAL command-handling code of the renderer: it includes
 * maneuver_render/main.c (renamed main), so the command drain, CLEAR handling,
 * reveal-from-clear, fade-in, progress handoff and FRAME_READY/FRAME_CLEARED
 * event logic are the production statements.  Only the leaves are replaced:
 *   - the TCP server (scripted command batches, one batch per loop iteration),
 *   - GL/render/maneuver/scene/lane-panel/platform (recording stubs).
 * No GPU, EGL, sockets or QNX headers are needed.
 *
 * For every sequence and for three packings (all commands in one drain, one
 * command per loop iteration, two per iteration) the test asserts that after
 * the final MANEUVER the renderer:
 *   - is not in the cleared/blank state,
 *   - holds and DRAWS the final maneuver,
 *   - last drew it at full alpha (the fade-in finished, i.e. not black),
 *   - re-announced FRAME_READY after its last FRAME_CLEARED,
 *   - and (when it is the first maneuver after the last CLEAR) has the arrow
 *     progress state requested by the last MANEUVER/PROGRESS packet.
 *
 * Output: "maneuver_reroute_clear: REPRODUCED <case>" per failing case, or
 * "maneuver_reroute_clear: PASS ...".  Exit code 1 if any case reproduced.
 *
 * Build (see scripts/run_tests.sh):
 *   cc -std=gnu99 -O1 -Wall -Imaneuver_render -Icommon \
 *      tests/maneuver_reroute_clear_test.c -lpthread -lm
 */
/* No GL headers on the host: satisfy main.c's three GL uses ourselves. */
#define CR_GL_COMPAT_H 1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
void glReadPixels(int x, int y, int w, int h, unsigned format, unsigned type, void *pixels);

#define main renderer_application_main
#include "../maneuver_render/main.c"
#undef main

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Recording state                                                     */
/* ------------------------------------------------------------------ */
typedef struct { int n; cr_cmd_t c[16]; } t_batch_t;
#define T_MAX_BATCH 48
#define T_TRAIL 12                     /* empty iterations after the last command */

static t_batch_t t_batches[T_MAX_BATCH];
static int t_nb, t_iter, t_cur, t_qpos;

static int t_seq, t_last_clear_seq, t_last_ready_seq, t_ready_count, t_clear_count;
static float t_alpha;
static float t_alpha_at[T_MAX_BATCH];   /* last global alpha set in each loop iteration */
static int t_delay_ms[T_MAX_BATCH];     /* real sleep before an iteration's drain */
static int t_draw_icon_none;            /* draws issued with ICON_NONE (real maneuver_draw draws nothing) */
static int t_draws;
static maneuver_state_t t_draw_state;
static float t_draw_alpha;

float g_3d_offset_adjust;

struct cr_scene { int unused; };
struct cr_lane_panel { int unused; };
static struct cr_scene t_scene_a, t_scene_b;
static int t_scene_toggle;
static struct cr_lane_panel t_panel;
static cr_scene_info_t t_scene_info;

/* ------------------------------------------------------------------ */
/* Stubs (signatures match the headers included above)                */
/* ------------------------------------------------------------------ */
void glReadPixels(int x, int y, int w, int h, unsigned format, unsigned type, void *pixels)
{ (void)x; (void)y; (void)w; (void)h; (void)format; (void)type; (void)pixels; }

/* TCP server: scripted */
int cr_server_init(int port) { (void)port; return 0; }
void cr_server_poll(void) {
    t_cur = t_iter++; t_qpos = 0;
    if (t_cur < T_MAX_BATCH && t_delay_ms[t_cur] > 0) {
        struct timespec ts;
        ts.tv_sec = t_delay_ms[t_cur] / 1000;
        ts.tv_nsec = (long)(t_delay_ms[t_cur] % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
}
int cr_server_read_cmd(cr_cmd_t *out) {
    if (t_cur < t_nb && t_qpos < t_batches[t_cur].n) {
        *out = t_batches[t_cur].c[t_qpos++];
        return 1;
    }
    return 0;
}
void cr_server_shutdown(void) {}
int cr_server_peer_closed(void) { return 0; }
void cr_server_clear_peer_closed(void) {}
void cr_server_send_heartbeat(void) {}
void cr_server_mark_ready(void) {}
void cr_server_mark_frame_ready(void) { t_last_ready_seq = ++t_seq; t_ready_count++; }
void cr_server_clear_frame_ready(void) {}
void cr_server_mark_frame_cleared(void) { t_last_clear_seq = ++t_seq; t_clear_count++; }

/* platform */
int platform_init(int w, int h) { (void)w; (void)h; return 0; }
int platform_swap(void) { return 1; }
void platform_poll(void) {}
int platform_should_close(void) { return t_iter >= t_nb; }
void platform_shutdown(void) {}
void platform_get_framebuffer_size(int *w, int *h) { *w = CR_DEFAULT_WIDTH; *h = CR_DEFAULT_HEIGHT; }
void platform_get_routing_ids(int *d, int *c, int *o) { *d = CR_DISPLAY_ID; *c = CR_CONTEXT_ID; *o = CR_DISPLAYABLE_ID; }
void platform_ensure_focus(void) {}
void platform_check_and_recover_window(void) {}
void platform_release_displayable(void) {}
int platform_key_tap(int key) { (void)key; return 0; }

/* render */
int render_init(int w, int h) { (void)w; (void)h; return 0; }
int render_load_flag_atlas(const char *p, int w, int h, int n) { (void)p; (void)w; (void)h; (void)n; return 0; }
void render_set_debug_grid(int on) { (void)on; }
void render_debug_grid(void) {}
void render_set_perspective(int on) { (void)on; }
void render_set_frame_step(float f) { (void)f; }
float render_frame_step(void) { return 1.0f; }
void render_get_layout_matrix(float out[16]) {
    int i;
    for (i = 0; i < 16; i++) out[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}
void render_get_visible_area(cr_rect_t *cur, cr_rect_t *tgt) {
    cr_rect_t r;
    r.x = CR_POPUP_X; r.y = CR_POPUP_Y; r.w = CR_POPUP_W; r.h = CR_POPUP_H;
    if (cur) *cur = r;
    if (tgt) *tgt = r;
}
int render_set_visible_area(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; return 0; }
void render_invalidate_masks(void) {}
int render_is_animating(void) { return 0; }
void render_reset_content_offset(void) {}
void render_set_content_framing(float x, float y, float d) { (void)x; (void)y; (void)d; }
void render_set_global_alpha(float a) {
    t_alpha = a;
    if (t_cur >= 0 && t_cur < T_MAX_BATCH) t_alpha_at[t_cur] = a;
}
void render_set_route_progress(float f, float p, float g) { (void)f; (void)p; (void)g; }
void render_set_viewport(int w, int h) { (void)w; (void)h; }
void render_begin_frame(void) {}
void render_end_frame(void) {}
void render_shutdown(void) {}

/* maneuver (drawing/animation engine): instantaneous transitions */
void maneuver_commit_pushed_state(const maneuver_state_t *s) { (void)s; }
void maneuver_draw(const maneuver_state_t *s, const maneuver_state_t *n) {
    (void)n;
    t_draws++;
    if (s->icon == ICON_NONE && n == NULL) t_draw_icon_none++;
    t_draw_state = *s;
    t_draw_alpha = t_alpha;
}
int maneuver_is_animating(void) { return 0; }
int maneuver_is_debug(void) { return 0; }
int maneuver_is_pushing(void) { return 0; }
int maneuver_needs_redraw(void) { return 0; }
void maneuver_prepare_frame(const maneuver_state_t *s, const maneuver_state_t *n) { (void)s; (void)n; }
void maneuver_set_scene_provider(const maneuver_scene_provider_t *p) { (void)p; }
void maneuver_set_slide(float t) { (void)t; }
void maneuver_start_push(void) {}
void maneuver_toggle_debug(void) {}

/* scenes / lane panel */
cr_scene_t *cr_scene_create(void) { return (t_scene_toggle++ & 1) ? &t_scene_b : &t_scene_a; }
void cr_scene_destroy(cr_scene_t *s) { (void)s; }
void cr_scene_configure_provider(maneuver_scene_provider_t *p) { (void)p; }
int cr_scene_prepare(cr_scene_t *s, const cr_scene_input_t *in, const cr_scene_view_t *v) { (void)s; (void)in; (void)v; return 1; }
const route_path_t *cr_scene_route(const cr_scene_t *s) { (void)s; return NULL; }
const cr_scene_info_t *cr_scene_info(const cr_scene_t *s) { (void)s; return &t_scene_info; }
int cr_scene_is_native(const cr_scene_t *s) { (void)s; return 1; }
void cr_scene_paint(cr_scene_t *s, float a, float b, float c, float d) { (void)s; (void)a; (void)b; (void)c; (void)d; }

cr_lane_panel_t *cr_lane_panel_create(void) { return &t_panel; }
void cr_lane_panel_destroy(cr_lane_panel_t *p) { (void)p; }
void cr_lane_panel_clear(cr_lane_panel_t *p) { (void)p; }
int cr_lane_panel_update(cr_lane_panel_t *p, const cr_lane_guidance_t *l, float w, double now)
{ (void)p; (void)l; (void)w; (void)now; return 0; }
int cr_lane_panel_animating(const cr_lane_panel_t *p, double now) { (void)p; (void)now; return 0; }
void cr_lane_panel_draw(const cr_lane_panel_t *p, cr_rect_t v, double now) { (void)p; (void)v; (void)now; }
void cr_lane_panel_framing(cr_lane_panel_t *p, const cr_scene_t *c, const cr_scene_t *n, float out[3]) {
    (void)p; (void)c; (void)n;
    out[0] = out[1] = out[2] = 0.0f;
}

/* ------------------------------------------------------------------ */
/* Packet builders                                                     */
/* ------------------------------------------------------------------ */
/* Maneuver 0: the short turn (90 deg, no side streets).  Maneuver 1: a different turn. */
static void t_man(cr_cmd_t *c, int which, int state, int level, int refresh) {
    memset(c, 0, sizeof(*c));
    c->cmd = CMD_MANEUVER;
    c->flags = (uint8_t)(MAN_FLAG_PROGRESS | CR_PROGRESS_FLAG | (refresh ? MAN_FLAG_REFRESH : 0));
    c->payload[0] = ICON_TURN;
    if (which == 0) {
        c->payload[1] = 1;
        c->payload[2] = 0x00; c->payload[3] = 90;
    } else {
        c->payload[1] = (uint8_t)0xFF;                      /* -1 */
        c->payload[2] = 0xFF; c->payload[3] = 0xD3;         /* -45 */
        c->payload[5] = 1; c->payload[6] = 0; c->payload[7] = 30;
    }
    c->payload[42] = (uint8_t)state;
    c->payload[44] = (uint8_t)level;
    c->payload[45] = (uint8_t)(state != CR_PROGRESS_OFF);
}
static void t_prog(cr_cmd_t *c, int state, int level) {
    memset(c, 0, sizeof(*c));
    c->cmd = CMD_PROGRESS;
    c->flags = CR_PROGRESS_FLAG;
    c->payload[0] = (uint8_t)level;
    c->payload[1] = (uint8_t)(state != CR_PROGRESS_OFF);
    c->payload[2] = (uint8_t)state;
}
static void t_op(cr_cmd_t *c, char op) {
    switch (op) {
    case 'A': t_man(c, 0, CR_PROGRESS_FILL, 3, 0); break;
    case 'a': t_man(c, 0, CR_PROGRESS_BLINK_LOW, 0, 0); break;
    case 'B': t_man(c, 1, CR_PROGRESS_FILL, 3, 0); break;
    case 'r': t_man(c, 0, CR_PROGRESS_FILL, 3, 1); break;    /* roads-only REFRESH */
    case 'f': t_prog(c, CR_PROGRESS_FILL, 1); break;
    case 'l': t_prog(c, CR_PROGRESS_BLINK_LOW, 0); break;
    case 'h': t_prog(c, CR_PROGRESS_BLINK_HIGH, 0); break;
    case 'o': t_prog(c, CR_PROGRESS_OFF, 16); break;
    default:  memset(c, 0, sizeof(*c)); c->cmd = CMD_CLEAR; break;   /* 'C' */
    }
}

/* ------------------------------------------------------------------ */
/* One run                                                             */
/* ------------------------------------------------------------------ */
static int t_failures;

static void t_fail(const char *seq, const char *packing, const char *why) {
    t_failures++;
    if (t_failures <= 12)
        printf("maneuver_reroute_clear: REPRODUCED %s/%s: %s\n", seq, packing, why);
}

static void t_run(const char *seq, int packing, const char *packing_name) {
    int len = (int)strlen(seq), i, per, groups;
    int last_clear = -1, last_man = -1, mans_after_clear = 0;
    int exp_state = CR_PROGRESS_OFF;
    cr_cmd_t cmds[16], mcmd;
    maneuver_state_t expect;
    char *argv[2];

    memset(t_batches, 0, sizeof(t_batches));
    for (i = 0; i < len; i++) t_op(&cmds[i], seq[i]);

    per = packing == 0 ? len : packing == 1 ? 1 : 2;
    groups = (len + per - 1) / per;
    for (i = 0; i < len; i++) {
        t_batch_t *b = &t_batches[i / per];
        b->c[b->n++] = cmds[i];
    }
    t_nb = groups + T_TRAIL;

    /* Expected outcome from the wire order (TCP order wins). */
    for (i = 0; i < len; i++) if (seq[i] == 'C') last_clear = i;
    for (i = last_clear + 1; i < len; i++) {
        if (cmds[i].cmd == CMD_MANEUVER) {
            last_man = i;
            mans_after_clear++;
            exp_state = cr_progress_decode(cmds[i].flags, cmds[i].payload[42], cmds[i].payload[45]);
        } else if (cmds[i].cmd == CMD_PROGRESS) {
            exp_state = cr_progress_decode(cmds[i].flags, cmds[i].payload[2], cmds[i].payload[1]);
        }
    }
    if (last_man < 0) { t_fail(seq, packing_name, "bad test sequence (no MANEUVER after last CLEAR)"); return; }
    mcmd = cmds[last_man];
    cr_decode_maneuver(&mcmd, &expect);

    t_iter = t_cur = t_qpos = 0;
    t_seq = t_last_clear_seq = t_last_ready_seq = t_ready_count = t_clear_count = 0;
    t_alpha = 0.0f; t_draws = 0; t_draw_alpha = 0.0f;
    memset(&t_draw_state, 0, sizeof(t_draw_state));
    g_anim_prev_rendered = 0;
    argv[0] = (char *)"maneuver_render"; argv[1] = NULL;

    renderer_application_main(1, argv);

    if (t_iter < t_nb) { t_fail(seq, packing_name, "renderer loop exited early"); return; }
    if (g_cleared)
        t_fail(seq, packing_name, "renderer still in cleared/blank state after final MANEUVER");
    else if (!g_engine.has_current || g_engine.current.icon != expect.icon
             || g_engine.current.exit_angle != expect.exit_angle
             || g_engine.current.direction != expect.direction
             || g_engine.current.junction_angle_count != expect.junction_angle_count)
        t_fail(seq, packing_name, "engine current maneuver is not the final MANEUVER");
    else if (t_draws == 0)
        t_fail(seq, packing_name, "nothing was ever drawn");
    else if (t_draw_state.icon != expect.icon || t_draw_state.exit_angle != expect.exit_angle
             || t_draw_state.direction != expect.direction)
        t_fail(seq, packing_name, "last drawn maneuver is not the final MANEUVER");
    else if (t_draw_alpha < 0.999f)
        t_fail(seq, packing_name, "last frame drawn at alpha<1 (black/transparent pill)");
    else if (t_alpha < 0.999f)
        t_fail(seq, packing_name, "global alpha stuck below 1 after final MANEUVER");
    else if (t_clear_count > 0 && t_last_ready_seq < t_last_clear_seq)
        t_fail(seq, packing_name, "FRAME_READY not re-announced after last FRAME_CLEARED");
    else if (mans_after_clear == 1 && g_arrow.state != exp_state)
        t_fail(seq, packing_name, "arrow progress state differs from last MANEUVER/PROGRESS");
}

/* ------------------------------------------------------------------ */
/* Captured field sequence (Java -> renderer wire, "omitted distance") */
/* ------------------------------------------------------------------ */
/* Encoded exactly as RendererServer.sendManeuverPacket / sendProgress do:
 *   MANEUVER: flags = MAN_FLAG_BAP_GEOMETRY(0x04) | MAN_FLAG_PROGRESS(0x02) | PROGRESS_FLAG(0x20)
 *             payload[0]=icon payload[42]=progressState payload[44]=level payload[45]=mode
 *   PROGRESS: flags = PROGRESS_FLAG(0x20); payload[0]=level payload[1]=mode payload[2]=state
 *   CLEAR:    all zero.
 * The perspective flag is not part of the capture and is omitted (irrelevant to visibility).
 * Ops: 'E' establish MANEUVER(icon=2,ps=1 FILL,mode=1,lvl=0)
 *      'M' MANEUVER(icon=2,ps=0 OFF,mode=0,lvl=0)   'P' PROGRESS(lvl=0,ps=0,mode=0)
 *      'C' CLEAR   '|' next frame, ~200 ms later (spread only; burst ignores it). */
static void t_cap_op(cr_cmd_t *c, char op) {
    memset(c, 0, sizeof(*c));
    switch (op) {
    case 'E': case 'M': {
        int ps = op == 'E' ? CR_PROGRESS_FILL : CR_PROGRESS_OFF;
        c->cmd = CMD_MANEUVER;
        c->flags = (uint8_t)(MAN_FLAG_BAP_GEOMETRY | MAN_FLAG_PROGRESS | CR_PROGRESS_FLAG);
        c->payload[0] = ICON_TURN;
        c->payload[1] = 1;
        c->payload[2] = 0x00; c->payload[3] = 90;
        c->payload[42] = (uint8_t)ps;
        c->payload[44] = 0;
        c->payload[45] = (uint8_t)(op == 'E' ? 1 : 0);
        break;
    }
    case 'P':
        c->cmd = CMD_PROGRESS; c->flags = CR_PROGRESS_FLAG;
        c->payload[0] = 0; c->payload[1] = 0; c->payload[2] = CR_PROGRESS_OFF;
        break;
    default: c->cmd = CMD_CLEAR; break;
    }
}

static int t_cap_failures;

static void t_cap_fail(const char *name, const char *why) {
    t_cap_failures++;
    printf("maneuver_reroute_clear: REPRODUCED %s: %s\n", name, why);
}

/* One captured-sequence case; prints one REPRODUCED/PASS line. */
static void t_cap_run(const char *name, const char *seq, int spread) {
    int len = (int)strlen(seq), i, frame = 0, nframes, last_man_frame = -1, first_bright = -1;
    cr_cmd_t cmd, mcmd;
    maneuver_state_t expect;
    char *argv[2];
    char why[200];
    float peak = 0.0f;

    memset(&mcmd, 0, sizeof(mcmd));
    memset(t_batches, 0, sizeof(t_batches));
    memset(t_delay_ms, 0, sizeof(t_delay_ms));
    memset(t_alpha_at, 0, sizeof(t_alpha_at));
    for (i = 0; i < len; i++) {
        if (seq[i] == '|') { if (spread) { frame++; t_delay_ms[frame] = 200; } continue; }
        t_cap_op(&cmd, seq[i]);
        t_batches[frame].c[t_batches[frame].n++] = cmd;
        if (seq[i] == 'M' || seq[i] == 'E') { last_man_frame = frame; mcmd = cmd; }
    }
    nframes = frame + 1;
    t_nb = nframes + T_TRAIL;
    cr_decode_maneuver(&mcmd, &expect);

    t_iter = t_cur = t_qpos = 0;
    t_seq = t_last_clear_seq = t_last_ready_seq = t_ready_count = t_clear_count = 0;
    t_alpha = 0.0f; t_draws = 0; t_draw_alpha = 0.0f; t_draw_icon_none = 0;
    memset(&t_draw_state, 0, sizeof(t_draw_state));
    g_anim_prev_rendered = 0;
    argv[0] = (char *)"maneuver_render"; argv[1] = NULL;

    renderer_application_main(1, argv);

    for (i = last_man_frame; i < t_nb && i < T_MAX_BATCH; i++) {
        if (t_alpha_at[i] > peak) peak = t_alpha_at[i];
        if (first_bright < 0 && t_alpha_at[i] > 0.99f) first_bright = i - last_man_frame;
    }
    printf("  [%s] final MANEUVER frame=%d g_cleared=%d g_fade_alpha=%.3f alpha_ramp_frames=%d "
           "draws=%d draw_icon=%d draw_alpha=%.3f arrow(state=%d path=%.2f glow=%.2f) "
           "FRAME_READY=%d FRAME_CLEARED=%d\n",
           name, last_man_frame, g_cleared, (double)g_fade_alpha, first_bright, t_draws,
           t_draw_state.icon, (double)t_draw_alpha, g_arrow.state,
           (double)g_arrow.path_weight, (double)g_arrow.glow, t_ready_count, t_clear_count);

    why[0] = 0;
    if (t_iter < t_nb) snprintf(why, sizeof why, "renderer loop exited early");
    else if (g_cleared) snprintf(why, sizeof why, "g_cleared still set (main.c:214/323) after final MANEUVER");
    else if (g_fade_alpha <= 0.99f || peak <= 0.99f)
        snprintf(why, sizeof why, "global alpha never exceeded 0.99 (g_fade_alpha=%.3f)", (double)g_fade_alpha);
    else if (first_bright < 0 || first_bright > (int)(1.0f / FADE_SPEED) + 2)
        snprintf(why, sizeof why, "alpha ramp took %d frames (> fade window)", first_bright);
    else if (!g_engine.has_current || g_engine.current.icon == ICON_APPROACH
             || g_engine.current.icon == ICON_NONE)
        snprintf(why, sizeof why, "engine current is the APPROACH/NONE stub, not the maneuver");
    else if (g_engine.current.icon != expect.icon || g_engine.current.exit_angle != expect.exit_angle
             || g_engine.current.direction != expect.direction)
        snprintf(why, sizeof why, "engine current differs from the final MANEUVER");
    else if (g_engine.phase != ENGINE_IDLE || g_engine.has_next || g_engine.has_pending)
        snprintf(why, sizeof why, "engine not settled on the maneuver (phase=%d)", g_engine.phase);
    else if (t_draws == 0) snprintf(why, sizeof why, "nothing was drawn");
    else if (t_draw_icon_none)
        snprintf(why, sizeof why, "maneuver_draw called with ICON_NONE (real one draws nothing)");
    else if (t_draw_state.icon != expect.icon || t_draw_state.exit_angle != expect.exit_angle)
        snprintf(why, sizeof why, "last drawn maneuver is not the final MANEUVER");
    else if (t_draw_alpha <= 0.99f)
        snprintf(why, sizeof why, "last draw at alpha %.3f (not visible)", (double)t_draw_alpha);
    else {
        cr_rect_t cur, tgt;
        render_get_visible_area(&cur, &tgt);
        if (cur.w <= 0 || cur.h <= 0) snprintf(why, sizeof why, "visible area empty");
        else if (t_ready_count == 0 || t_last_ready_seq < t_last_clear_seq)
            snprintf(why, sizeof why, "FRAME_READY not announced after last FRAME_CLEARED");
    }
    if (why[0]) t_cap_fail(name, why);
    else printf("maneuver_reroute_clear: PASS %s (arrow drawn, alpha>0.99 after %d frames, FRAME_READY announced)\n",
                name, first_bright);
}

int main(void) {
    static const char *seqs[] = {
        /* identical maneuver after CLEAR, progress/blink phases around it */
        "AfCAlhl", "AfCAhlh", "AlhCAlhl",
        /* double / triple / rapid CLEAR-MANEUVER cycles */
        "ACCA", "ACCCA", "ACA", "ACACA", "ACACACA",
        /* changed maneuver */
        "ACBCA", "ACBCACB", "ACB",
        /* progress and blink interleaved before and after the MANEUVER */
        "AflCfAhl", "ACflAhl", "AChAlh", "AlCflhA", "ACfCoA",
        /* blink-embedded maneuver, refresh flag after CLEAR, cold CLEAR */
        "aCAl", "aCahl", "ACrhl", "AfCrh", "CA", "CfA"
    };
    static const char *names[] = { "burst", "spread", "pairs" };
    unsigned s, p;
    int runs = 0;

    /* The renderer logs every command to stderr; keep the test output readable. */
    if (!getenv("RGI_TEST_VERBOSE")) {
        if (!freopen("/dev/null", "w", stderr)) { /* keep going with noisy stderr */ }
    }
    for (s = 0; s < sizeof(seqs) / sizeof(seqs[0]); s++)
        for (p = 0; p < 3; p++) { t_run(seqs[s], (int)p, names[p]); runs++; }

    /* Captured field sequences: one drain, and spread over frames ~200 ms apart. */
    t_cap_run("captured-omitted-distance/burst",  "EPCPPPM", 0);
    t_cap_run("captured-omitted-distance/spread", "E|PCP|PPM", 1);
    t_cap_run("captured-omitted-distance/spread-return-burst", "E|PCPPPM", 1);
    /* Cold: the MANEUVER(mode=0/ps=0) is the first maneuver after a CLEAR, no prior FILL. */
    t_cap_run("captured-omitted-distance-cold/burst",  "CM", 0);
    t_cap_run("captured-omitted-distance-cold/spread", "PCP|PPM", 1);
    t_cap_run("captured-omitted-distance-cold/first-ever", "M", 0);
    t_failures += t_cap_failures;
    runs += 6;

    fflush(stdout);
    if (t_failures) {
        printf("maneuver_reroute_clear: REPRODUCED %d of %d cases failed\n", t_failures, runs);
        return 1;
    }
    printf("maneuver_reroute_clear: PASS (%d sequences x packings; CLEAR then identical/changed MANEUVER always redraws)\n", runs);
    return 0;
}
