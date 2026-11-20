#pragma once
#include "sensors/icm42688.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace autoaimer {
/**
 * 类名称：LinuxIcm426xxSensor
 * 类作用：通过 Linux i2c-dev 初始化并读取 ICM-42605 或 ICM-42688-P。
 */
class LinuxIcm426xxSensor final : public ImuSensor {
public:
    LinuxIcm426xxSensor() = default;
    LinuxIcm426xxSensor(const LinuxIcm426xxSensor&) = delete;
    LinuxIcm426xxSensor& operator=(const LinuxIcm426xxSensor&) = delete;
    /**
     * 方法名称：LinuxIcm426xxSensor::~LinuxIcm426xxSensor
     * 方法作用：关闭 I²C 设备并释放内核句柄。
     */
    ~LinuxIcm426xxSensor() override;

    /**
     * 方法名称：LinuxIcm426xxSensor::open
     * 方法作用：打开 I²C 设备，校验芯片身份并配置 100 Hz、±4 g、±500 °/s 采样。
     */
    bool open(const std::string& device, std::string& error);

    /**
     * 方法名称：LinuxIcm426xxSensor::poll
     * 方法作用：同步读取一份加速度和角速度，并返回融合后的重力与倾角。
     */
    ImuData poll(TimePoint now) override;

    /**
     * 方法名称：LinuxIcm426xxSensor::modelName
     * 方法作用：返回 open() 识别到的具体芯片型号，供诊断日志使用。
     */
    const char* modelName() const;

private:
    // 方法作用：写入一个芯片寄存器。
    bool writeRegister(std::uint8_t address, std::uint8_t value);
    // 方法作用：接口模式切换返回 NACK 时，读回寄存器确认写入是否实际生效。
    bool writeInterfaceRegister(std::uint8_t address, std::uint8_t value);
    // 方法作用：使用重复起始条件连续读取芯片寄存器。
    bool readRegisters(std::uint8_t address, std::uint8_t* data, std::size_t size);
    // 方法作用：关闭当前设备并清除融合状态。
    void closeDevice();

    int descriptor = -1;
    Icm426xxMotionFilter filter;
    Icm426xxModel model = Icm426xxModel::Unknown;
};
} // namespace autoaimer
