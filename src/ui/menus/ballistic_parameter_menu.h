#pragma once
#include "ui/menu.h"

namespace autoaimer {
/**
 * 类名称：BallisticParameterMenu
 * 类作用：编辑一套弹道预设的初速、水平/竖直偏移、水平/竖直角度、阻力和阻力模型。
 */
class BallisticParameterMenu final : public Menu {
public:
    /**
     * 方法名称：BallisticParameterMenu::BallisticParameterMenu
     * 方法作用：复制指定预设建立编辑草稿，避免未确认的值立即影响主界面。
     */
    BallisticParameterMenu(std::size_t presetIndex, BallisticPreset preset);
    /**
     * 方法名称：BallisticParameterMenu::update
     * 方法作用：上下选择参数或取消行，左右调整；确认保存并启用或放弃草稿。
     */
    MenuAction update(Input input, MenuContext context) override;
    /**
     * 方法名称：BallisticParameterMenu::render
     * 方法作用：显示七个参数的草稿值、单位和当前编辑行。
     */
    void render(HudCanvas& hud) const override;

private:
    void change(int direction);

    std::size_t presetIndex;
    BallisticPreset draft;
    std::size_t selected = 0;
};
} // namespace autoaimer
