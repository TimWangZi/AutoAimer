#pragma once
#include "ui/menu.h"

namespace autoaimer {
/**
 * 类名称：CalibrationMenu
 * 类作用：引导拍摄棋盘格、请求采样，并呈现样本保存结果。
 */
class CalibrationMenu final : public Menu {
public:
    /**
     * 方法名称：CalibrationMenu::update
     * 方法作用：读取采样状态，根据按键发出采样或返回请求。
     */
    MenuAction update(Input input, MenuContext context) override;
    /**
     * 方法名称：CalibrationMenu::render
     * 方法作用：显示采样数量、最后一次保存结果和操作提示。
     */
    void render(HudCanvas& hud) const override;

private:
    CalibrationSamples samples;
    bool parametersLoaded = false;
};
} // namespace autoaimer
