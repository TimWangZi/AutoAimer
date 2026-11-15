#include "ui/menus/ballistic_menu.h"
#include "ui/menus/ballistic_parameter_menu.h"
#include <string>

namespace autoaimer {
namespace {
constexpr Rgba white{255, 255, 255, 255};
constexpr Rgba green{80, 255, 125, 255};

std::string presetLabel(std::size_t index, bool active) {
    return "P" + std::to_string(index + 1) + (active ? " ACTIVE" : " READY");
}
} // namespace

BallisticMenu::BallisticMenu(std::size_t activePreset)
    : selected(activePreset < ballisticPresetCount ? activePreset : 0), active(selected) {
}

MenuAction BallisticMenu::update(Input input, MenuContext context) {
    active = context.config.activeBallisticPreset;
    if (input.key == Key::Up)
        selected = (selected + ballisticPresetCount - 1) % ballisticPresetCount;
    if (input.key == Key::Down)
        selected = (selected + 1) % ballisticPresetCount;
    if (input.key == Key::Confirm) {
        if (context.config.activeBallisticPreset != selected) {
            context.config.activeBallisticPreset = selected;
            ++context.config.ballisticRevision;
        }
        active = selected;
    }
    if (input.key == Key::Right)
        return {MenuAction::Push,
                std::make_unique<BallisticParameterMenu>(selected,
                                                         context.config.ballisticPresets[selected])};
    if (input.key == Key::Left)
        return {MenuAction::Pop, {}};
    return {};
}

void BallisticMenu::render(HudCanvas& hud) const {
    hud.centeredText(24, "BALLISTIC", white, 2);
    for (std::size_t index = 0; index < ballisticPresetCount; ++index)
        hud.centeredText(70 + static_cast<int>(index) * 27, presetLabel(index, index == active),
                         index == selected ? green : white);
    hud.centeredText(160, "UP DOWN SELECT", white);
    hud.centeredText(179, "OK USE", white);
    hud.centeredText(198, "RIGHT EDIT", white);
    hud.centeredText(217, "LEFT BACK", white);
}
} // namespace autoaimer
