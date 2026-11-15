#include "compute/ballistic_preset.h"
#include "compute/trajectory.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void near(double actual, double expected, double tolerance, const char* name) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        std::cerr << name << ": " << actual << " vs " << expected << '\n';
        std::exit(1);
    }
}
} // namespace

int main() {
    autoaimer::BallisticPreset preset;
    preset.muzzleSpeedMetersPerSecond = 30.0;
    preset.horizontalOffsetCentimeters = -3.0;
    preset.verticalOffsetCentimeters = 5.0;
    preset.horizontalAngleDegrees = 20.0;
    preset.verticalAngleDegrees = 10.0;
    preset.dragCoefficient = 0.2;
    preset.quadraticDrag = false;
    const auto fromPreset = autoaimer::makeMotion(preset, {1.0, 2.0, 3.0});
    constexpr double pitch = 10.0 * 3.14159265358979323846 / 180.0;
    constexpr double yaw = 20.0 * 3.14159265358979323846 / 180.0;
    near(fromPreset.origin.x, -0.03, 1e-12, "preset horizontal offset");
    near(fromPreset.origin.y, 0.05, 1e-12, "preset vertical offset");
    near(fromPreset.velocity.x, 30.0 * std::cos(pitch) * std::sin(yaw), 1e-12,
         "preset horizontal angle x");
    near(fromPreset.velocity.y, -30.0 * std::sin(pitch), 1e-12,
         "preset vertical angle y");
    near(fromPreset.velocity.z, 30.0 * std::cos(pitch) * std::cos(yaw), 1e-12,
         "preset angles z");
    near(fromPreset.drag, 0.2, 0, "preset drag");
    if (fromPreset.model != autoaimer::DragModel::Linear || fromPreset.gravity.x != 1.0 ||
        fromPreset.gravity.y != 2.0 || fromPreset.gravity.z != 3.0) {
        std::cerr << "Preset did not select linear drag or preserve gravity\n";
        return 1;
    }
    preset.quadraticDrag = true;
    if (autoaimer::makeMotion(preset).model != autoaimer::DragModel::Quadratic) {
        std::cerr << "Preset did not select quadratic drag\n";
        return 1;
    }

    autoaimer::Trajectory result, refined;
    autoaimer::Motion motion;
    const double t = 1.2;
    motion.model = autoaimer::DragModel::None;
    autoaimer::predict(motion, result);
    near(result.size, 121, 0, "sample count");
    near(result.points.back().y, 0.04 + 0.5 * 9.81 * t * t, 1e-12, "gravity drop");
    near(result.points.back().z, 0.1 + 20 * t, 1e-12, "no drag distance");

    motion.model = autoaimer::DragModel::Linear;
    motion.drag = 0.5;
    autoaimer::predict(motion, result);
    const double a = (1 - std::exp(-motion.drag * t)) / motion.drag;
    near(result.points.back().z, 0.1 + 20 * a, 1e-12, "linear drag distance");
    near(result.points.back().y, 0.04 + 9.81 * (t - a) / motion.drag, 1e-12, "linear gravity drop");
    motion.drag = 1e-12;
    autoaimer::predict(motion, result);
    near(result.points.back().y, 0.04 + 0.5 * 9.81 * t * t, 1e-10, "linear zero limit");

    motion.model = autoaimer::DragModel::Quadratic;
    motion.drag = 0.08;
    motion.gravity = {};
    autoaimer::predict(motion, result);
    near(result.points.back().z, 0.1 + std::log1p(0.08 * 20 * t) / 0.08, 1e-9,
         "quadratic closed form");

    motion.gravity = {0, 9.81, 0};
    autoaimer::predict(motion, result, 5);
    autoaimer::predict(motion, refined, 10);
    near(result.points.back().y, refined.points.back().y, 1e-8, "RK4 step halving");
    near(result.points.back().z, refined.points.back().z, 1e-8, "RK4 step halving");

    autoaimer::Vec3 from{0, 0, -0.1}, to{0, 0, 1};
    if (autoaimer::project({}, from) || !autoaimer::clipNear(from, to))
        return 1;
    const auto center = autoaimer::project({}, from);
    if (!center)
        return 1;
    near(center->x, 120, 0, "near plane projection");
    autoaimer::CameraModel lens;
    lens.k1 = 0.1;
    const auto warped = autoaimer::project(lens, {0.5, 0, 1});
    if (!warped)
        return 1;
    near(warped->x, 222.5, 1e-12, "radial distortion");
    lens = {};
    lens.k3 = 0.1;
    const auto warpedK3 = autoaimer::project(lens, {0.5, 0, 1});
    if (!warpedK3)
        return 1;
    near(warpedK3->x, 220.15625, 1e-12, "third radial distortion coefficient");
    std::cout << "Preset conversion, analytic baselines, RK4 convergence, clipping and distortion passed\n";
}
