#include "compute/camera_calibration.h"
#include "media/video_geometry.h"
#include <cmath>
#include <iostream>

namespace {
bool near(double actual, double expected) {
    return std::isfinite(actual) && std::abs(actual - expected) < 1e-12;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    autoaimer::CameraCalibration calibration;
    std::string error;
    if (autoaimer::loadCameraCalibration(argv[1], calibration, error) !=
        autoaimer::CalibrationLoadStatus::Loaded) {
        std::cerr << error << '\n';
        return 1;
    }
    const auto camera = autoaimer::cameraForCrop(calibration, 640, 480, 80, 0, 480, 240, 240);
    if (calibration.imageWidth != 640 || calibration.imageHeight != 480 ||
        !near(camera.fx, 200) || !near(camera.fy, 210) || !near(camera.cx, 110) ||
        !near(camera.cy, 115) || !near(camera.k1, 0.01) || !near(camera.k2, -0.02) ||
        !near(camera.p1, 0.003) || !near(camera.p2, -0.004) || !near(camera.k3, 0.005) ||
        !near(calibration.rmsPixels, 0.123)) {
        std::cerr << "Calibration values or crop transform changed unexpectedly\n";
        return 1;
    }
    if (autoaimer::loadCameraCalibration("this-file-does-not-exist.conf", calibration, error) !=
        autoaimer::CalibrationLoadStatus::Missing) {
        std::cerr << "A missing optional calibration file must not be an error\n";
        return 1;
    }

    const auto geometry = autoaimer::makeVideoGeometry(640, 480, 0);
    const auto projected = autoaimer::cameraForProjection(calibration, geometry, 0, 240, 240);
    if (!near(projected.fx, camera.fx) || !near(projected.fy, camera.fy) ||
        !near(projected.cx, camera.cx) || !near(projected.cy, camera.cy)) {
        std::cerr << "Projection calibration did not match the base crop conversion\n";
        return 1;
    }
    const auto zoomed = autoaimer::cameraForProjection(calibration, geometry, 1, 240, 240);
    if (!near(zoomed.fx, 300) || !near(zoomed.fy, 315) || !near(zoomed.cx, 105) ||
        !near(zoomed.cy, 112.5)) {
        std::cerr << "Projection calibration did not follow the active zoom crop\n";
        return 1;
    }
    const auto alignedGeometry = autoaimer::makeVideoGeometry(640, 480, 478);
    const auto aligned =
        autoaimer::cameraForProjection(calibration, alignedGeometry, 1, 240, 240);
    const double alignedScale = 240.0 / 318.0;
    if (!near(aligned.fx, 400 * alignedScale) ||
        !near(aligned.fy, 420 * alignedScale) ||
        !near(aligned.cx, (300 - 160) * alignedScale) ||
        !near(aligned.cy, (230 - 80) * alignedScale)) {
        std::cerr << "Projection calibration ignored the even-aligned zoom crop\n";
        return 1;
    }
    const auto fallback = autoaimer::cameraForProjection(std::nullopt, geometry, 1, 240, 240);
    if (!near(fallback.fx, 300) || !near(fallback.fy, 300) || !near(fallback.cx, 120) ||
        !near(fallback.cy, 120)) {
        std::cerr << "Default trajectory projection did not follow the active zoom crop\n";
        return 1;
    }
    const auto tiny = autoaimer::cameraForProjection(
        calibration, autoaimer::makeVideoGeometry(640, 480, 2), 3, 240, 240);
    if (!near(tiny.fx, 600) || !near(tiny.fy, 600) || !near(tiny.cx, 120) ||
        !near(tiny.cy, 120)) {
        std::cerr << "Zero-sized zoom crop produced an invalid projection model\n";
        return 1;
    }
}
