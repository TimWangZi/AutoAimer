#include "media/video_geometry.h"
#include "ui/menu.h"
#include "ui/menus/ballistic_menu.h"
#include "ui/menus/ballistic_parameter_menu.h"
#include "ui/menus/calibration_menu.h"
#include <cmath>
#include <iostream>

int main() {
    const auto now = autoaimer::Clock::now();
    const autoaimer::SensorData sensors;
    autoaimer::Config config;
    autoaimer::MenuStack menus(autoaimer::makeViewMenu());
    autoaimer::Menu& root = menus.top();
    if (menus.apply({autoaimer::MenuAction::Pop, {}, autoaimer::MenuCommand::None})) {
        std::cerr << "The root menu must remain on the stack\n";
        return 1;
    }

    // 主界面的确认键只触发测距，不应切换页面。
    const auto ranging = menus.top().update({autoaimer::Key::Confirm}, {config, sensors, now});
    if (ranging.kind != autoaimer::MenuAction::Stay ||
        ranging.command != autoaimer::MenuCommand::MeasureDistance) {
        std::cerr << "Confirm on the main view must request one distance sample\n";
        return 1;
    }
    autoaimer::HudCanvas hud(autoaimer::outputWidth, autoaimer::outputHeight);
    root.render(hud);
    std::size_t trajectoryPixels = 0;
    for (const auto pixel : hud.pixels())
        if (pixel.r == 80 && pixel.g == 255 && pixel.b == 125 && pixel.a == 255)
            ++trajectoryPixels;
    if (trajectoryPixels == 0) {
        std::cerr << "Main view must draw the active preset with fallback gravity when IMU is absent\n";
        return 1;
    }

    const auto press = [&](autoaimer::Key key) {
        return menus.apply(menus.top().update({key}, {config, sensors, now}));
    };

    // 主界面上下键在固定档位间放大，两端必须停在有效范围。
    for (unsigned expected = 1; expected < autoaimer::digitalZooms.size(); ++expected) {
        if (!press(autoaimer::Key::Up) || config.zoomLevel != expected) {
            std::cerr << "Up did not select the next digital zoom level\n";
            return 1;
        }
    }
    if (!press(autoaimer::Key::Up) || config.zoomLevel + 1 != autoaimer::digitalZooms.size()) {
        std::cerr << "Digital zoom exceeded its upper limit\n";
        return 1;
    }
    while (config.zoomLevel > 0) {
        const unsigned expected = config.zoomLevel - 1;
        if (!press(autoaimer::Key::Down) || config.zoomLevel != expected) {
            std::cerr << "Down did not select the previous digital zoom level\n";
            return 1;
        }
    }
    if (!press(autoaimer::Key::Down) || config.zoomLevel != 0) {
        std::cerr << "Digital zoom dropped below 1x\n";
        return 1;
    }

    if (!press(autoaimer::Key::Right) || &menus.top() == &root) {
        std::cerr << "Right did not open settings\n";
        return 1;
    }
    autoaimer::Menu& settings = menus.top();
    using autoaimer::Key;
    if (!press(Key::Confirm) || !dynamic_cast<autoaimer::BallisticMenu*>(&menus.top())) {
        std::cerr << "Confirm did not open ballistic presets\n";
        return 1;
    }
    autoaimer::Menu& ballistics = menus.top();
    const auto originalPreset = config.ballisticPresets[1];
    if (!press(Key::Down) || !press(Key::Confirm) || config.activeBallisticPreset != 1 ||
        !press(Key::Right) ||
        !dynamic_cast<autoaimer::BallisticParameterMenu*>(&menus.top()) ||
        !press(Key::Right) || !press(Key::Down) || !press(Key::Right) || !press(Key::Down) ||
        !press(Key::Right) || !press(Key::Down) || !press(Key::Right) || !press(Key::Down) ||
        !press(Key::Right) || !press(Key::Down) || !press(Key::Right) || !press(Key::Down) ||
        !press(Key::Right) || !press(Key::Confirm) ||
        &menus.top() != &ballistics ||
        config.activeBallisticPreset != 1 ||
        std::abs(config.ballisticPresets[1].muzzleSpeedMetersPerSecond -
                 (originalPreset.muzzleSpeedMetersPerSecond + 0.5)) > 1e-12 ||
        std::abs(config.ballisticPresets[1].horizontalOffsetCentimeters -
                 (originalPreset.horizontalOffsetCentimeters + 0.5)) > 1e-12 ||
        std::abs(config.ballisticPresets[1].verticalOffsetCentimeters -
                 (originalPreset.verticalOffsetCentimeters + 0.5)) > 1e-12 ||
        std::abs(config.ballisticPresets[1].horizontalAngleDegrees -
                 (originalPreset.horizontalAngleDegrees + 0.25)) > 1e-12 ||
        std::abs(config.ballisticPresets[1].verticalAngleDegrees -
                 (originalPreset.verticalAngleDegrees + 0.25)) > 1e-12 ||
        std::abs(config.ballisticPresets[1].dragCoefficient -
                 (originalPreset.dragCoefficient + 0.005)) > 1e-12 ||
        config.ballisticPresets[1].quadraticDrag == originalPreset.quadraticDrag ||
        config.ballisticRevision != 2) {
        std::cerr << "P2 selection or parameter editing did not save and activate the preset\n";
        return 1;
    }
    const auto savedPreset = config.ballisticPresets[1];
    if (!press(Key::Right) ||
        !dynamic_cast<autoaimer::BallisticParameterMenu*>(&menus.top()) || !press(Key::Right)) {
        std::cerr << "Could not reopen the active preset editor\n";
        return 1;
    }
    for (int row = 0; row < 7; ++row) {
        if (!press(Key::Down)) {
            std::cerr << "Could not select the preset editor cancel row\n";
            return 1;
        }
    }
    if (!press(Key::Confirm) || &menus.top() != &ballistics ||
        std::abs(config.ballisticPresets[1].muzzleSpeedMetersPerSecond -
                 savedPreset.muzzleSpeedMetersPerSecond) > 1e-12 ||
        config.ballisticRevision != 2) {
        std::cerr << "Cancel must discard the preset draft without marking configuration dirty\n";
        return 1;
    }
    if (!press(Key::Left) || &menus.top() != &settings) {
        std::cerr << "Left did not return from ballistic presets to settings\n";
        return 1;
    }
    if (!press(Key::Down) || !press(Key::Confirm) ||
        !dynamic_cast<autoaimer::CalibrationMenu*>(&menus.top())) {
        std::cerr << "Down and Confirm must open calibration\n";
        return 1;
    }
    const auto capture = menus.top().update({Key::Confirm}, {config, sensors, now});
    if (capture.kind != autoaimer::MenuAction::Stay ||
        capture.command != autoaimer::MenuCommand::CaptureCalibrationFrame) {
        std::cerr << "Confirm in calibration must request one frame\n";
        return 1;
    }
    if (!press(Key::Left) || &menus.top() != &settings) {
        std::cerr << "Left must return from calibration to settings\n";
        return 1;
    }
    if (!press(Key::Left) || &menus.top() != &root) {
        std::cerr << "Left did not restore the previous menu instance\n";
        return 1;
    }
}
