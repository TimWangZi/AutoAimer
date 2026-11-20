#include "app.h"
#ifdef AUTOAIMER_SPI_DISPLAY
#include "input/button_linux.h"
#include "io/gpio_linux.h"
#include "io/spi_linux.h"
#include "media/display_gc9a01.h"
#endif
#ifdef AUTOAIMER_SENSOR_HARDWARE
#include "sensors/icm42688_linux.h"
#include "sensors/sdm20_linux.h"
#endif
#include <atomic>
#include <charconv>
#include <csignal>
#include <iostream>
#include <string_view>

namespace {
// 退出信号可能递送到任一线程；只做无锁原子写，随后由主线程正常收尾。
static_assert(std::atomic<bool>::is_always_lock_free, "Signal flag must be lock-free");
std::atomic<bool> keepRunning{true};
void stop(int) {
    keepRunning.store(false, std::memory_order_relaxed);
}
bool running() {
    return keepRunning.load(std::memory_order_relaxed);
}

void usage() {
#if defined(AUTOAIMER_BOARD) || defined(AUTOAIMER_SPI_DISPLAY)
    std::cerr << "Usage: autoaimer [--frames N] [--ppm PATH]";
#else
    std::cerr << "Usage: autoaimer_sim [--frames N] [--ppm PATH]";
#endif
#ifdef AUTOAIMER_BOARD
    std::cerr << " [--iq-dir PATH]";
#endif
    std::cerr << " [--crop-size EVEN_PIXELS]";
    std::cerr << " [--calibration-dir PATH]";
    std::cerr << " [--camera-calibration PATH]";
    std::cerr << " [--ballistic-presets PATH]";
#ifdef AUTOAIMER_SPI_DISPLAY
    std::cerr << " [--spi-hz 100000..40000000] [--buttons]";
#endif
#ifdef AUTOAIMER_SENSOR_HARDWARE
    std::cerr << " [--sensors]";
#endif
    std::cerr << '\n';
}

bool parse(int argc, char** argv, autoaimer::Options& options, std::uint32_t& spiHz,
           bool& enableButtons, bool& enableSensors) {
    (void)spiHz;
    (void)enableButtons;
    (void)enableSensors;
#if !defined(AUTOAIMER_BOARD) && !defined(AUTOAIMER_SPI_DISPLAY)
    options.frames = 60;
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string_view key = argv[i];
#ifdef AUTOAIMER_SENSOR_HARDWARE
        if (key == "--sensors") {
            enableSensors = true;
            continue;
        }
#endif
#ifdef AUTOAIMER_SPI_DISPLAY
        if (key == "--buttons") {
            enableButtons = true;
            continue;
        }
        if (key == "--spi-hz" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            const auto end = value.data() + value.size();
            const auto result = std::from_chars(value.data(), end, spiHz);
            if (result.ec == std::errc{} && result.ptr == end && spiHz >= 100000 &&
                spiHz <= 40000000)
                continue;
            return false;
        }
#endif
#ifdef AUTOAIMER_BOARD
        if (key == "--iq-dir" && i + 1 < argc) {
            options.iqDir = argv[++i];
            continue;
        }
#endif
        if (key == "--calibration-dir" && i + 1 < argc) {
            options.calibrationDirectory = argv[++i];
            continue;
        }
        if (key == "--camera-calibration" && i + 1 < argc) {
            options.cameraCalibrationFile = argv[++i];
            continue;
        }
        if (key == "--ballistic-presets" && i + 1 < argc) {
            options.ballisticPresetFile = argv[++i];
            continue;
        }
        if (key == "--crop-size" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            const auto end = value.data() + value.size();
            const auto result = std::from_chars(value.data(), end, options.cropSize);
            if (result.ec == std::errc{} && result.ptr == end && options.cropSize > 0 &&
                options.cropSize % 2 == 0)
                continue;
            return false;
        }
        if (key == "--ppm" && i + 1 < argc) {
            options.ppm = argv[++i];
            continue;
        }
        if (key == "--frames" && i + 1 < argc) {
            const std::string_view count = argv[++i];
            const auto end = count.data() + count.size();
            const auto result = std::from_chars(count.data(), end, options.frames);
            if (result.ec == std::errc{} && result.ptr == end && options.frames > 0)
                continue;
        }
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        usage();
        return 0;
    }
    autoaimer::Options options;
#ifdef AUTOAIMER_BOARD
    options.calibrationDirectory = "/userdata/calibration-samples";
    options.cameraCalibrationFile = "/userdata/autoaimer/camera-calibration.conf";
    options.ballisticPresetFile = "/userdata/autoaimer/ballistic-presets.conf";
#endif
    std::uint32_t spiHz = 40000000;
    bool enableButtons = false;
    bool enableSensors = false;
    if (!parse(argc, argv, options, spiHz, enableButtons, enableSensors)) {
        usage();
        return 2;
    }
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    autoaimer::NullButtonInput nullButtons;
    autoaimer::ButtonInput* buttons = &nullButtons;
#ifdef AUTOAIMER_SPI_DISPLAY
    // 硬件先创建、最后销毁；App 退出时会先等待显示线程结束。
    autoaimer::LinuxSpiDevice spi;
    autoaimer::LinuxGpioOutput dc, reset;
    autoaimer::Gc9a01Display display(spi, dc, reset);
    std::string error;
    autoaimer::LinuxButtonInput gpioButtons;
    if (enableButtons) {
        if (!gpioButtons.open(error)) {
            std::cerr << error << '\n';
            return 1;
        }
        buttons = &gpioButtons;
    }
    // 标准 Pico：DC 为物理脚 4 / GPIO1_C7，RESET 为脚 5 / GPIO1_C6。
    // gpio1 是 bank 标签，23/22 是 bank 内偏移，不是全局 GPIO 编号。
    if (!spi.open({"/dev/spidev0.0", spiHz}, error) ||
        !dc.open("gpio1", 2 * 8 + 7, true, "autoaimer-dc", error) ||
        !reset.open("gpio1", 2 * 8 + 6, true, "autoaimer-reset", error) || !display.init(error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cerr << "GC9A01 240x240, SPI0 mode 0, " << spiHz << " Hz\n";
#else
    autoaimer::NullDisplay display;
#endif
    autoaimer::NullImuSensor nullImu;
    autoaimer::NullDistanceSensor nullDistance;
    autoaimer::ImuSensor* imu = &nullImu;
    autoaimer::DistanceSensor* distance = &nullDistance;
#ifdef AUTOAIMER_SENSOR_HARDWARE
    autoaimer::LinuxIcm426xxSensor boardImu;
    autoaimer::LinuxSdm20DistanceSensor boardDistance;
    if (enableSensors) {
        std::string sensorError;
        if (boardImu.open("/dev/i2c-3", sensorError)) {
            imu = &boardImu;
            std::cerr << boardImu.modelName() << " on I2C3\n";
        } else {
            std::cerr << "ICM-426xx unavailable: " << sensorError << '\n';
        }
        sensorError.clear();
        if (boardDistance.open("/dev/ttyS3", sensorError)) {
            distance = &boardDistance;
            std::cerr << "SDM20 on UART3 at 460800 baud\n";
        } else {
            std::cerr << "SDM20 unavailable: " << sensorError << '\n';
        }
    }
#endif
    autoaimer::App app(display, *buttons, *imu, *distance);
    return app.run(options, running);
}
