#include "media/video.h"
#include "media/video_geometry.h"
#ifdef AUTOAIMER_BOARD
#include "media/video_rga.h"
#endif
#include <algorithm>
#include <fstream>

namespace autoaimer {
namespace {
struct Rgb {
    int r, g, b;
};

#ifndef AUTOAIMER_BOARD
Rgb sampleNv12(const FrameView& frame, int x, int y) {
    const auto* uv = frame.data + frame.yStride * frame.storageHeight;
    const int chroma = (y / 2) * frame.yStride + (x & ~1);
    const int luma = std::max(0, int(frame.data[y * frame.yStride + x]) - 16);
    const int u = uv[chroma] - 128, v = uv[chroma + 1] - 128;
    // 按 BT.601 有限范围转换，与 RGA 路径的目标色彩空间一致。
    return {std::clamp((298 * luma + 409 * v + 128) / 256, 0, 255),
            std::clamp((298 * luma - 100 * u - 208 * v + 128) / 256, 0, 255),
            std::clamp((298 * luma + 516 * u + 128) / 256, 0, 255)};
}
#endif

std::uint16_t rgb565(Rgb rgb) {
    return (rgb.r >> 3) << 11 | (rgb.g >> 2) << 5 | (rgb.b >> 3);
}

// 仅半透明像素需要混合；保留原来的四舍五入，保证输出逐像素一致。
std::uint16_t blend(Rgb rgb, Rgba overlay) {
    const auto mix = [alpha = overlay.a](int base, int foreground) {
        return (base * (255 - alpha) + foreground * alpha + 127) / 255;
    };
    return (mix(rgb.r, overlay.r) >> 3) << 11 | (mix(rgb.g, overlay.g) >> 2) << 5 |
           (mix(rgb.b, overlay.b) >> 3);
}
} // namespace

VideoComposer::VideoComposer()
    : buffers{std::vector<std::uint16_t>(outputWidth * outputHeight),
              std::vector<std::uint16_t>(outputWidth * outputHeight)} {
}
VideoComposer::~VideoComposer() = default;

bool VideoComposer::init(std::string& error) {
#ifdef AUTOAIMER_BOARD
    rga = std::make_unique<RgaVideo>();
    return rga->init(error);
#else
    (void)error;
    return true;
#endif
}

bool VideoComposer::compose(const FrameView& frame, const HudCanvas& hud, std::string& error,
                            unsigned buffer) {
    if (frame.width <= 0 || frame.height <= 0 || frame.width % 2 || frame.height % 2 ||
        frame.yStride < frame.width || frame.storageHeight < frame.height) {
        error = "Unsupported NV12 dimensions or stride";
        return false;
    }
#ifdef AUTOAIMER_BOARD
    const auto* rgb = rga->convert(frame, error);
    if (!rgb)
        return false;
#else
    if (!frame.data) {
        error = "Missing NV12 frame";
        return false;
    }
    const auto crop = centeredSquare(frame.width, frame.height);
#endif
    auto& pixels = buffers[buffer];
    // 直接在选定画布上采样和混合 HUD；圆外保持初始化的黑色，免去转换。
    for (int y = 0; y < outputHeight; ++y) {
        const auto row = screenRows[y];
        for (int x = row.first; x < row.end; ++x) {
            const int i = y * outputWidth + x;
            const auto overlay = hud.pixels()[i];
            // 实色 UI 完全覆盖背景，连 NV12 采样/转换也可以跳过。
            if (overlay.a == 255) {
                pixels[i] = rgb565({overlay.r, overlay.g, overlay.b});
                continue;
            }
#ifdef AUTOAIMER_BOARD
            const Rgb color{rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]};
#else
            const Rgb color = sampleNv12(frame, crop.x + x * crop.size / outputWidth,
                                         crop.y + y * crop.size / outputHeight);
#endif
            // 透明像素直接输出背景，避免对绝大多数像素执行三通道混合。
            pixels[i] = overlay.a == 0 ? rgb565(color) : blend(color, overlay);
        }
    }
    currentBuffer = buffer;
    return true;
}

bool writePpm(const std::string& path, OutputFrame frame) {
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (int i = 0; i < frame.width * frame.height; ++i) {
        const auto rgb = frame.pixels[i];
        const unsigned char pixel[]{static_cast<unsigned char>((rgb >> 11) * 255 / 31),
                                    static_cast<unsigned char>(((rgb >> 5) & 63) * 255 / 63),
                                    static_cast<unsigned char>((rgb & 31) * 255 / 31)};
        out.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
    }
    out.close();
    return bool(out);
}
} // namespace autoaimer
