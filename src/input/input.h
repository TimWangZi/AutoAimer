#pragma once

namespace autoaimer {
enum class Key { None, Confirm, Up, Down, Left, Right };
struct Input {
    Key key = Key::None;
};

/**
 * 类名称：ButtonInput
 * 类作用：定义消抖后的单次按键事件读取接口。
 */
class ButtonInput {
public:
    /**
     * 方法名称：ButtonInput::~ButtonInput
     * 方法作用：允许通过基类指针安全销毁按键实现。
     */
    virtual ~ButtonInput() = default;
    /**
     * 方法名称：ButtonInput::poll
     * 方法作用：读取本轮单次按键事件，无新事件时返回 Key::None。
     * 说明：返回消抖后的单次按键事件；按住按钮不应在每帧重复触发。
     */
    virtual Input poll() = 0;
};

/**
 * 类名称：NullButtonInput
 * 类作用：在实体按键未接入时返回无按键事件。
 * 说明：五个实体按键接入前使用此空接口，始终返回无按键。
 */
class NullButtonInput final : public ButtonInput {
public:
    /**
     * 方法名称：NullButtonInput::poll
     * 方法作用：返回无按键事件。
     */
    Input poll() override { return {}; }
};
} // namespace autoaimer
