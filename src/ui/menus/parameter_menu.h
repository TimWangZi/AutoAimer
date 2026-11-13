#pragma once
#include "ui/menu.h"

namespace autoaimer {
/**
 * 类名称：ParameterMenu
 * 类作用：编辑速度参数草稿，确认后写回配置，返回时取消。
 */
class ParameterMenu final : public Menu {
public:
    /**
     * 方法名称：ParameterMenu::ParameterMenu
     * 方法作用：从当前配置建立本页的整数速度草稿。
     */
    explicit ParameterMenu(double speed);
    /**
     * 方法名称：ParameterMenu::update
     * 方法作用：修改本页草稿；确认写回速度配置，左键取消并返回。
     */
    MenuAction update(Input input, MenuContext context) override;
    /**
     * 方法名称：ParameterMenu::render
     * 方法作用：绘制速度草稿与确认、取消提示。
     */
    void render(HudCanvas& hud) const override;

private:
    int draft; // 编辑中的速度草稿；确认才写回配置，返回则直接丢弃。
};
} // namespace autoaimer
