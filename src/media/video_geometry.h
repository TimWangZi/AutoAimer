#pragma once
#include <algorithm>
#include <array>

namespace autoaimer {
struct SquareCrop {
    int x, y, size;
};

/**
 * 类名称：VideoGeometry
 * 类作用：记录当前输入尺寸和 1× 画面使用的基础裁剪边长。
 */
struct VideoGeometry {
    int sourceWidth{};
    int sourceHeight{};
    int baseCropSize{};
};

/**
 * 方法名称：makeVideoGeometry
 * 方法作用：根据实际输入尺寸和用户配置生成本轮画面几何信息。
 */
inline VideoGeometry makeVideoGeometry(int width, int height, int configuredCropSize) {
    return {width, height,
            configuredCropSize == 0 ? std::min(width, height) : configuredCropSize};
}

/**
 * 类名称：DigitalZoom
 * 类作用：描述一个离散数字放大倍率及界面标签。
 */
struct DigitalZoom {
    int numerator;
    int denominator;
    const char* label;
};

// 放大仍由一次居中裁剪和一次缩放完成。分数表示最终画面相对 1× 的放大倍率。
inline constexpr std::array<DigitalZoom, 4> digitalZooms{{
    {1, 1, "1X"},
    {3, 2, "1.5X"},
    {2, 1, "2X"},
    {3, 1, "3X"},
}};

/**
 * 方法名称：zoomedCropSize
 * 方法作用：根据基础裁剪边长和离散放大级别计算 NV12 对齐后的实际裁剪边长。
 */
inline int zoomedCropSize(int baseSize, unsigned zoomLevel) {
    const auto zoom = digitalZooms[zoomLevel];
    return (baseSize * zoom.denominator / zoom.numerator) & ~1;
}

/**
 * 方法名称：digitalZoomScale
 * 方法作用：返回 NV12 偶数对齐后的实际放大倍率；尚无输入尺寸时返回档位名义倍率。
 */
inline double digitalZoomScale(int baseSize, unsigned zoomLevel) {
    const auto zoom = digitalZooms[zoomLevel];
    const double nominal = static_cast<double>(zoom.numerator) / zoom.denominator;
    if (baseSize <= 0)
        return nominal;
    const int cropped = zoomedCropSize(baseSize, zoomLevel);
    return cropped > 0 ? static_cast<double>(baseSize) / cropped : nominal;
}

/**
 * 方法名称：digitalZoomCoordinate
 * 方法作用：把 1× 显示坐标换算到实际缩放裁剪后的显示坐标，并包含偶数起点对齐。
 */
inline double digitalZoomCoordinate(double coordinate, int sourceExtent, int baseSize,
                                    int targetExtent, unsigned zoomLevel) {
    const int cropped = zoomedCropSize(baseSize, zoomLevel);
    if (sourceExtent <= 0 || baseSize <= 0 || targetExtent <= 0 || cropped <= 0)
        return coordinate;
    const int baseStart = ((sourceExtent - baseSize) / 2) & ~1;
    const int zoomStart = ((sourceExtent - cropped) / 2) & ~1;
    const double sourceCoordinate =
        baseStart + coordinate * static_cast<double>(baseSize) / targetExtent;
    return (sourceCoordinate - zoomStart) * targetExtent / cropped;
}

/**
 * 方法名称：centeredSquare
 * 方法作用：计算居中正方形裁剪区域，起点按 NV12 的偶数像素要求对齐。
 * 说明：边长由调用方验证。CPU 与 RGA 使用同一个裁剪区域，保持比例并填满圆屏。
 */
inline SquareCrop centeredSquare(int width, int height, int size) {
    return {((width - size) / 2) & ~1, ((height - size) / 2) & ~1, size};
}
} // namespace autoaimer
