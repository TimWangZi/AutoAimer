#include "media/display_gc9a01.h"
#include "recording_io.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
using autoaimer::test::IoEvent;
using autoaimer::test::IoOperation;
struct Call {
    std::uint8_t command;
    std::vector<std::uint8_t> data;
};

// 解码真实的 GPIO/SPI 交错记录，而不是由测试替身直接模拟 command 接口。
bool decode(const std::vector<IoEvent>& events, std::size_t begin, std::size_t end,
            std::vector<Call>& calls) {
    calls.clear();
    while (begin < end) {
        if (begin + 1 >= end || events[begin].operation != IoOperation::DcLevel ||
            events[begin].value != 0 || events[begin + 1].operation != IoOperation::SpiWrite ||
            events[begin + 1].bytes.size() != 1)
            return false;
        calls.push_back({events[begin + 1].bytes[0], {}});
        begin += 2;
        if (begin < end && events[begin].operation == IoOperation::DcLevel &&
            events[begin].value == 1) {
            if (begin + 1 >= end || events[begin + 1].operation != IoOperation::SpiWrite ||
                events[begin + 1].bytes.empty())
                return false;
            calls.back().data = events[begin + 1].bytes;
            begin += 2;
        }
    }
    return true;
}

bool sameEvent(const IoEvent& left, const IoEvent& right) {
    return left.operation == right.operation && left.value == right.value &&
           left.bytes == right.bytes;
}

bool hasEvent(const std::vector<IoEvent>& events, std::size_t index, IoOperation operation,
              unsigned value) {
    return index < events.size() && sameEvent(events[index], {operation, value, {}});
}

