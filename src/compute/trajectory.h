#pragma once
#include <array>
#include <cstddef>
#include <optional>

namespace autoaimer {
// 相机坐标约定：x 向右、y 向下、z 向前；位置用米，时间用秒。
struct Vec3 {
    double x{}, y{}, z{};
};
struct Pixel {
    double x{}, y{};
};

enum class DragModel { None, Linear, Quadratic };

struct Motion {
    Vec3 origin{0, 0.04, 0.1};
    Vec3 velocity{0, 0, 20};
    Vec3 gravity{0, 9.81, 0};
    DragModel model = DragModel::Quadratic;
    double drag = 0.08; // 线性模型为 1/s，平方阻力模型为 1/m，无阻力模型忽略此值。
};

struct CameraModel {
    // 内参以最终显示坐标为单位：原 320x240 中心裁成 240x240 后，
    // 焦距不变，主点从 (160,120) 平移至 (120,120)。
    double fx = 200, fy = 200, cx = 120, cy = 120;
    double k1 = 0, k2 = 0, p1 = 0, p2 = 0;
};

struct Trajectory {
    std::array<Vec3, 121> points{};
    std::size_t size = 0;
};

/**
 * 方法名称：predict
 * 方法作用：根据运动参数计算轨迹采样点，写入调用方提供的结果对象。
 * 说明：输出 0～1.2 秒的轨迹，每 10 ms 一个显示采样点；积分子步独立控制精度。
 */
void predict(const Motion& motion, Trajectory& result, int substeps = 5);
/**
 * 方法名称：project
 * 方法作用：将三维点映射为含畸变的显示坐标，无效或过近的点返回空结果。
 */
std::optional<Pixel> project(const CameraModel& camera, Vec3 point);
/**
 * 方法名称：clipNear
 * 方法作用：原地裁剪穿过近面的线段；两端都在近面后方时返回 false。
 */
bool clipNear(Vec3& from, Vec3& to);
} // namespace autoaimer
