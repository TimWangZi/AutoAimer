#include "media/frame.h"
#include <cstddef>
#include <ostream>

namespace autoaimer {
bool writeLumaPgm(std::ostream& output, const FrameView& frame) {
    if (!frame.data || frame.width <= 0 || frame.height <= 0 || frame.yStride < frame.width ||
        frame.storageHeight < frame.height)
        return false;

    output << "P5\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (int y = 0; y < frame.height && output; ++y) {
        const auto* row = frame.data + static_cast<std::size_t>(y) * frame.yStride;
        output.write(reinterpret_cast<const char*>(row), frame.width);
    }
    return static_cast<bool>(output);
}
} // namespace autoaimer
