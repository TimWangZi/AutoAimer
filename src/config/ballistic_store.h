#pragma once
#include "compute/ballistic_preset.h"
#include <array>
#include <string>

namespace autoaimer {
enum class BallisticLoadStatus { Missing, Loaded, Error };

/**
 * 方法名称：loadBallisticPresets
 * 方法作用：读取已保存的三套弹道预设和当前选择；缺少文件时保留调用方默认值。
 */
BallisticLoadStatus loadBallisticPresets(
    const std::string& path, std::array<BallisticPreset, ballisticPresetCount>& presets,
    std::size_t& active, std::string& error);

/**
 * 方法名称：saveBallisticPresets
 * 方法作用：先写临时文件，再通过同目录重命名原子替换弹道预设配置。
 */
bool saveBallisticPresets(const std::string& path,
                          const std::array<BallisticPreset, ballisticPresetCount>& presets,
                          std::size_t active, std::string& error);
} // namespace autoaimer
