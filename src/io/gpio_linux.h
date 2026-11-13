#pragma once
#include "io/gpio.h"

namespace autoaimer {
/**
 * 类名称：LinuxGpioOutput
 * 类作用：使用 Linux GPIO 字符设备独占申请并控制一个输出引脚。
 * 说明：按 bank 标签定位设备，使用 SDK 与板端内核均支持的 v1 line-handle ABI。
 */
class LinuxGpioOutput final : public GpioOutput {
public:
    /**
     * 方法名称：LinuxGpioOutput::LinuxGpioOutput
     * 方法作用：创建尚未申请引脚的对象。
     */
    LinuxGpioOutput() = default;
    /**
     * 方法名称：LinuxGpioOutput::~LinuxGpioOutput
     * 方法作用：关闭 line handle，释放引脚占用。
     */
    ~LinuxGpioOutput() override;
    /**
     * 方法名称：LinuxGpioOutput::LinuxGpioOutput（复制）
     * 方法作用：禁止复制引脚句柄的所有权。
     */
    LinuxGpioOutput(const LinuxGpioOutput&) = delete;
    /**
     * 方法名称：LinuxGpioOutput::operator=
     * 方法作用：禁止通过复制赋值共享引脚句柄。
     */
    LinuxGpioOutput& operator=(const LinuxGpioOutput&) = delete;
    /**
     * 方法名称：LinuxGpioOutput::open
     * 方法作用：按 bank 标签与 bank 内偏移申请输出，同时设置初始电平。
     * 说明：在启动阶段调用一次；consumer 用于内核显示占用者，成功后才能 set。
     */
    bool open(const char* bankLabel, unsigned offset, bool initialHigh, const char* consumer,
              std::string& error);
    /**
     * 方法名称：LinuxGpioOutput::set
     * 方法作用：通过一次 GPIO ioctl 设置物理电平。
     */
    bool set(bool high, std::string& error) override;

private:
    int descriptor = -1;
};
} // namespace autoaimer
