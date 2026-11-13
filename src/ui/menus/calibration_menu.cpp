#include "ui/menus/calibration_menu.h"

namespace autoaimer {
namespace {
constexpr Rgba white{255, 255, 255, 255};
constexpr Rgba amber{255, 210, 80, 255};
} // namespace

MenuAction CalibrationMenu::update(Input input, MenuContext) {
    if (input.key == Key::Left)
        return {MenuAction::Pop, {}};
    return {};
}

void CalibrationMenu::render(HudCanvas& hud) const {
    hud.centeredText(32, "CALIBRATION", white, 2);
    hud.centeredText(92, "CAPTURE AND", amber);
    hud.centeredText(112, "SOLVER PENDING", amber);
    hud.centeredText(218, "LEFT BACK", white);
}
} // namespace autoaimer
