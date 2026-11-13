#pragma once
#include "media/display.h"
#include "media/frame.h"
#include "ui/hud.h"
#include <array>
#include <memory>
#include <vector>

namespace autoaimer {
#ifdef AUTOAIMER_BOARD
class RgaVideo;
#endif

/**
 * 类名称：VideoComposer
 * 类作用：将输入图像与 UI 合成为最终 RGB565 画布，并持有两块输出缓冲区。
 * 说明：只在主线程调用：一次裁剪/缩放后叠加 UI，输出最终 240×240 RGB565 画布。
 */
class VideoComposer {
public:
    /**
     * 方法名称：VideoComposer::VideoComposer
     * 方法作用：分配两块固定尺寸的 RGB565 输出画布。
     */
    VideoComposer();
    /**
     * 方法名称：VideoComposer::~VideoComposer
     * 方法作用：释放输出画布及可选的 RGA 资源。
     */
    ~VideoComposer();
    /**
     * 方法名称：VideoComposer::init
     * 方法作用：初始化合成资源；真实相机版本同时初始化 RGA。
     */
    bool init(std::string& error);
    /**
     * 方法名称：VideoComposer::compose
     * 方法作用：将输入图像与透明 UI 合成到指定画布；失败时返回错误。
     * 说明：buffer 只能为 0 或 1，并由 DisplayWorker::acquire 确认当前可写。
     * 说明：另一个缓冲区可以被发送线程读取；这里不负责跨线程同步。
     */
    bool compose(const FrameView& frame, const HudCanvas& hud, std::string& error,
                 unsigned buffer = 0);
    /**
     * 方法名称：VideoComposer::output
     * 方法作用：返回最近一次成功合成画布的只读视图，不复制像素。
     */
    OutputFrame output() const { return output(currentBuffer); }
    /**
     * 方法名称：VideoComposer::output（指定画布）
     * 方法作用：返回编号为 0 或 1 的画布视图，供显示线程建立借用关系。
     */
    OutputFrame output(unsigned buffer) const {
        return {buffers[buffer].data(), outputWidth, outputHeight};
    }

private:
    // 两块最终 RGB565 画布，初始化后不再调整容量，不复制整帧进行交接。
    std::array<std::vector<std::uint16_t>, 2> buffers;
    unsigned currentBuffer = 0;
#ifdef AUTOAIMER_BOARD
    std::unique_ptr<RgaVideo> rga;
#endif
};
} // namespace autoaimer
