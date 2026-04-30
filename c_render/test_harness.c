/*
 * Test harness for c_render — standalone GLFW binary (macOS only).
 *
 * Acts as the **TCP SERVER** on 127.0.0.1:CR_TCP_PORT.  After the
 * 2026-04 topology flip the renderer is the TCP client, so the harness
 * matches what Java BAPBridge does in production.
 *
 * Arrow keys cycle through maneuver presets.
 * R = random maneuver, Space = screenshot, Q/ESC = quit.
 *
 * Workflow:
 *   1. Run harness (binds + listens).
 *   2. Run c_render (connects to 127.0.0.1:CR_TCP_PORT).
 *   3. Press keys in the harness window — commands stream to renderer.
 *
 * Heartbeats from renderer are drained silently so the kernel TCP
 * buffer doesn't fill.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <GLFW/glfw3.h>
#include "protocol.h"
#include "maneuver.h"   /* ICON_* constants, MAX_JUNCTION_ANGLES */

/* ================================================================
 * TCP server — accepts the renderer client, sends commands, drains
 * any inbound heartbeat traffic.
 * ================================================================ */

static int g_listen_fd = -1;
static int g_client_fd = -1;

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int tcp_setup_server(void) {
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        fprintf(stderr, "harness: socket: %s\n", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(g_listen_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(CR_TCP_PORT);

    if (bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "harness: bind %d: %s\n", CR_TCP_PORT, strerror(errno));
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    if (listen(g_listen_fd, 1) < 0) {
        fprintf(stderr, "harness: listen: %s\n", strerror(errno));
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    fprintf(stderr, "harness: listening on 127.0.0.1:%d\n", CR_TCP_PORT);
    return 0;
}

/* Try to accept a renderer connection (non-blocking).  Drain any
 * pending inbound bytes (heartbeats) on the existing client to prevent
 * buffer fill.  Call once per main-loop tick. */
static void tcp_pump(void) {
    if (g_client_fd < 0 && g_listen_fd >= 0) {
        int fd = accept(g_listen_fd, NULL, NULL);
        if (fd >= 0) {
            set_nonblocking(fd);
            g_client_fd = fd;
            fprintf(stderr, "harness: renderer connected (fd=%d)\n", fd);
        }
    }

    if (g_client_fd >= 0) {
        uint8_t scratch[256];
        for (;;) {
            int n = (int)recv(g_client_fd, scratch, sizeof(scratch), 0);
            if (n > 0) {
                /* drain heartbeats — content ignored */
                continue;
            }
            if (n == 0) {
                fprintf(stderr, "harness: renderer disconnected\n");
                close(g_client_fd);
                g_client_fd = -1;
                break;
            }
            /* n < 0 */
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            fprintf(stderr, "harness: recv error: %s\n", strerror(errno));
            close(g_client_fd);
            g_client_fd = -1;
            break;
        }
    }
}

static int tcp_send(const void *data, int len) {
    if (g_client_fd < 0) {
        fprintf(stderr, "harness: no renderer connected — start it first\n");
        return -1;
    }
    int n = (int)send(g_client_fd, data, len, 0);
    if (n < 0) {
        fprintf(stderr, "harness: send error: %s\n", strerror(errno));
        close(g_client_fd);
        g_client_fd = -1;
        return -1;
    }
    return 0;
}

/* ================================================================
 * Maneuver presets
 * ================================================================ */

typedef struct {
    int icon;
    int exit_angle;
    int direction;
    const char *label;
    int junction_angles[MAX_JUNCTION_ANGLES];
    int junction_angle_count;
} preset_t;

static const preset_t g_presets[] = {
    { ICON_APPROACH,    0,    0, "APPROACH",     {0}, 0 },
    { ICON_TURN,        30,   0, "SLIGHT_RIGHT", {0}, 0 },
    { ICON_TURN,        90,   0, "RIGHT",        {0}, 0 },
    { ICON_TURN,       135,   0, "SHARP_RIGHT",  {0}, 0 },
    { ICON_TURN,       -30,   0, "SLIGHT_LEFT",  {0}, 0 },
    { ICON_TURN,       -90,   0, "LEFT",         {0}, 0 },
    { ICON_TURN,      -135,   0, "SHARP_LEFT",   {0}, 0 },
    { ICON_UTURN,       0,    0, "UTURN",        {0}, 0 },
    { ICON_MERGE,       0,    1, "MERGE_R",      {0}, 0 },
    { ICON_EXIT, 0,   -1, "LANE_CHG_L",  {0}, 0 },
    { ICON_EXIT, 0,    1, "LANE_CHG_R",   {0}, 0 },
    /* Roundabout -- 4-exit, enter */
    { ICON_ROUNDABOUT,  90,   0, "RAB_ENTER",    {90, 0, -90}, 3 },
    /* Roundabout -- 4-exit, right exit */
    { ICON_ROUNDABOUT,  90,   0, "RAB_EXIT_1",   {90, 0, -90, -150}, 4 },
    /* Roundabout -- 5-exit, straight */
    { ICON_ROUNDABOUT,   0,   0, "RAB_EXIT_3",   {90, 45, 0, -45, -90}, 5 },
    /* Roundabout -- 7-exit, left */
    { ICON_ROUNDABOUT, -90,   0, "RAB_EXIT_6",   {120, 80, 40, 0, -40, -90, -140}, 7 },
    /* Roundabout -- 4-exit, u-turn */
    { ICON_ROUNDABOUT, 180,   0, "RAB_UTURN",    {90, 0, -90, 180}, 4 },
    /* Roundabout -- from real CarPlay: exit_angle=-134, junctions=-36,100,14 */
    { ICON_ROUNDABOUT, -134,  0, "RAB_REAL",     {-36, 100, 14}, 3 },
    /* Roundabout -- U-turn (exit near entry, full loop) */
    { ICON_ROUNDABOUT, -170,  0, "RAB_FULL_U",   {-90, 0, 90, -170}, 4 },
    /* Roundabout -- 180° exit (same direction as entry = full loop back) */
    { ICON_ROUNDABOUT, -180,  0, "RAB_180",      {-90, 0, 90, -180}, 4 },
    { ICON_ARRIVED,     0,    0, "ARRIVED",      {0}, 0 },
    { ICON_ARRIVED,     0,   -1, "ARRIVED_L",    {0}, 0 },
    { ICON_ARRIVED,     0,    1, "ARRIVED_R",    {0}, 0 },
};
#define PRESET_COUNT (int)(sizeof(g_presets) / sizeof(g_presets[0]))

static int g_preset_idx = 0;
static int g_perspective = 1;
static int g_bargraph_on = 0;
static int g_bargraph_level = 8;

/* ================================================================
 * Packet encoding
 * ================================================================ */

static void encode_maneuver(const preset_t *p, cr_cmd_t *cmd) {
    int i;
    int16_t angle;

    memset(cmd, 0, sizeof(*cmd));
    cmd->cmd = CMD_MANEUVER;

    cmd->payload[0] = (uint8_t)p->icon;
    cmd->payload[1] = (uint8_t)(int8_t)p->direction;
    angle = (int16_t)p->exit_angle;
    cmd->payload[2] = (uint8_t)((angle >> 8) & 0xFF);
    cmd->payload[3] = (uint8_t)(angle & 0xFF);
    cmd->payload[4] = 0; /* driving_side RHT */
    cmd->payload[5] = (uint8_t)p->junction_angle_count;

    for (i = 0; i < p->junction_angle_count && i < 18; i++) {
        int16_t ja = (int16_t)p->junction_angles[i];
        cmd->payload[6 + i * 2] = (uint8_t)((ja >> 8) & 0xFF);
        cmd->payload[7 + i * 2] = (uint8_t)(ja & 0xFF);
    }
}

static void send_preset(int idx) {
    cr_cmd_t cmd;
    encode_maneuver(&g_presets[idx], &cmd);
    /* Random bargraph */
    cmd.flags |= MAN_FLAG_BARGRAPH;
    g_bargraph_level = rand() % 17;
    g_bargraph_on = 1 + (rand() % 2);
    cmd.payload[44] = (uint8_t)g_bargraph_level;
    cmd.payload[45] = (uint8_t)g_bargraph_on;
    if (tcp_send(&cmd, sizeof(cmd)) == 0)
        fprintf(stderr, "harness: sent [%d/%d] %s bar=%d/%d\n",
                idx + 1, PRESET_COUNT, g_presets[idx].label,
                g_bargraph_level, g_bargraph_on);
}

static void send_random_icon(int icon, uint8_t flags) {
    cr_cmd_t cmd;
    int16_t angle;
    int i, jcount;
    int dirs[] = {-1, 0, 1};
    int dir = dirs[rand() % 3];
    int exit_angle = (rand() % 361) - 180;        /* -180..180 */
    int driving_side = rand() % 2;

    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = CMD_MANEUVER;
    cmd.flags = flags;

    cmd.payload[0] = (uint8_t)icon;
    cmd.payload[1] = (uint8_t)(int8_t)dir;
    angle = (int16_t)exit_angle;
    cmd.payload[2] = (uint8_t)((angle >> 8) & 0xFF);
    cmd.payload[3] = (uint8_t)(angle & 0xFF);
    cmd.payload[4] = (uint8_t)driving_side;

    if (icon == ICON_ROUNDABOUT) {
        jcount = 3 + rand() % 6;  /* 3..8 exits */
    } else if (icon == ICON_TURN || icon == ICON_APPROACH) {
        jcount = rand() % 5;      /* 0..4 side streets */
    } else {
        jcount = 0;
    }
    if (jcount > 18) jcount = 18;
    cmd.payload[5] = (uint8_t)jcount;
    for (i = 0; i < jcount; i++) {
        int16_t ja = (int16_t)((rand() % 361) - 180);
        cmd.payload[6 + i * 2] = (uint8_t)((ja >> 8) & 0xFF);
        cmd.payload[7 + i * 2] = (uint8_t)(ja & 0xFF);
    }

    if (flags & MAN_FLAG_SET_PERSP) {
        cmd.payload[43] = (uint8_t)g_perspective;
    }

    /* Random bargraph in every maneuver */
    cmd.flags |= MAN_FLAG_BARGRAPH;
    g_bargraph_level = rand() % 17;          /* 0..16 */
    g_bargraph_on = 1 + (rand() % 2);       /* 1=on, 2=blink */
    cmd.payload[44] = (uint8_t)g_bargraph_level;
    cmd.payload[45] = (uint8_t)g_bargraph_on;

    if (tcp_send(&cmd, sizeof(cmd)) == 0)
        fprintf(stderr, "harness: random icon=%d angle=%d dir=%d ds=%d junc=%d bar=%d/%d flags=0x%02x\n",
                icon, exit_angle, dir, driving_side, jcount,
                g_bargraph_level, g_bargraph_on, cmd.flags);
}

static void send_random_maneuver(uint8_t flags) {
    int icon = (rand() % (ICON_COUNT - 1)) + 1;
    send_random_icon(icon, flags);
}

static void send_screenshot(const char *label) {
    cr_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = CMD_SCREENSHOT;
    if (label)
        strncpy((char *)cmd.payload, label, 16);
    tcp_send(&cmd, sizeof(cmd));
}

static void send_perspective(int enabled) {
    cr_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = CMD_PERSPECTIVE;
    cmd.payload[0] = (uint8_t)(enabled ? 1 : 0);
    tcp_send(&cmd, sizeof(cmd));
    g_perspective = enabled;
    fprintf(stderr, "harness: perspective=%s\n", enabled ? "ON" : "OFF");
}

static void send_debug_toggle(void) {
    cr_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = CMD_DEBUG;
    cmd.payload[0] = 2;  /* grid toggle */
    tcp_send(&cmd, sizeof(cmd));
    fprintf(stderr, "harness: debug toggle\n");
}

static void send_bargraph(int level, int on) {
    cr_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = CMD_BARGRAPH;
    cmd.payload[0] = (uint8_t)level;
    cmd.payload[1] = (uint8_t)on;
    tcp_send(&cmd, sizeof(cmd));
    fprintf(stderr, "harness: bargraph level=%d on=%d\n", level, on);
}

static void send_shutdown(void) {
    cr_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = CMD_SHUTDOWN;
    tcp_send(&cmd, sizeof(cmd));
}

/* ================================================================
 * GLFW key callback
 * ================================================================ */

static int g_should_close = 0;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)window; (void)scancode; (void)mods;
    if (action != GLFW_PRESS) return;

    switch (key) {
    case GLFW_KEY_RIGHT:
        g_preset_idx = (g_preset_idx + 1) % PRESET_COUNT;
        send_preset(g_preset_idx);
        break;
    case GLFW_KEY_LEFT:
        g_preset_idx = (g_preset_idx - 1 + PRESET_COUNT) % PRESET_COUNT;
        send_preset(g_preset_idx);
        break;
    case GLFW_KEY_R:
        send_random_maneuver(0);
        break;
    case GLFW_KEY_P:
        send_perspective(!g_perspective);
        break;
    case GLFW_KEY_T:
        send_random_maneuver(MAN_FLAG_SET_PERSP);
        break;
    case GLFW_KEY_1:
        send_random_icon(ICON_APPROACH, 0);
        break;
    case GLFW_KEY_2:
        send_random_icon(ICON_TURN, 0);
        break;
    case GLFW_KEY_3:
        send_random_icon(ICON_UTURN, 0);
        break;
    case GLFW_KEY_4:
        send_random_icon(ICON_ROUNDABOUT, 0);
        break;
    case GLFW_KEY_5:
        send_random_icon(ICON_ARRIVED, 0);
        break;
    case GLFW_KEY_LEFT_BRACKET: {
        cr_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = CMD_DEBUG;
        cmd.payload[0] = 3;  /* 3D offset up (content moves down) */
        tcp_send(&cmd, sizeof(cmd));
        fprintf(stderr, "harness: 3D offset +\n");
        break;
    }
    case GLFW_KEY_RIGHT_BRACKET: {
        cr_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = CMD_DEBUG;
        cmd.payload[0] = 4;  /* 3D offset down (content moves up) */
        tcp_send(&cmd, sizeof(cmd));
        fprintf(stderr, "harness: 3D offset -\n");
        break;
    }
    case GLFW_KEY_G: {
        /* Toggle grid on the renderer (send as CMD_DEBUG with payload[0]=2) */
        cr_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = CMD_DEBUG;
        cmd.payload[0] = 2;  /* grid toggle */
        tcp_send(&cmd, sizeof(cmd));
        fprintf(stderr, "harness: grid toggle\n");
        break;
    }
    case GLFW_KEY_D:
        send_debug_toggle();
        break;
    case GLFW_KEY_B:
        g_bargraph_on = (g_bargraph_on + 1) % 3;  /* 0=off, 1=on, 2=blink */
        send_bargraph(g_bargraph_level, g_bargraph_on);
        break;
    case GLFW_KEY_UP:
        if (g_bargraph_level < 16) g_bargraph_level++;
        send_bargraph(g_bargraph_level, g_bargraph_on);
        break;
    case GLFW_KEY_DOWN:
        if (g_bargraph_level > 0) g_bargraph_level--;
        send_bargraph(g_bargraph_level, g_bargraph_on);
        break;
    case GLFW_KEY_SPACE:
        send_screenshot(g_presets[g_preset_idx].label);
        fprintf(stderr, "harness: screenshot requested\n");
        break;
    case GLFW_KEY_Q:
    case GLFW_KEY_ESCAPE:
        send_shutdown();
        g_should_close = 1;
        break;
    }
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (!glfwInit()) {
        fprintf(stderr, "harness: glfwInit failed\n");
        return 1;
    }

    /* Tiny invisible window just for key input */
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(320, 100, "c_render harness", NULL, NULL);
    if (!window) {
        fprintf(stderr, "harness: window creation failed\n");
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(window, key_callback);

    fprintf(stderr, "harness: ready (L/R=cycle, R=random, Space=snap, Q=quit)\n");

    if (tcp_setup_server() < 0) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    fprintf(stderr, "harness: now run the renderer to connect\n");

    while (!glfwWindowShouldClose(window) && !g_should_close) {
        /* Pump TCP first: accept renderer, drain heartbeats. */
        tcp_pump();
        /* Short timeout so accept/drain stays responsive. */
        glfwWaitEventsTimeout(0.05);
    }

    if (g_client_fd >= 0) { close(g_client_fd); g_client_fd = -1; }
    if (g_listen_fd >= 0) { close(g_listen_fd); g_listen_fd = -1; }

    glfwDestroyWindow(window);
    glfwTerminate();
    fprintf(stderr, "harness: exit\n");
    return 0;
}
