#pragma once

namespace autoaimer {
struct SquareCrop {
    int x, y, size;
};

/**
 * 方法名称：centeredSquare
 * 方法作用：计算居中正方形裁剪区域，起点按 NV12 的偶数像素要求对齐。
 * 说明：边长由调用方验证。CPU 与 RGA 使用同一个裁剪区域，保持比例并填满圆屏。
 */
inline SquareCrop centeredSquare(int width, int height, int size) {
    return {((width - size) / 2) & ~1, ((height - size) / 2) & ~1, size};
}
} // namespace autoaimer
