#include "media/camera.h"
#include <vector>

namespace autoaimer {
namespace {
constexpr int width = 640;
constexpr int height = 480;
constexpr int stride = 672; // 两个平面均保留行尾填充，用来模拟 VI 的实际步幅。
} // namespace

struct Camera::State {
    std::vector<std::uint8_t> nv12 = std::vector<std::uint8_t>(stride * height * 3 / 2);
    std::uint64_t frame = 0;
    bool leased = false;
};

Camera::Camera() = default;
Camera::~Camera() = default;

bool Camera::init(const std::string&, std::string&) {
    state = std::make_unique<State>();
    return true;
}

FrameLease Camera::acquire(int) {
    if (!state || state->leased)
        return FrameLease(FrameStatus::Error);
    const unsigned shift = state->frame++ % 80;
    auto* y = state->nv12.data();
    auto* uv = y + stride * height;
    for (int row = 0; row < height; ++row) {
        for (int x = 0; x < width; ++x)
            y[row * stride + x] = 48 + ((x + shift) / 40 + row / 40) % 2 * 55 + row / 6;
    }
    for (int row = 0; row < height / 2; ++row) {
        for (int x = 0; x < width; x += 2) {
            uv[row * stride + x] = x < width / 2 ? 145 : 110;
            uv[row * stride + x + 1] = x < width / 2 ? 113 : 149;
        }
    }
    state->leased = true;
    return FrameLease({y, width, height, stride, height, -1, state->frame * 33333}, state.get(),
                      [](void* p) { static_cast<State*>(p)->leased = false; });
}
} // namespace autoaimer
