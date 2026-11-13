#include "test_devices.h"
#include <array>
#include <cmath>
#include <iostream>

namespace {
bool near(double actual, double expected) {
    return std::abs(actual - expected) < 1e-12;
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}
} // namespace

int main() {
    using autoaimer::Key;
    autoaimer::test::ScriptedButtons buttons{Key::Up, Key::Down, Key::Left, Key::Right,
                                             Key::Confirm};
    constexpr std::array expected{Key::Up,    Key::Down,    Key::Left,
                                  Key::Right, Key::Confirm, Key::None};
    for (const auto key : expected) {
        if (buttons.poll().key != key)
            return fail("Scripted button order changed or did not finish at None");
    }

    autoaimer::NullButtonInput noButtons;
    if (noButtons.poll().key != Key::None)
        return fail("The no-hardware button adapter must report None");

    const auto now = autoaimer::Clock::now();
    autoaimer::test::TestImuSensor imu({0.2, 9.7, -0.1}, {12.0, -4.0, 1.5});
    autoaimer::test::TestDistanceSensor distance(8.5);

    auto data = autoaimer::pollSensorData(imu, distance, now);
    if (data.gravity.status != autoaimer::SampleStatus::Valid ||
        data.orientation.status != autoaimer::SampleStatus::Valid ||
        data.distanceMeters.status != autoaimer::SampleStatus::Unavailable ||
        !near(data.gravity.value.y, 9.7) || !near(data.orientation.value.rollDegrees, 12.0) ||
        !near(data.orientation.value.pitchDegrees, -4.0) || data.gravity.sampledAt != now ||
        data.orientation.sampledAt != now)
        return fail("The IMU fixture was not collected as one valid sample");

    distance.measure(now);
    data = autoaimer::pollSensorData(imu, distance, now);
    if (distance.measureCount != 1 || imu.pollCount != 2 ||
        data.distanceMeters.status != autoaimer::SampleStatus::Valid ||
        !near(data.distanceMeters.value, 8.5) || data.distanceMeters.sampledAt != now)
        return fail("A single distance request must publish one 8.5 m sample");

    autoaimer::NullImuSensor noImu;
    autoaimer::NullDistanceSensor noDistance;
    const auto unavailable = autoaimer::pollSensorData(noImu, noDistance, now);
    if (unavailable.gravity.status != autoaimer::SampleStatus::Unavailable ||
        unavailable.orientation.status != autoaimer::SampleStatus::Unavailable ||
        unavailable.distanceMeters.status != autoaimer::SampleStatus::Unavailable)
        return fail("No-hardware sensor adapters must remain unavailable");
}
