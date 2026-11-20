#pragma once
#include "sensors/sdm20.h"
#include "sensors/sensor.h"
#include <string>

namespace autoaimer {
/**
 * 类名称：LinuxSdm20DistanceSensor
 * 类作用：通过 Linux UART 读取 SDM20 连续数据流，并提供按键触发的一次测距结果。
 */
class LinuxSdm20DistanceSensor final : public DistanceSensor {
public:
    LinuxSdm20DistanceSensor() = default;
    LinuxSdm20DistanceSensor(const LinuxSdm20DistanceSensor&) = delete;
    LinuxSdm20DistanceSensor& operator=(const LinuxSdm20DistanceSensor&) = delete;
    /**
     * 方法名称：LinuxSdm20DistanceSensor::~LinuxSdm20DistanceSensor
     * 方法作用：关闭 UART 并释放内核句柄。
     */
    ~LinuxSdm20DistanceSensor() override;

    /**
     * 方法名称：LinuxSdm20DistanceSensor::open
     * 方法作用：以 460800 baud、8N1、无流控方式打开非阻塞串口。
     */
    bool open(const std::string& device, std::string& error);

    /**
     * 方法名称：LinuxSdm20DistanceSensor::poll
     * 方法作用：排空当前串口数据，并在测距请求后发布下一份有效距离。
     */
    Sample<double> poll(TimePoint now) override;

    /**
     * 方法名称：LinuxSdm20DistanceSensor::measure
     * 方法作用：丢弃请求前的串口数据，并等待下一帧作为本次测量结果。
     */
    void measure(TimePoint now) override;

private:
    // 方法作用：关闭当前串口并清除解析和测量状态。
    void closeDevice();

    int descriptor = -1;
    Sdm20Parser parser;
    Sample<double> latest;
    TimePoint requestedAt{};
    bool measurementPending = false;
    bool pendingIoError = false;
};
} // namespace autoaimer
