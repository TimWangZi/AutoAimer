#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace autoaimer {
/**
 * 类名称：SpiDevice
 * 类作用：提供面向已选中 SPI 设备的同步字节写入接口。
 * 说明：不解释字节内容，也不管理外设的 DC、复位或协议时序。
 */
class SpiDevice {
public:
    /**
     * 方法名称：SpiDevice::~SpiDevice
     * 方法作用：允许通过接口安全销毁具体实现。
     */
    virtual ~SpiDevice() = default;
    /**
     * 方法名称：SpiDevice::write
     * 方法作用：同步发送一块字节，完整写入后返回成功，否则填写错误。
     * 说明：借用 data[0..size)，调用返回后即可复用；一次调用对应一次传输，
     *       调用方提供有效的非空数据块，并保证同一设备不被并发访问。
     */
    virtual bool write(const std::uint8_t* data, std::size_t size, std::string& error) = 0;
};
} // namespace autoaimer
