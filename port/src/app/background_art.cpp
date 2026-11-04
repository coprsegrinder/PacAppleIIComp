#include "background_art.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "framed_layout.h"

namespace pacman::app {

namespace {

struct Rgb {
    float r, g, b;
};

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline std::uint32_t byteOf(float v) { return static_cast<std::uint32_t>(clamp01(v) * 255.0f + 0.5f); }
inline std::uint32_t pack(Rgb c) { return 0xFF000000u | (byteOf(c.r) << 16) | (byteOf(c.g) << 8) | byteOf(c.b); }
inline Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline Rgb scaled(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }
inline float cover(float signedDistance) { return clamp01(0.5f - signedDistance); }   // anti-aliased coverage of a shape from its signed distance in pixels (negative inside)

// signed distance to a rounded rectangle (centre, half sizes, corner radius)
inline float sdRoundRect(float px, float py, float cx, float cy, float hx, float hy, float r) {
    const float qx = std::fabs(px - cx) - (hx - r), qy = std::fabs(py - cy) - (hy - r);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

inline float hashNoise(int x, int y) {   // 0..1, no pattern
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

struct Rect {
    float x0, y0, x1, y1;
};

void fillRect(std::vector<std::uint32_t>& img, int W, int H, int x0, int y0, int x1, int y1, Rgb c) {
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, W);
    y1 = std::min(y1, H);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) img[static_cast<std::size_t>(y) * static_cast<std::size_t>(W) + static_cast<std::size_t>(x)] = pack(c);
}

void blendRect(std::vector<std::uint32_t>& img, int W, int H, int x0, int y0, int x1, int y1, Rgb c, float alpha) {
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, W);
    y1 = std::min(y1, H);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            std::uint32_t& p = img[static_cast<std::size_t>(y) * static_cast<std::size_t>(W) + static_cast<std::size_t>(x)];
            const Rgb old{static_cast<float>((p >> 16) & 0xFF) / 255.0f, static_cast<float>((p >> 8) & 0xFF) / 255.0f, static_cast<float>(p & 0xFF) / 255.0f};
            p = pack(mix(old, c, alpha));
        }
}

// plain words in the Apple II character font, every font pixel `scale` pixels large
void drawText(std::vector<std::uint32_t>& img, int W, int H, const core::CharacterRom& font, const std::string& text, int x, int y, int scale, Rgb c) {
    if (!font.loaded || text.empty() || text.size() > 40) return;
    core::TextScreen screen;
    screen.clear(0xA0);
    screen.setCursor(0, 0);
    for (char ch : text) screen.put(static_cast<std::uint8_t>(ch | 0x80));
    std::vector<std::uint8_t> bits;
    core::renderText(screen, font, false, -1, -1, bits);
    for (int fy = 0; fy < 8; ++fy)
        for (int fx = 0; fx < static_cast<int>(text.size()) * 7; ++fx)
            if (bits[static_cast<std::size_t>(fy) * core::kTextWidth + static_cast<std::size_t>(fx)]) fillRect(img, W, H, x + fx * scale, y + fy * scale, x + (fx + 1) * scale, y + (fy + 1) * scale, c);
}

