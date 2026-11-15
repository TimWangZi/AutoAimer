#pragma once
#include "ui/menu.h"

namespace autoaimer {
/**
 * 类名称：SettingsMenu
 * 类作用：显示设置选项，并导航到弹道预设页或标定页。
 */
class SettingsMenu final : public Menu {
public:
    /**
     * 方法名称：SettingsMenu::update
     * 方法作用：切换选项、打开所选页面，或返回上一页。
     */
    MenuAction update(Input input, MenuContext context) override;
    /**
     * 方法名称：SettingsMenu::render
     * 方法作用：绘制设置项和当前选中状态。
     */
    void render(HudCanvas& hud) const override;

private:
    int selected = 0; // 0：弹道预设；1：镜头标定入口。
};
} // namespace autoaimer
