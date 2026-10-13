/*
 * upmix_level.c — the MMI Sound effects "UpMix" level (Java row, java_patch
 * com/luka/carplay/tone/UpmixToneUi.java).
 *
 * The hook owns the level: it loads it at init (before any stream), takes
 * CMD_UPMIX [u8 level] from the row, saves it, and publishes it back as the
 * sticky EVT_UPMIX_LEVEL so the row shows the active level after every
 * (re)connect.  The scene instances pick spa_upmix_level up on their own
 * thread (audio_hook.c); this file never touches audio state.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "audio_hook.h"
#include "stereo_upmix.h"
#include "../framework/bus.h"
#include "../framework/logging.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

DEFINE_LOG_MODULE(UPMIX);

#define UPMIX_LEVEL_DIR  "/mnt/persist/mhi2q-carplay"
#define UPMIX_LEVEL_FILE UPMIX_LEVEL_DIR "/upmix_level"

static const char *const kLevelNames[SPA_UPMIX_LEVEL_COUNT] = {
    "off", "moderate", "normal", "full", "aggressive"
};

static void upmix_publish(void)
{
    uint8_t level = (uint8_t)spa_upmix_level;

    (void)bus_send(EVT_UPMIX_LEVEL, BUS_FLAG_STICKY | BUS_FLAG_BINARY, &level, 1u);
}

/* write-new-then-rename: a power cut leaves the old or the new level, never half */
static void upmix_save(int level)
{
    FILE *f;

    if (mkdir(UPMIX_LEVEL_DIR, 0700) != 0 && errno != EEXIST) {
        LOG_WARN(LOG_MODULE, "upmix level not saved: %s errno=%d", UPMIX_LEVEL_DIR, errno);
        return;
    }
    f = fopen(UPMIX_LEVEL_FILE ".new", "w");
    if (!f || fprintf(f, "%d\n", level) < 0 || fclose(f) != 0 ||
        rename(UPMIX_LEVEL_FILE ".new", UPMIX_LEVEL_FILE) != 0)
        LOG_WARN(LOG_MODULE, "upmix level not saved errno=%d", errno);
}

static void upmix_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (len < 1u || payload[0] >= SPA_UPMIX_LEVEL_COUNT) {
        LOG_WARN(LOG_MODULE, "CMD_UPMIX rejected len=%u level=%u", (unsigned)len, len ? payload[0] : 0u);
        return;
    }
    if (payload[0] != spa_upmix_level) {
        spa_upmix_level = payload[0];
        upmix_save(payload[0]);
    }
    LOG_WARN(LOG_MODULE, "upmix level %s (from the MMI)", kLevelNames[spa_upmix_level]);
    upmix_publish();
}

static void upmix_init(void)
{
    FILE *f = fopen(UPMIX_LEVEL_FILE, "r");
    int level = SPA_UPMIX_LEVEL_NORMAL;

    if (f) {
        if (fscanf(f, "%d", &level) != 1 || level < 0 || level >= SPA_UPMIX_LEVEL_COUNT)
            level = SPA_UPMIX_LEVEL_NORMAL;
        fclose(f);
    }
    spa_upmix_level = level;
    bus_on(CMD_UPMIX, upmix_on_cmd, NULL);
    upmix_publish();
    LOG_WARN(LOG_MODULE, "upmix level %s (%s)", kLevelNames[level], f ? "saved" : "default");
}

static void upmix_shutdown(void)
{
    bus_off(CMD_UPMIX);
}

const hook_module_def_t upmix_level_module_def = {
    .name = "upmix_level",
    .priority = HOOK_PRIORITY_LOW,
    .on_init = upmix_init,
    .on_shutdown = upmix_shutdown,
};
