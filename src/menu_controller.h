#pragma once

#include "media/calibration_capture.h"
#include "ui/menu.h"

namespace autoaimer {
/**
 * 类名称：MenuController
 * 类作用：为页面提供输入快照，并分派页面请求的设备操作。
 */
class MenuController {
public:
    /**
     * 方法名称：MenuController::MenuController
     * 方法作用：借用按键、传感器和相机接口，准备本次运行的采样会话。
     */
    MenuController(ButtonInput& buttons, ImuSensor& imu, DistanceSensor& distance, Camera& camera,
                   const std::string& calibrationDirectory);
    /**
     * 方法名称：MenuController::update
     * 方法作用：轮询一次输入，并向当前页面提供传感器和采样状态。
     */
    MenuAction update(Menu& menu, Config& config, TimePoint now);
    /**
     * 方法名称：MenuController::dispatch
     * 方法作用：执行页面请求的设备操作，采样时同步使用当前帧租约。
     */
    void dispatch(const MenuAction& action, const FrameLease& frame, TimePoint now);

private:
    ButtonInput& buttons;
    ImuSensor& imu;
    DistanceSensor& distance;
    Camera& camera;
    CalibrationCapture calibrationCapture;
};
} // namespace autoaimer
