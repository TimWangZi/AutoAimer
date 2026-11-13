#include "ui/menus/parameter_menu.h"
#include <algorithm>
#include <string>

namespace autoaimer {
namespace {
constexpr Rgba white{255, 255, 255, 255};
constexpr Rgba green{80, 255, 125, 255};
} // namespace

ParameterMenu::ParameterMenu(double speed) : draft(static_cast<int>(speed)) {
}

MenuAction ParameterMenu::update(Input input, MenuContext context) {
    if (input.key == Key::Up)
        draft = std::min(100, draft + 1);
    if (input.key == Key::Down)
        draft = std::max(1, draft - 1);
    if (input.key == Key::Confirm) {
        context.config.motion.velocity.z = draft;
        return {MenuAction::Pop, {}};
    }
    if (input.key == Key::Left)
        return {MenuAction::Pop, {}};
    return {};
}

void ParameterMenu::render(HudCanvas& hud) const {
    hud.centeredText(32, "SPEED", white, 2);
    hud.centeredText(94, std::to_string(draft), green, 3);
    hud.centeredText(126, "M PER S", white);
    hud.centeredText(172, "UP DOWN CHANGE", white);
    hud.centeredText(200, "OK SAVE", white);
    hud.centeredText(218, "LEFT CANCEL", white);
}
} // namespace autoaimer
