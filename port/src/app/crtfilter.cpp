#include "crtfilter.h"

#include <algorithm>
#include <cmath>

namespace pacman::app {

namespace {

const char* const kKeys[kCrtOptionCount] = {"crt_scanlines", "crt_softness", "crt_transition", "crt_glow", "crt_blend", "crt_curvature", "crt_afterglow", "crt_brightness", "crt_contrast"};
const char* const kLabels[kCrtOptionCount] = {"SCANLINES", "SOFTNESS", "TRANSITION", "GLOW", "COLOUR BLEND", "CURVATURE", "AFTERGLOW", "BRIGHTNESS", "CONTRAST"};

std::uint8_t* field(CrtFilter& f, CrtOption o) noexcept {
    switch (o) {
    case CrtOption::Scanlines: return &f.scanlines;
    case CrtOption::Softness: return &f.softness;
    case CrtOption::Transition: return &f.transition;
    case CrtOption::Glow: return &f.glow;
    case CrtOption::Blend: return &f.blend;
    case CrtOption::Curvature: return &f.curvature;
    case CrtOption::Afterglow: return &f.afterglow;
    case CrtOption::Brightness: return &f.brightness;
    case CrtOption::Contrast: return &f.contrast;
    }
    return &f.scanlines;
}

}  // namespace

int crtLevelCount(CrtOption o) noexcept { return (o == CrtOption::Brightness || o == CrtOption::Contrast) ? 5 : 4; }
int crtGet(const CrtFilter& f, CrtOption o) noexcept { return *field(const_cast<CrtFilter&>(f), o); }
void crtSet(CrtFilter& f, CrtOption o, int level) noexcept { *field(f, o) = static_cast<std::uint8_t>(std::clamp(level, 0, crtLevelCount(o) - 1)); }
void crtStep(CrtFilter& f, CrtOption o, int dir) noexcept {
    const int n = crtLevelCount(o);
    crtSet(f, o, ((crtGet(f, o) + dir) % n + n) % n);
}
const char* crtOptionKey(CrtOption o) noexcept { return kKeys[static_cast<int>(o)]; }
const char* crtOptionLabel(CrtOption o) noexcept { return kLabels[static_cast<int>(o)]; }
std::string crtLevelLabel(CrtOption o, int level) {
    static const char* const four[4] = {"OFF", "LOW", "MEDIUM", "HIGH"};
    static const char* const five[5] = {"LOWEST", "LOW", "NORMAL", "HIGH", "HIGHEST"};
    level = std::clamp(level, 0, crtLevelCount(o) - 1);
    return crtLevelCount(o) == 5 ? five[level] : four[level];
}

CrtFilter crtPresetFilter(CrtPreset p) noexcept {
    CrtFilter f;
    if (p == CrtPreset::Subtle) {
        f.scanlines = 1;
        f.softness = 1;
        f.glow = 1;
    } else if (p == CrtPreset::Classic) {
        f.scanlines = 2;
        f.softness = 2;
        f.transition = 1;
        f.glow = 2;
        f.blend = 1;
    }
    return f;
}
CrtPreset crtPresetOf(const CrtFilter& f) noexcept {
    for (CrtPreset p : {CrtPreset::Off, CrtPreset::Subtle, CrtPreset::Classic})
        if (f == crtPresetFilter(p)) return p;
    return CrtPreset::Custom;
}
CrtPreset crtNextPreset(CrtPreset p, int dir) noexcept {
    const int cur = p == CrtPreset::Custom ? 0 : static_cast<int>(p);
    return static_cast<CrtPreset>(((cur + (p == CrtPreset::Custom ? 0 : dir)) % 3 + 3) % 3);
}
const char* crtPresetLabel(CrtPreset p) noexcept {
    switch (p) {
    case CrtPreset::Off: return "OFF";
    case CrtPreset::Subtle: return "SUBTLE";
    case CrtPreset::Classic: return "CLASSIC";
    case CrtPreset::Custom: return "CUSTOM";
    }
    return "OFF";
}

CrtParams crtParams(const CrtFilter& f) noexcept {
    static const float scan[4] = {0.0f, 0.25f, 0.45f, 0.70f};
    static const float sigma[4] = {0.0f, 0.35f, 0.60f, 0.90f};
    static const float tau[4] = {0.0f, 0.35f, 0.70f, 1.20f};
    static const float glow[4] = {0.0f, 0.15f, 0.30f, 0.50f};
    static const float blend[4] = {0.0f, 0.35f, 0.65f, 1.0f};
    static const float curv[4] = {0.0f, 0.03f, 0.06f, 0.10f};
    static const float gain[5] = {0.70f, 0.85f, 1.0f, 1.15f, 1.30f};
    static const float offs[5] = {-0.06f, -0.03f, 0.0f, 0.03f, 0.06f};
    const auto c4 = [](int v) { return std::clamp(v, 0, 3); };
    const auto c5 = [](int v) { return std::clamp(v, 0, 4); };
    return CrtParams{scan[c4(f.scanlines)], sigma[c4(f.softness)], tau[c4(f.transition)], glow[c4(f.glow)], blend[c4(f.blend)], curv[c4(f.curvature)], gain[c5(f.contrast)], offs[c5(f.brightness)]};
}

float crtAfterglowDecay(int level) noexcept {
    static const float d[4] = {0.0f, 0.35f, 0.55f, 0.75f};
    return d[std::clamp(level, 0, 3)];
}

bool applyAfterglow(std::vector<std::uint32_t>& prev, std::uint32_t* cur, std::size_t count, float factor) {
    if (prev.size() != count) prev.assign(count, 0xFF000000u);
    bool trail = false;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint32_t out = 0xFF000000u;
        for (int sh = 0; sh <= 16; sh += 8) {
            const int c = static_cast<int>((cur[i] >> sh) & 0xFF);
            const int p = static_cast<int>(static_cast<float>((prev[i] >> sh) & 0xFF) * factor);
            if (p > c) trail = true;
            out |= static_cast<std::uint32_t>(std::max(c, p)) << sh;
        }
        cur[i] = out;
        prev[i] = out;
    }
    return trail;
}

