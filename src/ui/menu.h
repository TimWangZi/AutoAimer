#pragma once
#include "compute/ballistic_preset.h"
#include "compute/camera_calibration.h"
#include "input/input.h"
#include "media/calibration_capture_status.h"
#include "media/video_geometry.h"
#include "sensors/sensor.h"
#include "ui/hud.h"
#include <array>
#include <cstdint>
#include <memory>

namespace autoaimer {
struct Config {
    std::array<BallisticPreset, ballisticPresetCount> ballisticPresets{};
    std::size_t activeBallisticPreset = 0;
    // 菜单每次提交预设或切换活动项时递增；App 据此只在变更时写持久化文件。
    std::uint64_t ballisticRevision = 0;
    std::optional<CameraCalibration> cameraCalibration;
    unsigned zoomLevel = 0;
    VideoGeometry video;

    /**
     * 方法名称：Config::activePreset
     * 方法作用：返回当前选中的弹道预设，供主界面生成轨迹。
     */
    BallisticPreset& activePreset() { return ballisticPresets[activeBallisticPreset]; }
    const BallisticPreset& activePreset() const {
        return ballisticPresets[activeBallisticPreset];
    }
};

struct CalibrationSamples {
    unsigned count = 0;
    SampleCaptureStatus status = SampleCaptureStatus::Idle;
};

struct MenuContext {
    Config& config;
    const SensorData& sensors;
    TimePoint now;
    CalibrationSamples calibration{};
};

class Menu;
enum class MenuCommand { None, MeasureDistance, CaptureCalibrationFrame };

struct MenuAction {
    enum Kind { Stay, Push, Pop } kind = Stay;
    std::unique_ptr<Menu> page;
    MenuCommand command = MenuCommand::None;
};

/**
 * 类名称：Menu
 * 类作用：定义页面更新与绘制接口，页面通过返回动作请求导航或测距。
 * 说明：update 修改页面状态并返回动作，render 只绘制；菜单不直接调用硬件驱动。
 */
class Menu {
public:
    /**
     * 方法名称：Menu::~Menu
     * 方法作用：允许菜单栈通过基类指针安全释放具体页面。
     */
    virtual ~Menu() = default;
    /**
     * 方法名称：Menu::update
     * 方法作用：处理本轮输入与传感器快照，更新页面状态并返回动作。
     */
    virtual MenuAction update(Input input, MenuContext context) = 0;
    /**
     * 方法名称：Menu::render
     * 方法作用：把当前页面绘制到 UI 图层，不修改页面状态。
     */
    virtual void render(HudCanvas& hud) const = 0;
};

/**
 * 类名称：MenuStack
 * 类作用：保存页面栈，管理当前页面、压栈和返回操作。
 * 说明：只有栈顶页面参与本轮更新和绘制；App 在绘制后执行压栈/返回动作。
 */
class MenuStack {
public:
    /**
     * 方法名称：MenuStack::MenuStack
     * 方法作用：将传入页面设为根页面，建立深度为 1 的菜单栈。
     */
    explicit MenuStack(std::unique_ptr<Menu> root);
    /**
     * 方法名称：MenuStack::top
     * 方法作用：返回当前栈顶页面的引用。
     */
    Menu& top() const { return *pages[depth - 1]; }
    /**
     * 方法名称：MenuStack::apply
     * 方法作用：执行停留、压栈或返回；非法动作返回 false，根页面不能弹出。
     */
    bool apply(MenuAction action);

private:
    // 被新页面盖住时保留对象和编辑状态；返回后继续使用原页面。
    std::array<std::unique_ptr<Menu>, 8> pages;
    std::size_t depth = 1;
};

/**
 * 方法名称：makeViewMenu
 * 方法作用：创建主界面，并将其所有权交给调用方。
 */
std::unique_ptr<Menu> makeViewMenu();
} // namespace autoaimer
