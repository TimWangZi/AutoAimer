#pragma once
#include <cstdint>
#include <string>

namespace autoaimer {
// 最终 RGB565 图像的借用视图；不分配内存，也不拥有 pixels 指向的画布。
struct OutputFrame {
    const std::uint16_t* pixels;
    int width, height;
};

/**
 * 类名称：Display
 * 类作用：定义同步显示接口，屏蔽具体面板与传输方式。
 */
class Display {
public:
    /**
     * 方法名称：Display::~Display
     * 方法作用：允许通过基类指针安全销毁具体显示实现。
     */
    virtual ~Display() = default;
    /**
     * 方法名称：Display::present
     * 方法作用：同步消费一张完整 RGB565 画布；失败时返回错误。
     * 说明：同步借用像素：present 返回前，调用方不能改写或释放这张画布。
     */
    virtual bool present(OutputFrame frame, std::string& error) = 0;
};

/**
 * 类名称：NullDisplay
 * 类作用：提供不访问硬件的空显示实现，供主机运行和图像导出使用。
 */
class NullDisplay final : public Display {
public:
    /**
     * 方法名称：NullDisplay::present
     * 方法作用：直接报告成功，不读取画布或访问硬件。
     */
    bool present(OutputFrame, std::string&) override { return true; }
};

/**
 * 方法名称：writePpm
 * 方法作用：将 RGB565 画布转换为 RGB888 并保存为 PPM 文件。
 */
bool writePpm(const std::string& path, OutputFrame frame);
} // namespace autoaimer
