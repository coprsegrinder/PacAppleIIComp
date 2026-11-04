// TASK-034-7 (modernization): the picture behind the "Ambient Glow" background. The bars show a soft, dim copy of the game's own picture, repeated outwards (mirrored at its edges) at
// a little less than the picture's own size, so that the shapes of the maze and the movement of the ghosts are visible far in the background: the 280 x 192 picture, kept for a very
// short time so that it follows the game within a few frames, blurred, and drawn by the renderer over the whole window (the picture itself hides the middle). Nothing is added to the
// picture itself (no bloom).
// The blur is done here, on the small picture, with a radius in GAME pixels (kBlurRadius, the softness the glow always had: it is the same at every window size); the shader only
// lifts the dim content and fades it with the distance.
// The edge: so that the picture's border dissolves into the background instead of meeting it with a step, the bars take the picture's OWN colours right at its edge (the outer three
// pixels, smoothed along the edge so that single pixels do not streak outwards) and fade from them into the backdrop over a short distance. These edge colours follow the picture at
// once (they must equal what is next to them). Only the bars show them; the picture is never changed. Pure.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "display_layout.h"

namespace pacman::app {

inline constexpr int kEdgeW = kGameW;   // the edge texture: 280 wide, 4 rows: top, bottom (280 colours each), left, right (192 each, the rest unused)

class GlowField {
public:
    static constexpr int kBlurRadius = 10;           // game pixels: two box blurs of this radius per axis, about 7 pixels of Gaussian spread
    static constexpr double kBackdropScale = 0.8;    // the repeated copies are drawn at this fraction of the picture's own scale on the screen: a little further back
    static constexpr double kTimeConstant = 0.06;   // seconds for the backdrop to cover 63 % of a sudden change: movement shows within a few frames, a blink is softened

    // The 280 x 192 ARGB picture and the time since the last call; `blurRadius` in game pixels (0 = none). The first call (or reset) takes the picture as it is.
    void update(const std::uint32_t* argb, double dtSeconds, int blurRadius) {
        const double alpha = first_ ? 1.0 : (dtSeconds <= 0.0 ? 0.0 : 1.0 - std::exp(-dtSeconds / kTimeConstant));
        first_ = false;
        for (std::size_t i = 0; i < kPixels; ++i) {
            const std::uint32_t p = argb[i];
            const float t[3] = {static_cast<float>((p >> 16) & 0xFF) / 255.0f, static_cast<float>((p >> 8) & 0xFF) / 255.0f, static_cast<float>(p & 0xFF) / 255.0f};
            for (int k = 0; k < 3; ++k) smooth_[i * 3 + static_cast<std::size_t>(k)] += static_cast<float>((t[k] - smooth_[i * 3 + static_cast<std::size_t>(k)]) * alpha);
        }
        blurred_ = smooth_;
        for (int pass = 0; pass < 2; ++pass) {   // two box blurs on each axis: close to a Gaussian
            boxBlur(blurred_, blurRadius, true);
            boxBlur(blurred_, blurRadius, false);
        }
        for (std::size_t i = 0; i < kPixels; ++i) {
            std::uint8_t* px = &bytes_[i * 4];
            px[0] = toByte(blurred_[i * 3 + 2]);   // B
            px[1] = toByte(blurred_[i * 3 + 1]);   // G
            px[2] = toByte(blurred_[i * 3 + 0]);   // R
            px[3] = 255;
        }
    }

