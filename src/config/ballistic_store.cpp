#include "config/ballistic_store.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sys/stat.h>

namespace autoaimer {
namespace {
namespace fs = std::filesystem;

bool number(const std::map<std::string, std::string>& fields, const std::string& name,
            double& value) {
    const auto found = fields.find(name);
    if (found == fields.end() || found->second.empty())
        return false;
    char* end = nullptr;
    errno = 0;
    value = std::strtod(found->second.c_str(), &end);
    return errno == 0 && end == found->second.c_str() + found->second.size() &&
           std::isfinite(value);
}

bool valid(const BallisticPreset& preset) {
    return preset.muzzleSpeedMetersPerSecond >= 1 &&
           preset.muzzleSpeedMetersPerSecond <= 100 &&
           preset.horizontalOffsetCentimeters >= -30 &&
           preset.horizontalOffsetCentimeters <= 30 &&
           preset.verticalOffsetCentimeters >= -30 && preset.verticalOffsetCentimeters <= 30 &&
           preset.horizontalAngleDegrees >= -15 && preset.horizontalAngleDegrees <= 15 &&
           preset.verticalAngleDegrees >= -15 && preset.verticalAngleDegrees <= 15 &&
           preset.dragCoefficient >= 0 && preset.dragCoefficient <= 1;
}

std::string key(std::size_t index, const char* field) {
    return "p" + std::to_string(index + 1) + "." + field;
}
} // namespace

BallisticLoadStatus loadBallisticPresets(
    const std::string& path, std::array<BallisticPreset, ballisticPresetCount>& presets,
    std::size_t& active, std::string& error) {
    if (path.empty())
        return BallisticLoadStatus::Missing;
    struct stat details {};
    if (stat(path.c_str(), &details) != 0) {
        if (errno == ENOENT)
            return BallisticLoadStatus::Missing;
        error = "Could not inspect ballistic preset file: " + path;
        return BallisticLoadStatus::Error;
    }
    std::ifstream input(path);
    if (!input) {
        error = "Could not open ballistic preset file: " + path;
        return BallisticLoadStatus::Error;
    }

    std::map<std::string, std::string> fields;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty())
            continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 == line.size() ||
            !fields.emplace(line.substr(0, separator), line.substr(separator + 1)).second) {
            error = "Malformed ballistic preset file: " + path;
            return BallisticLoadStatus::Error;
        }
    }
    if (!input.eof()) {
        error = "Malformed ballistic preset file: " + path;
        return BallisticLoadStatus::Error;
    }
    const auto format = fields.find("format");
    const bool version1 = format != fields.end() && format->second == "autoaimer-ballistics-v1";
    const bool version2 = format != fields.end() && format->second == "autoaimer-ballistics-v2";
    if (!version1 && !version2) {
        error = "Malformed ballistic preset file: " + path;
        return BallisticLoadStatus::Error;
    }

    double activeValue{};
    std::array<BallisticPreset, ballisticPresetCount> loaded{};
    if (!number(fields, "active", activeValue) || activeValue < 0 ||
        activeValue >= ballisticPresetCount || std::floor(activeValue) != activeValue) {
        error = "Invalid ballistic preset values in: " + path;
        return BallisticLoadStatus::Error;
    }
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        double quadratic{};
        auto& preset = loaded[index];
        bool parametersValid =
            number(fields, key(index, "speed"), preset.muzzleSpeedMetersPerSecond) &&
            number(fields, key(index, "drag"), preset.dragCoefficient) &&
            number(fields, key(index, "quadratic"), quadratic);
        if (version1) {
            preset.horizontalOffsetCentimeters = 0;
            preset.horizontalAngleDegrees = 0;
            parametersValid =
                parametersValid &&
                number(fields, key(index, "offset_cm"), preset.verticalOffsetCentimeters) &&
                number(fields, key(index, "angle_deg"), preset.verticalAngleDegrees);
        } else {
            parametersValid =
                parametersValid &&
                number(fields, key(index, "horizontal_offset_cm"),
                       preset.horizontalOffsetCentimeters) &&
                number(fields, key(index, "vertical_offset_cm"),
                       preset.verticalOffsetCentimeters) &&
                number(fields, key(index, "horizontal_angle_deg"),
                       preset.horizontalAngleDegrees) &&
                number(fields, key(index, "vertical_angle_deg"), preset.verticalAngleDegrees);
        }
        if (!parametersValid || (quadratic != 0 && quadratic != 1) || !valid(preset)) {
            error = "Invalid ballistic preset values in: " + path;
            return BallisticLoadStatus::Error;
        }
        preset.quadraticDrag = quadratic == 1;
    }
    presets = loaded;
    active = static_cast<std::size_t>(activeValue);
    return BallisticLoadStatus::Loaded;
}

bool saveBallisticPresets(const std::string& path,
                          const std::array<BallisticPreset, ballisticPresetCount>& presets,
                          std::size_t active, std::string& error) {
    if (path.empty() || active >= presets.size()) {
        error = "Invalid ballistic preset destination or active index";
        return false;
    }
    for (const auto& preset : presets) {
        if (!valid(preset)) {
            error = "Refusing to save invalid ballistic preset values";
            return false;
        }
    }

    const fs::path destination(path);
    std::error_code status;
    if (!destination.parent_path().empty())
        fs::create_directories(destination.parent_path(), status);
    if (status) {
        error = "Could not create ballistic preset directory: " + status.message();
        return false;
    }
    auto partial = destination;
    partial += ".part";
    std::ofstream output(partial, std::ios::trunc);
    if (!output) {
        error = "Could not open ballistic preset temporary file: " + partial.string();
        return false;
    }
    output << std::setprecision(17) << "format=autoaimer-ballistics-v2\nactive=" << active << '\n';
    for (std::size_t index = 0; index < presets.size(); ++index) {
        const auto& preset = presets[index];
        output << key(index, "speed") << '=' << preset.muzzleSpeedMetersPerSecond << '\n'
               << key(index, "horizontal_offset_cm") << '='
               << preset.horizontalOffsetCentimeters << '\n'
               << key(index, "vertical_offset_cm") << '='
               << preset.verticalOffsetCentimeters << '\n'
               << key(index, "horizontal_angle_deg") << '='
               << preset.horizontalAngleDegrees << '\n'
               << key(index, "vertical_angle_deg") << '=' << preset.verticalAngleDegrees << '\n'
               << key(index, "drag") << '=' << preset.dragCoefficient << '\n'
               << key(index, "quadratic") << '=' << (preset.quadraticDrag ? 1 : 0) << '\n';
    }
    output.close();
    if (!output) {
        error = "Could not finish ballistic preset temporary file: " + partial.string();
        fs::remove(partial, status);
        return false;
    }
    fs::rename(partial, destination, status);
    if (status) {
        error = "Could not replace ballistic preset file: " + status.message();
        fs::remove(partial, status);
        return false;
    }
    return true;
}
} // namespace autoaimer
