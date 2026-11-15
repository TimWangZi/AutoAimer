#include "media/calibration_capture.h"
#include "media/camera.h"
#include <fstream>

namespace autoaimer {
namespace {
namespace fs = std::filesystem;

fs::path createSampleSession(const fs::path& directory, std::string& error) {
    std::error_code status;
    fs::create_directories(directory, status);
    for (unsigned index = 1; !status; ++index) {
        const auto session = directory / ("session-" + std::to_string(index));
        if (fs::create_directory(session, status))
            return session;
    }
    error = "Could not create sample session in " + directory.string() + ": " + status.message();
    return {};
}

bool saveSample(Camera& camera, const FrameView& frame, const fs::path& path, std::string& error) {
    // PGM 写完并关闭后才公布正式文件，避免读取到半张样本。
    auto partial = path;
    partial += ".part";
    std::ofstream output(partial, std::ios::binary);
    if (!output) {
        error = "Could not open " + partial.string();
        return false;
    }
    const bool saved = camera.saveLuma(output, frame, error);
    output.close();

    std::error_code status;
    if (saved && output) {
        fs::rename(partial, path, status);
        if (!status)
            return true;
        error = "Could not save " + path.string() + ": " + status.message();
    } else if (saved) {
        error = "Could not finish writing " + path.string();
    }
    fs::remove(partial, status);
    return false;
}
} // namespace

CalibrationCapture::CalibrationCapture(const std::string& directory) : directory(directory) {}

SampleCaptureStatus CalibrationCapture::capture(Camera& camera, const FrameLease& frame,
                                               std::string& error) {
    if (frame.state() != FrameStatus::Ready) {
        lastStatus = SampleCaptureStatus::NoFrame;
        return lastStatus;
    }

    if (session.empty())
        session = createSampleSession(directory, error);
    lastStatus = SampleCaptureStatus::Failed;
    if (!session.empty()) {
        const auto path = session / ("sample-" + std::to_string(sampleCount + 1) + ".pgm");
        if (saveSample(camera, frame.view(), path, error)) {
            ++sampleCount;
            lastStatus = SampleCaptureStatus::Saved;
        }
    }
    return lastStatus;
}
} // namespace autoaimer