// ---- the CPU model (the shader in d3drenderer.cpp is the same maths) ----
namespace {

struct Vec3 {
    float r = 0, g = 0, b = 0;
};

Vec3 texel(const std::uint32_t* src, int texW, float x, int row) {   // x in game pixels (0..280)
    const int ix = std::clamp(static_cast<int>(std::floor(x * static_cast<float>(texW) / 280.0f + 0.002f)), 0, texW - 1);
    const std::uint32_t p = src[static_cast<std::size_t>(row) * static_cast<std::size_t>(texW) + static_cast<std::size_t>(ix)];
    return Vec3{static_cast<float>((p >> 16) & 0xFF) / 255.0f, static_cast<float>((p >> 8) & 0xFF) / 255.0f, static_cast<float>(p & 0xFF) / 255.0f};
}

Vec3 bilinear(const std::uint32_t* src, int texW, float gx, float gy) {   // like the hardware's linear filter with clamped edges; gx, gy in game pixels
    const float tx = gx * static_cast<float>(texW) / 280.0f - 0.5f, ty = gy - 0.5f;
    const float fx0 = std::floor(tx), fy0 = std::floor(ty);
    const float ax = tx - fx0, ay = ty - fy0;
    const int x0 = static_cast<int>(fx0), y0 = static_cast<int>(fy0);
    Vec3 out;
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            const int x = std::clamp(x0 + i, 0, texW - 1), y = std::clamp(y0 + j, 0, 191);
            const std::uint32_t p = src[static_cast<std::size_t>(y) * static_cast<std::size_t>(texW) + static_cast<std::size_t>(x)];
            const float w = (i ? ax : 1.0f - ax) * (j ? ay : 1.0f - ay);
            out.r += w * static_cast<float>((p >> 16) & 0xFF) / 255.0f;
            out.g += w * static_cast<float>((p >> 8) & 0xFF) / 255.0f;
            out.b += w * static_cast<float>(p & 0xFF) / 255.0f;
        }
    return out;
}

float luma(const Vec3& v) { return 0.299f * v.r + 0.587f * v.g + 0.114f * v.b; }

}  // namespace

