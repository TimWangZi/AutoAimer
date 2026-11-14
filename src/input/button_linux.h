#pragma once
#include "input/input.h"
#include <string>

namespace autoaimer {
/**
 * 类名称：LinuxButtonInput
 * 类作用：从 Linux gpio-keys 读取五个实体按钮的单次按下事件。
 */
class LinuxButtonInput final : public ButtonInput {
public:
    LinuxButtonInput() = default;
    ~LinuxButtonInput() override;
    LinuxButtonInput(const LinuxButtonInput&) = delete;
    LinuxButtonInput& operator=(const LinuxButtonInput&) = delete;

    /**
     * 方法名称：LinuxButtonInput::open
     * 方法作用：打开设备树创建的按键设备，以非阻塞方式读取事件。
     */
    bool open(std::string& error);

    /**
     * 方法名称：LinuxButtonInput::poll
     * 方法作用：跳过松开和长按重复，按顺序返回一个已消抖的按下事件。
     */
    Input poll() override;

private:
    int descriptor = -1;
    bool droppingEvents = false;
};
} // namespace autoaimer
