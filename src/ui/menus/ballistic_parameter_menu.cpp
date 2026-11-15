#include "ui/menus/ballistic_parameter_menu.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace autoaimer {
namespace {
constexpr Rgba white{255, 255, 255, 255};
constexpr Rgba green{80, 255, 125, 255};
constexpr std::size_t parameterCount = 7;
constexpr std::size_t rowCount = parameterCount + 1;

std::string decimal(double value, int places) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), places == 3 ? "%.3f" : places == 2 ? "%.2f" : "%.1f",
                  value);
    return buffer;
}
} // namespace

BallisticParameterMenu::BallisticParameterMenu(std::size_t index, BallisticPreset preset)
    : presetIndex(index < ballisticPresetCount ? index : 0), draft(preset) {
}

void BallisticParameterMenu::change(int direction) {
    switch (selected) {
        case 0:
            draft.muzzleSpeedMetersPerSecond =
                std::clamp(draft.muzzleSpeedMetersPerSecond + direction * 0.5, 1.0, 100.0);
            break;
        case 1:
            draft.horizontalOffsetCentimeters =
                std::clamp(draft.horizontalOffsetCentimeters + direction * 0.5, -30.0, 30.0);
            break;
        case 2:
            draft.verticalOffsetCentimeters =
                std::clamp(draft.verticalOffsetCentimeters + direction * 0.5, -30.0, 30.0);
            break;
        case 3:
            draft.horizontalAngleDegrees =
                std::clamp(draft.horizontalAngleDegrees + direction * 0.25, -15.0, 15.0);
            break;
        case 4:
            draft.verticalAngleDegrees =
                std::clamp(draft.verticalAngleDegrees + direction * 0.25, -15.0, 15.0);
            break;
        case 5:
            draft.dragCoefficient =
                std::clamp(draft.dragCoefficient + direction * 0.005, 0.0, 1.0);
            break;
        case 6:
            draft.quadraticDrag = !draft.quadraticDrag;
            break;
    }
}

MenuAction BallisticParameterMenu::update(Input input, MenuContext context) {
    if (input.key == Key::Up)
        selected = (selected + rowCount - 1) % rowCount;
    if (input.key == Key::Down)
        selected = (selected + 1) % rowCount;
    if (input.key == Key::Left && selected < parameterCount)
        change(-1);
    if (input.key == Key::Right && selected < parameterCount)
        change(1);
    if (input.key == Key::Confirm) {
        if (selected == parameterCount)
            return {MenuAction::Pop, {}};
        context.config.ballisticPresets[presetIndex] = draft;
        context.config.activeBallisticPreset = presetIndex;
        ++context.config.ballisticRevision;
        return {MenuAction::Pop, {}};
    }
    return {};
}

void BallisticParameterMenu::render(HudCanvas& hud) const {
    const std::string rows[parameterCount]{
        "SPEED " + decimal(draft.muzzleSpeedMetersPerSecond, 1) + " MPS",
        "HOR OFF " + decimal(draft.horizontalOffsetCentimeters, 1) + " CM",
        "VER OFF " + decimal(draft.verticalOffsetCentimeters, 1) + " CM",
        "HOR ANG " + decimal(draft.horizontalAngleDegrees, 2) + " DEG",
        "VER ANG " + decimal(draft.verticalAngleDegrees, 2) + " DEG",
        "DRAG " + decimal(draft.dragCoefficient, 3),
        std::string("MODEL ") + (draft.quadraticDrag ? "QUADRATIC" : "LINEAR")};

    hud.centeredText(8, "EDIT P" + std::to_string(presetIndex + 1), white, 2);
    for (std::size_t index = 0; index < parameterCount; ++index)
        hud.centeredText(34 + static_cast<int>(index) * 16, rows[index],
                         index == selected ? green : white);
    hud.centeredText(146, "CANCEL", selected == parameterCount ? green : white);
    hud.centeredText(176, "UP DOWN FIELD", white);
    hud.centeredText(195, "LEFT RIGHT CHANGE", white);
    hud.centeredText(214, selected == parameterCount ? "OK CANCEL" : "OK SAVE AND USE", white);
}
} // namespace autoaimer
