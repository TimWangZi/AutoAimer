#pragma once
// 只复制到诊断构建中：每帧记录在内存，退出后统一输出，避免打印干扰帧率。
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <time.h>
#include <vector>

namespace autoaimer::profile {
enum Stage {
    Frame,
    Cpu,
    Acquire,
    Update,
    Clear,
    Render,
    Compose,
    Present,
    Navigate,
    Pack,
    Gpio,
    SpiSmall,
    SpiPixels,
    Count
};
inline constexpr const char* names[]{"frame",
                                     "process_cpu",
                                     "camera_acquire",
                                     "input_update",
                                     "hud_clear",
                                     "hud_render",
                                     "compose",
                                     "present",
                                     "navigation",
                                     "pixel_pack",
                                     "gpio_ioctl",
                                     "spi_command_address",
                                     "spi_pixel_payload"};
inline bool active = false;
inline std::array<double, Count> current{};
inline std::array<std::uint64_t, Count> calls{}, frameCalls{};
inline std::vector<std::array<double, Count>> samples;
inline std::uint64_t frameStart = 0, cpuStart = 0;

inline std::uint64_t now(clockid_t clock = CLOCK_MONOTONIC) {
    timespec value{};
    clock_gettime(clock, &value);
    return std::uint64_t(value.tv_sec) * 1000000000 + value.tv_nsec;
}
inline void elapsed(Stage stage, std::uint64_t start) {
    if (active) {
        const auto end = now();
        current[stage] += end - start;
        ++frameCalls[stage];
    }
}
struct Scope {
    Stage stage;
    std::uint64_t start;
    explicit Scope(Stage stage) : stage(stage), start(active ? now() : 0) {}
    ~Scope() { elapsed(stage, start); }
};
inline void begin(unsigned frame) {
    active = frame >= 30; // 初始化及前 30 帧预热不计入统计。
    if (active) {
        if (samples.empty())
            samples.reserve(600);
        current.fill(0);
        frameCalls.fill(0);
        cpuStart = now(CLOCK_PROCESS_CPUTIME_ID);
        frameStart = now();
    }
}
inline void end() {
    if (!active)
        return;
    current[Frame] = now() - frameStart;
    current[Cpu] = now(CLOCK_PROCESS_CPUTIME_ID) - cpuStart;
    frameCalls[Frame] = frameCalls[Cpu] = 1;
    for (int i = 0; i < Count; ++i)
        calls[i] += frameCalls[i];
    samples.push_back(current);
    active = false;
}
inline void dump() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "PROFILE detail=" << AA_PROFILE_DETAIL << " frames=" << samples.size()
              << " warmup=30 clock=MONOTONIC units=us_per_frame\n";
    double frameSum = 0;
    for (int i = 0; i < Count && !samples.empty(); ++i) {
        if (!calls[i])
            continue;
        std::vector<double> values;
        double sum = 0;
        for (const auto& sample : samples) {
            values.push_back(sample[i] / 1000);
            sum += values.back();
        }
        std::sort(values.begin(), values.end());
        std::cout << "stage=" << names[i] << " mean_us=" << sum / values.size()
                  << " median_us=" << values[values.size() / 2]
                  << " p95_us=" << values[(values.size() * 95 + 99) / 100 - 1]
                  << " max_us=" << values.back()
                  << " calls_per_frame=" << double(calls[i]) / values.size() << '\n';
        if (i == Frame)
            frameSum = sum;
    }
    if (frameSum)
        std::cout << "PROFILE fps=" << samples.size() * 1000000 / frameSum << '\n';
    const auto start = now();
    for (int i = 0; i < 10000; ++i)
        (void)now();
    std::cout << "clock_call_average_ns=" << double(now() - start) / 10000 << '\n';
}
} // namespace autoaimer::profile
