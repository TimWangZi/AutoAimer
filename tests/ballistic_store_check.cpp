#include "config/ballistic_store.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
bool same(double first, double second) {
    return std::abs(first - second) < 1e-12;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    const std::filesystem::path path(argv[1]);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path.string() + ".part", ignored);

    std::array<autoaimer::BallisticPreset, autoaimer::ballisticPresetCount> presets{};
    std::size_t active = 0;
    std::string error;
    if (autoaimer::loadBallisticPresets(path.string(), presets, active, error) !=
        autoaimer::BallisticLoadStatus::Missing) {
        std::cerr << "Missing preset file must preserve defaults\n";
        return 1;
    }

    presets[0] = {15.5, 1.5, 3.0, -1.0, 1.25, 0.04, false};
    presets[1] = {22.0, -4.0, -2.5, 2.0, -0.5, 0.12, true};
    presets[2] = {30.0, 0.5, 6.0, 3.0, 2.0, 0.2, false};
    if (!autoaimer::saveBallisticPresets(path.string(), presets, 2, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::array<autoaimer::BallisticPreset, autoaimer::ballisticPresetCount> loaded{};
    if (autoaimer::loadBallisticPresets(path.string(), loaded, active, error) !=
            autoaimer::BallisticLoadStatus::Loaded ||
        active != 2 || !same(loaded[0].muzzleSpeedMetersPerSecond, 15.5) ||
        !same(loaded[1].horizontalOffsetCentimeters, -4.0) ||
        !same(loaded[1].verticalOffsetCentimeters, -2.5) ||
        !same(loaded[2].horizontalAngleDegrees, 3.0) ||
        !same(loaded[2].verticalAngleDegrees, 2.0) ||
        !same(loaded[1].dragCoefficient, 0.12) ||
        loaded[0].quadraticDrag || !loaded[1].quadraticDrag) {
        std::cerr << "Preset round trip changed values\n";
        return 1;
    }

    // v1 只有竖直偏移和竖直角；加载时迁移到 v2 结构，新增水平项归零。
    {
        std::ofstream legacy(path, std::ios::trunc);
        legacy << "format=autoaimer-ballistics-v1\nactive=1\n";
        for (std::size_t index = 0; index < autoaimer::ballisticPresetCount; ++index) {
            const std::string prefix = "p" + std::to_string(index + 1) + ".";
            legacy << prefix << "speed=20\n"
                   << prefix << "offset_cm=" << index + 1 << "\n"
                   << prefix << "angle_deg=" << -static_cast<int>(index) << "\n"
                   << prefix << "drag=0.08\n"
                   << prefix << "quadratic=1\n";
        }
    }
    if (autoaimer::loadBallisticPresets(path.string(), loaded, active, error) !=
            autoaimer::BallisticLoadStatus::Loaded ||
        active != 1 || !same(loaded[2].horizontalOffsetCentimeters, 0) ||
        !same(loaded[2].verticalOffsetCentimeters, 3) ||
        !same(loaded[2].horizontalAngleDegrees, 0) ||
        !same(loaded[2].verticalAngleDegrees, -2)) {
        std::cerr << "Legacy v1 preset migration changed vertical values or added horizontal bias\n";
        return 1;
    }

    // 第二次保存覆盖旧文件，验证临时文件重命名更新的是完整配置。
    loaded[2].muzzleSpeedMetersPerSecond = 31.5;
    if (!autoaimer::saveBallisticPresets(path.string(), loaded, 1, error) ||
        autoaimer::loadBallisticPresets(path.string(), presets, active, error) !=
            autoaimer::BallisticLoadStatus::Loaded ||
        active != 1 || !same(presets[2].muzzleSpeedMetersPerSecond, 31.5)) {
        std::cerr << (error.empty() ? "Atomic replacement did not persist the second save" : error)
                  << '\n';
        return 1;
    }

    std::ofstream(path, std::ios::trunc) << "format=broken\n";
    if (autoaimer::loadBallisticPresets(path.string(), presets, active, error) !=
        autoaimer::BallisticLoadStatus::Error) {
        std::cerr << "Malformed preset file must be reported\n";
        return 1;
    }
    std::filesystem::remove(path, ignored);
}
