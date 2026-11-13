#include "ui/menus/settings_menu.h"
#include "ui/menus/calibration_menu.h"
#include "ui/menus/parameter_menu.h"

namespace autoaimer {
namespace {
constexpr Rgba white{255, 255, 255, 255};
constexpr Rgba green{80, 255, 125, 255};
} // namespace

MenuAction SettingsMenu::update(Input input, MenuContext context) {
    if (input.key == Key::Up || input.key == Key::Down)
        selected = 1 - selected;
    if (input.key == Key::Left)
        return {MenuAction::Pop, {}};
    if (input.key == Key::Confirm) {
        if (selected == 0)
            return {MenuAction::Push,
                    std::make_unique<ParameterMenu>(context.config.motion.velocity.z)};
        return {MenuAction::Push, std::make_unique<CalibrationMenu>()};
    }
    return {};
}

void SettingsMenu::render(HudCanvas& hud) const {
    hud.centeredText(32, "SETTINGS", white, 2);
    hud.centeredText(88, "SIMULATION SPEED", selected == 0 ? green : white);
    hud.centeredText(112, "LENS CALIBRATION", selected == 1 ? green : white);
    hud.centeredText(186, "UP DOWN SELECT", white);
    hud.centeredText(206, "OK OPEN", white);
    hud.centeredText(224, "LEFT BACK", white);
}
} // namespace autoaimer
