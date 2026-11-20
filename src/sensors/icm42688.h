#pragma once
#include "sensors/sensor.h"
#include <array>
#include <cstdint>

namespace autoaimer {
/**
 * 枚举名称：Icm426xxModel
 * 枚举作用：标识当前兼容驱动已验证支持的具体 ICM-426xx 型号。
 */
enum class Icm426xxModel {
    Unknown,
    Icm42605,
    Icm42688P,
};

/**
 * 方法名称：identifyIcm426xx
 * 方法作用：把 WHO_AM_I 寄存器值转换为已验证支持的具体型号。
 */
Icm426xxModel identifyIcm426xx(std::uint8_t identity);

/**
 * 方法名称：icm426xxModelName
 * 方法作用：返回适合日志显示的型号名称。
 */
const char* icm426xxModelName(Icm426xxModel model);

/**
 * 类名称：Icm426xxMotionFilter
 * 类作用：把兼容 ICM-426xx 的原始加速度和角速度转换为相机坐标系中的重力与倾角。
 */
class Icm426xxMotionFilter {
public:
    /**
     * 方法名称：Icm426xxMotionFilter::update
     * 方法作用：解码从 ACCEL_DATA_X1 开始的 12 字节，并用互补滤波更新运动状态。
     */
    ImuData update(const std::array<std::uint8_t, 12>& registers, TimePoint now);

    /**
     * 方法名称：Icm426xxMotionFilter::reset
     * 方法作用：清除滤波历史，令下一份有效样本重新确定初始重力方向。
     */
    void reset();

private:
    Vec3 gravity{};
    double yawDegrees{};
    TimePoint previousSample{};
    bool initialized = false;
};
} // namespace autoaimer
