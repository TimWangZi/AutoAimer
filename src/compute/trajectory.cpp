#include "compute/trajectory.h"
#include <cmath>

namespace autoaimer {
namespace {
constexpr double samplePeriod = 0.01;
constexpr double nearPlane = 0.05;

Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 operator-(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 operator*(Vec3 v, double s) {
    return {v.x * s, v.y * s, v.z * s};
}
bool finite(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

struct State {
    Vec3 position, velocity;
    State advance(const State& delta, double step) const {
        return {position + delta.position * step, velocity + delta.velocity * step};
    }
};

State slope(const Motion& motion, State state) {
    const Vec3 v = state.velocity;
    const double speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return {v, motion.gravity - v * (motion.drag * speed)};
}

State stepRk4(const Motion& motion, State state, double h) {
    const auto a = slope(motion, state);
    const auto b = slope(motion, state.advance(a, h / 2));
    const auto c = slope(motion, state.advance(b, h / 2));
    const auto d = slope(motion, state.advance(c, h));
    return state.advance({a.position + (b.position + c.position) * 2 + d.position,
                          a.velocity + (b.velocity + c.velocity) * 2 + d.velocity},
                         h / 6);
}

Vec3 analytic(const Motion& motion, double t) {
    double a = t, b = 0.5 * t * t;
    if (motion.model == DragModel::Linear && motion.drag > 0) {
        const double q = motion.drag * t;
        // q 接近零时直接相减会损失精度，改用短级数展开。
        if (q < 1e-3) {
            a = t * (1 - q / 2 + q * q / 6);
            b = t * t * (0.5 - q / 6 + q * q / 24);
        } else {
            a = -std::expm1(-q) / motion.drag;
            b = (t - a) / motion.drag;
        }
    }
    return motion.origin + motion.velocity * a + motion.gravity * b;
}
} // namespace

void predict(const Motion& motion, Trajectory& result, int substeps) {
    result.size = 0;
    if (!finite(motion.origin) || !finite(motion.velocity) || !finite(motion.gravity) ||
        !std::isfinite(motion.drag) || motion.drag < 0 || substeps < 1)
        return;

    const bool numeric = motion.model == DragModel::Quadratic && motion.drag > 0;
    State state{motion.origin, motion.velocity};
    // 积分步长比显示采样间隔更小，计算精度不依赖绘制点的疏密。
    for (std::size_t i = 0; i < result.points.size(); ++i) {
        const Vec3 point = numeric ? state.position : analytic(motion, i * samplePeriod);
        if (!finite(point)) {
            result.size = 0;
            return;
        }
        result.points[result.size++] = point;
        if (numeric && i + 1 < result.points.size()) {
            for (int step = 0; step < substeps; ++step)
                state = stepRk4(motion, state, samplePeriod / substeps);
        }
    }
}

std::optional<Pixel> project(const CameraModel& c, Vec3 p) {
    if (!finite(p) || p.z < nearPlane)
        return std::nullopt;
    const double x = p.x / p.z, y = p.y / p.z;
    const double r2 = x * x + y * y;
    const double radial = 1 + r2 * (c.k1 + r2 * c.k2);
    // 将理想归一化坐标映射到带畸变的图像坐标，与相机画面保持一致。
    const Pixel uv{c.cx + c.fx * (x * radial + 2 * c.p1 * x * y + c.p2 * (r2 + 2 * x * x)),
                   c.cy + c.fy * (y * radial + c.p1 * (r2 + 2 * y * y) + 2 * c.p2 * x * y)};
    if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
        return std::nullopt;
    return uv;
}

bool clipNear(Vec3& a, Vec3& b) {
    if (a.z < nearPlane && b.z < nearPlane)
        return false;
    if (a.z < nearPlane) {
        a = a + (b - a) * ((nearPlane - a.z) / (b.z - a.z));
        a.z = nearPlane;
    }
    if (b.z < nearPlane) {
        b = b + (a - b) * ((nearPlane - b.z) / (a.z - b.z));
        b.z = nearPlane;
    }
    return true;
}
} // namespace autoaimer
