// Tests of the hi-res layer (src/hires). Synthetic data only, except the last tests, which need the real image (--image) to build the data pack
// and compare against hashes computed with the verified Python models (tools/make_hires_vectors.py, 2026-11-01). Those hashes are of derived pages.
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../src/datapack/datapack.h"
#include "../src/hires/hires.h"
#include "../src/verify/sha256.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using hires::Page;

namespace {

std::size_t countNot(const Page& p, std::uint8_t v) {
    std::size_t n = 0;
    for (const auto b : p.bytes()) n += b != v;
    return n;
}

std::string pageHash(const Page& p) { return verify::toHexUpper(verify::sha256(p.bytes())); }

}  // namespace

TEST(hires_row_offsets_are_the_apple_ii_interleave) {
    CHECK(hires::rowOffset(0) == 0x0000);
    CHECK(hires::rowOffset(1) == 0x0400);
    CHECK(hires::rowOffset(8) == 0x0080);
    CHECK(hires::rowOffset(64) == 0x0028);
    CHECK(hires::rowOffset(128) == 0x0050);
    CHECK(hires::rowOffset(191) == 0x1FD0);
    std::set<std::size_t> seen;
    for (int y = 0; y < hires::kHeight; ++y) {
        const std::size_t o = hires::rowOffset(y);
        CHECK(o + hires::kBytesPerRow <= hires::kPageBytes);
        for (int b = 0; b < hires::kBytesPerRow; ++b) CHECK(seen.insert(o + static_cast<std::size_t>(b)).second);  // no two rows share a byte
    }
    CHECK(seen.size() == 192u * 40u);
}

TEST(hires_pixel_cells) {
    CHECK(hires::pixelCell(0).column == 0 && hires::pixelCell(0).shift == 0);
    CHECK(hires::pixelCell(6).column == 0 && hires::pixelCell(6).shift == 6);
    CHECK(hires::pixelCell(7).column == 1 && hires::pixelCell(7).shift == 0);
    CHECK(hires::pixelCell(279).column == 39 && hires::pixelCell(279).shift == 6);
    CHECK(hires::pixelCell(256).column == 36 && hires::pixelCell(256).shift == 4);  // x >= 256 uses the same formula (V in the original)
}

TEST(hires_page_starts_black_and_clears) {
    Page p;
    CHECK(countNot(p, 0x80) == 0);
    p.set(5, 0x12);
    CHECK(countNot(p, 0x80) == 1);
    p.clear();
    CHECK(countNot(p, 0x80) == 0);
    p.clear(0);
    CHECK(countNot(p, 0) == 0);
    p.set(hires::kPageBytes, 1);  // outside: dropped
    p.orInto(hires::kPageBytes + 5, 1);
    CHECK(countNot(p, 0) == 0);
}

TEST(hires_plot_and_lines) {
    Page p;
    hires::plotPixel(p, 9, 5);  // column 1, bit 2
    CHECK(p.byte(hires::rowOffset(5) + 1) == (0x80 | 0x04));
    hires::plotPixel(p, -1, 5);
    hires::plotPixel(p, 9, 192);
    CHECK(countNot(p, 0x80) == 1);
    Page q;
    hires::hLine(q, 0, 13, 5, 2, true);  // every second pixel: x = 0, 2, ..., 12
    CHECK(q.byte(hires::rowOffset(5)) == 0xD5);
    CHECK(q.byte(hires::rowOffset(5) + 1) == 0xAA);
    Page r;
    hires::hLine(r, 0, 13, 5, 1, true);  // adjacent pixels
    CHECK(r.byte(hires::rowOffset(5)) == 0xFF && r.byte(hires::rowOffset(5) + 1) == 0xFF);
    Page s;
    s.clear(0xFF);
    hires::hLine(s, 0, 6, 0, 1, false);  // palette bit cleared on touched bytes
    CHECK(s.byte(0) == 0x7F && s.byte(1) == 0xFF);
    Page v;
    hires::vLine(v, 10, 2, 4);  // column 1, bit 3, rows 2..4
    for (int y = 0; y < 8; ++y) CHECK(v.byte(hires::rowOffset(y) + 1) == ((y >= 2 && y <= 4) ? 0x88 : 0x80));
    Page none;
    hires::hLine(none, 5, 3, 0, 2, true);  // x0 > x1 draws nothing
    CHECK(countNot(none, 0x80) == 0);
}

