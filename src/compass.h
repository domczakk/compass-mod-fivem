#pragma once
#include "config.h"

// Called only from the ScriptHookV script fiber, never from DllMain.
bool DrawCompassSafely(const Config& config);
