#pragma once
#include "media/camera.h"
#include "media/video.h"
#include "ui/menu.h"

namespace autoaimer {
struct Options {
    unsigned frames = 0; // 合成帧数上限；0 表示持续运行，丢帧不影响这个计数。
    std::string ppm;     // 非空时，在退出前保存最后合成帧。
    std::string iqDir = "/etc/iqfiles";
};

/**
 * 类名称：App
 * 类作用：组织主线程中的取帧、菜单更新、合成、提交和退出流程。
 * 说明：主线程的流程组织者：拥有相机、合成器、菜单与配置，借用外部硬件接口。
 */
class App {
public:
    /**
     * 方法名称：App::App
     * 方法作用：借用显示和传感器接口，建立本应用的资源与菜单状态。
     */
    App(Display& display, ButtonInput& buttons, ImuSensor& imu, DistanceSensor& distance)
        : display(display), buttons(buttons), imu(imu), distance(distance) {}
    /**
     * 方法名称：App::run
     * 方法作用：执行逐帧主循环；正常结束返回 0，初始化或运行失败返回 1。
     */
    int run(const Options& options, bool (*running)());

private:
    // 成员按声明顺序的逆序销毁：先释放 RGA，再由 Camera 关闭 RKMPI。
    Camera camera;
    VideoComposer video;
    Display& display;
    ButtonInput& buttons;
    ImuSensor& imu;
    DistanceSensor& distance;
    HudCanvas hud{outputWidth, outputHeight};
    MenuStack menus{makeViewMenu()};
    Config config;
};
} // namespace autoaimer
