#pragma once
#include "ui/menu.h"

namespace autoaimer {
/**
 * 类名称：CalibrationMenu
 * 类作用：提供镜头标定入口与占位提示。
 * 说明：当前仅保留菜单入口和占位提示；尚未接入采样、求解或参数保存。
 */
class CalibrationMenu final : public Menu {
public:
    /**
     * 方法名称：CalibrationMenu::update
     * 方法作用：处理返回操作，当前不执行标定采样或求解。
     */
    MenuAction update(Input input, MenuContext context) override;
    /**
     * 方法名称：CalibrationMenu::render
     * 方法作用：绘制标定功能尚待实现的提示。
     */
    void render(HudCanvas& hud) const override;
};
} // namespace autoaimer
