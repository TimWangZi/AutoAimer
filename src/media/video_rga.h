#pragma once
#include "media/frame.h"
#include <rk_mpi_mb.h>
#include <string>

namespace autoaimer {
/**
 * 类名称：RgaVideo
 * 类作用：管理 SDK 的 RGA 转换资源，将 NV12 图像转换为显示尺寸的 RGB888。
 */
class RgaVideo {
public:
    /**
     * 方法名称：RgaVideo::RgaVideo
     * 方法作用：建立尚未申请媒体缓冲区的转换对象。
     */
    RgaVideo() = default;
    /**
     * 方法名称：RgaVideo::~RgaVideo
     * 方法作用：释放本对象申请的 SDK 媒体缓冲区。
     */
    ~RgaVideo();
    /**
     * 方法名称：RgaVideo::RgaVideo（复制）
     * 方法作用：禁止复制 SDK 缓冲区所有权。
     */
    RgaVideo(const RgaVideo&) = delete;
    /**
     * 方法名称：RgaVideo::operator=
     * 方法作用：禁止通过复制赋值共享 SDK 缓冲区。
     */
    RgaVideo& operator=(const RgaVideo&) = delete;

    /**
     * 方法名称：RgaVideo::init
     * 方法作用：申请并映射固定尺寸的 RGB888 转换缓冲区。
     */
    bool init(std::string& error);
    /**
     * 方法名称：RgaVideo::convert
     * 方法作用：同步裁剪、缩放并转换 NV12，刷新缓存后返回 RGB888 地址；失败返回空指针。
     */
    const std::uint8_t* convert(const FrameView& frame, std::string& error);

private:
    MB_BLK rgb = nullptr;
    int fd = -1;
    const std::uint8_t* address = nullptr;
};
} // namespace autoaimer
