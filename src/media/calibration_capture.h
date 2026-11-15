#pragma once

#include "media/calibration_capture_status.h"
#include <filesystem>
#include <string>

namespace autoaimer {
class Camera;
class FrameLease;

/**
 * 类名称：CalibrationCapture
 * 类作用：管理一次运行中的标定采样会话、原始灰度图保存和采样结果。
 */
class CalibrationCapture {
public:
    /**
     * 方法名称：CalibrationCapture::CalibrationCapture
     * 方法作用：设置样本根目录，等首次取得有效样本帧时再创建会话目录。
     */
    explicit CalibrationCapture(const std::string& directory);
    /**
     * 方法名称：CalibrationCapture::capture
     * 方法作用：在租约有效期间同步保存原始亮度图，记录结果和成功张数。
     */
    SampleCaptureStatus capture(Camera& camera, const FrameLease& frame, std::string& error);
    /**
     * 方法名称：CalibrationCapture::count
     * 方法作用：返回本次运行中成功保存的样本数。
     */
    unsigned count() const { return sampleCount; }
    /**
     * 方法名称：CalibrationCapture::status
     * 方法作用：返回最近一次采样结果，供界面显示。
     */
    SampleCaptureStatus status() const { return lastStatus; }

private:
    std::filesystem::path directory;
    std::filesystem::path session;
    unsigned sampleCount = 0;
    SampleCaptureStatus lastStatus = SampleCaptureStatus::Idle;
};
} // namespace autoaimer
