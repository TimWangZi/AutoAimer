#pragma once
#include "compute/trajectory.h"
#include <optional>
#include <string>

namespace autoaimer {
struct VideoGeometry;
/**
 * 类名称：CameraCalibration
 * 类作用：保存相机原始分辨率下的内参与 Brown 畸变参数。
 */
struct CameraCalibration {
    int imageWidth{};
    int imageHeight{};
    CameraModel camera;
    double rmsPixels{};
};

enum class CalibrationLoadStatus { Missing, Loaded, Error };

/**
 * 方法名称：loadCameraCalibration
 * 方法作用：读取主机标定工具导出的轻量键值配置；缺少文件不视为错误。
 */
CalibrationLoadStatus loadCameraCalibration(const std::string& path, CameraCalibration& result,
                                            std::string& error);

/**
 * 方法名称：cameraForCrop
 * 方法作用：把原图内参转换到一次居中裁剪和缩放后的显示坐标，畸变系数保持不变。
 */
CameraModel cameraForCrop(const CameraCalibration& calibration, int sourceWidth, int sourceHeight,
                          int cropX, int cropY, int cropSize, int targetWidth, int targetHeight);

/**
 * 方法名称：cameraForProjection
 * 方法作用：根据原始标定参数和当前裁剪窗口生成轨迹投影使用的相机模型。
 * 说明：该模型只用于三维轨迹投影，不修改相机图像。
 */
CameraModel cameraForProjection(const std::optional<CameraCalibration>& calibration,
                                const VideoGeometry& video, unsigned zoomLevel, int targetWidth,
                                int targetHeight);
} // namespace autoaimer
