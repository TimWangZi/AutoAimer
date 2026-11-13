#include "media/display_gc9a01.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

namespace autoaimer {
namespace {
struct Register {
    std::uint8_t command, size;
    std::array<std::uint8_t, 12> data;
};
// 参数来自七针蓝板随附的「1.28寸初始化HSD+GC9A01.txt」，逐字节保留。
// 命令解释依据同目录「GC9A01芯片手册.pdf」：GC9A01A Rev.1.0，2019-07-02。
// 以下页码均为该手册页脚页码。手册与厂家表的差异单独注明；
// 本表不擅自补参或改变已验证的顺序。
constexpr Register initialization[]{
    // 0xFE / Inter Register Enable1：与紧随的 0xEF 共同开放内部寄存器。无参数；§6.4.8，p172。
    {0xFE, 0, {}},
    // 0xEF / Inter Register Enable2：完成内部寄存器使能，复位后需重新使能。无参数；§6.4.9，p173。
    {0xEF, 0, {}},
    {0xEB, 1, {0x14}},
    {0x84, 1, {0x40}},
    {0x85, 1, {0xF1}},
    {0x86, 1, {0x98}},
    {0x87, 1, {0x28}},
    {0x88, 1, {0x0A}},
    {0x89, 1, {0x21}},
    {0x8A, 1, {0x00}},
    {0x8B, 1, {0x80}},
    {0x8C, 1, {0x01}},
    {0x8D, 1, {0x00}},
    {0x8E, 1, {0xDF}},
    {0x8F, 1, {0x52}},
    // 0xB6 / Display Function Control：控制驱动扫描方向与排列（§6.3.3，p158–160）。
    // 手册列出 3 个参数，且第 1 个无效但必须写；厂家仅发送 0x20，不能将它直接解释为第 2 参数。
    {0xB6, 1, {0x20}},
    // 0x36 / MADCTL：显存访问方向和色序。0x48 = MX=1、BGR=1，其余位为 0；§6.2.18，p127。
    {0x36, 1, {0x48}},
    // 0x3A / COLMOD：像素格式。0x05 的 DBI=101 表示 MCU/SPI 使用 RGB565；§6.2.22，p135。
    // DPI=000 对当前未使用的 RGB 并行接口无影响；方向/BGR 由上面的 0x36 设置。
    {0x3A, 1, {0x05}},
    {0x90, 4, {0x08, 0x08, 0x08, 0x08}},
    {0xBD, 1, {0x06}},
    {0xBF, 1, {0x1C}},
    // 0xA7 / Power Control 7：设置核心电压档位；0x45 的 vdd_ad[3:0]=5。§6.4.7，p171。
    {0xA7, 1, {0x45}},
    {0xA9, 1, {0xBB}},
    {0xB8, 1, {0x63}},
    {0xBC, 1, {0x00}},
    {0xFF, 3, {0x60, 0x01, 0x04}},
    // 0xC3 / Power Control 2：灰阶参考电压调节，vreg1_vbp_d[6:0]=0x17。§6.4.4，p168。
    {0xC3, 1, {0x17}},
    // 0xC4 / Power Control 3：另一组灰阶参考电压调节，vreg1_vbn_d[6:0]=0x17。§6.4.5，p169。
    {0xC4, 1, {0x17}},
    // 0xC9 / Power Control 4：灰阶参考电压基准，vrh[5:0]=0x25，与 C3/C4 共同决定电压。§6.4.6，p170。
    {0xC9, 1, {0x25}},
    {0xBE, 1, {0x11}},
    {0xE1, 2, {0x10, 0x0E}},
    {0xDF, 3, {0x21, 0x10, 0x02}},
    // 0xF0 / SET_GAMMA1：负极性 Gamma 梯度及参考点的前半组；6 字节为厂家曲线参数。§6.4.10，p174。
    {0xF0, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
    // 0xF1 / SET_GAMMA2：负极性 Gamma 参考点的后半组；6 字节与 F0 配套。§6.4.11，p176。
    {0xF1, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
    // 0xF2 / SET_GAMMA3：正极性 Gamma 梯度及参考点的前半组；6 字节为厂家曲线参数。§6.4.12，p178。
    {0xF2, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
    // 0xF3 / SET_GAMMA4：正极性 Gamma 参考点的后半组；6 字节与 F2 配套。§6.4.13，p180。
    {0xF3, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
    {0xED, 2, {0x1B, 0x0B}},
    {0xAC, 1, {0x47}},
    {0xAE, 1, {0x77}},
    {0xCB, 1, {0x02}},
    {0xCD, 1, {0x63}},
    {0x70, 9, {0x07, 0x09, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}},
    // 0xE8：手册名称为 Frame Rate；§6.4.1 p164 实际描述 DINV 反转模式，且与 p94–95 的表不一致。
    // 保留厂家唯一参数 0x34；不补写参数，不据此推算面板扫描帧率或应用送屏 FPS。
    {0xE8, 1, {0x34}},
    {0x62, 12, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}},
    {0x63, 12, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}},
    {0x64, 7, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}},
    {0x66, 10, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}},
    {0x67, 10, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}},
    {0x74, 7, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}},
    // 0x35 / Tearing Effect Line ON：使能 TE 输出；§6.2.17，p125 规定 1 个模式参数。
    // 厂家表未发送该参数，本表保留原样；七针模块未引出 TE，程序也没有用它同步刷新。
    {0x35, 0, {}},
    // 0x21 / Display Inversion ON：开启显示反转模式，不改写显存内容。无参数；§6.2.8，p108。
    {0x21, 0, {}}};
} // namespace

