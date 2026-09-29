/* CarPlay Hook Library - the module table.
 *
 * This is the whole wiring of the hook.  The framework never names a module;
 * it registers whatever stands here, in this order, at the first real Cinemo
 * boundary, and tears it down in reverse from its destructor.  What each
 * module wants from Identify, iAP2 messages, the raw transport, AirPlay SETUP,
 * /info and TearDown is declared in its own hook_module_def_t, next to the
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 * code that implements it. */

#include "framework/hook_framework.h"

#include "routeguidance/rgd_hook.h"
#include "coverart/coverart_hook.h"
#include "altscreen/altscreen_hook.h"
#include "telephony/tel_hook.h"
#if HOOK_WIRELESS
#include "wireless/wireless_hook.h"
#endif
/* HOOK_AUDIO=0 (scripts/build_hook.sh HOOK_AUDIO=0) leaves the type-103 audio
 * module and its QSA/AirPlay audio interposers out of the build entirely:
 * stock CarPlay audio, no libcarplay_apac.so. */
#ifndef HOOK_AUDIO
#define HOOK_AUDIO 1
#endif
#if HOOK_AUDIO
#include "audio/audio_hook.h"
#endif

const hook_module_def_t* const hook_module_table[] = {
#if HOOK_WIRELESS
    &wireless_module_def,
#endif
    &rgd_module_def,
    &coverart_module_def,
    &altscreen_module_def,
    &tel_module_def,
#if HOOK_AUDIO
    &audio_module_def,
#endif
};

const size_t hook_module_table_count =
    sizeof(hook_module_table) / sizeof(hook_module_table[0]);
