/*
 * upmix_level.c — the MMI Sound effects "CarPlay UpMix" level, hook side.
 *
 * Java owns the level (stock HMI persistence, java_patch com/luka/carplay/core/
 * UpmixSetting.java).  It starts before dio_manager and leaves the level in
 * SPA_UPMIX_LEVEL_FILE, read at init and at every stream start
 * (spa_upmix_level_reload, audio_hook.c); a change in the row arrives live as
 * CMD_UPMIX [u8 level].  The scene instances pick spa_upmix_level up on their
 * own thread (audio_hook.c); this file never touches audio state.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "audio_hook.h"
#include "stereo_upmix.h"
#include "../framework/bus.h"
#include "../framework/logging.h"

DEFINE_LOG_MODULE(UPMIX);

static const char *const kLevelNames[SPA_UPMIX_LEVEL_COUNT] = {
    "off", "moderate", "normal", "full", "aggressive"
};

static void upmix_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (len < 1u || payload[0] >= SPA_UPMIX_LEVEL_COUNT) {
        LOG_WARN(LOG_MODULE, "CMD_UPMIX rejected len=%u level=%u", (unsigned)len, len ? payload[0] : 0u);
        return;
    }
    spa_upmix_level = payload[0];
    LOG_WARN(LOG_MODULE, "upmix level %s (from the MMI)", kLevelNames[payload[0]]);
}

static void upmix_init(void)
{
    const int ok = spa_upmix_level_reload();

    bus_on(CMD_UPMIX, upmix_on_cmd, NULL);
    LOG_WARN(LOG_MODULE, "upmix level %s (%s)", kLevelNames[spa_upmix_level],
             ok ? "from Java" : "no Java file yet, default; re-read at every stream start");
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