Gc9a01Display::Gc9a01Display(SpiDevice& spi, GpioOutput& dc, GpioOutput& reset,
                             DelayMilliseconds delay)
    : spi(spi), dc(dc), reset(reset), delay(std::move(delay)) {
    // 每个条带取所有可见行的包围矩形，启动时只算一次。
    // 保留 30 个窗口，允许少量圆外像素，避免逐行贴合圆边增加控制开销。
    for (std::size_t block = 0; block < windows.size(); ++block) {
        const int top = static_cast<int>(block) * rowsPerBlock;
        const int bottom = std::min(top + rowsPerBlock, height) - 1;
        int left = width, right = 0;
        for (int y = top; y <= bottom; ++y) {
            left = std::min(left, screenRows[y].first);
            right = std::max(right, screenRows[y].end - 1);
        }
        windows[block] = {left, top, right, bottom};
    }
}

void Gc9a01Display::waitMilliseconds(unsigned milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

bool Gc9a01Display::writeCommand(std::uint8_t command, const std::uint8_t* data, std::size_t size,
                                 std::string& error) {
    // GC9A01 四线 SPI：DC=0 表示命令，DC=1 表示参数/像素；§4.1.8–4.1.9，p28–30。
    // 电平切换与 SPI 写入均同步完成，数据按整块发送，保持原有 CS 边界。
    if (!dc.set(false, error) || !spi.write(&command, 1, error))
        return false;
    return size == 0 || (dc.set(true, error) && spi.write(data, size, error));
}

bool Gc9a01Display::init(std::string& error) {
    if (!reset.set(true, error))
        return false;
    delay(10);
    if (!reset.set(false, error))
        return false;
    delay(20);
    if (!reset.set(true, error))
        return false;
    delay(120);
    for (const auto& item : initialization)
        if (!writeCommand(item.command, item.data.data(), item.size, error))
            return false;
    delay(120);
    // 0x11 / Sleep Out：退出休眠，启动内部电源与扫描；无参数。§6.2.4，p103–104。
    // 保留厂家规定的 120 ms 等待，然后才写显存并开屏。
    if (!writeCommand(0x11, nullptr, 0, error))
        return false;
    delay(120);
    // 显示打开前先清除显存，避免随机内容闪屏。
    const std::vector<std::uint16_t> black(width * height, 0);
    if (!present({black.data(), width, height}, error))
        return false;
    // 0x29 / Display ON：显示已写入显存的画面，不改变其内容；无参数。§6.2.10，p110。
    return writeCommand(0x29, nullptr, 0, error);
}

bool Gc9a01Display::present(OutputFrame frame, std::string& error) {
    if (!frame.pixels || frame.width != width || frame.height != height) {
        error = "GC9A01 requires a composed 240x240 RGB565 frame";
        return false;
    }
    // 输入已是最终画布，不再缩放。只打包条带内的像素，省去两侧黑角；
    // 每个可见像素仍覆盖一次，窗口数量和 SPI/DC 事务数保持不变。
    for (const auto& window : windows) {
        const std::uint8_t columns[]{0, static_cast<std::uint8_t>(window.left), 0,
                                     static_cast<std::uint8_t>(window.right)};
        const std::uint8_t rows[]{0, static_cast<std::uint8_t>(window.top), 0,
                                  static_cast<std::uint8_t>(window.bottom)};
        std::size_t bytes = 0;
        for (int y = window.top; y <= window.bottom; ++y) {
            for (int x = window.left; x <= window.right; ++x) {
                const auto pixel = frame.pixels[y * width + x];
                wire[bytes++] = static_cast<std::uint8_t>(pixel >> 8);
                wire[bytes++] = static_cast<std::uint8_t>(pixel);
            }
        }
        // 0x2A / Column Address Set：4 字节为起始列和终止列，高字节先发，含两端；§6.2.11，p111。
        // 0x2B / Row Address Set：同样设置起始行和终止行；§6.2.12，p113。
        // 0x2C / Memory Write：从窗口起点连续写入 RGB565，地址按 MADCTL 自动递增；§6.2.13，p115。
        if (!writeCommand(0x2A, columns, sizeof(columns), error) ||
            !writeCommand(0x2B, rows, sizeof(rows), error) ||
            !writeCommand(0x2C, wire.data(), bytes, error))
            return false;
    }
    return true;
}
} // namespace autoaimer
