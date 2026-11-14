#include "ui/menu.h"
#include "ui/menus/calibration_menu.h"
#include "ui/menus/parameter_menu.h"
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

    const auto press = [&](autoaimer::Key key) {
        return menus.apply(menus.top().update({key}, {config, sensors, now}));
    };
    if (!press(autoaimer::Key::Right) || &menus.top() == &root) {
        std::cerr << "Right did not open settings\n";
        return 1;
    }
    autoaimer::Menu& settings = menus.top();
    using autoaimer::Key;
    const double originalSpeed = config.motion.velocity.z;
    if (!press(Key::Confirm) || !dynamic_cast<autoaimer::ParameterMenu*>(&menus.top()) ||
        !press(Key::Up) || !press(Key::Left) || &menus.top() != &settings ||
        config.motion.velocity.z != originalSpeed) {
        std::cerr << "Left must discard the parameter draft\n";
        return 1;
    }
    if (!press(Key::Down) || !press(Key::Confirm) ||
        !dynamic_cast<autoaimer::CalibrationMenu*>(&menus.top()) || !press(Key::Left) ||
        &menus.top() != &settings) {
        std::cerr << "Down and Confirm must open calibration, and Left must return\n";
        return 1;
    }
    if (!press(Key::Up) || !press(Key::Confirm) ||
        !dynamic_cast<autoaimer::ParameterMenu*>(&menus.top()) || !press(Key::Up) ||
        !press(Key::Confirm) || &menus.top() != &settings ||
        config.motion.velocity.z != originalSpeed + 1) {
        std::cerr << "Confirm must save the parameter draft and return to settings\n";
        return 1;
    }
    if (!press(Key::Left) || &menus.top() != &root) {
        std::cerr << "Left did not restore the previous menu instance\n";
        return 1;
    }
}
