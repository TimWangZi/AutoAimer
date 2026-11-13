#pragma once
#include "compute/trajectory.h"
#include "ui/screen_geometry.h"
#include <cstdint>
#include <string_view>
#include <vector>

namespace autoaimer {
struct Rgba {
    std::uint8_t r{}, g{}, b{}, a{};
}; // 非预乘透明度；RGB 分量尚未乘以 alpha。

/**
 * 类名称：HudCanvas
 * 类作用：维护透明 UI 图层，提供点、线、圆和点阵文字绘制。
 */
class HudCanvas {
public:
    /**
     * 方法名称：HudCanvas::HudCanvas
     * 方法作用：按指定尺寸创建透明 UI 画布。
     */
    HudCanvas(int width, int height);
    /**
     * 方法名称：HudCanvas::clear
     * 方法作用：清空整张 UI 图层，使全部像素恢复透明。
     */
    void clear();
    /**
     * 方法名称：HudCanvas::point
     * 方法作用：写入一个 UI 像素，忽略画布或圆屏可见范围之外的位置。
     */
    void point(int x, int y, Rgba color);
    /**
     * 方法名称：HudCanvas::line
     * 方法作用：裁剪线段并在 UI 图层绘制可见部分。
     */
    void line(Pixel a, Pixel b, Rgba color);
    /**
     * 方法名称：HudCanvas::circle
     * 方法作用：按指定圆心和半径绘制圆形轮廓。
     */
    void circle(Pixel center, int radius, Rgba color);
    /**
     * 方法名称：HudCanvas::text
     * 方法作用：按整数倍缩放绘制数字和大写字母点阵，其他字符仅保留间距。
     */
    void text(int x, int y, std::string_view text, Rgba color, int scale = 1);
    /**
     * 方法名称：HudCanvas::centeredText
     * 方法作用：计算文字宽度后，在指定高度水平居中绘制。
     */
    void centeredText(int y, std::string_view text, Rgba color, int scale = 1);
    /**
     * 方法名称：HudCanvas::pixels
     * 方法作用：返回 UI 像素数组的只读引用，供图像合成器使用。
     */
    const std::vector<Rgba>& pixels() const { return buffer; }

private:
    int width, height;
    std::vector<Rgba> buffer;
};
} // namespace autoaimer
