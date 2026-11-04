// TASK-034-9 (modernization): the CRT filter, written from the maths (no outside shader, no licence question; docs/CRT_RESEARCH.md). Nine independent options, each with several degrees;
// everything off (the default) leaves the picture exactly as it was. This file is pure: the settings, the degrees' values, the afterglow, and a CPU model of the filter that the
// Direct3D pixel shader (d3drenderer.cpp) mirrors and that the tests compare it against.
//   scanlines   dark gap between the picture lines          softness    the monitor's limited bandwidth (symmetric blur along the line)
//   transition  trailing edge: a dot's light falls off slowly to its right (amplifier speed)
//   glow        light spilling around bright dots           blend       colour bleeding between neighbouring dots (Original Colour; no effect in Native Black & White)
//   curvature   gently curved glass (the 280:192 viewport stays; the picture bends inside it)
//   afterglow   phosphor persistence over time (CPU side, per colour channel)
//   brightness  black level (the monitor's knob)            contrast    white level (the monitor's knob)
// The light is mixed in linear-ish space (squares) and shown with the square root, so black stays black and the final stage is one place (the HDR section changes only that).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pacman::app {

enum class CrtOption { Scanlines, Softness, Transition, Glow, Blend, Curvature, Afterglow, Brightness, Contrast };
inline constexpr int kCrtOptionCount = 9;

struct CrtFilter {
    std::uint8_t scanlines = 0, softness = 0, transition = 0, glow = 0, blend = 0, curvature = 0, afterglow = 0;   // 0 = off, 1 low, 2 medium, 3 high
    std::uint8_t brightness = 2, contrast = 2;                                                                      // 0..4, 2 = normal
    bool operator==(const CrtFilter&) const = default;
    bool neutral() const noexcept { return *this == CrtFilter{}; }
    bool shaderNeutral() const noexcept { return scanlines == 0 && softness == 0 && transition == 0 && glow == 0 && blend == 0 && curvature == 0 && brightness == 2 && contrast == 2; }   // nothing for the picture shader to do
};

int crtLevelCount(CrtOption o) noexcept;                       // 4 (off/low/medium/high) or 5 (brightness, contrast)
int crtGet(const CrtFilter& f, CrtOption o) noexcept;
void crtSet(CrtFilter& f, CrtOption o, int level) noexcept;     // out-of-range levels are clamped
void crtStep(CrtFilter& f, CrtOption o, int dir) noexcept;      // next / previous degree, wrapping round
const char* crtOptionKey(CrtOption o) noexcept;                 // settings file word: "crt_scanlines" ...
const char* crtOptionLabel(CrtOption o) noexcept;               // menu word: "SCANLINES" ...
std::string crtLevelLabel(CrtOption o, int level);              // "OFF" "LOW" "MEDIUM" "HIGH"; "LOWEST" "LOW" "NORMAL" "HIGH" "HIGHEST"

enum class CrtPreset { Off, Subtle, Classic, Custom };
CrtFilter crtPresetFilter(CrtPreset p) noexcept;                // Custom = the defaults (off)
CrtPreset crtPresetOf(const CrtFilter& f) noexcept;             // which preset these settings are exactly, else Custom
CrtPreset crtNextPreset(CrtPreset p, int dir) noexcept;         // Off -> Subtle -> Classic -> Off (Custom goes to Off)
const char* crtPresetLabel(CrtPreset p) noexcept;

// The values of the degrees.
struct CrtParams {
    float scan, sigma, tau, glow, blend, curv, gain, offset;
};
CrtParams crtParams(const CrtFilter& f) noexcept;
float crtAfterglowDecay(int level) noexcept;                    // factor per 1/60 s (0 = none)

// Afterglow: each colour channel keeps the larger of its new value and the old one decayed by `factor` (already scaled for the time between pictures); `prev` follows the result.
// Returns true while any trail is still visible (so the window keeps repainting until it has faded).
bool applyAfterglow(std::vector<std::uint32_t>& prev, std::uint32_t* cur, std::size_t count, float factor);

// The CPU model of the picture shader: `src` is texW x 192 ARGB (280, or 560 for the monochrome picture), the result vw x vh ARGB for a viewport at whole-number `scale`.
void renderCrt(const CrtFilter& f, const std::uint32_t* src, int texW, int vw, int vh, int scale, std::vector<std::uint32_t>& out);

}  // namespace pacman::app
