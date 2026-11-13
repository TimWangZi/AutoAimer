#pragma once
#include "media/display.h"
#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace autoaimer {
/**
 * 类名称：DisplayWorker
 * 类作用：管理发送线程和双缓冲归属，优先发送最新完整画面。
 * 说明：一名生产者和一名发送者，共用两块固定画布；这里只管理归属，不分配像素。
 * 说明：buffers 和 display 必须活到 finish 返回；Display::present 的同步借用契约不变。
 */
class DisplayWorker {
public:
    /**
     * 方法名称：DisplayWorker::DisplayWorker
     * 方法作用：借用显示接口和两块画布视图；此时尚未启动线程。
     */
    DisplayWorker(Display& display, std::array<OutputFrame, 2> buffers)
        : display(display), buffers(buffers) {}
    /**
     * 方法名称：DisplayWorker::~DisplayWorker
     * 方法作用：停止并等待发送线程，保证对象销毁后不再访问外部画布。
     */
    ~DisplayWorker();
    /**
     * 方法名称：DisplayWorker::start
     * 方法作用：启动一个发送线程；线程创建失败时返回错误。
     * 说明：主线程调用一次 start；退出前调用 finish，析构也会兜住提前返回路径。
     */
    bool start(std::string& error);

    /**
     * 方法名称：DisplayWorker::acquire
     * 方法作用：领取可写画布索引，必要时收回未发送旧帧；发送失败时返回 -1。
     * 说明：主线程依次 acquire → 写完整帧 → submit。返回 -1 表示发送线程已失败。
     * 说明：若另一块正在发送，则收回尚未发送的旧帧，优先让最新输入使用这块画布。
     */
    int acquire(std::string& error);
    /**
     * 方法名称：DisplayWorker::submit
     * 方法作用：将已写完整的画布设为最新待发送帧，并通知发送线程。
     */
    bool submit(unsigned buffer, std::string& error);
    /**
     * 方法名称：DisplayWorker::finish
     * 方法作用：请求发送线程收尾，送完已提交帧并等待退出；回传发送错误。
     * 说明：发送最后一张完整帧后 join；未提交的半帧不会送屏。可重复调用。
     */
    bool finish(std::string& error);

private:
    /**
     * 方法名称：DisplayWorker::sendLoop
     * 方法作用：等待并认领完整画布，在锁外送屏，随后释放画布或记录错误。
     */
    void sendLoop();
    Display& display;
    // 两个只保存地址/尺寸的视图；实际像素由 VideoComposer 持有。
    const std::array<OutputFrame, 2> buffers;
    std::mutex stateMutex;
    std::condition_variable frameAvailable;
    std::thread senderThread;
    // 以下状态由 stateMutex 保护；索引只能是 0/1，-1 表示没有对应画布。
    // readyBuffer 可被生产者收回；sendingBuffer 必须等 present 返回才能再写。
    int readyBuffer = -1, sendingBuffer = -1;
    bool stopRequested = false, sendFailed = false;
    std::string sendError;
};
} // namespace autoaimer
