#pragma once
#include "compute/trajectory.h"
#include <chrono>

namespace autoaimer {
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
enum class SampleStatus { Unavailable, Valid, Stale, Error };

template <class T> struct Sample {
    T value{};
    TimePoint sampledAt{};
    SampleStatus status =
        SampleStatus::Unavailable; // 必须先判断状态，value 的默认值不代表测量成功。
};

struct Orientation {
    double rollDegrees{};
    double pitchDegrees{};
    double yawDegrees{};
};

struct ImuData {
    Sample<Vec3> gravity;            // 相机坐标系中的重力，单位 m/s²。
    Sample<Orientation> orientation; // IMU 给出的设备倾角，单位为度。
};

struct SensorData {
    Sample<Vec3> gravity;
    Sample<Orientation> orientation;
    Sample<double> distanceMeters;
};

/**
 * 类名称：ImuSensor
 * 类作用：定义 IMU 重力和设备倾角数据的读取接口。
 */
class ImuSensor {
public:
    /**
     * 方法名称：ImuSensor::~ImuSensor
     * 方法作用：允许通过基类指针安全销毁 IMU 驱动。
     */
    virtual ~ImuSensor() = default;
    /**
     * 方法名称：ImuSensor::poll
     * 方法作用：读取重力和设备倾角样本，携带时间及有效性状态。
     */
    virtual ImuData poll(TimePoint now) = 0;
};

/**
 * 类名称：DistanceSensor
 * 类作用：定义单次测距触发和最近测量结果的读取接口。
 */
class DistanceSensor {
public:
    /**
     * 方法名称：DistanceSensor::~DistanceSensor
     * 方法作用：允许通过基类指针安全销毁测距驱动。
     */
    virtual ~DistanceSensor() = default;
    /**
     * 方法名称：DistanceSensor::poll
     * 方法作用：读取最近完成的测距样本，距离单位为米。
     */
    virtual Sample<double> poll(TimePoint now) = 0;
    /**
     * 方法名称：DistanceSensor::measure
     * 方法作用：触发一次新的距离测量。
     * 说明：触发一次测距；poll() 返回最近一次已完成的结果，不阻塞等待本次测量。
     */
    virtual void measure(TimePoint now) = 0;
};

/**
 * 类名称：NullImuSensor
 * 类作用：在 IMU 未接入时返回不可用样本。
 * 说明：正式程序的空接口：硬件驱动接入前明确返回不可用，不生成测试读数。
 */
class NullImuSensor final : public ImuSensor {
public:
    /**
     * 方法名称：NullImuSensor::poll
     * 方法作用：返回重力和倾角均不可用的样本。
     */
    ImuData poll(TimePoint) override { return {}; }
};

/**
 * 类名称：NullDistanceSensor
 * 类作用：在测距硬件未接入时返回不可用样本。
 */
class NullDistanceSensor final : public DistanceSensor {
public:
    /**
     * 方法名称：NullDistanceSensor::poll
     * 方法作用：返回不可用的距离样本。
     */
    Sample<double> poll(TimePoint) override { return {}; }
    /**
     * 方法名称：NullDistanceSensor::measure
     * 方法作用：忽略测距请求，不伪造硬件读数。
     */
    void measure(TimePoint) override {}
};

/**
 * 方法名称：pollSensorData
 * 方法作用：轮询 IMU 和测距接口，组成一次完整的传感器快照。
 */
inline SensorData pollSensorData(ImuSensor& imu, DistanceSensor& distance, TimePoint now) {
    const auto motion = imu.poll(now);
    return {motion.gravity, motion.orientation, distance.poll(now)};
}
} // namespace autoaimer
