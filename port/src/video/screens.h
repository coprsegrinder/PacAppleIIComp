// The two other screens of the Apple II that the program shows besides the hi-res page (TASK-033-4g): the lo-res page (the title effect fills it with one colour) and the text page
// (the prompts), as 280 x 192 ARGB pictures. Display functions only: the memory pages stay the original's bytes.
#pragma once

#include <cstdint>
#include <span>

namespace pacman::video {

// The 16 lo-res colours (the usual Apple II palette values: black, magenta, dark blue, purple, dark green, grey, medium blue, light blue, brown, orange, grey, pink, green, yellow, aqua,
// white). The lo-res byte $DD of the title effect is colour 13 in both halves: yellow.
inline constexpr std::uint32_t kLoresColour[16] = {0xFF000000u, 0xFFE31E60u, 0xFF604EBDu, 0xFFFF44FDu, 0xFF00A360u, 0xFF9C9C9Cu, 0xFF14CFFDu, 0xFFD0C3FFu,
                                                   0xFF607203u, 0xFFFF6A3Cu, 0xFF9C9C9Cu, 0xFFFFA0D0u, 0xFF14F53Cu, 0xFFD0DD8Du, 0xFF72FFD0u, 0xFFFFFFFFu};

// The 1,024 bytes of a lo-res/text page (40 x 24 cells; each byte is two blocks: low nibble on top, high nibble below) as 280 x 192 pixels, full screen (no mixed mode).
void renderLoresArgb(std::span<const std::uint8_t> page, std::span<std::uint32_t> out) noexcept;

// 280 x 192 values 0/1 (core/textscreen.h renderText) as white on black.
void renderBitmapArgb(std::span<const std::uint8_t> pixels, std::span<std::uint32_t> out) noexcept;

}  // namespace pacman::video
