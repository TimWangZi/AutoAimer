#pragma once
#include "io/gpio.h"
#include "io/spi.h"
#include "media/display.h"
#include "ui/screen_geometry.h"
#include <array>
#include <cstddef>
#include <functional>

namespace autoaimer {
/**
 * 类名称：Gc9a01Display
 * 类作用：负责 GC9A01 的命令、DC/复位时序和条带像素打包。
 * 说明：借用通用 SPI 与 GPIO 接口，不依赖 Linux；硬件对象必须比本对象活得久。
 *       init 在启动线程中完成，之后由唯一的显示线程调用 present。
 */
class Gc9a01Display final : public Display {
public:
    using DelayMilliseconds = std::function<void(unsigned)>;
    static constexpr int width = outputWidth, height = outputHeight;
    /**
     * 方法名称：Gc9a01Display::Gc9a01Display
     * 方法作用：借用 SPI、DC、复位引脚并预计算 30 个圆屏条带窗口。
     * 说明：默认延时正常等待；测试可注入记录时间的回调，刷新过程不调用它。
     */
    Gc9a01Display(SpiDevice& spi, GpioOutput& dc, GpioOutput& reset,
                  DelayMilliseconds delay = waitMilliseconds);
    /**
     * 方法名称：Gc9a01Display::init
     * 方法作用：执行复位和厂家初始化序列，清屏后打开显示。
     */
    bool init(std::string& error);
    /**
     * 方法名称：Gc9a01Display::present
     * 方法作用：按条带打包 RGB565 高低字节并同步送屏，遇错立即结束。
     */
    bool present(OutputFrame frame, std::string& error) override;

private:
    /**
     * 方法名称：Gc9a01Display::writeCommand
     * 方法作用：按 DC 低电平发送命令，再按 DC 高电平发送可选的数据块。
     * 说明：每一步失败立即返回；无参数命令不切换到数据阶段。
     */
    bool writeCommand(std::uint8_t command, const std::uint8_t* data, std::size_t size,
                      std::string& error);
    /**
     * 方法名称：Gc9a01Display::waitMilliseconds
     * 方法作用：等待面板初始化所需的毫秒数。
     */
    static void waitMilliseconds(unsigned milliseconds);
    SpiDevice& spi;
    GpioOutput& dc;
    GpioOutput& reset;
    DelayMilliseconds delay;
    // 固定 8 行一块；按最宽行计算也不会越过 spidev 的 4096 字节限制。
    static constexpr int rowsPerBlock = 4096 / (width * 2);
    struct Window {
        int left, top, right, bottom;
    }; // 两端均包含。
    std::array<Window, (height + rowsPerBlock - 1) / rowsPerBlock> windows{};
    std::array<std::uint8_t, 4096> wire{};
};
} // namespace autoaimer
