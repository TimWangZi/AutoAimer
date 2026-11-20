#include "ui/menus/menu_controller.h"
#include <iostream>

namespace autoaimer {
MenuController::MenuController(ButtonInput& buttons, ImuSensor& imu, DistanceSensor& distance,
                               Camera& camera, const std::string& calibrationDirectory)
    : buttons(buttons), imu(imu), distance(distance), camera(camera),
      calibrationCapture(calibrationDirectory) {}

MenuAction MenuController::update(Menu& menu, Config& config, TimePoint now) {
    const auto input = buttons.poll();
    const auto sensors = pollSensorData(imu, distance, now);
    const CalibrationSamples samples{calibrationCapture.count(), calibrationCapture.status()};
    return menu.update(input, {config, sensors, now, samples});
}

void MenuController::dispatch(const MenuAction& action, const FrameLease& frame, TimePoint now) {
    switch (action.command) {
        case MenuCommand::None:
            break;
        case MenuCommand::MeasureDistance:
            distance.measure(now);
            break;
        case MenuCommand::CaptureCalibrationFrame: {
            std::string error;
            if (calibrationCapture.capture(camera, frame, error) == SampleCaptureStatus::Failed)
                std::cerr << error << '\n';
            break;
        }
    }
}
} // namespace autoaimer
