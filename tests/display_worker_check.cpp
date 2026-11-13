#include "media/display_worker.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void require(bool ok, const char* message) {
    // 并发测试失败时直接结束进程，避免阻塞中的假显示器影响失败诊断。
    if (!ok) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

struct Buffers {
    std::array<std::vector<std::uint16_t>, 2> pixels{std::vector<std::uint16_t>(256),
                                                     std::vector<std::uint16_t>(256)};
    std::array<autoaimer::OutputFrame, 2> views() {
        return {{{pixels[0].data(), 16, 16}, {pixels[1].data(), 16, 16}}};
    }
    void fill(int index, std::uint16_t value) {
        std::fill(pixels[index].begin(), pixels[index].end(), value);
    }
};

// 用条件变量控制发送结束时刻，确定性地模拟“SPI 阻塞，生产者继续画”。
class GateDisplay final : public autoaimer::Display {
public:
    bool present(autoaimer::OutputFrame frame, std::string& error) override {
        std::unique_lock<std::mutex> lock(mutex);
        const std::vector<std::uint16_t> snapshot(frame.pixels,
                                                  frame.pixels + frame.width * frame.height);
        values.push_back(snapshot.front());
        addresses.push_back(frame.pixels);
        threads.push_back(std::this_thread::get_id());
        changed.notify_all();
        changed.wait(lock, [this] { return permits > 0; });
        --permits;
        intact &= std::equal(snapshot.begin(), snapshot.end(), frame.pixels);
        if (fail) {
            error = "injected display failure";
            return false;
        }
        return true;
    }
    void waitFor(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex);
        // GCC 11 的 TSan 未拦截 pthread_cond_clockwait；使用受支持的实时钟超时。
        const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
        require(changed.wait_until(lock, deadline, [&] { return values.size() >= count; }),
                "Display worker did not receive the expected frame");
    }
    void release(unsigned count) {
        std::lock_guard<std::mutex> lock(mutex);
        permits += count;
        changed.notify_all();
    }
    bool fail = false, intact = true;
    std::vector<std::uint16_t> values;
    std::vector<const std::uint16_t*> addresses;
    std::vector<std::thread::id> threads;

private:
    std::mutex mutex;
    std::condition_variable changed;
    unsigned permits = 0;
};

void checkLatestAndOwnership() {
    Buffers buffers;
    GateDisplay display;
    autoaimer::DisplayWorker output(display, buffers.views());
    std::string error;
    require(output.start(error), "Could not start display worker");
    const int first = output.acquire(error);
    require(first >= 0, "Could not acquire first buffer");
    buffers.fill(first, 0x1111);
    require(output.submit(first, error), "Could not submit first frame");
    display.waitFor(1);

    const int second = output.acquire(error);
    require(second >= 0 && second != first, "Producer acquired the buffer still being sent");
    buffers.fill(second, 0x2222);
    require(output.submit(second, error), "Could not submit pending frame");
    require(output.acquire(error) == second,
            "Pending frame was not reclaimed for the latest input");
    buffers.fill(second, 0x3333);
    require(output.submit(second, error), "Could not replace pending frame");
    display.release(1);
    display.waitFor(2);

    // 收回待发送画布后只写半帧，不提交；finish 不能发送这张半帧。
    require(output.acquire(error) == first, "Completed send buffer was not reusable");
    buffers.fill(first, 0x4444);
    require(output.submit(first, error), "Could not submit third frame");
    require(output.acquire(error) == first, "Could not reclaim third frame");
    buffers.pixels[first][0] = 0x5555;
    display.release(10);
    require(output.finish(error), "Could not finish display worker");
    require(display.intact, "A buffer changed while the display was reading it");
    require(display.values == std::vector<std::uint16_t>{0x1111, 0x3333},
            "Stale or partially composed frame was sent");
    require(display.addresses[0] == buffers.pixels[first].data() &&
                display.addresses[1] == buffers.pixels[second].data(),
            "Unexpected full-frame copy");
    require(display.threads[0] != std::this_thread::get_id() &&
                display.threads[0] == display.threads[1],
            "SPI did not stay on one sender thread");
}

void checkShutdownAndError() {
    for (bool fail : {false, true}) {
        Buffers buffers;
        GateDisplay display;
        display.fail = fail;
        display.release(10);
        autoaimer::DisplayWorker output(display, buffers.views());
        std::string error;
        require(output.start(error), "Could not start shutdown test");
        const int buffer = output.acquire(error);
        require(buffer >= 0, "Could not acquire shutdown buffer");
        buffers.fill(buffer, 17);
        require(output.submit(buffer, error), "Could not submit final frame");
        require(output.finish(error) == !fail, "Sender failure was swallowed or shutdown failed");
        require(display.values == std::vector<std::uint16_t>{17},
                "Final frame was lost or retried");
        if (fail) {
            require(error == "injected display failure", "Sender error text was lost");
            require(output.acquire(error) == -1 && !output.submit(buffer, error),
                    "Producer continued after sender failure");
        }
    }
    Buffers buffers;
    GateDisplay display;
    autoaimer::DisplayWorker idle(display, buffers.views());
    std::string error;
    require(idle.start(error) && idle.finish(error), "Idle sender did not wake and join");
    require(display.values.empty(), "Idle shutdown sent an uninitialized frame");
}

class CheckingDisplay final : public autoaimer::Display {
public:
    bool present(autoaimer::OutputFrame frame, std::string&) override {
        const auto value = frame.pixels[0];
        intact &= value > last;
        for (int i = 0; i < frame.width * frame.height; ++i) {
            intact &= frame.pixels[i] == value;
            if (i % 32 == 0)
                std::this_thread::yield();
        }
        last = value;
        ++count;
        return true;
    }
    bool intact = true;
    unsigned last = 0, count = 0;
};

void checkConcurrentFrames() {
    Buffers buffers;
    CheckingDisplay display;
    autoaimer::DisplayWorker output(display, buffers.views());
    std::string error;
    require(output.start(error), "Could not start concurrent frame test");
    for (unsigned n = 1; n <= 2000; ++n) {
        const int buffer = output.acquire(error);
        require(buffer >= 0, "Concurrent acquire failed");
        buffers.fill(buffer, static_cast<std::uint16_t>(n));
        require(output.submit(buffer, error), "Concurrent submit failed");
        if (n % 32 == 0)
            std::this_thread::yield();
    }
    require(output.finish(error), "Concurrent finish failed");
    require(display.intact && display.last == 2000 && display.count > 0,
            "Concurrent frames were torn, reordered, or the latest frame was lost");
}
} // namespace

int main() {
    checkLatestAndOwnership();
    checkShutdownAndError();
    checkConcurrentFrames();
    std::cout << "Double buffer: ownership, latest frame, shutdown and errors passed\n";
}
