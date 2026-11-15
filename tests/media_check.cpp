#include "media/camera.h"
#include "media/video.h"
#include "media/video_geometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <sstream>

namespace {
bool checkLumaCapture() {
    const std::array<std::uint8_t, 12> padded{1, 2, 3, 99, 4, 5, 6, 99, 7, 8, 9, 99};
    const autoaimer::FrameView frame{padded.data(), 3, 2, 4, 3};
    std::ostringstream output(std::ios::binary);
    const std::string expected = "P5\n3 2\n255\n\x01\x02\x03\x04\x05\x06";
    if (!autoaimer::writeLumaPgm(output, frame) || output.str() != expected)
        return false;
    return !autoaimer::writeLumaPgm(output, {});
}

bool checkCameraLease() {
    autoaimer::Camera camera;
    std::string error;
    if (!camera.init("", error))
        return false;
    {
        auto frame = camera.acquire(0);
        if (frame.state() != autoaimer::FrameStatus::Ready ||
            camera.acquire(0).state() != autoaimer::FrameStatus::Error)
            return false;
    }
    return camera.acquire(0).state() == autoaimer::FrameStatus::Ready;
}

bool checkComposition() {
    // 用带行尾填充的 NV12 样本，检查步幅和 UV 起始偏移是否正确。
    constexpr int width = 8, height = 6, stride = 12, storageHeight = 8;
    constexpr int uvOffset = stride * storageHeight;
    std::array<std::uint8_t, uvOffset + stride * storageHeight / 2> nv12{};
    for (int y = 0; y < height; ++y)
        std::fill_n(nv12.data() + y * stride, width, 127);
    for (int y = 0; y < height / 2; ++y) {
        for (int x = 0; x < width; x += 2) {
            nv12[uvOffset + y * stride + x] = 110;
            nv12[uvOffset + y * stride + x + 1] = 149;
        }
    }
    autoaimer::HudCanvas hud(autoaimer::outputWidth, autoaimer::outputHeight);
    hud.point(120, 120, {255, 0, 0, 255});
    autoaimer::VideoComposer video;
    std::string error;
    if (!video.init(error) ||
        !video.compose({nv12.data(), width, height, stride, storageHeight}, hud, error)) {
        std::cerr << error << '\n';
        return false;
    }
    const auto output = video.output();
    // 实色 UI 像素应为红色，其余可见像素应来自 BT.601 背景转换。
    if (output.width != 240 || output.height != 240)
        return false;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 240; ++x) {
            const double dx = x - 119.5, dy = y - 119.5;
            const auto expected = dx * dx + dy * dy > 14400 ? 0
                                  : x == 120 && y == 120    ? 0xf800
                                                            : 0xa3ab;
            if (output.pixels[y * 240 + x] != expected)
                return false;
        }
    }
    // 同一背景遍历全部 alpha，端点快路径和半透明路径都须保持原颜色语义。
    // 上面的 NV12 样本经 BT.601 转换得到未量化的 RGB(163,119,93)。
    hud.clear();
    for (int alpha = 0; alpha < 256; ++alpha)
        hud.point(56 + alpha % 128, 119 + alpha / 128,
                  {17, 203, 71, static_cast<std::uint8_t>(alpha)});
    if (!video.compose({nv12.data(), width, height, stride, storageHeight}, hud, error))
        return false;
    for (int alpha = 0; alpha < 256; ++alpha) {
        const double opacity = alpha / 255.0;
        const int red = std::lround(163 * (1 - opacity) + 17 * opacity);
        const int green = std::lround(119 * (1 - opacity) + 203 * opacity);
        const int blue = std::lround(93 * (1 - opacity) + 71 * opacity);
        const auto expected = (red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3);
        if (video.output().pixels[(119 + alpha / 128) * 240 + 56 + alpha % 128] != expected)
            return false;
    }
    // UI 清除后应恢复背景，不能保留上一帧实色快路径的结果。
    hud.clear();
    if (!video.compose({nv12.data(), width, height, stride, storageHeight}, hud, error) ||
        video.output().pixels[120 * 240 + 183] != 0xa3ab)
        return false;
    return true;
}