int textWidth(const std::string& text, int scale) { return static_cast<int>(text.size()) * 7 * scale; }

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The Apple II CRT monitor bezel: a platinum-beige plastic housing (like the Apple Monitor II / III) with a bevelled edge, a recessed dark frame and the rounded glass opening that the
// picture sits in; vent slots, a knob and a power light in the chin below. Everything is sized in game pixels times the picture's scale, so it follows the picture at every size.
void drawBezel(const ArtInput& in, std::vector<std::uint32_t>& out) {
    const int W = in.width, H = in.height;
    const Viewport& vp = in.viewport;
    const float S = vp.scale > 0 ? static_cast<float>(vp.scale) : static_cast<float>(vp.w) / kGameW;
    // The screen of a CRT monitor is 4:3. The glass is exactly that shape, centred on the game picture and a little wider than it; the picture (1.458:1) is wider than 4:3, so there is
    // black glass above and below it, as on a real tube. The housing around the glass has a FIXED shape (its size follows only the picture's scale): a wider or taller window shows more
    // wall around the monitor, it never stretches the monitor.
    const float vcx = vp.x + vp.w / 2.0f, vcy = vp.y + vp.h / 2.0f, vhx = vp.w / 2.0f;
    const float glassHx = vhx + 1.0f * S, glassHy = glassHx * 0.75f, glassR = 3.2f * S;     // 4:3
    const float recessHx = glassHx + 2.6f * S, recessHy = glassHy + 2.6f * S, recessR = 5.4f * S;
    const Rect house{vcx - (recessHx + 8.4f * S), vcy - (recessHy + 6.3f * S), vcx + (recessHx + 8.4f * S), vcy + recessHy + 18.0f * S};
    const float hcx = (house.x0 + house.x1) / 2, hcy = (house.y0 + house.y1) / 2, hhx = (house.x1 - house.x0) / 2, hhy = (house.y1 - house.y0) / 2, Rh = 6.5f * S;
    const float chinTop = vcy + recessHy, chinCy = chinTop + 9.0f * S;
    const float ledX = house.x1 - 5.5f * S, knobX = house.x1 - 11.0f * S, knobR = 1.8f * S, grilleCx = house.x0 + 11.5f * S;
    const float lx = -0.55f, ly = -0.83f;   // the light comes from the upper left
    const Rgb plastic{0.80f, 0.77f, 0.69f}, recessColour{0.10f, 0.10f, 0.11f};

    const auto gradient = [&](auto sd, float px, float py, float& gx, float& gy) {
        gx = sd(px + 0.75f, py) - sd(px - 0.75f, py);
        gy = sd(px, py + 0.75f) - sd(px, py - 0.75f);
        const float n = std::sqrt(gx * gx + gy * gy);
        if (n > 1e-6f) {
            gx /= n;
            gy /= n;
        }
    };
    const auto houseSd = [&](float px, float py) { return sdRoundRect(px, py, hcx, hcy, hhx, hhy, Rh); };
    const auto recessSd = [&](float px, float py) { return sdRoundRect(px, py, vcx, vcy, recessHx, recessHy, recessR); };

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            std::uint32_t& dst = out[static_cast<std::size_t>(y) * static_cast<std::size_t>(W) + static_cast<std::size_t>(x)];
            if (x >= vp.x && x < vp.x + vp.w && y >= vp.y && y < vp.y + vp.h) {   // under the picture: never shown
                dst = 0xFF000000u;
                continue;
            }
            const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
            // the wall or desk behind the monitor: very dark, a little lighter in the middle, with the housing's shadow on it
            const float dx = px / W - 0.5f, dy = py / H - 0.5f;
            Rgb c = scaled(Rgb{0.062f, 0.060f, 0.058f}, 1.0f - 0.9f * (dx * dx + dy * dy));
            const float hd = houseSd(px, py);
            if (hd > 0.0f && hd < 12.0f * S) c = scaled(c, 1.0f - 0.7f * std::exp(-std::max(0.0f, houseSd(px, py - 1.4f * S)) / (2.2f * S)));
            if (hd < 0.5f) {
                // the plastic face with its bevelled edge
                const float t = clamp01(-hd / (2.4f * S));
                Rgb face = scaled(plastic, 1.035f - 0.07f * (py - house.y0) / (house.y1 - house.y0));
                if (t < 1.0f) {
                    float gx, gy;
                    gradient(houseSd, px, py, gx, gy);
                    const float bevel = std::pow(1.0f - t, 1.6f);
                    face = scaled(face, 1.0f + 0.42f * bevel * (gx * lx + gy * ly));
                }
                face = scaled(face, 1.0f + (hashNoise(x, y) - 0.5f) * 0.03f);   // a little grain
                if (py > chinTop) {   // the chin: vents, a knob and the power light
                    for (int k = -2; k <= 2; ++k) {
                        const float sy = chinCy + k * 1.35f * S;
                        face = mix(face, scaled(plastic, 1.12f), cover(sdRoundRect(px, py, grilleCx, sy + 0.3f * S, 7.5f * S, 0.28f * S, 0.28f * S)) * 0.7f);   // the lower lip of a slot
                        face = mix(face, Rgb{0.23f, 0.21f, 0.18f}, cover(sdRoundRect(px, py, grilleCx, sy, 7.5f * S, 0.28f * S, 0.28f * S)));
                    }
                    const float kd = std::sqrt((px - knobX) * (px - knobX) + (py - chinCy) * (py - chinCy));
                    face = mix(face, Rgb{0.36f, 0.34f, 0.30f}, cover(kd - knobR - 0.35f * S));   // the knob's dark rim
                    const float lit = clamp01(0.5f + 0.5f * (((px - knobX) * lx + (py - chinCy) * ly) / knobR));
                    face = mix(face, scaled(Rgb{0.66f, 0.63f, 0.57f}, 0.78f + 0.3f * lit), cover(kd - knobR));
                    const float ld = std::sqrt((px - ledX) * (px - ledX) + (py - chinCy) * (py - chinCy));
                    face.g += 0.30f * std::exp(-ld / (1.8f * S));   // the power light's glow on the plastic
                    face.r += 0.06f * std::exp(-ld / (1.8f * S));
                    face = mix(face, Rgb{0.20f, 0.19f, 0.17f}, cover(ld - 1.05f * S));
                    face = mix(face, Rgb{0.38f, 1.0f, 0.50f}, cover(ld - 0.7f * S));
                }
                c = mix(c, face, cover(hd));
            }
            // the recessed dark frame around the glass, shaded as a step down into the housing
            const float rd = recessSd(px, py);
            if (rd < 0.5f) {
                const float t2 = clamp01(-rd / (1.4f * S));
                Rgb rc = recessColour;
                if (t2 < 1.0f) {
                    float gx, gy;
                    gradient(recessSd, px, py, gx, gy);
                    rc = scaled(rc, std::clamp(1.0f - 0.95f * (1.0f - t2) * (gx * lx + gy * ly), 0.15f, 2.2f));
                }
                c = mix(c, rc, cover(rd));
            }
            // the glass opening: black (the unlit tube), with the faintest reflection of a window
            const float gd = sdRoundRect(px, py, vcx, vcy, glassHx, glassHy, glassR);
            if (gd < 0.5f) {
                const float diag = (px - (vcx - glassHx)) / (2 * glassHx) + (py - (vcy - glassHy)) / (2 * glassHy);
                const float sheen = 0.030f * std::exp(-(diag - 0.30f) * (diag - 0.30f) / 0.012f);
                c = mix(c, Rgb{sheen, sheen, sheen * 1.05f}, cover(gd));
            }
            dst = pack(c);
        }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The Apple II desktop: the soft grey-blue dithered desktop of a 1980s Apple graphical shell, a menu bar with plain menu words and the game in a window with a striped title bar, a
