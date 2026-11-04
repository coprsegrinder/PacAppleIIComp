// TASK-034-8 (modernization): "Native Black & White": the picture as a monochrome Apple II monitor showed it (docs/MONOCHROME_RESEARCH.md). The Apple II's video signal has no colour;
// a monochrome monitor draws every lit dot in the colour of its phosphor, and Apple's own Monitor II / III were green (P31). So this works from the DOTS, not from the colour picture:
//   * the dot positions are 560 across (the 14.3 MHz dot clock): every hi-res dot is two positions wide, and a byte with the palette bit set (bit 7) is drawn one position further to the
//     right (the "half-dot shift"), as on the real machine; text dots are two positions wide; a lo-res colour is a 4-position pattern of its four bits (so each colour is a grey level or pattern);
//   * no softness: a position is lit or it is not (the softness of a real tube is the business of the CRT filter, which comes after this step);
//   * the lit colour is the phosphor's (green, P31-like), the rest is black.
// Display functions only: the memory pages stay the original's bytes. Pure.
#pragma once

#include <cstdint>
#include <span>

#include "../hires/hires.h"

namespace pacman::video {

inline constexpr int kDotsW = 560, kDotsH = 192;   // dot positions per line, lines

// each function writes kDotsW * kDotsH values 0 / 1 (a dot position lit or not)
void hiresDots(const hires::Page& page, std::span<std::uint8_t> dots) noexcept;                 // hi-res: two positions per dot, palette bit = one position to the right
void bitmapDots(std::span<const std::uint8_t> bitmap280x192, std::span<std::uint8_t> dots) noexcept;   // text (renderText's 0/1 values): two positions per dot, no shift
void loresDots(std::span<const std::uint8_t> page1024, std::span<std::uint8_t> dots) noexcept;   // lo-res: 14 positions per block, the block colour's 4 bits repeated (bit 0 first)
void argbDots(std::span<const std::uint32_t> argb280x192, std::span<std::uint8_t> dots) noexcept;  // any other 280 x 192 picture: a dot with any light is lit, two positions

// the level of each position: 255 for a lit one, 0 for a dark one (nothing in between: no softness)
void levelsFromDots(std::span<const std::uint8_t> dots, std::span<std::uint8_t> level) noexcept;

// The phosphor: peak colour of the green P31-like phosphor (0..1 per channel); black stays exactly black.
inline constexpr float kPhosphorR = 0.20f, kPhosphorG = 1.00f, kPhosphorB = 0.38f;
std::uint32_t phosphorArgb(std::uint8_t level) noexcept;   // 0xAARRGGBB of a level

// the picture the window shows: kDotsW x kDotsH ARGB from the levels
void phosphorImage(std::span<const std::uint8_t> level, std::span<std::uint32_t> argb560) noexcept;
// and the 280 x 192 picture of the same thing (each pixel the average of its two positions), for everything that works from the normal-size picture (the glow, the frame edges)
void phosphorImage280(std::span<const std::uint8_t> level, std::span<std::uint32_t> argb280) noexcept;

}  // namespace pacman::video