bool checkSingleCrop() {
    // 横向与纵向帧都验证：边缘被中心裁掉，不拉伸，也不产生第二次缩放。
    for (bool portrait : {false, true}) {
        const int width = portrait ? 8 : 12, height = portrait ? 12 : 8;
        constexpr int stride = 16, storageHeight = 12;
        std::array<std::uint8_t, stride * storageHeight * 3 / 2> nv12{};
        std::fill(nv12.begin() + stride * storageHeight, nv12.end(), 128);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                nv12[y * stride + x] = 16 + x * 9 + y * 7;
        autoaimer::HudCanvas hud(240, 240);
        autoaimer::VideoComposer video;
        std::string error;
        if (!video.init(error) ||
            !video.compose({nv12.data(), width, height, stride, storageHeight}, hud, error))
            return false;
        struct Probe {
            int x, y, sourceX, sourceY;
        };
        const Probe probes[]{
            {0, 120, 0, 4}, {120, 120, 4, 4}, {239, 120, 7, 4}, {120, 0, 4, 0}, {120, 239, 4, 7}};
        for (const auto point : probes) {
            const int sx = point.sourceX + (portrait ? 0 : 2);
            const int sy = point.sourceY + (portrait ? 2 : 0);
            const int gray = (298 * (sx * 9 + sy * 7) + 128) / 256;
            const auto expected = (gray >> 3) << 11 | (gray >> 2) << 5 | (gray >> 3);
            if (video.output().pixels[point.y * 240 + point.x] != expected)
                return false;
        }
    }
    return true;
}

bool checkAdjustedCrop() {
    // 18×18 居中区域的理论起点是 (11, 5)，NV12 需要向下对齐到 (10, 4)。
    constexpr int width = 40, height = 28, stride = 48;
    std::array<std::uint8_t, stride * height * 3 / 2> nv12{};
    std::fill(nv12.begin(), nv12.begin() + stride * height, 16);
    std::fill(nv12.begin() + stride * height, nv12.end(), 128);
    for (int y = 4; y < 22; ++y)
        std::fill_n(nv12.data() + y * stride + 10, 18, 235);
    const autoaimer::FrameView frame{nv12.data(), width, height, stride, height};
    autoaimer::HudCanvas hud(240, 240);
    autoaimer::VideoComposer video(18);
    std::string error;
    if (!video.init(error) || !video.compose(frame, hud, error))
        return false;
    const auto output = video.output();
    for (const int i : {120, 120 * 240, 120 * 240 + 239, 239 * 240 + 120}) {
        if (output.pixels[i] != 0xffff)
            return false;
    }
    for (const int invalidSize : {-2, 17, 30}) {
        autoaimer::VideoComposer invalid(invalidSize);
        if (!invalid.init(error) || invalid.compose(frame, hud, error))
            return false;
    }
    return true;
}

