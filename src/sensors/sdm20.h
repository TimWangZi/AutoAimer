#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace autoaimer {
enum class Sdm20ReadingStatus { Distance, NoTarget };

struct Sdm20Reading {
    Sdm20ReadingStatus status = Sdm20ReadingStatus::NoTarget;
    double meters{};
};

/**
 * 类名称：Sdm20Parser
 * 类作用：从任意分段的 UART 字节流中恢复 SDM20 四字节测距帧。
 */
class Sdm20Parser {
public:
    /**
     * 方法名称：Sdm20Parser::feed
     * 方法作用：接收一个字节；得到完整且校验正确的帧时返回最新测距结果。
     */
    std::optional<Sdm20Reading> feed(std::uint8_t byte);

    /**
     * 方法名称：Sdm20Parser::reset
     * 方法作用：丢弃尚未组成完整帧的字节。
     */
    void reset();

private:
    std::array<std::uint8_t, 4> frame{};
    std::size_t size{};
};
} // namespace autoaimer
