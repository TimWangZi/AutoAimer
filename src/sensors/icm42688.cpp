#include "sensors/icm42688.h"
#include <algorithm>
#include <cmath>

namespace autoaimer {
Icm426xxModel identifyIcm426xx(std::uint8_t identity) {
    switch (identity) {
    case 0x42:
        return Icm426xxModel::Icm42605;
    case 0x47:
        return Icm426xxModel::Icm42688P;
    default:
        return Icm426xxModel::Unknown;
    }
}

const char* icm426xxModelName(Icm426xxModel model) {
    switch (model) {
    case Icm426xxModel::Icm42605:
        return "ICM-42605";
    case Icm426xxModel::Icm42688P:
        return "ICM-42688-P";
    default:
        return "unknown ICM-426xx";
    }
}

namespace {
constexpr double standardGravity = 9.80665;
constexpr double accelLsbPerG = 8192.0; // ACCEL_CONFIG0 = 0x48，量程为 ±4 g。
constexpr double gyroLsbPerDegree = 65.5; // GYRO_CONFIG0 = 0x48，量程为 ±500 °/s。
constexpr double radiansPerDegree = 3.14159265358979323846 / 180.0;
constexpr double degreesPerRadian = 180.0 / 3.14159265358979323846;

std::int16_t signedBigEndian(std::uint8_t high, std::uint8_t low) {
    const auto value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(high) << 8) | low);
    return static_cast<std::int16_t>(value);
}

double length(Vec3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3 scaled(Vec3 value, double factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

Vec3 added(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

double dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 normalizedGravity(Vec3 value) {
    const double magnitude = length(value);
    return magnitude > 0.0 ? scaled(value, standardGravity / magnitude) : Vec3{};
}

// 实机安装方向：模块 +Y 向画面右、-X 向画面下、+Z 向镜头前方。
// 加速度计测得的是比力，因此先取反，再按相同安装旋转映射到相机坐标。
Vec3 accelerationToCamera(Vec3 sensorAcceleration) {
    return {-sensorAcceleration.y, sensorAcceleration.x, -sensorAcceleration.z};
}

Vec3 angularRateToCamera(Vec3 sensorRate) {
    return {sensorRate.y, -sensorRate.x, sensorRate.z};
}
} // namespace

ImuData Icm426xxMotionFilter::update(const std::array<std::uint8_t, 12>& registers,
                                     TimePoint now) {
    const double accelScale = standardGravity / accelLsbPerG;
    const Vec3 rawAcceleration{
        signedBigEndian(registers[0], registers[1]) * accelScale,
        signedBigEndian(registers[2], registers[3]) * accelScale,
        signedBigEndian(registers[4], registers[5]) * accelScale,
    };
    const Vec3 gyroDegrees{
        signedBigEndian(registers[6], registers[7]) / gyroLsbPerDegree,
        signedBigEndian(registers[8], registers[9]) / gyroLsbPerDegree,
        signedBigEndian(registers[10], registers[11]) / gyroLsbPerDegree,
    };
    const Vec3 measuredGravity = accelerationToCamera(rawAcceleration);
    const double measuredMagnitude = length(measuredGravity);
    if (!std::isfinite(measuredMagnitude) || measuredMagnitude < standardGravity * 0.25) {
        const Sample<Vec3> badGravity{{}, now, SampleStatus::Error};
        const Sample<Orientation> badOrientation{{}, now, SampleStatus::Error};
        return {badGravity, badOrientation};
    }

    const Vec3 cameraRateDegrees = angularRateToCamera(gyroDegrees);
    if (!initialized) {
        gravity = normalizedGravity(measuredGravity);
        previousSample = now;
        initialized = true;
    } else {
        double elapsed = std::chrono::duration<double>(now - previousSample).count();
        elapsed = std::clamp(elapsed, 0.0, 0.1);
        previousSample = now;

        const Vec3 angularRateRadians = scaled(cameraRateDegrees, radiansPerDegree);
        Vec3 predicted = added(gravity, scaled(cross(angularRateRadians, gravity), -elapsed));
        predicted = normalizedGravity(predicted);

        // 角速度负责短时响应，加速度在静止或缓慢移动时消除积分漂移。
        if (measuredMagnitude >= standardGravity * 0.5 &&
            measuredMagnitude <= standardGravity * 1.5) {
            const double correction = 1.0 - std::exp(-elapsed / 0.35);
            predicted = added(scaled(predicted, 1.0 - correction),
                              scaled(normalizedGravity(measuredGravity), correction));
        }
        gravity = normalizedGravity(predicted);
        const Vec3 down = scaled(gravity, 1.0 / standardGravity);
        yawDegrees = std::remainder(yawDegrees + dot(cameraRateDegrees, down) * elapsed, 360.0);
    }

    Orientation orientation;
    orientation.rollDegrees = std::atan2(gravity.x, gravity.y) * degreesPerRadian;
    orientation.pitchDegrees =
        std::atan2(gravity.z, std::hypot(gravity.x, gravity.y)) * degreesPerRadian;
    orientation.yawDegrees = yawDegrees;
    return {{gravity, now, SampleStatus::Valid}, {orientation, now, SampleStatus::Valid}};
}

void Icm426xxMotionFilter::reset() {
    gravity = {};
    yawDegrees = 0.0;
    previousSample = {};
    initialized = false;
}
} // namespace autoaimer
