/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac_codec_loader.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef int (*spa_apac_get_api_t)(uint32_t, spa_apac_codec_api_t *, size_t);

static pthread_mutex_t spa_apac_loader_lock = PTHREAD_MUTEX_INITIALIZER;
static spa_apac_codec_api_t spa_apac_loaded_api;
static void *spa_apac_library;
static int spa_apac_load_state; /* 0 untried, 1 ready, -1 rejected */
static const char *spa_apac_loaded_path;

const spa_apac_codec_api_t *spa_apac_runtime_load(const char **pathOut)
{
    const spa_apac_codec_api_t *result = 0;
    const char *path;
    spa_apac_get_api_t getApi;
    int flags = RTLD_NOW;

    pthread_mutex_lock(&spa_apac_loader_lock);
    path = getenv(SPA_APAC_CODEC_PATH_ENV);
    if (!path || !path[0]) path = SPA_APAC_CODEC_DEFAULT_PATH;
    if (!spa_apac_loaded_path) spa_apac_loaded_path = path;
    if (pathOut) *pathOut = spa_apac_loaded_path;

    if (spa_apac_load_state > 0) {
        result = &spa_apac_loaded_api;
        goto done;
    }
    if (spa_apac_load_state < 0) goto done;

#ifdef RTLD_LOCAL
    flags |= RTLD_LOCAL;
#endif
    spa_apac_library = dlopen(path, flags);
    if (!spa_apac_library) {
        spa_apac_load_state = -1;
        goto done;
    }
    getApi = (spa_apac_get_api_t)dlsym(spa_apac_library, "spa_apac_codec_get_api");
    if (!getApi ||
        getApi(SPA_APAC_CODEC_ABI_VERSION, &spa_apac_loaded_api,
               sizeof(spa_apac_loaded_api)) != SPA_APAC_OK ||
        spa_apac_loaded_api.structSize != sizeof(spa_apac_loaded_api) ||
        spa_apac_loaded_api.abiVersion != SPA_APAC_CODEC_ABI_VERSION ||
        !spa_apac_loaded_api.create || !spa_apac_loaded_api.decodeS16 ||
        !spa_apac_loaded_api.reset || !spa_apac_loaded_api.destroy ||
        !spa_apac_loaded_api.getInfo) {
        memset(&spa_apac_loaded_api, 0, sizeof(spa_apac_loaded_api));
        dlclose(spa_apac_library);
        spa_apac_library = 0;
        spa_apac_load_state = -1;
        goto done;
    }
    spa_apac_load_state = 1;
    result = &spa_apac_loaded_api;

done:
    pthread_mutex_unlock(&spa_apac_loader_lock);
    return result;
}
