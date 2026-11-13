#include "io/spi_linux.h"
#include <array>
#include <cerrno>
#include <cstdarg>
#include <fcntl.h>
#include <iostream>
#include <linux/spi/spidev.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
// 仅在测试链接时替换系统调用，不给生产驱动添加测试开关或额外间接层。
constexpr int testDescriptor = 73;
struct Syscalls {
    bool failOpen = false;
    int failConfigurationAt = -1;
    ssize_t writeResult = 4;
    std::string path;
    int flags = 0;
    unsigned closes = 0, writes = 0;
    const void* writtenData = nullptr;
    std::size_t writtenSize = 0;
    std::vector<unsigned long> requests;
    std::vector<unsigned> values;
} calls;

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}
} // namespace

extern "C" int __wrap_open(const char* path, int flags, ...) {
    calls.path = path;
    calls.flags = flags;
    errno = ENOENT;
    return calls.failOpen ? -1 : testDescriptor;
}

extern "C" int __wrap_ioctl(int descriptor, unsigned long request, ...) {
    if (descriptor != testDescriptor)
        return -1;
    va_list arguments;
    va_start(arguments, request);
    // ioctl 的参数保留原本指针类型，再读取其值。
    unsigned value;
    if (request == SPI_IOC_WR_MAX_SPEED_HZ)
        value = *va_arg(arguments, const std::uint32_t*);
    else
        value = *va_arg(arguments, const std::uint8_t*);
    va_end(arguments);
    calls.requests.push_back(request);
    calls.values.push_back(value);
    errno = EIO;
    return static_cast<int>(calls.requests.size() - 1) == calls.failConfigurationAt ? -1 : 0;
}

extern "C" ssize_t __wrap_write(int descriptor, const void* data, std::size_t size) {
    if (descriptor != testDescriptor)
        return -1;
    ++calls.writes;
    calls.writtenData = data;
    calls.writtenSize = size;
    errno = EIO;
    return calls.writeResult;
}

extern "C" int __wrap_close(int descriptor) {
    if (descriptor == testDescriptor)
        ++calls.closes;
    return 0;
}

int main() {
    std::string error;
    const std::array<std::uint8_t, 4> bytes{0x12, 0x34, 0x56, 0x78};
    {
        autoaimer::LinuxSpiDevice device;
        if (!device.open({"/dev/test-spi", 40000000}, error) || calls.path != "/dev/test-spi" ||
            calls.flags != (O_WRONLY | O_CLOEXEC) ||
            calls.requests != std::vector<unsigned long>{SPI_IOC_WR_MODE, SPI_IOC_WR_BITS_PER_WORD,
                                                         SPI_IOC_WR_LSB_FIRST,
                                                         SPI_IOC_WR_MAX_SPEED_HZ} ||
            calls.values != std::vector<unsigned>{0, 8, 0, 40000000})
            return fail("SPI configuration differs from the requested parameters");
        if (!device.write(bytes.data(), bytes.size(), error) || calls.writes != 1 ||
            calls.writtenData != bytes.data() || calls.writtenSize != bytes.size())
            return fail("SPI write copied, split or changed its input");
        calls.writeResult = 2;
        if (device.write(bytes.data(), bytes.size(), error) || error != "SPI short write" ||
            calls.writes != 2)
            return fail("Short write was hidden or retried");
        calls.writeResult = -1;
        if (device.write(bytes.data(), bytes.size(), error) || error.find("SPI write:") != 0 ||
            calls.writes != 3)
            return fail("Write error was hidden or retried");
    }
    if (calls.closes != 1)
        return fail("SPI handle was not released exactly once");

    for (int i = 0; i < 4; ++i) {
        calls = {};
        calls.failConfigurationAt = i;
        {
            autoaimer::LinuxSpiDevice device;
            if (device.open({"/dev/test-spi", 40000000}, error) || calls.closes != 1 ||
                calls.requests.size() != static_cast<unsigned>(i + 1) ||
                error.find("SPI configuration:") != 0)
                return fail("Configuration failure leaked a handle or continued configuring");
        }
        if (calls.closes != 1)
            return fail("Failed SPI initialization closed the descriptor twice");
    }
    calls = {};
    calls.failOpen = true;
    {
        autoaimer::LinuxSpiDevice device;
        if (device.open({"/dev/missing-spi", 40000000}, error) || !calls.requests.empty() ||
            error.find("/dev/missing-spi:") != 0)
            return fail("Device open failure was hidden");
    }
    if (calls.closes != 0)
        return fail("Open failure tried to close a descriptor it did not own");
    return 0;
}
