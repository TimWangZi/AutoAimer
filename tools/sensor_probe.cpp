#include "sensors/icm42688_linux.h"
#include "sensors/sdm20_linux.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
    using namespace autoaimer;
    LinuxIcm426xxSensor imu;
    LinuxSdm20DistanceSensor distance;
    std::string error;
    const bool imuOpen = imu.open("/dev/i2c-3", error);
    if (!imuOpen)
        std::cerr << error << '\n';
    else
        std::cout << "imu_model=" << imu.modelName() << '\n';
    error.clear();
    const bool distanceOpen = distance.open("/dev/ttyS3", error);
    if (!distanceOpen)
        std::cerr << error << '\n';
    if (!imuOpen && !distanceOpen)
        return 1;

    const auto start = Clock::now();
    if (distanceOpen)
        distance.measure(start);
    auto nextDistanceRequest = start + std::chrono::milliseconds(300);
    bool imuValid = false;
    bool distanceValid = false;
    unsigned iteration = 0;
    while (Clock::now() - start < std::chrono::seconds(3)) {
        const auto now = Clock::now();
        const auto motion = imuOpen ? imu.poll(now) : ImuData{};
        const auto range = distanceOpen ? distance.poll(now) : Sample<double>{};
        if (imuOpen && motion.gravity.status == SampleStatus::Valid &&
            motion.orientation.status == SampleStatus::Valid) {
            imuValid = true;
            if (iteration % 10 == 0) {
                std::cout << "gravity_mps2=" << motion.gravity.value.x << ','
                          << motion.gravity.value.y << ',' << motion.gravity.value.z
                          << " orientation_deg=" << motion.orientation.value.rollDegrees << ','
                          << motion.orientation.value.pitchDegrees << ','
                          << motion.orientation.value.yawDegrees << '\n';
            }
        }
        if (range.status == SampleStatus::Valid) {
            distanceValid = true;
            std::cout << "distance_m=" << range.value << '\n';
            break;
        }
        if (distanceOpen && now >= nextDistanceRequest) {
            distance.measure(now);
            nextDistanceRequest = now + std::chrono::milliseconds(300);
        }
        ++iteration;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (imuOpen && !imuValid)
        std::cerr << "ICM-426xx did not produce a valid sample\n";
    if (distanceOpen && !distanceValid)
        std::cerr << "SDM20 did not produce a valid target distance\n";
    return imuOpen && imuValid && distanceOpen && distanceValid ? 0 : 2;
}
