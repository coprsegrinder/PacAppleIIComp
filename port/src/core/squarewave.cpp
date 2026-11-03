#include "squarewave.h"

#include <algorithm>

namespace pacman::core {

Pcm renderClicks(const std::vector<std::uint32_t>& clicks, std::int64_t totalCycles, int sampleRate, std::int16_t amplitude) {
    Pcm out;
    out.sampleRate = sampleRate;
    const auto sampleOf = [&](std::int64_t cycles) { return cycles * sampleRate / kCpuHz; };
    const std::int64_t lastClick = clicks.empty() ? 0 : static_cast<std::int64_t>(clicks.back());
    const std::int64_t length = sampleOf(std::max(totalCycles, lastClick)) + 1;   // exactly the length of the sound's freeze: consecutive sounds keep the original's spacing
    const std::int64_t releaseAt = std::min(sampleOf(lastClick) + sampleRate / 1000 + 1, length);   // 1 ms after the last edge (or the end)
    out.samples.assign(static_cast<std::size_t>(length), 0);
    if (clicks.empty()) return out;
    std::size_t next = 0;
    bool high = false;
    bool any = false;
    for (std::int64_t i = 0; i < length; ++i) {
        while (next < clicks.size() && sampleOf(static_cast<std::int64_t>(clicks[next])) <= i) {   // every click up to this sample flips the cone
            high = !high;
            any = true;
            ++next;
        }
        if (i >= releaseAt) {
            out.samples[static_cast<std::size_t>(i)] = 0;
        } else {
            out.samples[static_cast<std::size_t>(i)] = !any ? std::int16_t{0} : (high ? amplitude : static_cast<std::int16_t>(-amplitude));
        }
    }
    return out;
}

}  // namespace pacman::core
