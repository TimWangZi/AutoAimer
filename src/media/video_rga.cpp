#include "media/video_rga.h"
#include "media/video.h"
#include <im2d.hpp>
#include <rk_mpi_sys.h>

namespace autoaimer {
RgaVideo::~RgaVideo() {
    if (rgb)
        RK_MPI_SYS_MmzFree(rgb);
}

bool RgaVideo::init(std::string& error) {
    if (RK_MPI_SYS_MmzAlloc_Cached(&rgb, "autoaimer_rgb", nullptr,
                                   outputWidth * outputHeight * 3) != RK_SUCCESS) {
        error = "RKMPI could not allocate the RGA output buffer";
        return false;
    }
    fd = RK_MPI_MB_Handle2Fd(rgb);
    address = static_cast<const std::uint8_t*>(RK_MPI_MB_Handle2VirAddr(rgb));
    if (fd < 0 || !address) {
        error = "RKMPI could not map the RGA output buffer";
        return false;
    }
    return true;
}

const std::uint8_t* RgaVideo::convert(const FrameView& frame, SquareCrop crop, std::string& error) {
    // 借用两个 DMA-BUF；一次同步 improcess 完成裁剪、缩放与颜色转换。
    auto src = wrapbuffer_fd_t(frame.dmaFd, frame.width, frame.height, frame.yStride,
                               frame.storageHeight, RK_FORMAT_YCbCr_420_SP);
    auto dst = wrapbuffer_fd_t(fd, outputWidth, outputHeight, outputWidth, outputHeight,
                               RK_FORMAT_RGB_888);
    src.color_space_mode = IM_YUV_TO_RGB_BT601_LIMIT;
    const im_rect sourceRect{crop.x, crop.y, crop.size, crop.size};
    const im_rect outputRect{0, 0, outputWidth, outputHeight};
    const auto supported = imcheck_t(src, dst, {}, sourceRect, outputRect, {}, IM_SYNC);
    if (supported != IM_STATUS_NOERROR) {
        error = std::string("RGA rejected the NV12/RGB888 buffers: ") + imStrError(supported);
        return nullptr;
    }
    const auto status = improcess(src, dst, {}, sourceRect, outputRect, {}, IM_SYNC);
    if (status != IM_STATUS_SUCCESS) {
        error = std::string("RGA resize and color conversion failed: ") + imStrError(status);
        return nullptr;
    }
    // 先同步 DMA 写入的缓存，再交给 CPU 读取并混合 UI。
    if (RK_MPI_SYS_MmzFlushCache(rgb, RK_TRUE) != RK_SUCCESS) {
        error = "RKMPI cache synchronization after RGA failed";
        return nullptr;
    }
    return address;
}
} // namespace autoaimer
