#include "compass.h"
#include "main.h"
#include "natives.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
// Older SDK headers expose these natives under different names or return Any.
// Read the float result bits through ScriptHookV's public native-call API.
float ScreenMetric(unsigned long long hash, bool takesBooleanArgument) {
    nativeInit(hash);
    if (takesBooleanArgument) nativePush64(0); // GET_ASPECT_RATIO(FALSE)
    const auto result = nativeCall();
    if (!result) return std::numeric_limits<float>::quiet_NaN();
    float value;
    std::memcpy(&value, result, sizeof(value));
    return value;
}
float Wrap(float angle) {
    angle = std::fmod(angle, 360.0f);
    return angle < 0.0f ? angle + 360.0f : angle;
}
bool CameraHeading(float* heading) {
    __try {
        const auto rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
        if (!std::isfinite(rotation.z)) return false;
        // GTA yaw grows counterclockwise; compass bearings grow clockwise.
        *heading = Wrap(-rotation.z);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool EntityHeading(float* heading) {
    __try {
        const auto ped = PLAYER::PLAYER_PED_ID();
        if (!ped) return false;
        const float yaw = ENTITY::GET_ENTITY_HEADING(ped);
        if (!std::isfinite(yaw)) return false;
        *heading = Wrap(-yaw);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
float SafeZone() {
    __try {
        const float value = ScreenMetric(0xBAF107B6BB2C97F0ULL, false); // GET_SAFE_ZONE_SIZE
        return std::isfinite(value) ? std::clamp(value, 0.8f, 1.0f) : 1.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 1.0f; }
}
float AspectRatio() {
    __try {
        const float value = ScreenMetric(0xF1307EF624A80D87ULL, true); // GET_ASPECT_RATIO
        return std::isfinite(value) && value > 0.0f ? value : 16.0f / 9.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 16.0f / 9.0f; }
}
void Text(const char* label, float x, float y, float scale, const Config& c) {
    UI::SET_TEXT_FONT(0);
    UI::SET_TEXT_SCALE(0.0f, scale);
    UI::SET_TEXT_COLOUR(c.colorR, c.colorG, c.colorB, 255);
    UI::SET_TEXT_CENTRE(TRUE);
    UI::SET_TEXT_DROPSHADOW(1, 0, 0, 0, 200);
    UI::SET_TEXT_OUTLINE();
    UI::_SET_TEXT_ENTRY(const_cast<char*>("STRING"));
    UI::_ADD_TEXT_COMPONENT_STRING(const_cast<char*>(label));
    UI::_DRAW_TEXT(x, y);
}
CompassFrame Draw(const Config& c) {
    float heading = 0.0f;
    if (!CameraHeading(&heading) && !EntityHeading(&heading)) return CompassFrame::HeadingUnavailable;
    const float safe = SafeZone();
    const float aspect = AspectRatio();
    const float width = c.barWidth * safe * std::min(1.0f, (16.0f / 9.0f) / aspect);
    const float y = 0.5f + (c.yPos - 0.5f) * safe;
    const float height = 0.055f * safe;
    GRAPHICS::DRAW_RECT(0.5f, y, width, height, 0, 0, 0, c.bgAlpha);
    for (int bearing = 0; bearing < 360; bearing += 15) {
        const float delta = Wrap(static_cast<float>(bearing) - heading + 180.0f) - 180.0f;
        if (std::fabs(delta) > c.fov * 0.5f) continue;
        const float x = 0.5f + delta / c.fov * width;
        const bool cardinal = bearing % 90 == 0;
        const float tickHeight = (cardinal ? 0.012f : 0.006f) * safe;
        const float thickness = 0.0015f * safe / aspect;
        // DRAW_LINE uses world coordinates. Thin DRAW_RECTs are actual HUD lines.
        if (std::fabs(x - 0.5f) + thickness * 0.5f > width * 0.5f) continue;
        GRAPHICS::DRAW_RECT(x, y - height * 0.5f + tickHeight * 0.5f,
            thickness, tickHeight, c.colorR, c.colorG, c.colorB, 255);
        if (!cardinal && !c.showDegrees) continue;
        char degrees[8]{};
        std::snprintf(degrees, sizeof(degrees), "%d", bearing);
        const char* label = cardinal ? c.labels[bearing / 90].c_str() : degrees;
        // Reserve a conservative text margin instead of drawing clipped edge labels.
        const float labelLength = static_cast<float>(cardinal ? c.labels[bearing / 90].size() : 3);
        const float margin = labelLength * 0.007f * safe / aspect;
        if (std::fabs(x - 0.5f) + margin > width * 0.5f) continue;
        Text(label, x, y - 0.005f * safe, 0.30f * safe, c);
    }
    GRAPHICS::DRAW_RECT(0.5f, y - height * 0.5f - 0.003f * safe,
        0.004f * safe / aspect, 0.010f * safe, c.colorR, c.colorG, c.colorB, 255);
    return CompassFrame::Drawn;
}
}

CompassFrame DrawCompassSafely(const Config& config) {
    __try { return Draw(config); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return CompassFrame::NativeFailure; }
}
