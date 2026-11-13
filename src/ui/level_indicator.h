#pragma once
#include "sensors/sensor.h"
#include "ui/hud.h"
#include <optional>

namespace autoaimer {
/**
 * 方法名称：drawLevelIndicator
 * 方法作用：根据可选的设备倾角绘制圆形水平仪，无有效倾角时显示占位状态。
 */
void drawLevelIndicator(HudCanvas& hud, const std::optional<Orientation>& orientation);
} // namespace autoaimer
