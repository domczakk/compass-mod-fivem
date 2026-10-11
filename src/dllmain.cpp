#include <Windows.h>
#include <atomic>
#include "main.h"
#include "config.h"
#include "compass.h"

namespace {
std::atomic_bool g_running{false};
HMODULE g_module = nullptr;

bool YieldFrame() {
    __try { WAIT(0); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void RunScript() {
    Config config;
    if (!config.Load(g_module) || !config.Save())
        OutputDebugStringA("compass: cannot load/save compass.ini; check plugin directory permissions.\n");
    bool keyWasDown = (GetAsyncKeyState(config.toggleKey) & 0x8000) != 0;
    ULONGLONG lastToggle = 0;
    bool rendererHealthy = true;
    while (g_running.load(std::memory_order_relaxed)) {
        const bool down = (GetAsyncKeyState(config.toggleKey) & 0x8000) != 0;
        const ULONGLONG now = GetTickCount64();
        HWND foreground = GetForegroundWindow();
        DWORD foregroundProcess = 0;
        if (foreground) GetWindowThreadProcessId(foreground, &foregroundProcess);
        if (down && !keyWasDown && foregroundProcess == GetCurrentProcessId()
            && now - lastToggle >= 250) {
            config.visible = !config.visible;
            lastToggle = now;
            if (!config.Save()) OutputDebugStringA("compass: failed to persist toggle.\n");
        }
        keyWasDown = down;
        if (config.visible && rendererHealthy && !DrawCompassSafely(config)) {
            rendererHealthy = false;
            OutputDebugStringA("compass: drawing native failed; renderer disabled until restart.\n");
        }
        if (!YieldFrame()) {
            OutputDebugStringA("compass: script yield failed; stopping.\n");
            break;
        }
    }
}
}

void ScriptMain() {
    try { RunScript(); }
    catch (...) { OutputDebugStringA("compass: script exception; stopped.\n"); }
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        g_running.store(true, std::memory_order_relaxed);
        scriptRegister(instance, ScriptMain);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running.store(false, std::memory_order_relaxed);
        scriptUnregister(instance);
    }
    return TRUE;
}
