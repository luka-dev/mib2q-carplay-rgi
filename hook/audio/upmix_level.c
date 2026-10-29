/*
 * upmix_level.c — the MMI "CarPlay settings" audio controls, hook side.
 *
 * Java owns the settings (stock HMI persistence, java_patch com/luka/carplay/
 * connectivity/CarPlaySettingsUi.java).  It starts before dio_manager and writes
 * key=value lines into SPA_SETTINGS_FILE ("/tmp/carplay_settings"), read at init
 * and at every session/stream start (spa_settings_reload, audio_hook.c); live
 * changes arrive as CMD_UPMIX [u8 level] and CMD_SPATIAL [u8 enable].  The scene
 * instances pick spa_upmix_level up on their own thread (audio_hook.c); this file
 * never touches audio state.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "audio_hook.h"
#include "stereo_upmix.h"
#include "../framework/bus.h"
#include "../framework/logging.h"

DEFINE_LOG_MODULE(UPMIX);

static const char *const kLevelNames[SPA_UPMIX_LEVEL_COUNT] = {
    "off", "normal", "intense"
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

static void spatial_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (len < 1u || payload[0] > 1u) {
        LOG_WARN(LOG_MODULE, "CMD_SPATIAL rejected len=%u enable=%u", (unsigned)len, len ? payload[0] : 0u);
        return;
    }
    spa_spatial_enable = payload[0];
    LOG_WARN(LOG_MODULE, "spatial audio %s (from the MMI)", payload[0] ? "on" : "off");
}

static void upmix_init(void)
{
    const int mask = spa_settings_reload();
    const int ok_upmix = (mask & 1) != 0;
    const int ok_spatial = (mask & 2) != 0;

    bus_on(CMD_UPMIX, upmix_on_cmd, NULL);
    bus_on(CMD_SPATIAL, spatial_on_cmd, NULL);
    LOG_WARN(LOG_MODULE, "upmix level %s (%s), spatial audio %s (%s)",
             kLevelNames[spa_upmix_level],
             ok_upmix ? "from Java" : "default; re-read at every stream start",
             spa_spatial_enable ? "on" : "off",
             ok_spatial ? "from Java" : "default; re-read at session/stream start");
}

static void upmix_shutdown(void)
{
    bus_off(CMD_UPMIX);
    bus_off(CMD_SPATIAL);
}

const hook_module_def_t upmix_level_module_def = {
    .name = "upmix_level",
    .priority = HOOK_PRIORITY_LOW,
    .on_init = upmix_init,
    .on_shutdown = upmix_shutdown,
};
