#include "ui/level_indicator.h"
#include <algorithm>
#include <cmath>

namespace autoaimer {
void drawLevelIndicator(HudCanvas& hud, const std::optional<Orientation>& orientation) {
    // 圆形仪表整体移入圆屏的左下区域，避免外沿和刻度被物理边框遮住。
    constexpr Pixel center{66, 178};
    constexpr int radius = 24;
    constexpr Rgba white{255, 255, 255, 255};
    constexpr Rgba cyan{80, 210, 255, 255};
    constexpr Rgba amber{255, 210, 80, 255};
    hud.circle(center, radius, white);
    hud.line({center.x, center.y - radius - 3}, {center.x, center.y - radius + 3}, white);

    if (orientation && std::isfinite(orientation->rollDegrees) &&
        std::isfinite(orientation->pitchDegrees)) {
        constexpr double radiansPerDegree = 0.017453292519943295;
        const double roll = orientation->rollDegrees * radiansPerDegree;
        const double pitch = std::clamp(orientation->pitchDegrees, -30.0, 30.0) * 0.55;
        const double dx = std::cos(roll) * 19;
        const double dy = std::sin(roll) * 19;
        const double lineY = center.y + pitch;
        hud.line({center.x - dx, lineY - dy}, {center.x + dx, lineY + dy}, cyan);
    } else {
        hud.line({center.x - 12, center.y}, {center.x + 12, center.y}, amber);
    }

    // 中心参考线保持固定，倾斜线随测量结果转动。
    hud.line({center.x - 17, center.y}, {center.x - 5, center.y}, white);
    hud.line({center.x + 5, center.y}, {center.x + 17, center.y}, white);
    hud.line({center.x - 5, center.y}, {center.x, center.y + 3}, white);
    hud.line({center.x, center.y + 3}, {center.x + 5, center.y}, white);
}
} // namespace autoaimer
