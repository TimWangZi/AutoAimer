#include "ui/menus/view_menu.h"
#include "ui/level_indicator.h"
#include "ui/menus/settings_menu.h"
#include <chrono>
#include <cmath>
#include <string>

namespace autoaimer {
namespace {
constexpr Rgba white{255, 255, 255, 255};
constexpr Rgba green{80, 255, 125, 255};
constexpr Rgba amber{255, 210, 80, 255};
constexpr Pixel viewCenter{outputWidth / 2, outputHeight / 2};

template <class T> bool fresh(const Sample<T>& sample, TimePoint now) {
    const auto age = now - sample.sampledAt;
    return sample.status == SampleStatus::Valid && age >= Clock::duration::zero() &&
           age <= std::chrono::milliseconds(200);
}

void drawReticle(HudCanvas& hud, Pixel center, int ring, int inner, int outer, Rgba color) {
    hud.circle(center, ring, color);
    hud.line({center.x - outer, center.y}, {center.x - inner, center.y}, color);
    hud.line({center.x + inner, center.y}, {center.x + outer, center.y}, color);
    hud.line({center.x, center.y - outer}, {center.x, center.y - inner}, color);
    hud.line({center.x, center.y + inner}, {center.x, center.y + outer}, color);
}

std::optional<Vec3> pointAtDepth(const Trajectory& trajectory, double depth) {
    for (std::size_t i = 1; i < trajectory.size; ++i) {
        const auto a = trajectory.points[i - 1];
        const auto b = trajectory.points[i];
        if ((depth - a.z) * (depth - b.z) > 0 || a.z == b.z)
            continue;
        const double mix = (depth - a.z) / (b.z - a.z);
        return Vec3{a.x + (b.x - a.x) * mix, a.y + (b.y - a.y) * mix, depth};
    }
    return std::nullopt;
}
} // namespace

MenuAction ViewMenu::update(Input input, MenuContext context) {
    camera = context.config.camera;
    if (fresh(context.sensors.gravity, context.now)) {
        auto motion = context.config.motion;
        motion.gravity = context.sensors.gravity.value;
        predict(motion, trajectory);
    } else {
        trajectory.size = 0;
    }

    orientation = fresh(context.sensors.orientation, context.now)
                      ? std::optional<Orientation>(context.sensors.orientation.value)
                      : std::nullopt;
    const auto& distance = context.sensors.distanceMeters;
    distanceMeters = distance.status == SampleStatus::Valid && std::isfinite(distance.value) &&
                             distance.value > 0
                         ? std::optional<double>(distance.value)
                         : std::nullopt;

    if (input.key == Key::Confirm)
        return {MenuAction::Stay, {}, MenuCommand::MeasureDistance};
    if (input.key == Key::Right)
        return {MenuAction::Push, std::make_unique<SettingsMenu>()};
    return {};
}

void ViewMenu::render(HudCanvas& hud) const {
    hud.centeredText(20, "DIST", white);
    if (distanceMeters) {
        const auto centimeters = std::lround(*distanceMeters * 100);
        hud.centeredText(34, std::to_string(centimeters) + " CM", white, 2);
    } else {
        hud.centeredText(34, "PRESS OK", amber, 2);
    }

    // 先画曲线，再画两个准星，避免曲线盖住准星。
    for (std::size_t i = 1; i < trajectory.size; ++i) {
        auto a = trajectory.points[i - 1], b = trajectory.points[i];
        if (!clipNear(a, b))
            continue;
        const auto start = project(camera, a), end = project(camera, b);
        if (start && end)
            hud.line(*start, *end, green);
    }

    if (distanceMeters) {
        const auto impact = pointAtDepth(trajectory, *distanceMeters);
        const auto pixel = impact ? project(camera, *impact) : std::nullopt;
        if (pixel)
            drawReticle(hud, *pixel, 3, 4, 8, amber);
    }

    drawReticle(hud, viewCenter, 8, 11, 21, white);
    drawLevelIndicator(hud, orientation);
    hud.centeredText(210, "OK RANGE", white);
    hud.centeredText(224, "RIGHT MENU", white);
}

std::unique_ptr<Menu> makeViewMenu() {
    return std::make_unique<ViewMenu>();
}
} // namespace autoaimer