    // Call with the same picture: the colours of the picture's four edges (BGRA, 280 x 4 texels: row 0 top, 1 bottom, 2 left, 3 right).
    void updateEdges(const std::uint32_t* argb) {
        constexpr int kDepth = 3, kRadius = 5;   // the outer three pixels, smoothed over 11 pixels along the edge
        const auto px = [&](int x, int y, int k) { return static_cast<float>((argb[static_cast<std::size_t>(y) * kGameW + static_cast<std::size_t>(x)] >> (16 - 8 * k)) & 0xFF) / 255.0f; };
        std::array<std::array<float, 3>, kGameW> line{};
        for (int row = 0; row < 4; ++row) {
            const int n = row < 2 ? kGameW : kGameH;
            for (int i = 0; i < n; ++i)
                for (int k = 0; k < 3; ++k) {
                    float s = 0;
                    for (int d = 0; d < kDepth; ++d) {
                        const int x = row == 0 || row == 1 ? i : (row == 2 ? d : kGameW - 1 - d);
                        const int y = row == 0 ? d : (row == 1 ? kGameH - 1 - d : i);
                        s += px(x, y, k);
                    }
                    line[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] = s / kDepth;
                }
            for (int i = 0; i < n; ++i) {
                std::uint8_t* out = &edge_[(static_cast<std::size_t>(row) * kEdgeW + static_cast<std::size_t>(i)) * 4];
                float c[3] = {0, 0, 0};
                int count = 0;
                for (int j = (std::max)(0, i - kRadius); j <= (std::min)(n - 1, i + kRadius); ++j, ++count)
                    for (int k = 0; k < 3; ++k) c[k] += line[static_cast<std::size_t>(j)][static_cast<std::size_t>(k)];
                out[0] = toByte(c[2] / static_cast<float>(count));   // B
                out[1] = toByte(c[1] / static_cast<float>(count));   // G
                out[2] = toByte(c[0] / static_cast<float>(count));   // R
                out[3] = 255;
            }
        }
    }

    void reset() {
        first_ = true;
        smooth_.assign(kPixels * 3, 0.0f);
        blurred_.assign(kPixels * 3, 0.0f);
        bytes_.assign(kPixels * 4, 0);
        edge_.fill(0);
    }

    static constexpr std::size_t kPixels = static_cast<std::size_t>(kGameW) * kGameH;
    const std::vector<std::uint8_t>& backdropBytes() const noexcept { return bytes_; }   // BGRA, row by row, 280 x 192
    const std::array<std::uint8_t, kEdgeW * 4 * 4>& edgeBytes() const noexcept { return edge_; }
    // r, g, b (0..1) of a backdrop pixel
    void backdrop(int x, int y, float& r, float& g, float& b) const noexcept {
        const std::size_t i = (static_cast<std::size_t>(y) * kGameW + static_cast<std::size_t>(x)) * 3;
        r = blurred_[i];
        g = blurred_[i + 1];
        b = blurred_[i + 2];
    }

private:
    static std::uint8_t toByte(float v) noexcept { return static_cast<std::uint8_t>(std::lround((std::min)(1.0f, (std::max)(0.0f, v)) * 255.0f)); }

    // a box blur of the given radius along one axis, edges extended by their own pixels
    static void boxBlur(std::vector<float>& img, int radius, bool horizontal) {
        if (radius <= 0) return;
        const int len = horizontal ? kGameW : kGameH, lines = horizontal ? kGameH : kGameW;
        std::vector<float> tmp(static_cast<std::size_t>(len));
        for (int ch = 0; ch < 3; ++ch)
            for (int l = 0; l < lines; ++l) {
                const auto at = [&](int i) -> float& {
                    return horizontal ? img[(static_cast<std::size_t>(l) * kGameW + static_cast<std::size_t>(i)) * 3 + static_cast<std::size_t>(ch)]
                                      : img[(static_cast<std::size_t>(i) * kGameW + static_cast<std::size_t>(l)) * 3 + static_cast<std::size_t>(ch)];
                };
                double sum = 0;
                for (int j = -radius; j <= radius; ++j) sum += at((std::min)(len - 1, (std::max)(0, j)));
                for (int i = 0; i < len; ++i) {
                    tmp[static_cast<std::size_t>(i)] = static_cast<float>(sum / (2 * radius + 1));
                    sum += at((std::min)(len - 1, i + radius + 1)) - at((std::max)(0, i - radius));
                }
                for (int i = 0; i < len; ++i) at(i) = tmp[static_cast<std::size_t>(i)];
            }
    }

    bool first_ = true;
    std::vector<float> smooth_ = std::vector<float>(kPixels * 3, 0.0f);    // the picture, smoothed over a very short time (r, g, b per pixel)
    std::vector<float> blurred_ = std::vector<float>(kPixels * 3, 0.0f);   // and blurred
    std::vector<std::uint8_t> bytes_ = std::vector<std::uint8_t>(kPixels * 4, 0);   // on the heap: it is 215 KB
    std::array<std::uint8_t, kEdgeW * 4 * 4> edge_{};
};

}  // namespace pacman::app
