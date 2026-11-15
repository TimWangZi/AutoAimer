#include "media/camera.h"
#include "menu_controller.h"
#include "test_devices.h"

#include <chrono>
#include <iostream>

namespace {
using namespace autoaimer;

class RecordingMenu final : public Menu {
public:
    MenuAction update(Input input, MenuContext context) override {
        key = input.key;
        now = context.now;
        sensors = context.sensors;
        samples = context.calibration;
        if (key == Key::Confirm)
            return {MenuAction::Stay, {}, MenuCommand::CaptureCalibrationFrame};
        return {};
    }

    void render(HudCanvas&) const override {}

    Key key = Key::None;
    TimePoint now{};
    SensorData sensors;
    CalibrationSamples samples;
};
} // namespace

int main() {
    using namespace autoaimer;
    test::ScriptedButtons buttons{Key::None, Key::Confirm};
    test::TestImuSensor imu{{0, 0, 1}, {7, 0, 0}};
    test::TestDistanceSensor distance{2};
    Camera camera;
    Config config;
    RecordingMenu menu;
    MenuController controller{buttons, imu, distance, camera, ""};
    const FrameLease noFrame{FrameStatus::Timeout};
    const auto first = TimePoint{} + std::chrono::milliseconds(1);
    const auto second = first + std::chrono::milliseconds(1);
    const auto third = second + std::chrono::milliseconds(1);

    const auto idle = controller.update(menu, config, first);
    if (idle.command != MenuCommand::None || menu.key != Key::None || menu.now != first ||
        menu.sensors.orientation.status != SampleStatus::Valid ||
        menu.sensors.orientation.sampledAt != first ||
        menu.sensors.orientation.value.rollDegrees != 7 ||
        menu.sensors.distanceMeters.status != SampleStatus::Unavailable ||
        menu.samples.status != SampleCaptureStatus::Idle || menu.samples.count != 0) {
        std::cerr << "Menu did not receive the current inputs and initial sample state\n";
        return 1;
    }
    controller.dispatch(idle, noFrame, first);

    const auto capture = controller.update(menu, config, second);
    if (capture.command != MenuCommand::CaptureCalibrationFrame || menu.key != Key::Confirm ||
        menu.samples.status != SampleCaptureStatus::Idle || menu.samples.count != 0) {
        std::cerr << "Idle dispatch changed capture state, or confirm was not forwarded\n";
        return 1;
    }
    controller.dispatch(capture, noFrame, second);

    const auto afterCapture = controller.update(menu, config, third);
    if (afterCapture.command != MenuCommand::None || menu.key != Key::None ||
        menu.samples.status != SampleCaptureStatus::NoFrame || menu.samples.count != 0 ||
        imu.pollCount != 3 || distance.pollCount != 3 || distance.measureCount != 0) {
        std::cerr << "Missing-frame outcome was lost or capture performed an unrelated operation\n";
        return 1;
    }
}