TEST(hires_walls_from_a_synthetic_table) {
    std::vector<std::uint8_t> table(hires::kWallTableBytes);
    for (std::size_t i = 0; i < 72; ++i) {  // horizontal triples (x0, x1, y): x0 = 1 > x1 = 0 draws nothing
        table[3 * i] = 1;
        table[3 * i + 1] = 0;
        table[3 * i + 2] = 0;
    }
    for (std::size_t i = 72; i < 142; ++i) {  // vertical triples (x, y0, y1): y0 = 1 > y1 = 0 draws nothing
        table[3 * i] = 0;
        table[3 * i + 1] = 1;
        table[3 * i + 2] = 0;
    }
    Page empty;
    CHECK(hires::drawWalls(empty, table, true));
    CHECK(countNot(empty, 0x80) == 0);
    table[0] = 0; table[1] = 13; table[2] = 5;  // first horizontal: x 0..13, row 5
    table[3 * 72] = 10; table[3 * 72 + 1] = 0; table[3 * 72 + 2] = 3;  // first vertical: x 10, rows 0..3
    Page colour, white;
    CHECK(hires::drawWalls(colour, table, true));
    CHECK(hires::drawWalls(white, table, false));
    CHECK(colour.byte(hires::rowOffset(5)) == 0xD5);
    CHECK(white.byte(hires::rowOffset(5)) == 0xFF);
    CHECK(colour.byte(hires::rowOffset(1) + 1) == 0x88 && white.byte(hires::rowOffset(1) + 1) == 0x88);  // the vertical line is the same in both modes
    CHECK(pageHash(colour) != pageHash(white));
    CHECK(!hires::drawWalls(colour, std::span<const std::uint8_t>(table).first(100), true));  // table too short
}

TEST(hires_blitters) {
    const std::array<std::uint8_t, 6> data{0x01, 0x02, 0x03, 0x84, 0x05, 0x06};
    Page a;
    hires::blitOr(a, 4, 10, 3, 2, data);
    CHECK(a.byte(hires::rowOffset(10) + 4) == 0x81 && a.byte(hires::rowOffset(10) + 5) == 0x82 && a.byte(hires::rowOffset(10) + 6) == 0x83);
    CHECK(a.byte(hires::rowOffset(11) + 4) == 0x84 && a.byte(hires::rowOffset(11) + 6) == 0x86);  // row 2 = data[3..5] | $80
    CHECK(countNot(a, 0x80) == 6);
    Page b;
    hires::blitOrClearPalette(b, 4, 10, 3, 2, data);  // (page | data) & $7F
    CHECK(b.byte(hires::rowOffset(10) + 4) == 0x01 && b.byte(hires::rowOffset(11) + 4) == 0x04);
    Page c;
    c.clear(0xFF);
    hires::blitMergePalette(c, 0, 0, 3, 2, data);  // (page & $7F) | data
    CHECK(c.byte(0) == 0x7F && c.byte(hires::rowOffset(1)) == 0xFF);
    Page d;
    hires::blitOr(d, 38, 190, 3, 2, data);  // column 38 + 2 spills over the row end exactly like the original; row 191 is the last
    CHECK(d.byte(hires::rowOffset(190) + 40) == 0x83);
    hires::blitOr(d, 0, 190, 3, 4, std::vector<std::uint8_t>(12, 0x01));  // rows 192 and 193 do not exist: skipped
    CHECK(d.byte(hires::rowOffset(191)) == 0x81);
    Page e;
    hires::blitOr(e, 0, 0, 3, 2, std::span<const std::uint8_t>(data).first(5));  // data too short: nothing
    CHECK(countNot(e, 0x80) == 0);
    const std::vector<std::uint8_t> seven(7 * 6, 0);
    CHECK(hires::shiftedCopy(seven, 15, 3, 2).size() == 6);                       // x = 15: shift 1
    CHECK(hires::shiftedCopy(seven, 15, 3, 2).data() == seven.data() + 6);
    CHECK(hires::shiftedCopy(seven, 6, 3, 2).data() == seven.data() + 36);
    CHECK(hires::shiftedCopy(std::span<const std::uint8_t>(seven).first(30), 6, 3, 2).empty());
}

TEST(hires_masked_clear_box_clear_dot_tile) {
    std::array<std::uint8_t, hires::kMaskTableBytes> mask{};
    for (std::size_t i = 0; i < mask.size(); ++i) mask[i] = static_cast<std::uint8_t>(0x10 + i);
    Page m;
    m.clear(0xFF);
    hires::maskedClear(m, 15, 20, mask);  // x = 15: column 2, shift 1: masks 4, 5, 6
    CHECK(m.byte(hires::rowOffset(20) + 2) == ((0xFF & 0x14) | 0x80));
    CHECK(m.byte(hires::rowOffset(29) + 4) == ((0xFF & 0x16) | 0x80));
    CHECK(m.byte(hires::rowOffset(30) + 2) == 0xFF);   // only 10 rows
    CHECK(m.byte(hires::rowOffset(20) + 5) == 0xFF);   // only 3 bytes
    Page bc;
    bc.clear(0);
    hires::boxClear(bc, 4, 100);
    CHECK(bc.byte(hires::rowOffset(100) + 4) == 0x80 && bc.byte(hires::rowOffset(111) + 11) == 0x80);
    CHECK(bc.byte(hires::rowOffset(112) + 4) == 0 && bc.byte(hires::rowOffset(100) + 12) == 0 && bc.byte(hires::rowOffset(100) + 3) == 0);
    CHECK(countNot(bc, 0) == 96);
    std::array<std::uint8_t, hires::kDotTableBytes> dots{};
    for (std::size_t i = 0; i < dots.size(); ++i) dots[i] = static_cast<std::uint8_t>(i + 1);
    Page dp;
    hires::smallDot(dp, 8, 50, dots);  // shift 1: bytes 3, 4 on two rows
    CHECK(dp.byte(hires::rowOffset(50) + 1) == (0x80 | 3) && dp.byte(hires::rowOffset(51) + 2) == (0x80 | 4));
    CHECK(countNot(dp, 0x80) == 4);
    std::array<std::uint8_t, hires::kTileTableBytes> tiles{};
    for (std::size_t i = 0; i < tiles.size(); ++i) tiles[i] = static_cast<std::uint8_t>(i);
    Page tp;
    hires::tile(tp, 14, 60, tiles);  // shift 0, column 2: rows 0..7, entries 0..15
    CHECK(tp.byte(hires::rowOffset(60) + 2) == 0x80 && tp.byte(hires::rowOffset(60) + 3) == 0x81);
    CHECK(tp.byte(hires::rowOffset(67) + 3) == (0x80 | 15));
    Page tp2;
    hires::tile(tp2, 15, 60, tiles);  // shift 1: starts at entry 16
    CHECK(tp2.byte(hires::rowOffset(60) + 2) == (0x80 | 16));
}