// close box and a zoom box. No logo, nothing busy: the window with the game stays the centre.
void drawDesktop(const ArtInput& in, std::vector<std::uint32_t>& out) {
    const int W = in.width, H = in.height;
    const Viewport& vp = in.viewport;
    const int S = std::max(1, vp.scale > 0 ? vp.scale : static_cast<int>(vp.w / kGameW));
    const int o = std::max(1, S / 3);               // the thickness of the thin black lines
    const int ps = std::max(1, S / 2);              // the size of a dot of the desktop's pattern
    const Rgb patternA{0.52f, 0.60f, 0.70f}, patternB{0.47f, 0.55f, 0.66f}, paper{0.97f, 0.97f, 0.96f}, ink{0.05f, 0.05f, 0.06f};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) out[static_cast<std::size_t>(y) * static_cast<std::size_t>(W) + static_cast<std::size_t>(x)] = pack(((x / ps + y / ps) & 1) ? patternA : patternB);
    // the menu bar
    const int mbH = 8 * S;
    fillRect(out, W, H, 0, 0, W, mbH, paper);
    fillRect(out, W, H, 0, mbH - o, W, mbH, ink);
    const int ts = std::max(1, static_cast<int>(S * 0.7f));
    int mx = 4 * S;
    if (in.font) {
        for (const char* word : {"FILE", "EDIT", "VIEW", "SPECIAL"}) {
            drawText(out, W, H, *in.font, word, mx, (mbH - o - 8 * ts) / 2, ts, ink);
            mx += textWidth(word, ts) + 14 * ts;
        }
    }
    // the window: a shadow, white paper, a black outline, the title bar and the thin line around the picture
    const int wx0 = vp.x - 3 * S, wy0 = vp.y - 11 * S, wx1 = vp.x + vp.w + 3 * S, wy1 = vp.y + vp.h + 3 * S;
    blendRect(out, W, H, wx0 + 2 * S, wy0 + 2 * S, wx1 + 2 * S, wy1 + 2 * S, ink, 0.35f);
    fillRect(out, W, H, wx0, wy0, wx1, wy1, paper);
    fillRect(out, W, H, wx0, wy0, wx1, wy0 + o, ink);
    fillRect(out, W, H, wx0, wy1 - o, wx1, wy1, ink);
    fillRect(out, W, H, wx0, wy0, wx0 + o, wy1, ink);
    fillRect(out, W, H, wx1 - o, wy0, wx1, wy1, ink);
    const int titleBottom = wy0 + 9 * S;
    for (int ly = wy0 + 2 * o; ly < titleBottom - 2 * o; ly += 2 * o) fillRect(out, W, H, wx0 + o, ly, wx1 - o, ly + o, scaled(ink, 2.2f));   // the stripes of the title bar
    fillRect(out, W, H, wx0, titleBottom, wx1, titleBottom + o, ink);
    const std::string title = "PAC-MAN";
    const int tw = in.font ? textWidth(title, ts) : 0;
    const int tx = (wx0 + wx1) / 2 - tw / 2;
    if (in.font) {
        fillRect(out, W, H, tx - 2 * S, wy0 + o, tx + tw + 2 * S, titleBottom, paper);
        drawText(out, W, H, *in.font, title, tx, wy0 + (9 * S - 8 * ts) / 2, ts, ink);
    }
    const int bs = 5 * S;                           // the close box (left) and the zoom box (right)
    const int by = wy0 + (9 * S - bs) / 2;
    for (int box = 0; box < 2; ++box) {
        const int bx = box == 0 ? wx0 + 2 * S : wx1 - 2 * S - bs;
        fillRect(out, W, H, bx, by, bx + bs, by + bs, ink);
        fillRect(out, W, H, bx + o, by + o, bx + bs - o, by + bs - o, paper);
        if (box == 1) fillRect(out, W, H, bx + 2 * o, by + 2 * o, bx + bs / 2 + o, by + bs / 2 + o, ink);
    }
    {   // the grow box in the lower right corner of the frame: two squares, drag it to resize the window
        const DeskRect g = desktopGrowBox(vp);
        const int a = g.x0 + S / 2, b = g.y0 + S / 2;
        fillRect(out, W, H, a, b, a + 2 * S, b + 2 * S, ink);
        fillRect(out, W, H, a + o, b + o, a + 2 * S - o, b + 2 * S - o, paper);
        fillRect(out, W, H, a + S, b + S, a + 2 * S - o, b + 2 * S - o, ink);
    }
    fillRect(out, W, H, vp.x - o, vp.y - o, vp.x + vp.w + o, vp.y, ink);                 // the thin black line around the picture
    fillRect(out, W, H, vp.x - o, vp.y + vp.h, vp.x + vp.w + o, vp.y + vp.h + o, ink);
    fillRect(out, W, H, vp.x - o, vp.y, vp.x, vp.y + vp.h, ink);
    fillRect(out, W, H, vp.x + vp.w, vp.y, vp.x + vp.w + o, vp.y + vp.h, ink);
    fillRect(out, W, H, vp.x, vp.y, vp.x + vp.w, vp.y + vp.h, Rgb{0, 0, 0});            // under the picture: never shown
}

}  // namespace

bool renderBackgroundArt(const ArtInput& in, std::vector<std::uint32_t>& argb) {
    if (in.width <= 0 || in.height <= 0 || in.viewport.empty() || !hasFrame(in.kind)) return false;
    argb.assign(static_cast<std::size_t>(in.width) * static_cast<std::size_t>(in.height), 0xFF000000u);
    if (in.kind == Background::CrtBezel) drawBezel(in, argb);
    else drawDesktop(in, argb);
    return true;
}

}  // namespace pacman::app
