#pragma once
#include "io/gpio.h"
#include "io/spi.h"
#include <utility>
#include <vector>

namespace autoaimer::test {
enum class IoOperation { SpiWrite, DcLevel, ResetLevel, Delay };
struct IoEvent {
    IoOperation operation;
    unsigned value = 0;
    std::vector<std::uint8_t> bytes;
};

/**
 * 类名称：IoRecording
 * 类作用：在测试中按发生顺序记录 SPI、GPIO 与等待事件，并注入一次失败。
 */
struct IoRecording {
    std::vector<IoEvent> events;
    int failAt = -1;
    /**
     * 方法名称：IoRecording::append
     * 方法作用：记录一次尝试，在指定事件处返回错误，便于检查驱动是否立即停止。
     */
    bool append(IoEvent event, std::string& error) {
        events.push_back(std::move(event));
        if (static_cast<int>(events.size() - 1) != failAt)
            return true;
        error = events.back().operation == IoOperation::SpiWrite ? "injected SPI error"
                                                                 : "injected GPIO error";
        return false;
    }
};

/**
 * 类名称：RecordingSpi
 * 类作用：记录通用 SPI 接口收到的原始字节，不解释屏幕命令。
 */
class RecordingSpi final : public SpiDevice {
public:
    /**
     * 方法名称：RecordingSpi::RecordingSpi
     * 方法作用：借用与 GPIO 共用的事件记录。
     */
    explicit RecordingSpi(IoRecording& recording) : recording(recording) {}
    /**
     * 方法名称：RecordingSpi::write
     * 方法作用：保留一次写入的边界和字节内容，并返回注入的结果。
     */
    bool write(const std::uint8_t* data, std::size_t size, std::string& error) override {
        return recording.append({IoOperation::SpiWrite, 0, {data, data + size}}, error);
    }

private:
    IoRecording& recording;
};

/**
 * 类名称：RecordingGpio
 * 类作用：记录某根 GPIO 的电平变化，与 SPI 操作共用一条时间线。
 */
class RecordingGpio final : public GpioOutput {
public:
    /**
     * 方法名称：RecordingGpio::RecordingGpio
     * 方法作用：指定事件记录及所代表的引脚。
     */
    RecordingGpio(IoRecording& recording, IoOperation operation)
        : recording(recording), operation(operation) {}
    /**
     * 方法名称：RecordingGpio::set
     * 方法作用：记录电平设置尝试，并返回注入的结果。
     */
    bool set(bool high, std::string& error) override {
        return recording.append({operation, static_cast<unsigned>(high), {}}, error);
    }

private:
    IoRecording& recording;
    IoOperation operation;
};
} // namespace autoaimer::test
