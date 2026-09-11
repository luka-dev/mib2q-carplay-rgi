/* CarPlay Hook Library - the module table.
 *
 * This is the whole wiring of the hook.  The framework never names a module;
 * it registers whatever stands here, in this order, at the first real Cinemo
 * boundary, and tears it down in reverse from its destructor.  What each
 * module wants from Identify, iAP2 messages and the raw transport is declared
 * in its own hook_module_def_t, next to the code that implements it.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "framework/hook_framework.h"

#include "routeguidance/rgd_hook.h"
#include "coverart/coverart_hook.h"

const hook_module_def_t* const hook_module_table[] = {
    &rgd_module_def,
    &coverart_module_def,
};

const size_t hook_module_table_count =
    sizeof(hook_module_table) / sizeof(hook_module_table[0]);
