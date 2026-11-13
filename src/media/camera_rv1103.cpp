#include "media/camera.h"
#include <rk_mpi_mb.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_vi.h>
#include <uAPI2/rk_aiq_user_api2_sysctl.h>
#include <cstdio>
#include <fstream>
#include <string_view>

namespace autoaimer {
namespace {
constexpr int device = 0;
constexpr int pipe = 0;
constexpr int channel = 0;
constexpr int width = 640;
constexpr int height = 480;
constexpr auto ispMode = RK_AIQ_WORKING_MODE_NORMAL;

bool check(int status, const char* step, std::string& error) {
    if (status == 0)
        return true;
    char code[16];
    std::snprintf(code, sizeof(code), "%#x", static_cast<unsigned>(status));
    error = std::string(step) + " failed (" + code + ")";
    return false;
}
} // namespace

// 初始化顺序参考 SDK 的 VI、ISP3 示例。只记录本应用已成功申请的资源，
// 中途失败时由析构按相反顺序释放，不接管其它进程的相机资源。
struct Camera::State {
    rk_aiq_sys_ctx_t* isp = nullptr;
    bool ispRunning = false;
    bool sys = false, dev = false, chn = false, leased = false;
    VIDEO_FRAME_INFO_S raw{};

    void release() {
        if (!leased)
            return;
        const auto status = RK_MPI_VI_ReleaseChnFrame(pipe, channel, &raw);
        if (status != RK_SUCCESS)
            std::fprintf(stderr, "RK_MPI_VI_ReleaseChnFrame: %#x\n", static_cast<unsigned>(status));
        leased = false;
    }

    ~State() {
        // 先归还借出的图像，再关闭 VI 通道和内存池。
        release();
        if (chn)
            RK_MPI_VI_DisableChn(pipe, channel);
        if (dev)
            RK_MPI_VI_DisableDev(device);
        if (sys) {
            RK_MPI_SYS_WaitFreeMB();
            RK_MPI_SYS_Exit();
        }
        if (ispRunning)
            rk_aiq_uapi2_sysctl_stop(isp, false);
        if (isp)
            rk_aiq_uapi2_sysctl_deinit(isp);
    }
};

Camera::Camera() = default;
Camera::~Camera() = default;

bool Camera::init(const std::string& iqDir, std::string& error) {
    auto next = std::make_unique<State>();
    rk_aiq_static_info_t sensor{};
    if (!check(rk_aiq_uapi2_sysctl_enumStaticMetasByPhyId(device, &sensor), "Enumerating sensor 0",
               error))
        return false;
    const char* name = sensor.sensor_info.sensor_name;
    const std::string_view sensorName(name);
    const char* iqFile;
    if (sensorName.find("sc3336") != std::string_view::npos)
        iqFile = "sc3336_CMK-OT2119-PC1_30IRC-F16.json";
    else if (sensorName.find("mis5001") != std::string_view::npos)
        iqFile = "mis5001_CMK-OT2115-PC1_30IRC-F16.json";
    else {
        error = std::string("Unsupported sensor in media graph: ") + name;
        return false;
    }
    if (!std::ifstream(iqDir + "/" + iqFile)) {
        error = std::string("Missing camera IQ file: ") + iqDir + "/" + iqFile;
        return false;
    }
    if (!check(rk_aiq_uapi2_sysctl_preInit_devBufCnt(name, "rkraw_rx", 2), "RKAIQ raw buffers",
               error))
        return false;
    next->isp = rk_aiq_uapi2_sysctl_init(name, iqDir.c_str(), nullptr, nullptr);
    if (!next->isp) {
        error = "RKAIQ initialization failed";
        return false;
    }
    if (!check(rk_aiq_uapi2_sysctl_prepare(next->isp, 0, 0, ispMode), "RKAIQ prepare", error) ||
        !check(rk_aiq_uapi2_sysctl_start(next->isp), "RKAIQ start", error))
        return false;
    next->ispRunning = true;
    if (!check(RK_MPI_SYS_Init(), "RK_MPI_SYS_Init", error))
        return false;
    next->sys = true;

    if (RK_MPI_VI_GetDevIsEnable(device) == RK_SUCCESS) {
        error = "VI device is already enabled; stop the existing camera service before running "
                "AutoAimer";
        return false;
    }
    VI_DEV_ATTR_S devAttr{};
    const auto devStatus = RK_MPI_VI_GetDevAttr(device, &devAttr);
    if (devStatus == RK_ERR_VI_NOT_CONFIG) {
        if (!check(RK_MPI_VI_SetDevAttr(device, &devAttr), "RK_MPI_VI_SetDevAttr", error))
            return false;
    } else if (!check(devStatus, "RK_MPI_VI_GetDevAttr", error))
        return false;

    if (!check(RK_MPI_VI_EnableDev(device), "RK_MPI_VI_EnableDev", error))
        return false;
    next->dev = true;
    VI_DEV_BIND_PIPE_S binding{};
    binding.u32Num = 1;
    binding.PipeId[0] = pipe;
    if (!check(RK_MPI_VI_SetDevBindPipe(device, &binding), "RK_MPI_VI_SetDevBindPipe", error))
        return false;

    VI_CHN_ATTR_S attrs{};
    attrs.stSize.u32Width = width;
    attrs.stSize.u32Height = height;
    attrs.stIspOpt.u32BufCount = 2;
    attrs.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    // VI 队列深度保持为 1；应用同一时刻最多借用一张相机帧。
    attrs.u32Depth = 1;
    attrs.enPixelFormat = RK_FMT_YUV420SP;
    attrs.enCompressMode = COMPRESS_MODE_NONE;
    attrs.stFrameRate.s32SrcFrameRate = -1;
    attrs.stFrameRate.s32DstFrameRate = -1;
    if (!check(RK_MPI_VI_SetChnAttr(pipe, channel, &attrs), "RK_MPI_VI_SetChnAttr", error) ||
        !check(RK_MPI_VI_EnableChn(pipe, channel), "RK_MPI_VI_EnableChn", error))
        return false;
    next->chn = true;
    state = std::move(next);
    return true;
}

FrameLease Camera::acquire(int timeoutMs) {
    if (!state || state->leased)
        return FrameLease(FrameStatus::Error);
    const auto status = RK_MPI_VI_GetChnFrame(pipe, channel, &state->raw, timeoutMs);
    if (status == RK_ERR_VI_BUF_EMPTY)
        return FrameLease(FrameStatus::Timeout, status);
    if (status != RK_SUCCESS)
        return FrameLease(FrameStatus::Error, status);

    state->leased = true;
    // RGA 直接读取 DMA-BUF，使用 VI 报告的实际存储步幅。
    const auto& frame = state->raw.stVFrame;
    const int fd = RK_MPI_MB_Handle2Fd(frame.pMbBlk);
    if (fd < 0 || frame.enPixelFormat != RK_FMT_YUV420SP ||
        frame.enCompressMode != COMPRESS_MODE_NONE) {
        state->release();
        return FrameLease(FrameStatus::Error, fd);
    }
    const FrameView view{nullptr,
                         static_cast<int>(frame.u32Width),
                         static_cast<int>(frame.u32Height),
                         static_cast<int>(frame.u32VirWidth),
                         static_cast<int>(frame.u32VirHeight),
                         fd,
                         frame.u64PTS};
    return FrameLease(view, state.get(), [](void* p) { static_cast<State*>(p)->release(); });
}
} // namespace autoaimer
