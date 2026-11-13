#pragma once
#include "io/spi.h"

namespace autoaimer {
/**
 * 类名称：SpiConfiguration
 * 类作用：集中描述 Linux SPI 设备路径与传输参数。
 * 说明：路径和时钟由启动代码指定；默认采用 mode 0、8 位、MSB 先发。
 */
struct SpiConfiguration {
    const char* devicePath;
    std::uint32_t speedHz;
    std::uint8_t mode = 0;
    std::uint8_t bitsPerWord = 8;
    bool lsbFirst = false;
};

/**
 * 类名称：LinuxSpiDevice
 * 类作用：拥有 spidev 句柄，配置 SPI 并执行同步字节写入。
 * 说明：直接使用 SDK 已有的 spidev ABI，不依赖屏幕或 GPIO 模块。
 */
class LinuxSpiDevice final : public SpiDevice {
public:
    /**
     * 方法名称：LinuxSpiDevice::LinuxSpiDevice
     * 方法作用：创建尚未打开设备的对象。
     */
    LinuxSpiDevice() = default;
    /**
     * 方法名称：LinuxSpiDevice::~LinuxSpiDevice
     * 方法作用：关闭所拥有的 SPI 句柄。
     */
    ~LinuxSpiDevice() override;
    /**
     * 方法名称：LinuxSpiDevice::LinuxSpiDevice（复制）
     * 方法作用：禁止复制设备句柄的所有权。
     */
    LinuxSpiDevice(const LinuxSpiDevice&) = delete;
    /**
     * 方法名称：LinuxSpiDevice::operator=
     * 方法作用：禁止通过复制赋值共享设备句柄。
     */
    LinuxSpiDevice& operator=(const LinuxSpiDevice&) = delete;
    /**
     * 方法名称：LinuxSpiDevice::open
     * 方法作用：打开指定设备并设置传输参数，失败时释放本次申请的句柄。
     * 说明：在启动阶段调用一次；成功后才能 write。
     */
    bool open(const SpiConfiguration& configuration, std::string& error);
    /**
     * 方法名称：LinuxSpiDevice::write
     * 方法作用：执行一次同步 write；短写或系统错误立即返回失败，不自动重试。
     */
    bool write(const std::uint8_t* data, std::size_t size, std::string& error) override;

private:
    int descriptor = -1;
};
} // namespace autoaimer
