#include "media/calibration_capture.h"
#include "media/camera.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
namespace fs = std::filesystem;
using autoaimer::SampleCaptureStatus;

bool hasOriginalLuma(const fs::path& path, const autoaimer::FrameView& frame) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    const std::string saved{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>{}};
    std::string expected = "P5\n" + std::to_string(frame.width) + ' ' +
                           std::to_string(frame.height) + "\n255\n";
    for (int row = 0; row < frame.height; ++row)
        expected.append(reinterpret_cast<const char*>(frame.data + row * frame.yStride),
                        frame.width);
    return saved == expected;
}

bool captureAndCheck(autoaimer::CalibrationCapture& capture, autoaimer::Camera& camera,
                     const fs::path& path, unsigned count, std::string& error) {
    const auto frame = camera.acquire(0);
    return frame.state() == autoaimer::FrameStatus::Ready &&
           capture.capture(camera, frame, error) == SampleCaptureStatus::Saved &&
           capture.status() == SampleCaptureStatus::Saved && capture.count() == count &&
           hasOriginalLuma(path, frame.view());
}

int fail(const std::string& message, const std::string& error = {}) {
    std::cerr << message << '\n';
    if (!error.empty())
        std::cerr << error << '\n';
    return 1;
}

int checkCapture(const fs::path& workspace) {
    const auto samples = workspace / "samples";
    const auto previousSession = samples / "session-1";
    const auto session = samples / "session-2";
    const auto first = session / "sample-1.pgm";
    const auto second = session / "sample-2.pgm";
    const auto partial = session / "sample-2.pgm.part";
    autoaimer::CalibrationCapture capture(samples.string());
    autoaimer::Camera camera;
    std::string error;
    if (!camera.init("", error))
        return fail("Could not initialize the simulated camera", error);

    const auto noFrame = autoaimer::FrameLease(autoaimer::FrameStatus::Timeout);
    if (capture.status() != SampleCaptureStatus::Idle || capture.count() != 0 ||
        fs::exists(samples) ||
        capture.capture(camera, noFrame, error) != SampleCaptureStatus::NoFrame ||
        capture.status() != SampleCaptureStatus::NoFrame || capture.count() != 0 ||
        fs::exists(samples))
        return fail("Capture without a frame must not create a session or increase the count");

    fs::create_directories(previousSession);
    if (!captureAndCheck(capture, camera, first, 1, error) || !fs::is_empty(previousSession))
        return fail("First sample must preserve raw luma and start a new session", error);

    // 用同名目录让最终重命名失败，验证已经写入的临时文件会被清理。
    fs::create_directory(second);
    {
        const auto frame = camera.acquire(0);
        if (frame.state() != autoaimer::FrameStatus::Ready ||
            capture.capture(camera, frame, error) != SampleCaptureStatus::Failed ||
            capture.status() != SampleCaptureStatus::Failed || capture.count() != 1 ||
            error.empty() || fs::exists(partial) || !fs::is_directory(second))
            return fail("Failed save must preserve the sample count and remove the partial file",
                        error);
    }

    fs::remove(second);
    error.clear();
    if (!captureAndCheck(capture, camera, second, 2, error) || !fs::exists(first) ||
        fs::exists(partial))
        return fail("Retry must save the next sample in the same session", error);

    for (const auto& path : {first, second, session, previousSession, samples, workspace})
        fs::remove(path);
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2)
        return 2;

    try {
        // 每次运行使用独占目录，前次失败留下的文件不影响重跑。
        fs::path workspace;
        for (unsigned index = 1; ; ++index) {
            workspace = std::string(argv[1]) + '-' + std::to_string(index);
            if (fs::create_directory(workspace))
                break;
        }
        return checkCapture(workspace);
    } catch (const fs::filesystem_error& error) {
        return fail("Could not manage the capture test files", error.what());
    }
}
