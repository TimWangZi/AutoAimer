#pragma once
#include <array>

namespace autoaimer {
inline constexpr int outputWidth = 240;
inline constexpr int outputHeight = 240;

/**
 * 方法名称：onScreen
 * 方法作用：判断像素中心是否位于画布内且落在圆形可见区域。
 * 说明：像素中心到圆心的距离：使用两倍坐标，避免运行时开方或浮点判断。
 */
constexpr bool onScreen(int x, int y) {
    const int dx = 2 * x + 1 - outputWidth, dy = 2 * y + 1 - outputHeight;
    return x >= 0 && y >= 0 && x < outputWidth && y < outputHeight &&
           dx * dx + dy * dy <= outputWidth * outputWidth;
}

struct ScreenRow {
    int first, end; // [first,end)，右端不包含。
    /**
     * 方法名称：ScreenRow::size
     * 方法作用：返回当前行的可见像素数量。
     */
    constexpr int size() const { return end - first; }
};

/**
 * 方法名称：makeScreenRows
 * 方法作用：在编译期建立每行可见区间表，供绘图与条带计算共用。
 */
constexpr auto makeScreenRows() {
    std::array<ScreenRow, outputHeight> rows{};
    for (int y = 0; y < outputHeight; ++y) {
        int first = 0;
        while (!onScreen(first, y))
            ++first;
        rows[y] = {first, outputWidth - first};
    }
    return rows;
}
inline constexpr auto screenRows = makeScreenRows();
} // namespace autoaimer
