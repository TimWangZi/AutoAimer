#include "compute/camera_calibration.h"
#include "media/video_geometry.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sys/stat.h>

namespace autoaimer {
namespace {
bool number(const std::map<std::string, std::string>& fields, const char* name, double& value) {
    const auto found = fields.find(name);
    if (found == fields.end() || found->second.empty())
        return false;
    char* end = nullptr;
    errno = 0;
    value = std::strtod(found->second.c_str(), &end);
    return errno == 0 && end == found->second.c_str() + found->second.size() &&
           std::isfinite(value);
}

bool integer(const std::map<std::string, std::string>& fields, const char* name, int& value) {
    double parsed{};
    if (!number(fields, name, parsed) || parsed < 1 || parsed > 65535 || std::floor(parsed) != parsed)
        return false;
    value = static_cast<int>(parsed);
    return true;
}
} // namespace

CalibrationLoadStatus loadCameraCalibration(const std::string& path, CameraCalibration& result,
                                            std::string& error) {
    if (path.empty())
        return CalibrationLoadStatus::Missing;

    struct stat details {};
    if (stat(path.c_str(), &details) != 0) {
        if (errno == ENOENT)
            return CalibrationLoadStatus::Missing;
        error = "Could not inspect camera calibration file: " + path;
        return CalibrationLoadStatus::Error;
    }
    std::ifstream input(path);
    if (!input) {
        error = "Could not open camera calibration file: " + path;
        return CalibrationLoadStatus::Error;
    }

    std::map<std::string, std::string> fields;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty())
            continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 == line.size() ||
            !fields.emplace(line.substr(0, separator), line.substr(separator + 1)).second) {
            error = "Malformed camera calibration file: " + path;
            return CalibrationLoadStatus::Error;
        }
    }
    if (!input.eof()) {
        error = "Could not read camera calibration file: " + path;
        return CalibrationLoadStatus::Error;
    }

    const auto format = fields.find("format");
    CameraCalibration next;
    if (format == fields.end() || format->second != "autoaimer-camera-v1" ||
        !integer(fields, "image_width", next.imageWidth) ||
        !integer(fields, "image_height", next.imageHeight) ||
        !number(fields, "fx", next.camera.fx) || !number(fields, "fy", next.camera.fy) ||
        !number(fields, "cx", next.camera.cx) || !number(fields, "cy", next.camera.cy) ||
        !number(fields, "k1", next.camera.k1) || !number(fields, "k2", next.camera.k2) ||
        !number(fields, "p1", next.camera.p1) || !number(fields, "p2", next.camera.p2) ||
        !number(fields, "k3", next.camera.k3) ||
        !number(fields, "rms_pixels", next.rmsPixels) || next.camera.fx <= 0 ||
        next.camera.fy <= 0 || next.rmsPixels < 0) {
        error = "Invalid camera calibration values in: " + path;
        return CalibrationLoadStatus::Error;
    }
    result = next;
    return CalibrationLoadStatus::Loaded;
}

CameraModel cameraForCrop(const CameraCalibration& calibration, int sourceWidth, int sourceHeight,
                          int cropX, int cropY, int cropSize, int targetWidth, int targetHeight) {
    const double sourceScaleX = static_cast<double>(sourceWidth) / calibration.imageWidth;
    const double sourceScaleY = static_cast<double>(sourceHeight) / calibration.imageHeight;
    const double targetScaleX = static_cast<double>(targetWidth) / cropSize;
    const double targetScaleY = static_cast<double>(targetHeight) / cropSize;
    CameraModel result = calibration.camera;
    result.fx *= sourceScaleX * targetScaleX;
    result.fy *= sourceScaleY * targetScaleY;
    result.cx = (result.cx * sourceScaleX - cropX) * targetScaleX;
    result.cy = (result.cy * sourceScaleY - cropY) * targetScaleY;
    return result;
}

CameraModel cameraForProjection(const std::optional<CameraCalibration>& calibration,
                                const VideoGeometry& video, unsigned zoomLevel, int targetWidth,
                                int targetHeight) {
    CameraModel result;
    const bool geometryReady = video.sourceWidth > 0 && video.sourceHeight > 0 &&
                               video.baseCropSize > 0 &&
                               video.baseCropSize <= std::min(video.sourceWidth, video.sourceHeight);
    const int cropSize = geometryReady ? zoomedCropSize(video.baseCropSize, zoomLevel) : 0;
    if (cropSize <= 0) {
        const double scale = digitalZoomScale(0, zoomLevel);
        result.fx *= scale;
        result.fy *= scale;
        result.cx = targetWidth / 2.0 + (result.cx - targetWidth / 2.0) * scale;
        result.cy = targetHeight / 2.0 + (result.cy - targetHeight / 2.0) * scale;
        return result;
    }

    const auto crop = centeredSquare(video.sourceWidth, video.sourceHeight, cropSize);
    if (calibration)
        return cameraForCrop(*calibration, video.sourceWidth, video.sourceHeight, crop.x, crop.y,
                             crop.size, targetWidth, targetHeight);

    const double scale = digitalZoomScale(video.baseCropSize, zoomLevel);
    result.fx *= scale;
    result.fy *= scale;
    result.cx = digitalZoomCoordinate(result.cx, video.sourceWidth, video.baseCropSize, targetWidth,
                                      zoomLevel);
    result.cy = digitalZoomCoordinate(result.cy, video.sourceHeight, video.baseCropSize,
                                      targetHeight, zoomLevel);
    return result;
}
} // namespace autoaimer
