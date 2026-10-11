#include "config.h"
#include <iostream>
#include <locale>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
struct CommaDecimal : std::numpunct<wchar_t> {
    wchar_t do_decimal_point() const override { return L','; }
};
void Run() {
    using namespace FakeWindows;
    Reset();
    Config c;
    Check(c.Load(nullptr), "load defaults");
    Check(c.visible && c.toggleKey == VK_F7 && c.barWidth == 0.32f, "default values");
    Check(c.Save() && writeCount == 11, "save all options");
    Check(lastWritePath == L"C:\\plugins\\compass.ini", "INI beside plugin");
    c.visible = false;
    Check(c.Save(), "save toggle");
    Config restored;
    Check(restored.Load(nullptr) && !restored.visible, "restore toggle");

    for (const auto* invalid : {L"", L"oops", L"123junk", L"1.5", L"0x76"}) {
        profile[L"toggleKey"] = invalid;
        Check(c.Load(nullptr) && c.toggleKey == VK_F7, "invalid key uses F7");
    }
    profile[L"colorR"] = L"-99999999999999999999999999999999";
    profile[L"colorG"] = L"99999999999999999999999999999999";
    profile[L"bgAlpha"] = L"oops";
    profile[L"toggleKey"] = L" 121 \t";
    Check(c.Load(nullptr), "load integer edge cases");
    Check(c.colorR == 0 && c.colorG == 255 && c.bgAlpha == 100 && c.toggleKey == 121,
          "integer overflow clamps, invalid integer defaults, whitespace accepted");
    profile[L"toggleKey"] = std::wstring(600, L'9') + L"invalid";
    Check(c.Load(nullptr) && c.toggleKey == VK_F7, "truncated INI value uses default");

    for (const auto* invalid : {L"NaN", L"inf", L"1e9999", L"0.2oops", L"0,2", L""}) {
        profile[L"barWidth"] = invalid;
        Check(c.Load(nullptr) && c.barWidth == 0.32f, "invalid float uses default");
    }
    profile[L"barWidth"] = L" 0.25 \t";
    profile[L"fov"] = L"999";
    profile[L"yPos"] = L"-1";
    Check(c.Load(nullptr) && c.barWidth == 0.25f && c.fov == 360 && c.yPos == 0.04f,
          "float clamping and whitespace");

    profile[L"labels"] = L" PN , WS , PD , Z " ;
    Check(c.Load(nullptr) && c.labels[0] == "PN" && c.labels[3] == "Z", "trim CSV");
    for (const auto* invalid : {L"N,E,S", L"N,E,S,W,X", L"N,,S,W", L"~r~,E,S,W",
                               L"TOO_LONG_LABEL,E,S,W", L"N E,E,S,W", L"N,E,S,", L"\u00f3,E,S,W"}) {
        profile[L"labels"] = invalid;
        Check(c.Load(nullptr) && c.labels[0] == "N" && c.labels[3] == "W",
              "invalid CSV restores defaults on reload");
    }

    const auto previousLocale = std::locale();
    std::locale::global(std::locale(previousLocale, new CommaDecimal));
    profile[L"barWidth"] = L"0.234567";
    Check(c.Load(nullptr) && c.barWidth == 0.234567f, "locale independent parsing");
    Check(c.Save() && profile[L"barWidth"].find(L',') == std::wstring::npos,
          "locale independent formatting");
    Check(restored.Load(nullptr) && restored.barWidth == c.barWidth, "float roundtrip");
    std::locale::global(previousLocale);

    failWrite = true;
    Check(!c.Save(), "write failure reported");
    failWrite = false;
    failPath = true;
    Check(!c.Load(nullptr) && !c.Save(), "path failure clears stale save path");
    failPath = false;
    modulePath = std::wstring(32768, L'x');
    Check(!c.Load(nullptr) && !c.Save(), "truncated module path rejected");
}
}
int main() {
    try {
        Run();
        std::cout << "PASS: " << checks << " configuration checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after check " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
