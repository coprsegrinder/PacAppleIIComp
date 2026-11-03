// TASK-033-4e: the text screen (COUT effect, scrolling, character generator pictures) and the keyboard rules of the flow that are modelled without an oracle run.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "../src/core/attractflow.h"
#include "../src/core/textscreen.h"
#include "test_framework.h"

using namespace pacman;

TEST(textscreen_cout_effect) {
    core::TextScreen t;
    CHECK(t.byte(0) == 0xA0 && t.byte(0x3FF) == 0xA0);
    for (char c : std::string("HI")) t.put(static_cast<std::uint8_t>(c) | 0x80);
    CHECK(t.byte(0) == 0xC8 && t.byte(1) == 0xC9 && t.col() == 2);
    t.put(0x8D);
    CHECK(t.col() == 0 && t.row() == 1);
    t.put(0xC1);
    CHECK(t.byte(core::TextScreen::rowOffset(1)) == 0xC1);
    // the interleaved rows: row 8 starts at $28, row 16 at $50, row 1 at $80
    CHECK(core::TextScreen::rowOffset(8) == 0x28 && core::TextScreen::rowOffset(16) == 0x50 && core::TextScreen::rowOffset(1) == 0x80);
    // line wrap and scrolling
    core::TextScreen s;
    s.setCursor(0, 23);
    s.put(0xC2);
    s.put(0x8D);                  // new line on the last row scrolls
    CHECK(s.byte(core::TextScreen::rowOffset(22)) == 0xC2 && s.byte(core::TextScreen::rowOffset(23)) == 0xA0 && s.row() == 23);
    core::TextScreen w;
    for (int i = 0; i < 41; ++i) w.put(0xC3);
    CHECK(w.row() == 1 && w.col() == 1);
}

TEST(textscreen_pictures_use_the_character_rom) {
    if (testfw::config().charRom.empty()) {
        std::printf("    SKIPPED (needs --charrom)\n");
        return;
    }
    core::CharacterRom rom;
    CHECK(rom.load(testfw::config().charRom));
    core::TextScreen t;
    t.put(0xC8);   // 'H'
    t.put(0x60);   // flashing space
    t.put(0x08);   // inverse 'H'
    std::vector<std::uint8_t> px;
    core::renderText(t, rom, false, -1, -1, px);
    CHECK(px.size() == static_cast<std::size_t>(core::kTextWidth) * core::kTextHeight);
    // 'H': rows 1-3 and 5-7 have lit pixels in columns 1 and 5, row 4 is the bar (columns 1-5)
    auto at = [&](int x, int y) { return px[static_cast<std::size_t>(y) * core::kTextWidth + static_cast<std::size_t>(x)]; };
    CHECK(at(1, 1) == 1 && at(5, 1) == 1 && at(3, 1) == 0 && at(3, 4) == 1 && at(0, 4) == 0 && at(1, 0) == 0);
    // the inverse 'H' (third cell) is the negative
    CHECK(at(14 + 1, 1) == 0 && at(14 + 3, 1) == 1 && at(14, 0) == 1);
    // the flashing space (second cell) is dark in one phase and lit in the other
    CHECK(at(7 + 3, 3) == 0);
    core::renderText(t, rom, true, -1, -1, px);
    CHECK(at(7 + 3, 3) == 1);
    // the cursor makes the cell under it a flashing one
    core::renderText(t, rom, true, 5, 0, px);
    CHECK(at(35 + 3, 3) == 1);
    // a picture to look at: scratch/text_prompt.pgm (P5)
    std::ofstream o("scratch/text_prompt.pgm", std::ios::binary);
    core::TextScreen p;
    const std::string line = "HOW MANY PLAYERS?  (1 - 5)   3";
    for (char c : line) p.put(static_cast<std::uint8_t>(c) | 0x80);
    p.setCursor(0, 11);
    for (char c : std::string("PRESS <RETURN> TO PLAY")) p.put(static_cast<std::uint8_t>(c) | 0x80);
    core::renderText(p, rom, false, 30, 0, px);
    o << "P5\n" << core::kTextWidth << " " << core::kTextHeight << "\n255\n";
    for (std::uint8_t v : px) o.put(static_cast<char>(v ? 255 : 0));
}
