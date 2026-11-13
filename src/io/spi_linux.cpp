#include "io/spi_linux.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace autoaimer {
namespace {
bool failed(const std::string& operation, std::string& error) {
    error = operation + ": " + std::strerror(errno);
    return false;
}
} // namespace

LinuxSpiDevice::~LinuxSpiDevice() {
    if (descriptor >= 0)
        ::close(descriptor);
}

bool LinuxSpiDevice::open(const SpiConfiguration& configuration, std::string& error) {
    const int opened = ::open(configuration.devicePath, O_WRONLY | O_CLOEXEC);
    if (opened < 0)
        return failed(configuration.devicePath, error);
    const std::uint8_t lsbFirst = configuration.lsbFirst;
    if (::ioctl(opened, SPI_IOC_WR_MODE, &configuration.mode) < 0 ||
        ::ioctl(opened, SPI_IOC_WR_BITS_PER_WORD, &configuration.bitsPerWord) < 0 ||
        ::ioctl(opened, SPI_IOC_WR_LSB_FIRST, &lsbFirst) < 0 ||
        ::ioctl(opened, SPI_IOC_WR_MAX_SPEED_HZ, &configuration.speedHz) < 0) {
        failed("SPI configuration", error);
        ::close(opened);
        return false;
    }
    descriptor = opened;
    return true;
}

bool LinuxSpiDevice::write(const std::uint8_t* data, std::size_t size, std::string& error) {
    // 一次 write 保留一次 SPI 事务及原有 CS 边界，不复制、不拆分数据。
    // 短写后由调用方终止上层操作，不能擅自续写或重发未知协议的数据。
    const auto written = ::write(descriptor, data, size);
    if (written < 0)
        return failed("SPI write", error);
    if (written != static_cast<ssize_t>(size)) {
        error = "SPI short write";
        return false;
    }
    return true;
}
} // namespace autoaimer
