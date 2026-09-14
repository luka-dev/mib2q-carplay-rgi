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
#include "audio/audio_hook.h"
extern const hook_module_def_t hu_startup_module_def;

const hook_module_def_t* const hook_module_table[] = {
    &rgd_module_def,
    &coverart_module_def,
    &altscreen_module_def,
    &audio_module_def,
    &hu_startup_module_def,
};

const size_t hook_module_table_count =
    sizeof(hook_module_table) / sizeof(hook_module_table[0]);
