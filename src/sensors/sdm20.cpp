#include "sensors/sdm20.h"
#include <algorithm>

namespace autoaimer {
namespace {
constexpr std::uint8_t frameHeader = 0x5c;
constexpr std::uint16_t minimumDistanceMillimeters = 50;
constexpr std::uint16_t noTargetMillimeters = 20000;
}

std::optional<Sdm20Reading> Sdm20Parser::feed(std::uint8_t byte) {
    if (size == 0 && byte != frameHeader)
        return std::nullopt;

    frame[size++] = byte;
    if (size != frame.size())
        return std::nullopt;

    const auto sum = static_cast<std::uint8_t>(frame[1] + frame[2]);
    const auto expected = static_cast<std::uint8_t>(~sum);
    if (frame[3] != expected) {
        // 坏帧中若已经出现下一帧头，保留它及其后续字节继续解析。
        const auto next = std::find(frame.begin() + 1, frame.end(), frameHeader);
        if (next == frame.end()) {
            size = 0;
        } else {
            const auto remaining = static_cast<std::size_t>(frame.end() - next);
            std::copy(next, frame.end(), frame.begin());
            size = remaining;
        }
        return std::nullopt;
    }

    const auto millimeters = static_cast<std::uint16_t>(frame[1]) |
                              (static_cast<std::uint16_t>(frame[2]) << 8);
    size = 0;
    if (millimeters < minimumDistanceMillimeters || millimeters >= noTargetMillimeters)
        return Sdm20Reading{Sdm20ReadingStatus::NoTarget, 0.0};
    return Sdm20Reading{Sdm20ReadingStatus::Distance, millimeters / 1000.0};
}

void Sdm20Parser::reset() {
    frame = {};
    size = 0;
}
} // namespace autoaimer
