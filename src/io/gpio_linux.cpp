#include "io/gpio_linux.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <glob.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace autoaimer {
namespace {
bool failed(const std::string& operation, std::string& error) {
    error = operation + ": " + std::strerror(errno);
    return false;
}

int openGpioBank(const char* bankLabel, std::string& error) {
    // gpiochipN 由探测顺序决定，不能把 bank 编号当作设备节点编号。
    glob_t devices{};
    if (::glob("/dev/gpiochip*", 0, nullptr, &devices) != 0) {
        ::globfree(&devices);
        error = "No GPIO character devices";
        return -1;
    }
    for (std::size_t i = 0; i < devices.gl_pathc; ++i) {
        const int chip = ::open(devices.gl_pathv[i], O_RDONLY | O_CLOEXEC);
        if (chip < 0) {
            failed(devices.gl_pathv[i], error);
            ::globfree(&devices);
            return -1;
        }
        gpiochip_info info{};
        if (::ioctl(chip, GPIO_GET_CHIPINFO_IOCTL, &info) < 0) {
            failed("GPIO chip info", error);
            ::close(chip);
            ::globfree(&devices);
            return -1;
        }
        if (std::strcmp(info.label, bankLabel) == 0) {
            ::globfree(&devices);
            return chip;
        }
        ::close(chip);
    }
    ::globfree(&devices);
    error = std::string("GPIO bank ") + bankLabel + " not found";
    return -1;
}
} // namespace

LinuxGpioOutput::~LinuxGpioOutput() {
    if (descriptor >= 0)
        ::close(descriptor);
}

bool LinuxGpioOutput::open(const char* bankLabel, unsigned offset, bool initialHigh,
                           const char* consumer, std::string& error) {
    const int chip = openGpioBank(bankLabel, error);
    if (chip < 0)
        return false;
    // 申请输出与设置初始电平一并完成，避免先申请再设置产生额外电平跳变。
    gpiohandle_request request{};
    request.lineoffsets[0] = offset;
    request.lines = 1;
    request.flags = GPIOHANDLE_REQUEST_OUTPUT;
    request.default_values[0] = initialHigh;
    std::strncpy(request.consumer_label, consumer, sizeof(request.consumer_label) - 1);
    const bool requested = ::ioctl(chip, GPIO_GET_LINEHANDLE_IOCTL, &request) == 0;
    if (!requested)
        failed(std::string("GPIO request ") + consumer, error);
    ::close(chip); // line handle 独立持有引脚，无需一直保留 chip fd。
    if (requested)
        descriptor = request.fd;
    return requested;
}

bool LinuxGpioOutput::set(bool high, std::string& error) {
    gpiohandle_data values{};
    values.values[0] = high;
    return ::ioctl(descriptor, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &values) == 0 ||
           failed("GPIO set value", error);
}
} // namespace autoaimer