TEST(hires_string_printer_with_synthetic_glyphs) {
    std::vector<std::uint8_t> glyphs(hires::kGlyphTableBytes);
    for (std::size_t g = 0; g < 33; ++g) {
        for (std::size_t r = 0; r < 9; ++r) glyphs[g * 9 + r] = static_cast<std::uint8_t>(g + 1);  // glyph g = 9 rows of g + 1
    }
    // column 3, row 10: glyph 4, blank, glyph 1 (two rows lower), the special glyph, end
    const std::vector<std::uint8_t> record{3, 10, 4, 0x20, 1, 0x21, 0xFF, 0x77};
    Page p;
    CHECK(hires::drawString(p, glyphs, record) == 7);  // consumed up to and including the terminator
    CHECK(p.byte(hires::rowOffset(10) + 3) == (0x80 | 5));
    CHECK(p.byte(hires::rowOffset(10) + 4) == 0x80);                  // the blank
    CHECK(p.byte(hires::rowOffset(12) + 5) == (0x80 | 2));            // glyph 1 starts two rows lower
    CHECK(p.byte(hires::rowOffset(10) + 6) == (0x80 | 33));           // code $21 = glyph index 32
    CHECK(hires::drawString(p, glyphs, std::span<const std::uint8_t>(record).first(5)) == 0);  // not terminated
}

namespace {

std::optional<std::vector<std::uint8_t>> loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

}  // namespace

// Needs --image. The expected hashes come from the Python models that were verified against the oracle (tests/test_wall_model.py, tests/test_text_model.py).
TEST(hires_real_walls_and_text_match_the_verified_models) {
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
    CHECK(!normal.empty() && !forced.empty());
    if (normal.empty() || forced.empty()) return;
    Page w1, w0;
    CHECK(hires::drawWalls(w1, normal, true));
    CHECK(hires::drawWalls(w0, forced, false));
    CHECK(pageHash(w1) == "DBE78BE0A8973BA18CB601043F7233B1714C6FDCF87AB264FB20D1CD7FC7B990");  // the normal game's maze (1,327 lit bytes)
    CHECK(pageHash(w0) == "7DAE9FE7E10656E528696CCBD297370B1FAF259F310DA18E1A4DBDF3DFF77BA7");  // the forced $1D = 0 mode

    const auto glyphs = pack.find(0x4553, hires::kGlyphTableBytes);
    const auto strings = pack.find(0x434F, 0x43D2 - 0x434F);  // the 12 string records of the attract screens ($434F-$43D1, 131 bytes: one manifest range)
    CHECK(!glyphs.empty() && !strings.empty());
    if (glyphs.empty() || strings.empty()) return;
    const std::array<const char*, 3> expected{"41A4BBA82AE9AB9A7BBFD29CDE82E5E3C4BA2F6CA57715E364901168FE422768",
                                              "4FD6F355224C174FA110B09772D9AB576090BA959BDBEFEF9C214E5E09AB578A",
                                              "B470A3E420D2751024931F3EC9F5F037B9124D39185B746970DA3A1929375167"};
    const std::array<int, 3> counts{1, 5, 12};
    Page t;
    std::size_t pos = 0;
    int done = 0;
    for (std::size_t k = 0; k < counts.size(); ++k) {
        while (done < counts[k]) {
            const std::size_t used = hires::drawString(t, glyphs, strings.subspan(pos));
            CHECK(used > 0);
            if (used == 0) return;
            pos += used;
            ++done;
        }
        CHECK(pageHash(t) == expected[k]);
    }
    CHECK(pos == strings.size());  // the 12 records fill the span exactly
}