bool checkDigitalZoom() {
    // 28 像素基础裁剪在 1.5x 时只取居中的 18×18 偶数区域，再直接缩放到屏幕。
    constexpr int width = 40, height = 28, stride = 48;
    std::array<std::uint8_t, stride * height * 3 / 2> nv12{};
    std::fill(nv12.begin() + stride * height, nv12.end(), 128);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            nv12[y * stride + x] = static_cast<std::uint8_t>(16 + x * 3 + y * 2);

    const autoaimer::FrameView frame{nv12.data(), width, height, stride, height};
    autoaimer::HudCanvas hud(240, 240);
    autoaimer::VideoComposer video;
    std::string error;
    if (!video.init(error) || !video.compose(frame, hud, error, 0, 1))
        return false;
    if (std::abs(autoaimer::digitalZoomScale(28, 1) - 28.0 / 18.0) > 1e-12 ||
        std::abs(autoaimer::digitalZoomScale(0, 1) - 1.5) > 1e-12 ||
        std::abs(autoaimer::digitalZoomCoordinate(120, 40, 28, 240, 1) - 400.0 / 3.0) >
            1e-12 ||
        std::abs(autoaimer::digitalZoomCoordinate(120, 28, 28, 240, 1) - 400.0 / 3.0) >
            1e-12)
        return false;

    struct Probe {
        int x, y, sourceX, sourceY;
    };
    // 理论起点 (11, 5) 向下对齐为 (10, 4)；输出坐标只映射这一次裁剪缩放。
    const Probe probes[]{{0, 120, 10, 13},
                         {120, 120, 19, 13},
                         {239, 120, 27, 13},
                         {120, 0, 19, 4},
                         {120, 239, 19, 21}};
    for (const auto point : probes) {
        const int luma = point.sourceX * 3 + point.sourceY * 2;
        const int gray = (298 * luma + 128) / 256;
        const auto expected = (gray >> 3) << 11 | (gray >> 2) << 5 | (gray >> 3);
        if (video.output().pixels[point.y * 240 + point.x] != expected)
            return false;
    }

    // 无效档位应该在读取数组前拒绝，不能改写已合成的画布。
    const auto before = video.output().pixels[120 * 240 + 120];
    if (video.compose(frame, hud, error, 1,
                      static_cast<unsigned>(autoaimer::digitalZooms.size())) ||
        video.output().pixels[120 * 240 + 120] != before)
        return false;
    return true;
}

bool checkDoubleBuffer() {
    autoaimer::Camera camera;
    autoaimer::VideoComposer video;
    autoaimer::HudCanvas hud(240, 240);
    std::string error;
    if (!camera.init("", error) || !video.init(error))
        return false;
    auto frame = camera.acquire(0);
    if (frame.state() != autoaimer::FrameStatus::Ready)
        return false;
    hud.point(120, 120, {255, 0, 0, 255});
    if (!video.compose(frame.view(), hud, error, 0))
        return false;
    const auto first = video.output();
    const std::vector<std::uint16_t> saved(first.pixels, first.pixels + 240 * 240);
    hud.point(120, 120, {0, 255, 0, 255});
    if (!video.compose(frame.view(), hud, error, 1))
        return false;
    const auto second = video.output();
    if (first.pixels == second.pixels || second.pixels[120 * 240 + 120] != 0x07e0 ||
        !std::equal(saved.begin(), saved.end(), first.pixels))
        return false;
    hud.point(120, 120, {0, 0, 255, 255});
    return video.compose(frame.view(), hud, error, 0) && video.output().pixels == first.pixels &&
           first.pixels[120 * 240 + 120] == 0x001f && second.pixels[120 * 240 + 120] == 0x07e0;
}
} // namespace

int main() {
    if (!checkLumaCapture()) {
        std::cerr << "PGM capture did not preserve visible luma rows\n";
        return 1;
    }
    if (!checkCameraLease()) {
        std::cerr << "The camera must release the frame before another acquisition\n";
        return 1;
    }
    if (!checkComposition()) {
        std::cerr << "Unexpected RGB565 output for padded NV12 and HUD fixture\n";
        return 1;
    }
    if (!checkSingleCrop()) {
        std::cerr << "Centered one-pass crop/scale did not preserve source coordinates\n";
        return 1;
    }
    if (!checkAdjustedCrop()) {
        std::cerr << "Adjusted NV12 crop did not align, resize, or reject invalid sizes\n";
        return 1;
    }
    if (!checkDigitalZoom()) {
        std::cerr << "Digital zoom did not use the expected centered one-pass crop\n";
        return 1;
    }
    if (!checkDoubleBuffer()) {
        std::cerr << "Composing one RGB565 buffer changed the other buffer\n";
        return 1;
    }
}
