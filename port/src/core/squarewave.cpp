#include "squarewave.h"

#include <algorithm>
#include <cmath>

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

Pcm renderSpeaker(const std::vector<std::uint32_t>& clicks, std::int64_t totalCycles, int sampleRate, std::int16_t amplitude, const SpeakerModel& m) {
    Pcm out;
    out.sampleRate = sampleRate;
    const auto sampleOf = [&](std::int64_t cycles) { return cycles * sampleRate / kCpuHz; };
    const std::int64_t lastClick = clicks.empty() ? 0 : static_cast<std::int64_t>(clicks.back());
    const std::int64_t nominal = sampleOf(std::max(totalCycles, lastClick)) + 1;
    const std::int64_t tail = clicks.empty() ? 0 : static_cast<std::int64_t>(m.tailMs) * sampleRate / 1000;
    out.nominal = static_cast<std::size_t>(nominal);
    out.samples.assign(static_cast<std::size_t>(nominal + tail), 0);
    if (clicks.empty()) return out;
    const double pi = 3.14159265358979323846;
    const double dt = 1.0 / sampleRate;
    const double hpA = 1.0 / (1.0 + 2.0 * pi * m.couplingHz * dt);       // first-order high-pass
    const double lpB = 1.0 - std::exp(-2.0 * pi * m.rolloffHz * dt);      // first-order low-pass
    const double w0 = 2.0 * pi * m.bodyHz * dt;                             // band-pass, constant 0 dB peak (RBJ)
    const double alpha = std::sin(w0) / (2.0 * m.bodyQ);
    const double a0 = 1.0 + alpha, a1 = -2.0 * std::cos(w0) / a0, a2 = (1.0 - alpha) / a0, b0 = alpha / a0, b2 = -alpha / a0;
    double hpIn = 0, hpOut = 0, lp = 0, y1 = 0, y2 = 0, x1 = 0, x2 = 0;
    std::size_t next = 0;
    bool high = false;
    for (std::size_t i = 0; i < out.samples.size(); ++i) {
        while (next < clicks.size() && sampleOf(static_cast<std::int64_t>(clicks[next])) <= static_cast<std::int64_t>(i)) {
            high = !high;
            ++next;
        }
        const double u = high ? 2.0 * amplitude : 0.0;   // the same peak-to-peak swing as the raw wave (+-amplitude)
        hpOut = hpA * (hpOut + u - hpIn);
        hpIn = u;
        const double x = hpOut;
        lp += lpB * (x - lp);
        const double y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        const double v = m.clickGain * lp + m.bodyGain * y;
        out.samples[i] = static_cast<std::int16_t>(std::max(-32767.0, std::min(32767.0, std::floor(v + 0.5))));
    }
    return out;
}

}  // namespace pacman::core
