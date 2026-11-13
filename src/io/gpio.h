#pragma once
#include <string>

namespace autoaimer {
/**
 * 类名称：GpioOutput
 * 类作用：提供单个 GPIO 输出的电平控制接口。
 * 说明：high 表示物理高电平；引脚的有效电平和用途由外设驱动决定。
 */
class GpioOutput {
public:
    /**
     * 方法名称：GpioOutput::~GpioOutput
     * 方法作用：允许通过接口安全销毁具体实现。
     */
    virtual ~GpioOutput() = default;
    /**
     * 方法名称：GpioOutput::set
     * 方法作用：设置输出电平，失败时填写错误。
     */
    virtual bool set(bool high, std::string& error) = 0;
};
} // namespace autoaimer
