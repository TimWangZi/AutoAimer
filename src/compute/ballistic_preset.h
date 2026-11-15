#pragma once
#include "compute/trajectory.h"
#include <cstddef>

namespace autoaimer {
inline constexpr std::size_t ballisticPresetCount = 3;

/**
 * 类名称：BallisticPreset
 * 类作用：保存一套可选择的弹道归零参数。
 * 说明：相机坐标为 x 向右、y 向下、z 向前；水平偏移向右为正，竖直偏移向下为正。
 * 说明：水平角正值向右，竖直角正值表示枪口上抬。
 */
struct BallisticPreset {
    double muzzleSpeedMetersPerSecond = 20.0;
    double horizontalOffsetCentimeters = 0.0;
    double verticalOffsetCentimeters = 4.0;
    double horizontalAngleDegrees = 0.0;
    double verticalAngleDegrees = 0.0;
    double dragCoefficient = 0.08;
    bool quadraticDrag = true;
};

/**
 * 方法名称：makeMotion
 * 方法作用：将弹道预设转换为轨迹求解器使用的位置、速度和阻力参数。
 */
Motion makeMotion(const BallisticPreset& preset, Vec3 gravity = {0.0, 9.81, 0.0});
} // namespace autoaimer