std::vector<Call> readVendorCommands(const char* path) {
    std::ifstream file(path);
    std::vector<Call> calls;
    std::string line;
    // 独立测试资料：从模组厂家的初始化文本抄录，每行是命令及其参数。
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream input(line);
        unsigned byte;
        if (!(input >> std::hex >> byte))
            return {};
        calls.push_back({static_cast<std::uint8_t>(byte), {}});
        while (input >> byte)
            calls.back().data.push_back(static_cast<std::uint8_t>(byte));
    }
    return calls;
}
int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2)
        return fail("Usage: display_check VENDOR_INIT.txt");
    const auto vendor = readVendorCommands(argv[1]);
    if (vendor.size() != 51)
        return fail("Expected 51 manufacturer initialization commands");

    autoaimer::test::IoRecording recording;
    autoaimer::test::RecordingSpi spi(recording);
    autoaimer::test::RecordingGpio dc(recording, IoOperation::DcLevel);
    autoaimer::test::RecordingGpio reset(recording, IoOperation::ResetLevel);
    autoaimer::Gc9a01Display display(spi, dc, reset, [&](unsigned ms) {
        recording.events.push_back({IoOperation::Delay, ms, {}});
    });
    std::string error;
    if (!display.init(error))
        return fail("Initialization failed");
    const auto initialized = recording.events;
    if (!hasEvent(initialized, 0, IoOperation::ResetLevel, 1) ||
        !hasEvent(initialized, 1, IoOperation::Delay, 10) ||
        !hasEvent(initialized, 2, IoOperation::ResetLevel, 0) ||
        !hasEvent(initialized, 3, IoOperation::Delay, 20) ||
        !hasEvent(initialized, 4, IoOperation::ResetLevel, 1) ||
        !hasEvent(initialized, 5, IoOperation::Delay, 120))
        return fail("Reset/sleep-out timing is missing");
    std::size_t afterVendor = 6;
    for (const auto& command : vendor)
        afterVendor += command.data.empty() ? 2 : 4;
    std::vector<Call> calls;
    if (initialized.size() < afterVendor + 4 || !decode(initialized, 6, afterVendor, calls) ||
        calls.size() != vendor.size())
        return fail("Invalid command/data GPIO ordering during initialization");
    for (std::size_t i = 0; i < vendor.size(); ++i)
        if (calls[i].command != vendor[i].command || calls[i].data != vendor[i].data)
            return fail("Manufacturer command bytes or order changed");
    if (!hasEvent(initialized, afterVendor, IoOperation::Delay, 120) ||
        !hasEvent(initialized, afterVendor + 3, IoOperation::Delay, 120) ||
        !decode(initialized, afterVendor + 1, afterVendor + 3, calls) || calls.size() != 1 ||
        calls[0].command != 0x11 || !calls[0].data.empty())
        return fail("Sleep Out must be parameterless and surrounded by 120 ms waits");
    if (!decode(initialized, afterVendor + 4, initialized.size(), calls) || calls.size() != 91 ||
        calls.back().command != 0x29 || !calls.back().data.empty())
        return fail("Expected clear of 30 windows followed by parameterless Display On");
    unsigned blackWindows = 0;
    for (const auto& call : calls) {
        if (call.command == 0x2C) {
            for (auto byte : call.data)
                if (byte != 0)
                    return fail("Initial RAM is not black");
            ++blackWindows;
        }
    }
    if (blackWindows != 30)
        return fail("Initial RAM clear is incomplete");

    // 还原 SPI 地址窗口：30 个条带覆盖全部圆内像素，不能重写或裁掉可见边缘。
    std::vector<std::uint16_t> source(240 * 240);
    for (int i = 0; i < 240 * 240; ++i)
        source[i] = static_cast<std::uint16_t>(i);
    source[120 * 240 + 119] = 0xF800;
    source[120 * 240 + 120] = 0x07E0;
    source[120 * 240 + 121] = 0x001F;
    recording.events.clear();
    if (!display.present({source.data(), 240, 240}, error))
        return fail("Present failed");
    const auto presented = recording.events;
    if (!decode(presented, 0, presented.size(), calls) || calls.size() != 30 * 3)
        return fail("Expected exactly 30 SPI windows");
    std::size_t writes = 0, totalBytes = 0, gpioCalls = 0;
    for (const auto& event : presented) {
        if (event.operation == IoOperation::SpiWrite) {
            ++writes;
            totalBytes += event.bytes.size();
            if (event.bytes.empty() || event.bytes.size() > 4096)
                return fail("Invalid SPI transaction size");
        } else if (event.operation == IoOperation::DcLevel) {
            ++gpioCalls;
        } else {
            return fail("Refresh unexpectedly resets the panel or waits");
        }
    }
    if (writes != 180 || totalBytes != 93770 || gpioCalls != 180)
        return fail("SPI/GPIO call count or wire byte count changed");
    std::vector<unsigned> visits(240 * 240);
    std::size_t payloadBytes = 0, maxTransfer = 0;
    const auto visible = [](int x, int y) {
        const double dx = x + 0.5 - 120, dy = y + 0.5 - 120;
        return dx * dx + dy * dy <= 120 * 120;
    };
    for (std::size_t block = 0; block < calls.size(); block += 3) {
        const auto& columns = calls[block];
        const auto& rows = calls[block + 1];
        const auto& ram = calls[block + 2];
        if (columns.command != 0x2A || columns.data.size() != 4 || rows.command != 0x2B ||
            rows.data.size() != 4 || ram.command != 0x2C || ram.data.size() > 4096)
            return fail("Wrong address window or transfer size");
        const int left = columns.data[0] * 256 + columns.data[1];
        const int right = columns.data[2] * 256 + columns.data[3];
        const int top = rows.data[0] * 256 + rows.data[1];
        const int bottom = rows.data[2] * 256 + rows.data[3];
        if (left < 0 || left > right || right >= 240 || top != static_cast<int>(block / 3) * 8 ||
            bottom != top + 7 ||
            ram.data.size() !=
                static_cast<std::size_t>((right - left + 1) * (bottom - top + 1) * 2))
            return fail("Invalid window bounds or byte count");
        bool touchesLeft = false, touchesRight = false;
        std::size_t i = 0;
        for (int y = top; y <= bottom; ++y) {
            touchesLeft |= visible(left, y);
            touchesRight |= visible(right, y);
            for (int x = left; x <= right; ++x) {
                if (ram.data[i] * 256 + ram.data[i + 1] != source[y * 240 + x])
                    return fail("Pixels changed size, position or RGB565 byte order");
                ++visits[y * 240 + x];
                i += 2;
            }
        }
        if (!touchesLeft || !touchesRight)
            return fail("Band includes avoidable side columns");
        payloadBytes += ram.data.size();
        if (ram.data.size() > maxTransfer)
            maxTransfer = ram.data.size();
    }
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 240; ++x) {
            if (visits[y * 240 + x] > 1 || (visible(x, y) && visits[y * 240 + x] != 1))
                return fail("Frame has duplicate pixels or missing visible pixels");
        }
    }
    if (payloadBytes != 93440 || maxTransfer != 3840)
        return fail("Unexpected band payload or maximum transfer size");
    std::cout << "Transmitted pixels: " << payloadBytes / 2 << ", SPI windows: " << calls.size() / 3
              << ", RGB565 bytes/frame: " << payloadBytes
              << ", total SPI bytes/frame: " << totalBytes << ", writes: " << writes
              << ", GPIO calls: " << gpioCalls << '\n';
    recording.events.clear();
    if (display.present({source.data(), 320, 240}, error) || !recording.events.empty() ||
        display.present({nullptr, 240, 240}, error) || !recording.events.empty())
        return fail("Wrong-size frames must be rejected, not resized by the display");

    // 每一个 DC 设置和 SPI 写入都可独立失败，失败后不再执行数据阶段或后续窗口。
    for (std::size_t i = 0; i < presented.size(); ++i) {
        recording.events.clear();
        recording.failAt = static_cast<int>(i);
        error.clear();
        const char* expected = presented[i].operation == IoOperation::SpiWrite
                                   ? "injected SPI error"
                                   : "injected GPIO error";
        if (display.present({source.data(), 240, 240}, error) || error != expected ||
            recording.events.size() != i + 1 ||
            !std::equal(recording.events.begin(), recording.events.end(), presented.begin(),
                        sameEvent))
            return fail("Transfer failure was swallowed, reordered or retried");
    }
    // 覆盖三次复位、首条命令、带参数命令的数据阶段以及最终开屏失败。
    for (const auto i :
         {std::size_t(0), std::size_t(2), std::size_t(4), std::size_t(6), std::size_t(7),
          std::size_t(12), std::size_t(13), initialized.size() - 1}) {
        recording.events.clear();
        recording.failAt = static_cast<int>(i);
        error.clear();
        const char* expected = initialized[i].operation == IoOperation::SpiWrite
                                   ? "injected SPI error"
                                   : "injected GPIO error";
        if (display.init(error) || error != expected || recording.events.size() != i + 1 ||
            !std::equal(recording.events.begin(), recording.events.end(), initialized.begin(),
                        sameEvent))
            return fail("Initialization failure was swallowed, reordered or retried");
    }
    std::cout << "Vendor commands: " << vendor.size() << "; failure stops checked at "
              << presented.size() << " frame operations and 8 initialization operations\n";
    return 0;
}
