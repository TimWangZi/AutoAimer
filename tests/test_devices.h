#pragma once

#include "input/input.h"
#include "sensors/sensor.h"
#include <initializer_list>
#include <vector>

namespace autoaimer::test {

/**
 * 类名称：ScriptedButtons
 * 类作用：按固定顺序提供有限个测试按键事件。
 * 说明：按固定顺序提供有限个测试按键事件；事件耗尽后返回 Key::None，模拟按键松开。
 */
class ScriptedButtons final : public ButtonInput {
public:
    /**
     * 方法名称：ScriptedButtons::ScriptedButtons
     * 方法作用：保存测试所需的固定按键序列。
     */
    ScriptedButtons(std::initializer_list<Key> keys) : keys(keys) {}

    /**
     * 方法名称：ScriptedButtons::poll
     * 方法作用：依次取出测试按键，序列耗尽后返回无事件。
     */
    Input poll() override {
        if (next == keys.size())
            return {};
        return {keys[next++]};
    }

private:
    std::vector<Key> keys;
    std::size_t next = 0;
};

/**
 * 类名称：TestImuSensor
 * 类作用：为测试提供带采样时间的固定 IMU 数据。
 * 说明：提供一组一致的测试 IMU 数据；真实驱动接入后负责把硬件读数转换为重力与倾角。
 */
class TestImuSensor final : public ImuSensor {
public:
    /**
     * 方法名称：TestImuSensor::TestImuSensor
     * 方法作用：保存用于测试的重力向量和设备倾角。
     */
    TestImuSensor(Vec3 gravity, Orientation orientation)
        : gravity(gravity), orientation(orientation) {}

    /**
     * 方法名称：TestImuSensor::poll
     * 方法作用：返回固定的有效 IMU 样本，并累计轮询次数。
     */
    ImuData poll(TimePoint now) override {
        ++pollCount;
        return {{gravity, now, SampleStatus::Valid}, {orientation, now, SampleStatus::Valid}};
    }

    unsigned pollCount = 0;

private:
    Vec3 gravity;
    Orientation orientation;
};

/**
 * 类名称：TestDistanceSensor
 * 类作用：为测试模拟单次测距和最近完成读数。
 * 说明：模拟单次测距：poll() 返回上次已完成读数，measure() 只执行一次新测量。
 */
class TestDistanceSensor final : public DistanceSensor {
public:
    /**
     * 方法名称：TestDistanceSensor::TestDistanceSensor
     * 方法作用：保存每次模拟测距应返回的距离值。
     */
    explicit TestDistanceSensor(double measuredMeters) : measuredMeters(measuredMeters) {}

    /**
     * 方法名称：TestDistanceSensor::poll
     * 方法作用：返回最近完成的测试测距结果，并累计轮询次数。
     */
    Sample<double> poll(TimePoint) override {
        ++pollCount;
        return latest;
    }

    /**
     * 方法名称：TestDistanceSensor::measure
     * 方法作用：生成一次有效测量结果，并累计测量次数。
     */
    void measure(TimePoint now) override {
        ++measureCount;
        latest = {measuredMeters, now, SampleStatus::Valid};
    }

    unsigned pollCount = 0;
    unsigned measureCount = 0;

private:
    double measuredMeters;
    Sample<double> latest;
};

} // namespace autoaimer::test
