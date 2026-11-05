// Turns the hi-res page into colours: the standard Apple II hi-res colour rules (docs/GRAPHICS.md section 9, checked against AppleWin on a real
// gameplay screen: 2,172 of 2,172 isolated pixels agree). This is a separate display function on purpose: the page stays exactly the original's bytes
// and the colours can be changed or filtered without touching the game.
//   * an unlit pixel is black;
//   * a lit pixel with a lit horizontal neighbour (x - 1 or x + 1 on the same row, across byte borders, whatever the palette bits) is white;
//   * an isolated lit pixel is violet (even x) or green (odd x) when its byte's palette bit 7 is 0, blue (even x) or orange (odd x) when it is 1.
// The numbers are the usual hi-res palette values (as in AppleWin's default palette); AppleWin's extra NTSC blur and the half-pixel shift of
// palette-1 pixels are display effects that this basic function does not reproduce (optional later).
#pragma once

#include <cstdint>
#include <span>

#include "../hires/hires.h"

namespace pacman::video {

enum class PixelClass : std::uint8_t { Black, Violet, Green, Blue, Orange, White };

PixelClass classify(const hires::Page& page, int x, int y) noexcept;  // x 0..279, y 0..191; anything else is Black

// 0xAARRGGBB with alpha 255.
inline constexpr std::uint32_t kColour[6] = {0xFF000000u, 0xFFFF44FDu, 0xFF14F53Cu, 0xFF14CFFDu, 0xFFFF6A3Cu, 0xFFFFFFFFu};
inline constexpr std::uint32_t colourOf(PixelClass c) noexcept { return kColour[static_cast<std::size_t>(c)]; }

// The whole page as 280 x 192 pixels (the lit pixels as classify() says, plus the fill of a pattern of every second pixel: an unlit pixel between two lit pixels of the same colour takes
// that colour, so the game's walls and ghost bodies are solid colour as on a colour display), row-major from the top (out must have 280 * 192 entries; otherwise nothing is written).
void renderArgb(const hires::Page& page, std::span<std::uint32_t> out) noexcept;

}  // namespace pacman::video
