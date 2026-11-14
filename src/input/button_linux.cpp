#include "input/button_linux.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/input.h>
#include <unistd.h>

namespace autoaimer {
namespace {
constexpr char buttonDevice[] = "/dev/input/by-path/platform-autoaimer-buttons-event";

Key mapKey(unsigned code) {
    switch (code) {
        case KEY_UP: return Key::Up;
        case KEY_DOWN: return Key::Down;
        case KEY_LEFT: return Key::Left;
        case KEY_RIGHT: return Key::Right;
        case KEY_ENTER: return Key::Confirm;
        default: return Key::None;
    }
}
} // namespace

LinuxButtonInput::~LinuxButtonInput() {
    if (descriptor >= 0)
        ::close(descriptor);
}

bool LinuxButtonInput::open(std::string& error) {
    if (descriptor >= 0)
        return true;
    descriptor = ::open(buttonDevice, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor >= 0) {
        droppingEvents = false;
        return true;
    }
    error = std::string(buttonDevice) + ": " + std::strerror(errno);
    return false;
}

Input LinuxButtonInput::poll() {
    if (descriptor < 0)
        return {};
    for (;;) {
        input_event event{};
        const auto size = ::read(descriptor, &event, sizeof(event));
        if (size < 0 && errno == EINTR)
            continue;
        if (size < 0 && errno == EAGAIN)
            return {};
        if (size != sizeof(event)) {
            std::cerr << "Button read: " << (size < 0 ? std::strerror(errno) : "incomplete event")
                      << '\n';
            ::close(descriptor);
            descriptor = -1;
            return {};
        }

        // 队列溢出时，丢弃不完整的事件批次，从下一个同步点继续接收。
        if (event.type == EV_SYN) {
            if (event.code == SYN_DROPPED)
                droppingEvents = true;
            else if (event.code == SYN_REPORT)
                droppingEvents = false;
        }
        if (!droppingEvents && event.type == EV_KEY && event.value == 1) {
            const auto key = mapKey(event.code);
            if (key != Key::None)
                return {key};
        }
    }
}
} // namespace autoaimer
