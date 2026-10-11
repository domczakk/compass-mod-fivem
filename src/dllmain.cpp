#include <Windows.h>
#include <atomic>
#include <cstdio>
#include "main.h"
#include "config.h"
#include "compass.h"

namespace {
std::atomic_bool g_running{false};
HMODULE g_module = nullptr;

// Minimal kernel32 file I/O also works before the script fiber starts.
// No CRT file streams, allocations, worker threads or waits inside DllMain.
void Log(const char* message) noexcept {
    OutputDebugStringA(message);
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(g_module, path, 32768);
    if (length == 0 || length >= 32768) return;
    DWORD prefix = length;
    while (prefix > 0 && path[prefix - 1] != L'\\' && path[prefix - 1] != L'/') --prefix;
    constexpr wchar_t name[] = L"compass.log";
    constexpr DWORD nameLength = static_cast<DWORD>(sizeof(name) / sizeof(name[0]));
    if (prefix == 0 || prefix + nameLength > 32768) return;
    for (DWORD i = 0; i < nameLength; ++i) path[prefix + i] = name[i];
    const HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("compass: cannot open compass.log; check directory permissions.\n");
        return;
    }
    DWORD bytes = 0;
    while (message[bytes] != '\0') ++bytes;
    DWORD written = 0;
    WriteFile(file, message, bytes, &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

bool YieldFrame() {
    __try { WAIT(0); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void RunScript() {
    Log("compass: ScriptMain started (diagnostics v1).");
    Config config;
    if (!config.Load(g_module)) Log("compass: cannot resolve INI path; using defaults.");
    if (!config.Save()) Log("compass: INI write failed; check plugin directory permissions.");
    char settings[128]{};
    std::snprintf(settings, sizeof(settings), "compass: visible=%d toggleKey=%d fov=%.1f barWidth=%.3f yPos=%.3f",
        config.visible ? 1 : 0, config.toggleKey, config.fov, config.barWidth, config.yPos);
    Log(settings);
    bool keyWasDown = (GetAsyncKeyState(config.toggleKey) & 0x8000) != 0;
    ULONGLONG lastToggle = 0;
    bool rendererHealthy = true;
    bool firstDraw = true;
    bool headingUnavailable = false;
    bool resumed = false;
    while (g_running.load(std::memory_order_relaxed)) {
        const bool down = (GetAsyncKeyState(config.toggleKey) & 0x8000) != 0;
        const ULONGLONG now = GetTickCount64();
        HWND foreground = GetForegroundWindow();
        DWORD foregroundProcess = 0;
        if (foreground) GetWindowThreadProcessId(foreground, &foregroundProcess);
        if (down && !keyWasDown && foregroundProcess != GetCurrentProcessId())
            Log("compass: toggle ignored; game process is not foreground.");
        if (down && !keyWasDown && foregroundProcess == GetCurrentProcessId()
            && now - lastToggle >= 250) {
            config.visible = !config.visible;
            lastToggle = now;
            Log(config.visible ? "compass: toggle visible=1." : "compass: toggle visible=0.");
            if (!config.Save()) Log("compass: failed to persist toggle.");
        }
        keyWasDown = down;
        if (config.visible && rendererHealthy) {
            const auto frame = DrawCompassSafely(config);
            if (frame == CompassFrame::NativeFailure) {
                rendererHealthy = false;
                Log("compass: drawing native exception; renderer disabled until restart.");
            } else if (frame == CompassFrame::HeadingUnavailable) {
                if (!headingUnavailable) Log("compass: camera and player heading unavailable; waiting.");
                headingUnavailable = true;
            } else {
                if (firstDraw || headingUnavailable)
                    Log("compass: drawing calls completed; this does not confirm on-screen visibility.");
                firstDraw = false;
                headingUnavailable = false;
            }
        }
        if (!YieldFrame()) {
            Log("compass: script yield failed; stopping.");
            break;
        }
        if (!resumed) {
            Log("compass: script loop resumed after WAIT(0).");
            resumed = true;
        }
    }
}
}

void ScriptMain() {
    try { RunScript(); }
    catch (...) { Log("compass: script exception; stopped."); }
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        g_running.store(true, std::memory_order_relaxed);
        Log("compass: DLL attached (diagnostics v1); registering script.");
        scriptRegister(instance, ScriptMain);
        Log("compass: scriptRegister returned; waiting for ScriptMain.");
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running.store(false, std::memory_order_relaxed);
        scriptUnregister(instance);
    }
    return TRUE;
}
