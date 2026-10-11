#pragma once
#include <Windows.h>
#include <array>
#include <string>

struct Config {
    bool visible = true;
    float barWidth = 0.32f;
    float yPos = 0.06f;
    int colorR = 255, colorG = 255, colorB = 255;
    int bgAlpha = 100;
    float fov = 120.0f;
    int toggleKey = VK_F7;
    bool showDegrees = false;
    std::array<std::string, 4> labels{{"N", "E", "S", "W"}};

    bool Load(HMODULE module);
    bool Save() const;
private:
    std::wstring path_;
};
