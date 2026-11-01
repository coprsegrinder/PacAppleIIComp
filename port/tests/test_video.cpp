// Tests of the colour function (src/video/colour). Synthetic pages first; the last test needs --image and compares colour-class counts of real pages
// with the verified Python colour model (tools/make_colour_vectors.py, 2026-11-01).
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "../src/datapack/datapack.h"
#include "../src/hires/hires.h"
#include "../src/verify/verify.h"
#include "../src/video/colour.h"
#include "test_framework.h"

using namespace pacman;
using hires::Page;
using video::PixelClass;

namespace {

// Light pixel x on row y; bit pattern of the byte decides the palette bit.
void light(Page& p, int x, int y, bool palette1) {
    const std::size_t off = hires::rowOffset(y) + static_cast<std::size_t>(x / 7);
    std::uint8_t b = p.byte(off);
    b = static_cast<std::uint8_t>((b & 0x7F) | (1u << (x % 7)) | (palette1 ? 0x80 : 0));
    p.set(off, b);
}

Page blank(std::uint8_t fill = 0) {
    Page p;
    p.clear(fill);
    return p;
}

}  // namespace

TEST(video_isolated_pixels_use_parity_and_palette) {
    Page p = blank();
    light(p, 0, 10, false);  // even x, palette 0
    light(p, 11, 10, false);  // odd x, palette 0
    light(p, 20, 10, true);   // even x, palette 1
    light(p, 31, 10, true);   // odd x, palette 1
    CHECK(video::classify(p, 0, 10) == PixelClass::Violet);
    CHECK(video::classify(p, 11, 10) == PixelClass::Green);
    CHECK(video::classify(p, 20, 10) == PixelClass::Blue);
    CHECK(video::classify(p, 31, 10) == PixelClass::Orange);
    CHECK(video::classify(p, 1, 10) == PixelClass::Black);
}

TEST(video_neighbours_make_white_across_bytes_and_palettes) {
    Page p = blank();
    light(p, 6, 3, false);  // last pixel of byte 0 ...
    light(p, 7, 3, true);   // ... and the first of byte 1, with the other palette bit
    CHECK(video::classify(p, 6, 3) == PixelClass::White);
    CHECK(video::classify(p, 7, 3) == PixelClass::White);
    Page q = blank();
    light(q, 100, 5, false);
    light(q, 100, 6, false);  // a lit pixel above or below does not count
    CHECK(video::classify(q, 100, 5) == PixelClass::Violet);
    Page w = blank();
    light(w, 40, 9, true);
    light(w, 42, 9, true);  // one pixel apart (the maze wall pattern): both stay blue
    CHECK(video::classify(w, 40, 9) == PixelClass::Blue && video::classify(w, 42, 9) == PixelClass::Blue);
    light(w, 41, 9, true);  // fill the gap: all three white
    CHECK(video::classify(w, 40, 9) == PixelClass::White && video::classify(w, 41, 9) == PixelClass::White && video::classify(w, 42, 9) == PixelClass::White);
}

TEST(video_black_page_and_edges) {
    Page p;  // cleared to $80: the palette bit alone lights nothing
    std::vector<std::uint32_t> img(280 * 192, 1);
    video::renderArgb(p, img);
    for (const auto v : img) CHECK(v == 0xFF000000u);
    CHECK(video::classify(p, -1, 0) == PixelClass::Black && video::classify(p, 280, 0) == PixelClass::Black && video::classify(p, 0, 192) == PixelClass::Black);
    Page e = blank();
    light(e, 0, 0, false);
    light(e, 279, 191, true);
    CHECK(video::classify(e, 0, 0) == PixelClass::Violet);       // no left neighbour at the border
    CHECK(video::classify(e, 279, 191) == PixelClass::Orange);   // no right neighbour at the border
    std::vector<std::uint32_t> small(10);
    video::renderArgb(e, small);  // too small: untouched
    CHECK(small[0] == 0);
}

TEST(video_render_writes_the_palette) {
    Page p = blank();
    light(p, 4, 0, false);    // violet at (4, 0)
    light(p, 7, 1, false);    // green at (7, 1)
    light(p, 14, 191, true);  // blue at (14, 191)
    light(p, 21, 100, true);  // orange at (21, 100)
    light(p, 50, 50, false);
    light(p, 51, 50, false);  // white pair
    std::vector<std::uint32_t> img(280 * 192, 0);
    video::renderArgb(p, img);
    CHECK(img[0 * 280 + 4] == video::colourOf(PixelClass::Violet));
    CHECK(img[1 * 280 + 7] == video::colourOf(PixelClass::Green));
    CHECK(img[191 * 280 + 14] == video::colourOf(PixelClass::Blue));
    CHECK(img[100 * 280 + 21] == video::colourOf(PixelClass::Orange));
    CHECK(img[50 * 280 + 50] == 0xFFFFFFFFu && img[50 * 280 + 51] == 0xFFFFFFFFu);
    std::size_t lit = 0;
    for (const auto v : img) lit += v != 0xFF000000u;
    CHECK(lit == 6);
}

namespace {

std::optional<std::vector<std::uint8_t>> loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::array<std::size_t, 6> histogram(const Page& p) {
    std::array<std::size_t, 6> h{};
    for (int y = 0; y < hires::kHeight; ++y) {
        for (int x = 0; x < hires::kWidth; ++x) ++h[static_cast<std::size_t>(video::classify(p, x, y))];
    }
    return h;
}

}  // namespace

// Expected counts from tools/make_colour_vectors.py (the Python model checked against AppleWin): {black, violet, green, blue, orange, white}.
TEST(video_real_pages_have_the_modelled_colours) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (no --image given)\n");
        return;
    }
    const auto img = loadFile(testfw::config().image);
    CHECK_MSG(img.has_value(), "cannot read the image");
    if (!img) return;
    const auto built = datapack::buildPack(*img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    const auto& pack = loaded.pack;
    const auto normal = pack.find(0x5F69, hires::kWallTableBytes);
    const auto forced = pack.find(0x5DBF, hires::kWallTableBytes);
    const auto glyphs = pack.find(0x4553, hires::kGlyphTableBytes);
    const auto strings = pack.find(0x434F, 0x43D2 - 0x434F);
    CHECK(!normal.empty() && !forced.empty() && !glyphs.empty() && !strings.empty());
    if (normal.empty() || forced.empty() || glyphs.empty() || strings.empty()) return;
    Page w1, w0, t;
    CHECK(hires::drawWalls(w1, normal, true));
    CHECK(hires::drawWalls(w0, forced, false));
    CHECK((histogram(w1) == std::array<std::size_t, 6>{51933, 0, 0, 1827, 0, 0}));       // the normal maze: all blue
    CHECK((histogram(w0) == std::array<std::size_t, 6>{51099, 0, 0, 533, 533, 1595}));   // the forced white-wall mode
    std::size_t pos = 0;
    for (int i = 0; i < 12; ++i) {
        const std::size_t used = hires::drawString(t, glyphs, strings.subspan(pos));
        CHECK(used > 0);
        if (used == 0) return;
        pos += used;
    }
    CHECK((histogram(t) == std::array<std::size_t, 6>{52335, 0, 0, 407, 356, 662}));
}
