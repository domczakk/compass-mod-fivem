#include "config.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <cerrno>
#include <locale>
#include <sstream>
#include <iomanip>
#include <limits>

namespace {
std::wstring Read(const std::wstring& path, const wchar_t* key, const wchar_t* fallback) {
    wchar_t buffer[512]{};
    const DWORD length = GetPrivateProfileStringW(L"Compass", key, fallback, buffer, 512, path.c_str());
    // Never accept a valid-looking prefix of an overlong, truncated value.
    if (length >= 511) return L"";
    return buffer;
}
int ReadInteger(const std::wstring& path, const wchar_t* key, int fallback,
                int minimum, int maximum) {
    const auto value = Read(path, key, L"");
    wchar_t* end = nullptr;
    errno = 0;
    const long long parsed = std::wcstoll(value.c_str(), &end, 10);
    if (end == value.c_str()) return fallback;
    while (std::iswspace(*end)) ++end;
    if (*end != L'\0') return fallback;
    // wcstoll saturates on overflow, so even very large valid integers clamp.
    if (errno != 0 && errno != ERANGE) return fallback;
    return static_cast<int>(std::clamp(parsed,
        static_cast<long long>(minimum), static_cast<long long>(maximum)));
}
float ReadFloat(const std::wstring& path, const wchar_t* key, float fallback,
                float minimum, float maximum) {
    std::wistringstream stream(Read(path, key, L""));
    stream.imbue(std::locale::classic());
    float parsed = 0.0f;
    if (!(stream >> parsed) || !std::isfinite(parsed)) return fallback;
    stream >> std::ws;
    return stream.eof() ? std::clamp(parsed, minimum, maximum) : fallback;
}
std::wstring FormatFloat(float value) {
    std::wostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return stream.str();
}
bool ParseLabels(const std::wstring& csv, std::array<std::string, 4>& result) {
    std::array<std::string, 4> parsed;
    std::size_t start = 0;
    for (std::size_t i = 0; i < parsed.size(); ++i) {
        const auto end = csv.find(L',', start);
        if ((i < 3) != (end != std::wstring::npos)) return false;
        auto part = csv.substr(start, end == std::wstring::npos ? end : end - start);
        const auto first = part.find_first_not_of(L" \t");
        if (first == std::wstring::npos) return false;
        part = part.substr(first, part.find_last_not_of(L" \t") - first + 1);
        if (part.size() > 8) return false;
        for (wchar_t ch : part) {
            // ASCII avoids unsupported game-font glyphs and '~' formatting injection.
            if (ch < 33 || ch > 126 || ch == L'~') return false;
            parsed[i].push_back(static_cast<char>(ch));
        }
        start = end == std::wstring::npos ? csv.size() : end + 1;
    }
    result = parsed;
    return true;
}
}

bool Config::Load(HMODULE module) {
    // Reset defaults, labels and path even when reusing a Config instance.
    *this = Config{};
    wchar_t filename[32768]{};
    const DWORD length = GetModuleFileNameW(module, filename, 32768);
    if (length == 0 || length >= 32768) return false;
    path_.assign(filename, length);
    const auto slash = path_.find_last_of(L"\\/");
    if (slash == std::wstring::npos) { path_.clear(); return false; }
    path_.resize(slash + 1);
    path_ += L"compass.ini";
    const auto integer = [this](const wchar_t* key, int fallback,
                                int minimum, int maximum) {
        return ReadInteger(path_, key, fallback, minimum, maximum);
    };
    visible = integer(L"visible", 1, -1, 1) != 0;
    barWidth = ReadFloat(path_, L"barWidth", 0.32f, 0.10f, 0.90f);
    yPos = ReadFloat(path_, L"yPos", 0.06f, 0.04f, 0.94f);
    colorR = integer(L"colorR", 255, 0, 255);
    colorG = integer(L"colorG", 255, 0, 255);
    colorB = integer(L"colorB", 255, 0, 255);
    bgAlpha = integer(L"bgAlpha", 100, 0, 255);
    fov = ReadFloat(path_, L"fov", 120.0f, 30.0f, 360.0f);
    toggleKey = integer(L"toggleKey", VK_F7, 1, 254);
    showDegrees = integer(L"showDegrees", 0, -1, 1) != 0;
    ParseLabels(Read(path_, L"labels", L"N,E,S,W"), labels);
    return true;
}

bool Config::Save() const {
    if (path_.empty()) return false;
    bool success = true;
    const auto write = [&](const wchar_t* key, const std::wstring& value) {
        if (!WritePrivateProfileStringW(L"Compass", key, value.c_str(), path_.c_str())) success = false;
    };
    write(L"visible", visible ? L"1" : L"0");
    write(L"barWidth", FormatFloat(barWidth));
    write(L"yPos", FormatFloat(yPos));
    write(L"colorR", std::to_wstring(colorR));
    write(L"colorG", std::to_wstring(colorG));
    write(L"colorB", std::to_wstring(colorB));
    write(L"bgAlpha", std::to_wstring(bgAlpha));
    write(L"fov", FormatFloat(fov));
    write(L"toggleKey", std::to_wstring(toggleKey));
    write(L"showDegrees", showDegrees ? L"1" : L"0");
    std::wstring csv;
    for (const auto& label : labels) {
        if (!csv.empty()) csv += L',';
        csv.append(label.begin(), label.end());
    }
    write(L"labels", csv);
    return success;
}
