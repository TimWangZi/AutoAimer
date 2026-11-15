#include "media/camera.h"
#include "media/display.h"
#include "media/display_gc9a01.h"
#include "media/video.h"
#include "test_devices.h"
#include "recording_io.h"
#include "ui/menu.h"
#include "ui/menus/ballistic_menu.h"
#include "ui/menus/ballistic_parameter_menu.h"
#include "ui/menus/calibration_menu.h"
#include "ui/menus/settings_menu.h"
#include <iostream>

namespace {
std::size_t painted(const autoaimer::HudCanvas& hud, int left, int top, int right, int bottom) {
    std::size_t count = 0;
    const auto& pixels = hud.pixels();
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            if (pixels[y * autoaimer::outputWidth + x].a != 0)
                ++count;
    return count;
}

bool hasLevelLinePixels(const autoaimer::HudCanvas& hud, int x, int y) {
    for (int row = y - 2; row <= y + 2; ++row) {
        for (int column = x - 2; column <= x + 2; ++column) {
            const auto& pixel = hud.pixels()[row * autoaimer::outputWidth + column];
            if (pixel.a == 255 && pixel.b > pixel.g && pixel.g > pixel.r)
                return true;
        }
    }
    return false;
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3)
        return fail("Usage: ui_preview_check OUTPUT.ppm [PANEL.ppm]");

    const auto now = autoaimer::Clock::now();
    autoaimer::test::ScriptedButtons buttons{autoaimer::Key::Confirm};
    autoaimer::test::TestImuSensor imu({0.0, 9.81, 0.0}, {12.0, -4.0, 0.0});
    autoaimer::test::TestDistanceSensor distance(8.5);
    autoaimer::Config config;
    auto page = autoaimer::makeViewMenu();

    // 第一次更新将确认键事件转换为一次测距命令。
    auto sensors = autoaimer::pollSensorData(imu, distance, now);
    const auto request = page->update(buttons.poll(), {config, sensors, now});
    if (request.command != autoaimer::MenuCommand::MeasureDistance)
        return fail("Confirm on the main view did not request a distance sample");
    distance.measure(now);

    // 下一次轮询把已完成的测量结果交给主界面更新与绘制。
    sensors = autoaimer::pollSensorData(imu, distance, now);
    const auto idle = page->update(buttons.poll(), {config, sensors, now});
    if (idle.command != autoaimer::MenuCommand::None || distance.measureCount != 1)
        return fail("The distance measurement command must be one-shot");

    autoaimer::HudCanvas hud(autoaimer::outputWidth, autoaimer::outputHeight);
    hud.clear();
    page->render(hud);

    // 按大区域检查各 UI 元素是否出现，避免把测试绑定到单个像素，
    // 让正常布局调整不必反复改写断言。
    if (painted(hud, 0, 0, autoaimer::outputWidth, 36) < 20)
        return fail("The measured distance is missing from the top of the HUD");
    if (painted(hud, 105, 105, 136, 136) < 8)
        return fail("The fixed central crosshair is missing");
    if (painted(hud, 100, 145, 141, 205) < 8)
        return fail("The trajectory curve or distance impact marker is missing");
    // 检查圆形仪表边界和倾斜线的两个端点。
    const auto& dialEdge = hud.pixels()[178 * autoaimer::outputWidth + 42];
    if (dialEdge.a != 255 || dialEdge.r != 255 || dialEdge.g != 255 || dialEdge.b != 255 ||
        !hasLevelLinePixels(hud, 48, 172) || !hasLevelLinePixels(hud, 84, 180))
        return fail("The circular level indicator did not render the IMU sample");

    autoaimer::Camera camera;
    autoaimer::VideoComposer video;
    std::string error;
    if (!camera.init("", error) || !video.init(error)) {
        std::cerr << error << '\n';
        return 1;
    }
    const auto frame = camera.acquire(0);
    if (frame.state() != autoaimer::FrameStatus::Ready ||
        !video.compose(frame.view(), hud, error)) {
        std::cerr << (error.empty() ? "Could not compose the preview frame" : error) << '\n';
        return 1;
    }
    if (!autoaimer::writePpm(argv[1], video.output()))
        return fail("Could not write the requested PPM preview");
    if (argc == 3) {
        autoaimer::test::IoRecording recording;
        autoaimer::test::RecordingSpi spi(recording);
        autoaimer::test::RecordingGpio dc(recording, autoaimer::test::IoOperation::DcLevel);
        autoaimer::test::RecordingGpio reset(recording, autoaimer::test::IoOperation::ResetLevel);
        autoaimer::Gc9a01Display panel(spi, dc, reset);
        if (!panel.present(video.output(), error) || !autoaimer::writePpm(argv[2], video.output()))
            return fail("Could not write the round-panel preview");
    }
    // 菜单直接绘制在同一张圆形画布上，分别导出图片供目视检查。
    const std::string path = argv[1];
    const auto prefix = path.substr(0, path.find_last_of('.'));
    autoaimer::SettingsMenu settings;
    autoaimer::BallisticMenu ballistics(config.activeBallisticPreset);
    autoaimer::BallisticParameterMenu ballisticParameters(config.activeBallisticPreset,
                                                           config.activePreset());
    autoaimer::CalibrationMenu calibration;
    const std::pair<const char*, const autoaimer::Menu*> pages[]{
        {"settings", &settings},
        {"ballistics", &ballistics},
        {"ballistic-parameters", &ballisticParameters},
        {"calibration", &calibration}};
    for (const auto& item : pages) {
        hud.clear();
        item.second->render(hud);
        if (!video.compose(frame.view(), hud, error) ||
            !autoaimer::writePpm(prefix + "-" + item.first + ".ppm", video.output()))
            return fail("Could not write menu preview");
    }
}
