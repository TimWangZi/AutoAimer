#pragma once
#include "ui/menu.h"

namespace autoaimer {
/**
 * 类名称：BallisticMenu
 * 类作用：在三套弹道预设中选择当前套用的预设，并进入参数编辑页。
 */
class BallisticMenu final : public Menu {
public:
    /**
     * 方法名称：BallisticMenu::BallisticMenu
     * 方法作用：以当前启用的预设作为初始光标位置。
     */
    explicit BallisticMenu(std::size_t activePreset);
    /**
     * 方法名称：BallisticMenu::update
     * 方法作用：上下选择，确认启用，右键编辑，左键返回。
     */
    MenuAction update(Input input, MenuContext context) override;
    /**
     * 方法名称：BallisticMenu::render
     * 方法作用：绘制 P1/P2/P3 列表，同时标明正在使用的预设。
     */
    void render(HudCanvas& hud) const override;

private:
    std::size_t selected = 0;
    std::size_t active = 0;
};
} // namespace autoaimer
