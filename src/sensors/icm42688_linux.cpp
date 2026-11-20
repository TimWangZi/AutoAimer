#include "sensors/icm42688_linux.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sstream>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace autoaimer {
namespace {
constexpr std::uint16_t deviceAddress = 0x68;
constexpr std::uint8_t deviceConfig = 0x11;
constexpr std::uint8_t driveConfig = 0x13;
constexpr std::uint8_t accelDataX1 = 0x1f;
constexpr std::uint8_t powerManagement = 0x4e;
constexpr std::uint8_t gyroConfig = 0x4f;
constexpr std::uint8_t accelConfig = 0x50;
constexpr std::uint8_t whoAmI = 0x75;
constexpr std::uint8_t registerBank = 0x76;
constexpr std::uint8_t interfaceConfig4 = 0x7a;
constexpr std::uint8_t interfaceConfig6 = 0x7c;

std::string systemError(const char* operation) {
    return std::string(operation) + ": " + std::strerror(errno);
}
} // namespace

LinuxIcm426xxSensor::~LinuxIcm426xxSensor() {
    closeDevice();
}

bool LinuxIcm426xxSensor::open(const std::string& device, std::string& error) {
    closeDevice();
    descriptor = ::open(device.c_str(), O_RDWR | O_CLOEXEC);
    if (descriptor < 0) {
        error = systemError("Cannot open ICM-426xx I2C device");
        return false;
    }

    if (!writeRegister(deviceConfig, 0x01)) {
        error = systemError("Cannot reset ICM-426xx");
        closeDevice();
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    std::uint8_t identity{};
    if (!readRegisters(whoAmI, &identity, 1)) {
        error = systemError("Cannot read ICM-426xx identity");
        closeDevice();
        return false;
    }
    model = identifyIcm426xx(identity);
    if (model == Icm426xxModel::Unknown) {
        std::ostringstream message;
        message << "Unsupported ICM-426xx WHO_AM_I 0x" << std::hex
                << static_cast<unsigned>(identity) << "; supported IDs are 0x42 and 0x47";
        error = message.str();
        closeDevice();
        return false;
    }

    // 数据手册 12.3 的 I²C 初始化。关闭 I3C SDR/DDR 与总线模式后切回 Bank 0，
    // 使后续寄存器访问始终使用标准 I²C 接口。
    std::uint8_t interface4{}, interface6{};
    if (!writeRegister(driveConfig, 0x09)) {
        error = systemError("Cannot configure ICM-426xx bus drive");
        closeDevice();
        return false;
    }
    if (!writeRegister(registerBank, 0x01)) {
        error = systemError("Cannot select ICM-426xx register Bank 1");
        closeDevice();
        return false;
    }
    if (!readRegisters(interfaceConfig4, &interface4, 1)) {
        error = systemError("Cannot read ICM-426xx INTF_CONFIG4");
        closeDevice();
        return false;
    }
    if (!readRegisters(interfaceConfig6, &interface6, 1)) {
        error = systemError("Cannot read ICM-426xx INTF_CONFIG6");
        closeDevice();
        return false;
    }
    interface4 = static_cast<std::uint8_t>(interface4 & ~0x40u);
    interface6 = static_cast<std::uint8_t>((interface6 | 0x10u) & ~0x03u);
    // 先设置 I3C 功能位，再切换总线模式，避免产生未配置完整的中间状态。
    if (!writeInterfaceRegister(interfaceConfig6, interface6)) {
        error = systemError("Cannot write ICM-426xx INTF_CONFIG6");
        closeDevice();
        return false;
    }
    if (!writeInterfaceRegister(interfaceConfig4, interface4)) {
        error = systemError("Cannot write ICM-426xx INTF_CONFIG4");
        closeDevice();
        return false;
    }
    if (!writeRegister(registerBank, 0x00)) {
        error = systemError("Cannot restore ICM-426xx register Bank 0");
        closeDevice();
        return false;
    }

    // ODR 100 Hz。高半字节选择 ±500 °/s 和 ±4 g，和解码比例保持一致。
    if (!writeRegister(gyroConfig, 0x48) || !writeRegister(accelConfig, 0x48) ||
        !writeRegister(powerManagement, 0x0f)) {
        error = systemError("Cannot configure ICM-426xx");
        closeDevice();
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    filter.reset();
    return true;
}

ImuData LinuxIcm426xxSensor::poll(TimePoint now) {
    std::array<std::uint8_t, 12> registers{};
    if (descriptor < 0 || !readRegisters(accelDataX1, registers.data(), registers.size())) {
        return {{{}, now, SampleStatus::Error}, {{}, now, SampleStatus::Error}};
    }
    return filter.update(registers, now);
}

const char* LinuxIcm426xxSensor::modelName() const {
    return icm426xxModelName(model);
}

bool LinuxIcm426xxSensor::writeRegister(std::uint8_t address, std::uint8_t value) {
    std::array<std::uint8_t, 2> bytes{address, value};
    i2c_msg message{};
    message.addr = deviceAddress;
    message.flags = 0;
    message.len = static_cast<__u16>(bytes.size());
    message.buf = bytes.data();
    i2c_rdwr_ioctl_data transfer{&message, 1};
    return ::ioctl(descriptor, I2C_RDWR, &transfer) == 1;
}

bool LinuxIcm426xxSensor::writeInterfaceRegister(std::uint8_t address, std::uint8_t value) {
    if (writeRegister(address, value))
        return true;

    // ICM-42605 在关闭 I3C SDR/DDR 时会在事务末尾返回 NACK，但寄存器已经更新。
    // 读回相同值即可确认成功；未知错误或不一致仍按失败处理。
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::uint8_t actual{};
        if (readRegisters(address, &actual, 1) && actual == value)
            return true;
    }
    errno = EIO;
    return false;
}

bool LinuxIcm426xxSensor::readRegisters(std::uint8_t address, std::uint8_t* data,
                                        std::size_t size) {
    if (size == 0 || size > 0xffff)
        return false;
    i2c_msg messages[2]{};
    messages[0].addr = deviceAddress;
    messages[0].flags = 0;
    messages[0].len = 1;
    messages[0].buf = &address;
    messages[1].addr = deviceAddress;
    messages[1].flags = I2C_M_RD;
    messages[1].len = static_cast<__u16>(size);
    messages[1].buf = data;
    i2c_rdwr_ioctl_data transfer{messages, 2};
    return ::ioctl(descriptor, I2C_RDWR, &transfer) == 2;
}

void LinuxIcm426xxSensor::closeDevice() {
    if (descriptor >= 0) {
        // 若初始化在 Bank 1 中途失败，尽力恢复 Bank 0，方便下一次诊断或重试。
        writeRegister(registerBank, 0x00);
        ::close(descriptor);
    }
    descriptor = -1;
    model = Icm426xxModel::Unknown;
    filter.reset();
}
} // namespace autoaimer
