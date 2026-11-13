#include "media/display_worker.h"
#include <system_error>

namespace autoaimer {
DisplayWorker::~DisplayWorker() {
    std::string ignored;
    finish(ignored);
}

bool DisplayWorker::start(std::string& error) {
    try {
        senderThread = std::thread(&DisplayWorker::sendLoop, this);
    } catch (const std::system_error& exception) {
        error = std::string("Could not start display thread: ") + exception.what();
        return false;
    }
    return true;
}

int DisplayWorker::acquire(std::string& error) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (sendFailed) {
        error = sendError;
        return -1;
    }
    // 优先空闲画布；没有空闲画布时，收回 readyBuffer，绝不碰 sendingBuffer。
    int buffer = 0;
    if (sendingBuffer >= 0) {
        // 发送中的画布只读，只能领取另外一块。
        buffer = 1 - sendingBuffer;
    } else if (readyBuffer >= 0) {
        // 发送线程尚未领取完整帧时，优先保留它，使用另一块空闲画布。
        buffer = 1 - readyBuffer;
    }
    if (readyBuffer == buffer)
        readyBuffer = -1;
    return buffer;
}

bool DisplayWorker::submit(unsigned buffer, std::string& error) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (sendFailed) {
        error = sendError;
        return false;
    }
    readyBuffer = static_cast<int>(buffer); // 替换旧的待发送帧，不建立 FIFO 队列。
    frameAvailable.notify_one();
    return true;
}

bool DisplayWorker::finish(std::string& error) {
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        stopRequested = true;
        frameAvailable.notify_one();
    }
    // join 必须在锁外：发送线程完成后还需要拿锁更新状态，否则双方会互相等待。
    if (senderThread.joinable())
        senderThread.join();
    if (sendFailed) {
        error = sendError;
        return false;
    }
    return true;
}

void DisplayWorker::sendLoop() {
    std::unique_lock<std::mutex> lock(stateMutex);
    for (;;) {
        frameAvailable.wait(lock, [this] { return stopRequested || readyBuffer >= 0; });
        if (readyBuffer < 0)
            return;
        sendingBuffer = readyBuffer;
        readyBuffer = -1;
        // 先在锁内认领画布再解锁；这里只复制地址和尺寸，不复制像素。
        const auto frame = buffers[sendingBuffer];
        lock.unlock();

        // SPI 可以阻塞；发送过程中不持有归属锁，主线程继续写另一块画布。
        std::string error;
        const bool ok = display.present(frame, error);

        lock.lock();
        sendingBuffer = -1;
        if (!ok) {
            sendFailed = true;
            sendError = error;
            readyBuffer = -1;
            return;
        }
    }
}
} // namespace autoaimer
