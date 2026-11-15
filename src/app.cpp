#include "app.h"
#include "config/ballistic_store.h"
#include "compute/camera_calibration.h"
#include "media/display_worker.h"
#include "media/video.h"
#include "menu_controller.h"
#include <iostream>

namespace autoaimer {
namespace {
// 启动配置文件统一在开启相机和显示线程前读取。
bool loadStartupConfig(const Options& options, Config& config, std::string& error) {
    const auto presetStatus = loadBallisticPresets(
        options.ballisticPresetFile, config.ballisticPresets, config.activeBallisticPreset, error);
    if (presetStatus == BallisticLoadStatus::Error)
        return false;
    if (presetStatus == BallisticLoadStatus::Loaded)
        std::cerr << "Ballistic presets loaded, active P" << config.activeBallisticPreset + 1
                  << '\n';
    CameraCalibration calibration;
    const auto calibrationStatus =
        loadCameraCalibration(options.cameraCalibrationFile, calibration, error);
    if (calibrationStatus == CalibrationLoadStatus::Error)
        return false;
    if (calibrationStatus == CalibrationLoadStatus::Loaded) {
        config.cameraCalibration = calibration;
        std::cerr << "Camera calibration loaded, RMS " << calibration.rmsPixels << " px\n";
    }
    return true;
}
} // namespace

int App::run(const Options& options, bool (*running)()) {
    std::string error;
    if (!loadStartupConfig(options, config, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    // 合成器先于 Camera 销毁，RGA 缓冲必须在关闭 RKMPI 前释放。
    VideoComposer video(options.cropSize);
    if (!camera.init(options.iqDir, error) || !video.init(error)) {
        std::cerr << error << '\n';
        return 1;
    }
    // 发送线程先停止并 join，随后才释放合成器的画布。
    DisplayWorker output(display, {video.output(0), video.output(1)});
    if (!output.start(error)) {
        std::cerr << error << '\n';
        return 1;
    }
    unsigned frames = 0, consecutiveTimeouts = 0;
    MenuController menuController(buttons, imu, distance, camera, options.calibrationDirectory);
    std::uint64_t savedBallisticRevision = config.ballisticRevision;
    while (running() && (options.frames == 0 || frames < options.frames)) {
        // 1. 取得本轮图像、按键和传感器快照，交给当前菜单更新状态。
        // 相机租约覆盖同步合成；发送线程只借用独立的 RGB565 成品画布。
        auto frame = camera.acquire(150);
        if (frame.state() == FrameStatus::Ready)
            config.video = makeVideoGeometry(frame.view().width, frame.view().height,
                                             options.cropSize);
        const auto now = Clock::now();
        auto action = menuController.update(menus.top(), config, now);
        if (config.ballisticRevision != savedBallisticRevision) {
            if (!options.ballisticPresetFile.empty() &&
                !saveBallisticPresets(options.ballisticPresetFile, config.ballisticPresets,
                                      config.activeBallisticPreset, error))
                std::cerr << error << '\n';
            savedBallisticRevision = config.ballisticRevision;
        }
        menuController.dispatch(action, frame, now);

        switch (frame.state()) {
            case FrameStatus::Ready: {
                // 2. 菜单只画透明 UI；合成器把 UI 与图像合成到可写画布。
                hud.clear();
                menus.top().render(hud);
                // 拿到新输入后才收回待发送画布，取帧等待期间旧的完整帧仍可送屏。
                const int buffer = output.acquire(error);
                if (buffer < 0) {
                    std::cerr << error << '\n';
                    return 1;
                }
                if (!video.compose(frame.view(), hud, error, static_cast<unsigned>(buffer),
                                   config.zoomLevel)) {
                    std::cerr << error << '\n';
                    return 1;
                }
                // 提交的是画布索引；SPI 由发送线程执行，主线程可以继续处理下一帧。
                if (!output.submit(static_cast<unsigned>(buffer), error)) {
                    std::cerr << error << '\n';
                    return 1;
                }
#ifdef AUTOAIMER_BOARD
                if (frames == 0) {
                    const auto& source = frame.view();
                    std::cerr << "Camera NV12 " << source.width << 'x' << source.height << ", crop "
                              << config.video.baseCropSize << " -> " << outputWidth << 'x'
                              << outputHeight << '\n';
                }
#endif
                ++frames;
                consecutiveTimeouts = 0;
                break;
            }
            case FrameStatus::Timeout:
                if (++consecutiveTimeouts >= 10) {
                    std::cerr << "VI timed out on 10 consecutive acquisitions\n";
                    return 1;
                }
                break;
            case FrameStatus::Error:
                std::cerr << "Camera frame acquisition failed: " << frame.errorCode() << '\n';
                return 1;
        }
        // 本帧绘制完再切换页面，保证更新与绘制由同一个菜单完成。
        if (!menus.apply(std::move(action)))
            std::cerr << "Menu navigation rejected\n";
    }
    // 3. 等发送线程收尾后，最后合成帧才可作为稳定的退出快照保存。
    if (!output.finish(error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (!options.ppm.empty() && (frames == 0 || !writePpm(options.ppm, video.output()))) {
        std::cerr << "Could not save composed frame to " << options.ppm << '\n';
        return 1;
    }
    return 0;
}
} // namespace autoaimer
