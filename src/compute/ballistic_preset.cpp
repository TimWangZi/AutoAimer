#include "compute/ballistic_preset.h"
#include <cmath>

namespace autoaimer {
Motion makeMotion(const BallisticPreset& preset, Vec3 gravity) {
    constexpr double pi = 3.14159265358979323846;
    const double yaw = preset.horizontalAngleDegrees * pi / 180.0;
    const double pitch = preset.verticalAngleDegrees * pi / 180.0;
    const double horizontalSpeed = preset.muzzleSpeedMetersPerSecond * std::cos(pitch);

    Motion motion;
    // y 轴向下，因此竖直上抬对应负 y；yaw 在水平 x-z 平面内向右旋转。
    motion.origin = {preset.horizontalOffsetCentimeters / 100.0,
                     preset.verticalOffsetCentimeters / 100.0, 0.1};
    motion.velocity = {horizontalSpeed * std::sin(yaw),
                       -preset.muzzleSpeedMetersPerSecond * std::sin(pitch),
                       horizontalSpeed * std::cos(yaw)};
    motion.gravity = gravity;
    motion.model = preset.quadraticDrag ? DragModel::Quadratic : DragModel::Linear;
    motion.drag = preset.dragCoefficient;
    return motion;
}
} // namespace autoaimer
