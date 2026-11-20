#include "sensors/sdm20_linux.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace autoaimer {
namespace {
constexpr auto measurementTimeout = std::chrono::milliseconds(250);

std::string systemError(const char* operation) {
    return std::string(operation) + ": " + std::strerror(errno);
}
} // namespace

LinuxSdm20DistanceSensor::~LinuxSdm20DistanceSensor() {
    closeDevice();
}

bool LinuxSdm20DistanceSensor::open(const std::string& device, std::string& error) {
    closeDevice();
    descriptor = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) {
        error = systemError("Cannot open SDM20 UART");
        return false;
    }

    termios settings{};
    if (::tcgetattr(descriptor, &settings) != 0) {
        error = systemError("Cannot read SDM20 UART settings");
        closeDevice();
        return false;
    }
    // 手动设置 raw 模式，避免旧版 uClibc 在严格 C++17 下不声明 cfmakeraw()。
    settings.c_iflag &= static_cast<tcflag_t>(
        ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON));
    settings.c_oflag &= static_cast<tcflag_t>(~OPOST);
    settings.c_lflag &= static_cast<tcflag_t>(~(ECHO | ECHONL | ICANON | ISIG | IEXTEN));
    if (::cfsetispeed(&settings, B460800) != 0 || ::cfsetospeed(&settings, B460800) != 0) {
        error = systemError("SDM20 UART does not support 460800 baud");
        closeDevice();
        return false;
    }
    settings.c_cflag |= CLOCAL | CREAD;
    settings.c_cflag &= static_cast<tcflag_t>(~(PARENB | CSTOPB | CSIZE));
    settings.c_cflag |= CS8;
#ifdef CRTSCTS
    settings.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);
#endif
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    if (::tcsetattr(descriptor, TCSANOW, &settings) != 0 || ::tcflush(descriptor, TCIFLUSH) != 0) {
        error = systemError("Cannot configure SDM20 UART");
        closeDevice();
        return false;
    }

    parser.reset();
    latest = {};
    measurementPending = false;
    pendingIoError = false;
    return true;
}

Sample<double> LinuxSdm20DistanceSensor::poll(TimePoint now) {
    std::array<std::uint8_t, 128> bytes{};
    bool captured = false;
    for (;;) {
        const auto count = ::read(descriptor, bytes.data(), bytes.size());
        if (count > 0) {
            for (ssize_t i = 0; i < count; ++i) {
                const auto reading = parser.feed(bytes[static_cast<std::size_t>(i)]);
                if (!measurementPending || captured || !reading)
                    continue;
                captured = true;
                measurementPending = false;
                latest.sampledAt = now;
                if (reading->status == Sdm20ReadingStatus::Distance) {
                    latest.value = reading->meters;
                    latest.status = SampleStatus::Valid;
                } else {
                    latest.value = 0.0;
                    latest.status = SampleStatus::Error;
                }
            }
            continue;
        }
        if (count == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        pendingIoError = true;
        break;
    }

    if (pendingIoError ||
        (measurementPending && now - requestedAt >= measurementTimeout)) {
        latest = {0.0, now, SampleStatus::Error};
        measurementPending = false;
        pendingIoError = false;
    }
    return latest;
}

void LinuxSdm20DistanceSensor::measure(TimePoint now) {
    parser.reset();
    latest = {};
    requestedAt = now;
    measurementPending = true;
    pendingIoError = descriptor < 0 || ::tcflush(descriptor, TCIFLUSH) != 0;
}

void LinuxSdm20DistanceSensor::closeDevice() {
    if (descriptor >= 0)
        ::close(descriptor);
    descriptor = -1;
    parser.reset();
    latest = {};
    measurementPending = false;
    pendingIoError = false;
}
} // namespace autoaimer
