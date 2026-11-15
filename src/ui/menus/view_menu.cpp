#include "ui/menus/view_menu.h"
#include "compute/camera_calibration.h"
#include "media/video_geometry.h"
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

bool ViewMenu::trajectoryMatches(const BallisticPreset& preset, Vec3 gravity) const {
    // 预设来自离散步进，可直接比较；重力容许约 0.005 m/s² 的采样抖动，
    // 避免静止时每帧重复执行 121 点 RK4。
    constexpr double gravityTolerance = 0.005;
    return trajectoryReady &&
           preset.muzzleSpeedMetersPerSecond == trajectoryPreset.muzzleSpeedMetersPerSecond &&
           preset.horizontalOffsetCentimeters == trajectoryPreset.horizontalOffsetCentimeters &&
           preset.verticalOffsetCentimeters == trajectoryPreset.verticalOffsetCentimeters &&
           preset.horizontalAngleDegrees == trajectoryPreset.horizontalAngleDegrees &&
           preset.verticalAngleDegrees == trajectoryPreset.verticalAngleDegrees &&
           preset.dragCoefficient == trajectoryPreset.dragCoefficient &&
           preset.quadraticDrag == trajectoryPreset.quadraticDrag &&
           std::abs(gravity.x - trajectoryGravity.x) < gravityTolerance &&
           std::abs(gravity.y - trajectoryGravity.y) < gravityTolerance &&
           std::abs(gravity.z - trajectoryGravity.z) < gravityTolerance;
}

MenuAction ViewMenu::update(Input input, MenuContext context) {
    if (input.key == Key::Up && context.config.zoomLevel + 1 < digitalZooms.size())
        ++context.config.zoomLevel;
    if (input.key == Key::Down && context.config.zoomLevel > 0)
        --context.config.zoomLevel;

    zoomLevel = context.config.zoomLevel;
    // 原始镜头标定只参与轨迹投影，相机图像仍由 VideoComposer 直接裁剪缩放。
    camera = cameraForProjection(context.config.cameraCalibration, context.config.video, zoomLevel,
                                 outputWidth, outputHeight);
    activePreset = context.config.activeBallisticPreset;
    Vec3 gravity{0.0, 9.81, 0.0};
    if (fresh(context.sensors.gravity, context.now)) {
        gravity = context.sensors.gravity.value;
    }
    const auto& preset = context.config.activePreset();
    // IMU 尚未接入时仍用水平放置的标准重力绘制归零轨迹。
    // 只有预设或有效重力发生可见变化时才重新积分，普通视频帧复用缓存。
    if (!trajectoryMatches(preset, gravity)) {
        predict(makeMotion(preset, gravity), trajectory);
        trajectoryPreset = preset;
        trajectoryGravity = gravity;
        trajectoryReady = true;
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
    hud.text(184, 58, digitalZooms[zoomLevel].label, white);
    hud.text(184, 70, "P" + std::to_string(activePreset + 1), amber);
    hud.centeredText(210, "OK RANGE", white);
    hud.centeredText(224, "UD ZOOM RT MENU", white);
}

std::unique_ptr<Menu> makeViewMenu() {
    return std::make_unique<ViewMenu>();
}
} // namespace autoaimer
