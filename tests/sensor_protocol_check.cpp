#include "sensors/icm42688.h"
#include "sensors/sdm20.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>

namespace {
int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool near(double actual, double expected, double tolerance = 1e-6) {
    return std::abs(actual - expected) <= tolerance;
}

std::optional<autoaimer::Sdm20Reading> feed(autoaimer::Sdm20Parser& parser,
                                            std::initializer_list<std::uint8_t> bytes) {
    std::optional<autoaimer::Sdm20Reading> result;
    for (const auto byte : bytes) {
        if (const auto reading = parser.feed(byte))
            result = reading;
    }
    return result;
}
} // namespace

int main() {
    using namespace autoaimer;

    Sdm20Parser parser;
    auto distance = feed(parser, {0x00, 0x5c, 0x02, 0x11, 0xec});
    if (!distance || distance->status != Sdm20ReadingStatus::Distance ||
        !near(distance->meters, 4.354))
        return fail("SDM20 did not recover a valid frame after noise");

    // 坏帧末尾同时是下一帧头，解析器必须保留它并继续处理分段输入。
    if (feed(parser, {0x5c, 0x02, 0x11, 0x5c}))
        return fail("SDM20 accepted an invalid checksum");
    distance = feed(parser, {0x32, 0x00, 0xcd});
    if (!distance || distance->status != Sdm20ReadingStatus::Distance ||
        !near(distance->meters, 0.050))
        return fail("SDM20 did not resynchronize after an invalid frame");

    const auto noTarget = feed(parser, {0x5c, 0x20, 0x4e, 0x91});
    if (!noTarget || noTarget->status != Sdm20ReadingStatus::NoTarget)
        return fail("SDM20 20000 mm must be reported as no target");

    const auto tooClose = feed(parser, {0x5c, 0x11, 0x00, 0xee});
    if (!tooClose || tooClose->status != Sdm20ReadingStatus::NoTarget)
        return fail("SDM20 readings below the documented 50 mm range must be rejected");

    if (identifyIcm426xx(0x42) != Icm426xxModel::Icm42605 ||
        identifyIcm426xx(0x47) != Icm426xxModel::Icm42688P ||
        identifyIcm426xx(0x00) != Icm426xxModel::Unknown)
        return fail("ICM-426xx identity mapping is wrong");
    if (std::string(icm426xxModelName(Icm426xxModel::Icm42605)) != "ICM-42605" ||
        std::string(icm426xxModelName(Icm426xxModel::Icm42688P)) != "ICM-42688-P")
        return fail("ICM-426xx model names are wrong");

    Icm426xxMotionFilter filter;
    std::array<std::uint8_t, 12> level{};
    // 当前实机的模块 -X 指向画面下方；静止时传感器 +X 读到 +1 g 比力。
    level[0] = 0x20;
    level[1] = 0x00;
    const auto firstTime = Clock::now();
    auto motion = filter.update(level, firstTime);
    if (motion.gravity.status != SampleStatus::Valid ||
        motion.orientation.status != SampleStatus::Valid ||
        !near(motion.gravity.value.x, 0.0) ||
        !near(motion.gravity.value.y, 9.80665, 1e-5) ||
        !near(motion.gravity.value.z, 0.0) ||
        !near(motion.orientation.value.rollDegrees, 0.0) ||
        !near(motion.orientation.value.pitchDegrees, 0.0))
        return fail("ICM-426xx level sample did not map to camera coordinates");

    // 模块 -X 对应相机向下轴；绕 -X 以 10 °/s 转动 100 ms，应累积约 1°相对航向。
    level[6] = 0xfd;
    level[7] = 0x71;
    motion = filter.update(level, firstTime + std::chrono::milliseconds(100));
    if (!near(motion.orientation.value.yawDegrees, 1.0, 0.01))
        return fail("ICM-426xx gyro conversion or integration scale is wrong");

    filter.reset();
    const std::array<std::uint8_t, 12> invalid{};
    motion = filter.update(invalid, firstTime);
    if (motion.gravity.status != SampleStatus::Error ||
        motion.orientation.status != SampleStatus::Error)
        return fail("ICM-426xx zero-magnitude acceleration must be rejected");
}
