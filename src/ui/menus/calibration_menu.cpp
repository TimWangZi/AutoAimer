#include "ui/menus/calibration_menu.h"
#include <string>

namespace autoaimer {
namespace {
constexpr Rgba white{255, 255, 255, 255};
constexpr Rgba amber{255, 210, 80, 255};
constexpr Rgba green{80, 255, 125, 255};
} // namespace

MenuAction CalibrationMenu::update(Input input, MenuContext context) {
    samples = context.calibration;
    parametersLoaded = context.config.cameraCalibration.has_value();
    if (input.key == Key::Left)
        return {MenuAction::Pop, {}};
    if (input.key == Key::Confirm)
        return {MenuAction::Stay, {}, MenuCommand::CaptureCalibrationFrame};
    return {};
}

void CalibrationMenu::render(HudCanvas& hud) const {
    hud.centeredText(32, "CALIBRATION", white, 2);
    hud.centeredText(67, "KEEP BOARD IN VIEW", white);
    hud.centeredText(83, "VARY ANGLE AND DISTANCE", white);
    hud.centeredText(112, parametersLoaded ? "PARAMETERS LOADED" : "NOT CALIBRATED",
                     parametersLoaded ? green : amber);
    hud.centeredText(145, "SAMPLES " + std::to_string(samples.count), white, 2);
    if (samples.status == SampleCaptureStatus::Saved)
        hud.centeredText(170, "SAVED", green);
    if (samples.status == SampleCaptureStatus::NoFrame)
        hud.centeredText(170, "NO CAMERA FRAME", amber);
    if (samples.status == SampleCaptureStatus::Failed)
        hud.centeredText(170, "COULD NOT SAVE", amber);
    hud.centeredText(199, "OK CAPTURE", white);
    hud.centeredText(218, "LEFT BACK", white);
}
} // namespace autoaimer
