#include "ui/hud.h"
#include <algorithm>
#include <cmath>

namespace autoaimer {
namespace {
// 5 列 × 7 行点阵：每个字节对应一列，最低位对应最上方像素。
constexpr unsigned char glyphs[][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
    {0, 0x42, 0x7f, 0x40, 0},       // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
    {1, 0x71, 9, 5, 3},             // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {6, 0x49, 0x49, 0x29, 0x1e},    // 9
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // A
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // D
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7f, 9, 9, 9, 1},             // F
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // G
    {0x7f, 8, 8, 8, 0x7f},          // H
    {0, 0x41, 0x7f, 0x41, 0},       // I
    {0x20, 0x40, 0x41, 0x3f, 1},    // J
    {0x7f, 8, 0x14, 0x22, 0x41},    // K
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7f, 2, 0x0c, 2, 0x7f},       // M
    {0x7f, 4, 8, 0x10, 0x7f},       // N
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // O
    {0x7f, 9, 9, 9, 6},             // P
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // Q
    {0x7f, 9, 0x19, 0x29, 0x46},    // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {1, 1, 0x7f, 1, 1},             // T
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // U
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // V
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // W
    {0x63, 0x14, 8, 0x14, 0x63},    // X
    {7, 8, 0x70, 8, 7},             // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}  // Z
};

bool clip(double p, double q, double& first, double& last) {
    if (p == 0)
        return q >= 0;
    const double t = q / p;
    if (p < 0)
        first = std::max(first, t);
    else
        last = std::min(last, t);
    return first <= last;
}
} // namespace

HudCanvas::HudCanvas(int width, int height) : width(width), height(height), buffer(width * height) {
}
void HudCanvas::clear() {
    std::fill(buffer.begin(), buffer.end(), Rgba{});
}
void HudCanvas::point(int x, int y, Rgba color) {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    if (width == outputWidth && height == outputHeight && !onScreen(x, y))
        return;
    buffer[y * width + x] = color;
}

void HudCanvas::line(Pixel a, Pixel b, Rgba color) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    double first = 0, last = 1;
    if (!clip(-dx, a.x, first, last) || !clip(dx, width - 1 - a.x, first, last) ||
        !clip(-dy, a.y, first, last) || !clip(dy, height - 1 - a.y, first, last))
        return;
    const int steps = std::max(
        1, static_cast<int>(std::ceil((last - first) * std::max(std::abs(dx), std::abs(dy)))));
    for (int i = 0; i <= steps; ++i) {
        const double t = first + (last - first) * i / steps;
        point(std::lround(a.x + dx * t), std::lround(a.y + dy * t), color);
    }
}

void HudCanvas::circle(Pixel center, int radius, Rgba color) {
    const int cx = std::lround(center.x), cy = std::lround(center.y);
    int x = radius, y = 0, decision = 1 - radius;
    while (x >= y) {
        point(cx + x, cy + y, color);
        point(cx + y, cy + x, color);
        point(cx - y, cy + x, color);
        point(cx - x, cy + y, color);
        point(cx - x, cy - y, color);
        point(cx - y, cy - x, color);
        point(cx + y, cy - x, color);
        point(cx + x, cy - y, color);
        ++y;
        if (decision < 0)
            decision += 2 * y + 1;
        else {
            --x;
            decision += 2 * (y - x) + 1;
        }
    }
}

void HudCanvas::text(int x, int y, std::string_view text, Rgba color, int scale) {
    for (const char c : text) {
        const auto* glyph = c >= 'A' && c <= 'Z'   ? glyphs[10 + c - 'A']
                            : c >= '0' && c <= '9' ? glyphs[c - '0']
                                                   : nullptr;
        if (glyph) {
            for (int col = 0; col < 5; ++col)
                for (int row = 0; row < 7; ++row)
                    if (glyph[col] & (1u << row))
                        for (int sy = 0; sy < scale; ++sy)
                            for (int sx = 0; sx < scale; ++sx)
                                point(x + col * scale + sx, y + row * scale + sy, color);
        }
        x += 6 * scale;
    }
}

void HudCanvas::centeredText(int y, std::string_view value, Rgba color, int scale) {
    const int textWidth = value.empty() ? 0 : (static_cast<int>(value.size()) * 6 - 1) * scale;
    text((width - textWidth) / 2, y, value, color, scale);
}
} // namespace autoaimer
