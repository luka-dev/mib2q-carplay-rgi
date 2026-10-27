/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef APAC_CODEC_LOADER_H
#define APAC_CODEC_LOADER_H

#include "apac_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPA_APAC_CODEC_PATH_ENV "AUDIO_SPATIAL_CODEC_LIB"
#define SPA_APAC_CODEC_DEFAULT_PATH "/mnt/app/root/hooks/libcarplay_apac.so"

/* Load and validate the optional decoder once.  The returned table remains
 * valid for the lifetime of dio_manager; the library is deliberately never
 * unloaded while decoder handles or audio threads may still exist. */
const spa_apac_codec_api_t *spa_apac_runtime_load(const char **pathOut);

#ifdef __cplusplus
}
#endif

#endif /* APAC_CODEC_LOADER_H */
