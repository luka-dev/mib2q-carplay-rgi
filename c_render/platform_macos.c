/*
 * macOS platform implementation -- GLFW + OpenGL 2.1
 */

#ifdef PLATFORM_MACOS

#include <stdio.h>
#include <stdlib.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "platform.h"

static GLFWwindow *g_window = NULL;

static void error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static int g_key_taps[CR_KEY_MAX] = {0};

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    else if (key == GLFW_KEY_LEFT)
        g_key_taps[CR_KEY_LEFT] = 1;
    else if (key == GLFW_KEY_RIGHT)
        g_key_taps[CR_KEY_RIGHT] = 1;
    else if (key == GLFW_KEY_UP)
        g_key_taps[CR_KEY_UP] = 1;
    else if (key == GLFW_KEY_DOWN)
        g_key_taps[CR_KEY_DOWN] = 1;
    else if (key == GLFW_KEY_P)
        g_key_taps[CR_KEY_P] = 1;
    else if (key == GLFW_KEY_SPACE)
        g_key_taps[CR_KEY_SPACE] = 1;
    else if (key == GLFW_KEY_S)
        g_key_taps[CR_KEY_S] = 1;
    else if (key == GLFW_KEY_A)
        g_key_taps[CR_KEY_A] = 1;
    else if (key == GLFW_KEY_LEFT_BRACKET)
        g_key_taps[CR_KEY_LBRACKET] = 1;
    else if (key == GLFW_KEY_RIGHT_BRACKET)
        g_key_taps[CR_KEY_RBRACKET] = 1;
    else if (key == GLFW_KEY_D)
        g_key_taps[CR_KEY_D] = 1;
}

int platform_init(int width, int height) {
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        fprintf(stderr, "platform_macos: glfwInit failed\n");
        return -1;
    }

    /* Request OpenGL 2.1 -- compatible with GLES2 shader subset */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_SAMPLES, 0);   /* no MSAA — FXAA post-process instead */
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    /* Retina default (TRUE) — gives 2x framebuffer, same as QNX SSAA 2x */

    g_window = glfwCreateWindow(width, height, "c_render", NULL, NULL);
    if (!g_window) {
        fprintf(stderr, "platform_macos: window creation failed\n");
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(g_window, key_callback);
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(0); /* no vsync -- frame pacing done in main loop */

    fprintf(stderr, "platform_macos: GL %s\n", glGetString(GL_VERSION));
    return 0;
}

void platform_swap(void) {
    if (g_window) glfwSwapBuffers(g_window);
}

void platform_poll(void) {
    glfwPollEvents();
}

int platform_should_close(void) {
    return g_window ? glfwWindowShouldClose(g_window) : 1;
}

void platform_shutdown(void) {
    if (g_window) {
        glfwDestroyWindow(g_window);
        g_window = NULL;
    }
    glfwTerminate();
    fprintf(stderr, "platform_macos: shutdown\n");
}

void platform_get_framebuffer_size(int *width, int *height) {
    if (g_window)
        glfwGetFramebufferSize(g_window, width, height);
}

int platform_key_tap(int key) {
    if (key < 0 || key >= CR_KEY_MAX) return 0;
    int v = g_key_taps[key];
    g_key_taps[key] = 0;
    return v;
}

#endif /* PLATFORM_MACOS */
