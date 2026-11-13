#pragma once
#include <cstdint>
#include <utility>

namespace autoaimer {
struct FrameView {
    // NV12 的 UV 平面从 yStride × storageHeight 开始；步幅可能大于可见宽度。
    const std::uint8_t* data{};
    int width{}, height{}, yStride{}, storageHeight{};
    int dmaFd = -1;
    std::uint64_t timestampUs{};
};

enum class FrameStatus { Ready, Timeout, Error };

/**
 * 类名称：FrameLease
 * 类作用：管理一张借出的相机帧，离开作用域时自动归还资源。
 * 说明：相机帧的归还凭证：离开作用域自动释放，可移动但不可复制，避免归还两次。
 * 说明：FrameView 只是借用视图，其数据必须在对应 FrameLease 存活期间使用。
 */
class FrameLease {
public:
    using Release = void (*)(void*);
    /**
     * 方法名称：FrameLease::FrameLease（状态）
     * 方法作用：建立超时或错误结果，不接管图像资源。
     */
    explicit FrameLease(FrameStatus status, int code = 0) : status(status), code(code) {}
    /**
     * 方法名称：FrameLease::FrameLease（图像）
     * 方法作用：接管一张就绪帧的视图、所有者和归还回调。
     */
    FrameLease(FrameView view, void* owner, Release release)
        : status(FrameStatus::Ready), frame(view), owner(owner), release(release) {}
    /**
     * 方法名称：FrameLease::~FrameLease
     * 方法作用：有归还回调时调用一次，释放借用的相机帧。
     */
    ~FrameLease() {
        if (release)
            release(owner);
    }
    /**
     * 方法名称：FrameLease::FrameLease（复制）
     * 方法作用：禁止复制，避免一张帧被归还两次。
     */
    FrameLease(const FrameLease&) = delete;
    /**
     * 方法名称：FrameLease::operator=
     * 方法作用：禁止复制赋值，保持唯一归还责任。
     */
    FrameLease& operator=(const FrameLease&) = delete;
    /**
     * 方法名称：FrameLease::FrameLease（移动）
     * 方法作用：转移归还责任，并清空原对象的回调。
     */
    FrameLease(FrameLease&& other) noexcept
        : status(std::exchange(other.status, FrameStatus::Error)), code(other.code),
          frame(other.frame), owner(other.owner), release(std::exchange(other.release, nullptr)) {}

    /**
     * 方法名称：FrameLease::state
     * 方法作用：读取帧获取结果：就绪、超时或错误。
     */
    FrameStatus state() const { return status; }
    /**
     * 方法名称：FrameLease::errorCode
     * 方法作用：读取底层错误码。
     */
    int errorCode() const { return code; }
    /**
     * 方法名称：FrameLease::view
     * 方法作用：借用图像描述；应先确认状态为就绪，且不得超出凭证生命周期。
     */
    const FrameView& view() const { return frame; }

private:
    FrameStatus status;
    int code = 0;
    FrameView frame{};
    void* owner = nullptr;
    Release release = nullptr;
};
} // namespace autoaimer
