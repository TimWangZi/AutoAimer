#pragma once
#include "ui/menu.h"
#include <optional>

namespace autoaimer {
/**
 * 类名称：ViewMenu
 * 类作用：显示主画面，包括测距结果、准星、轨迹和水平仪。
 */
class ViewMenu final : public Menu {
public:
    /**
     * 方法名称：ViewMenu::update
     * 方法作用：更新主界面计算状态；确认请求测距，右键进入设置。
     */
    MenuAction update(Input input, MenuContext context) override;
    /**
     * 方法名称：ViewMenu::render
     * 方法作用：绘制距离、曲线、两个准星、水平仪和按键提示。
     */
    void render(HudCanvas& hud) const override;

private:
    CameraModel camera;
    Trajectory trajectory;
    std::optional<Orientation> orientation;
    std::optional<double> distanceMeters;
};
} // namespace autoaimer