void renderCrt(const CrtFilter& f, const std::uint32_t* src, int texW, int vw, int vh, int scale, std::vector<std::uint32_t>& out) {
    const CrtParams q = crtParams(f);
    out.assign(static_cast<std::size_t>(vw) * static_cast<std::size_t>(vh), 0xFF000000u);
    const float W = static_cast<float>(vw), H = static_cast<float>(vh);
    for (int py = 0; py < vh; ++py)
        for (int px = 0; px < vw; ++px) {
            float ccx = (static_cast<float>(px) + 0.5f) / W * 2.0f - 1.0f, ccy = (static_cast<float>(py) + 0.5f) / H * 2.0f - 1.0f;
            if (q.curv > 0.0f) {
                const float nx = ccx * (1.0f + q.curv * ccy * ccy), ny = ccy * (1.0f + q.curv * ccx * ccx);
                ccx = nx;
                ccy = ny;
            }
            const float m = std::clamp((1.0f - std::fabs(ccx)) * W * 0.5f + 0.5f, 0.0f, 1.0f) * std::clamp((1.0f - std::fabs(ccy)) * H * 0.5f + 0.5f, 0.0f, 1.0f);
            if (m <= 0.0f) continue;
            const float gx = (ccx * 0.5f + 0.5f) * 280.0f, gy = (ccy * 0.5f + 0.5f) * 192.0f;
            const int row = std::clamp(static_cast<int>(std::floor(gy)), 0, 191);
            const float fr = gy - std::floor(gy);
            float scan = 1.0f;
            if (scale >= 3) scan = (1.0f - q.scan) + q.scan * (0.5f - 0.5f * std::cos(6.2831853f * fr));
            else if (scale >= 2) scan = fr < 0.5f ? 1.0f : 1.0f - q.scan;
            Vec3 acc;
            float sw = 0.0f;
            for (int t = -8; t <= 8; ++t) {
                const float d = static_cast<float>(t) * 0.25f;
                float wt = q.sigma > 0.0f ? std::exp(-d * d / (2.0f * q.sigma * q.sigma)) : (t == 0 ? 1.0f : 0.0f);
                if (q.tau > 0.0f && t < 0) wt += 0.25f * std::exp(d / q.tau);
                if (wt > 0.0f) {
                    const Vec3 c = texel(src, texW, gx + d, row);
                    acc.r += wt * c.r * c.r;
                    acc.g += wt * c.g * c.g;
                    acc.b += wt * c.b * c.b;
                    sw += wt;
                }
            }
            Vec3 col{acc.r / sw, acc.g / sw, acc.b / sw};
            if (q.blend > 0.0f) {
                Vec3 cb;
                for (int e = -2; e <= 2; ++e) {
                    const Vec3 c = texel(src, texW, gx + static_cast<float>(e) * 0.5f, row);
                    const Vec3 l{c.r * c.r, c.g * c.g, c.b * c.b};
                    const float y = luma(l), w = static_cast<float>(3 - std::abs(e));
                    cb.r += w * (l.r - y);
                    cb.g += w * (l.g - y);
                    cb.b += w * (l.b - y);
                }
                const float yc = luma(col);
                col.r += q.blend * (cb.r / 9.0f - (col.r - yc));
                col.g += q.blend * (cb.g / 9.0f - (col.g - yc));
                col.b += q.blend * (cb.b / 9.0f - (col.b - yc));
            }
            col.r *= scan;
            col.g *= scan;
            col.b *= scan;
            if (q.glow > 0.0f) {
                Vec3 g;
                float gw = 0.0f;
                for (int j = -2; j <= 2; ++j)
                    for (int i = -2; i <= 2; ++i) {
                        const float wt = std::exp(-static_cast<float>(i * i + j * j) / (2.0f * 1.44f));
                        const Vec3 c = bilinear(src, texW, gx + static_cast<float>(i), gy + static_cast<float>(j));
                        g.r += wt * c.r * c.r;
                        g.g += wt * c.g * c.g;
                        g.b += wt * c.b * c.b;
                        gw += wt;
                    }
                col.r += g.r / gw * q.glow;
                col.g += g.g / gw * q.glow;
                col.b += g.b / gw * q.glow;
            }
            const auto fin = [&](float v) { return static_cast<std::uint32_t>(std::clamp(std::clamp(std::sqrt(std::max(v, 0.0f)) * q.gain + q.offset, 0.0f, 1.0f) * m, 0.0f, 1.0f) * 255.0f + 0.5f); };
            out[static_cast<std::size_t>(py) * static_cast<std::size_t>(vw) + static_cast<std::size_t>(px)] = 0xFF000000u | (fin(col.r) << 16) | (fin(col.g) << 8) | fin(col.b);
        }
}

}  // namespace pacman::app
