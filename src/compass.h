#pragma once
#include "config.h"

// Called only from the ScriptHookV script fiber, never from DllMain.
enum class CompassFrame { Drawn, HeadingUnavailable, NativeFailure };
CompassFrame DrawCompassSafely(const Config& config);
