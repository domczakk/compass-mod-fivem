#pragma once
// Test-only profile API substitute. Never on the plugin's include path.
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <map>
#include <string>

using HMODULE = void*;
using DWORD = std::uint32_t;
constexpr int VK_F7 = 118;

namespace FakeWindows {
inline std::map<std::wstring, std::wstring> profile;
inline std::wstring modulePath = L"C:\\plugins\\compass.asi";
inline std::wstring lastWritePath;
inline bool failPath = false;
inline bool failWrite = false;
inline int writeCount = 0;
inline void Reset() {
    profile.clear();
    modulePath = L"C:\\plugins\\compass.asi";
    lastWritePath.clear();
    failPath = failWrite = false;
    writeCount = 0;
}
}

inline DWORD GetModuleFileNameW(HMODULE, wchar_t* buffer, DWORD capacity) {
    if (FakeWindows::failPath) return 0;
    if (FakeWindows::modulePath.size() >= capacity) return capacity;
    std::wcscpy(buffer, FakeWindows::modulePath.c_str());
    return static_cast<DWORD>(FakeWindows::modulePath.size());
}
inline DWORD GetPrivateProfileStringW(const wchar_t*, const wchar_t* key,
    const wchar_t* fallback, wchar_t* buffer, DWORD capacity, const wchar_t*) {
    const auto entry = FakeWindows::profile.find(key);
    const auto value = entry == FakeWindows::profile.end()
        ? std::wstring(fallback) : entry->second;
    const auto length = std::min(value.size(), static_cast<std::size_t>(capacity - 1));
    std::wmemcpy(buffer, value.data(), length);
    buffer[length] = L'\0';
    return static_cast<DWORD>(length);
}
inline int WritePrivateProfileStringW(const wchar_t*, const wchar_t* key,
    const wchar_t* value, const wchar_t* path) {
    ++FakeWindows::writeCount;
    FakeWindows::lastWritePath = path;
    if (FakeWindows::failWrite) return 0;
    FakeWindows::profile[key] = value;
    return 1;
}
