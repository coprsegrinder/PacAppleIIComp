// TASK-033-4e2: the hidden menu (Ctrl-V, Ctrl-C at the players prompt) against the oracle: tools/capture_hidden.py drove the original with 13 key lists (cartoons 1-3, ranks 7,
// 21, 22, 0, 12, 10, 13, 5, empty lines, editing keys); tools/export_hidden.py packs them. Needs --image and --hidden.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/attractflow.h"
#include "../src/core/cartoon.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;

namespace {

std::vector<std::uint8_t> readAllH(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool loadPackH(datapack::Pack& pack) {
    const auto built = datapack::buildPack(readAllH(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    if (!built.ok) return false;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    if (!loaded.ok) return false;
    pack = loaded.pack;
    return true;
}

// The oracle's stand-in for the line input prints the monitor's prompt character in front of what is typed; the original's entry $FD6F is behind the prompt output, so the
// character is taken out of the oracle's page before the comparison (in the rows that start with one of the three prompt texts).
void dropPromptChar(std::uint8_t* page, const std::vector<std::vector<std::uint8_t>>& texts) {
    for (int r = 0; r < 24; ++r) {
        std::uint8_t* row = page + core::TextScreen::rowOffset(r);
        for (const auto& t : texts) {
            if (std::memcmp(row, t.data(), t.size()) == 0 && row[t.size()] == 0xDD) {
                std::memmove(row + t.size(), row + t.size() + 1, 40 - t.size() - 1);
                row[39] = 0xA0;
                break;
            }
        }
    }
}

}  // namespace

TEST(hidden_menu_equals_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().hidden.empty()) {
        std::printf("    SKIPPED (needs --image and --hidden)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPackH(pack));
    const auto f = readAllH(testfw::config().hidden);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "HIDN", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    // the prompt texts as the oracle's page shows them
    std::vector<std::vector<std::uint8_t>> texts;
    {
        const auto s = pack.find(0x41DC, 59);
        if (s.size() < 59) return;
        texts.emplace_back(s.begin(), s.begin() + 17);
        texts.emplace_back(s.begin() + 0x25, s.begin() + 0x25 + 22);
        texts.emplace_back(s.begin() + 17, s.begin() + 17 + 20);
    }
    std::size_t p = 8, bad = 0, waits = 0, pageDiffs = 0, pagesCompared = 0;
    for (std::uint32_t run = 0; run < count; ++run) {
        std::uint32_t n = 0;
        std::memcpy(&n, f.data() + p, 4);
        p += 4;
        const std::uint8_t* keys = f.data() + p;
        p += n;
        core::AttractFlow flow;
        CHECK(flow.start(pack));
        flow.advance(100'000);
        flow.keyDown(core::kKeySpace);
        while (flow.mode() == core::AttractFlow::Mode::Attract) flow.advance(10'000);
        flow.advance(30'000'000);
        bool ok = true;
        for (std::uint32_t i = 0; i <= n; ++i) {
            std::uint32_t cyc = 0;
            std::memcpy(&cyc, f.data() + p, 4);
            const int rank = f.data()[p + 4];
            std::uint8_t page[1024];
            std::memcpy(page, f.data() + p + 6, 1024);
            p += 6 + 1024;
            if (i > 0) {
                flow.keyDown(keys[i - 1]);
                flow.advance(30'000'000);
            }
            ++waits;
            const bool clockOk = flow.promptClock() == static_cast<std::int64_t>(cyc) && flow.textReady();
            const bool rankOk = flow.startLevel() == rank;
            bool pageOk = true;
            dropPromptChar(page, texts);
            for (std::size_t b = 0; b < 1024; ++b)
                if (page[b] >= 0xE0) page[b] &= 0xDF;     // the stand-in echoes a lower-case key as typed; the ROM turns it into upper case before the echo ($FD7E)
            const bool standInWrong = run == 11 && i >= 4;   // run 12: after Ctrl-X the stand-in loses the prompt character and the typed key (its page shows neither)
            std::size_t d = 0;
            for (std::size_t b = 0; b < 1024 && !standInWrong; ++b) d += flow.text().byte(b) != page[b];
            if (!standInWrong) ++pagesCompared;
            if (d != 0) {
                ++pageDiffs;
                pageOk = false;
            }
            if (!clockOk || !rankOk || !pageOk) {
                ok = false;
                std::printf("    run %u key %u: clock mine %lld oracle %u, rank mine %d oracle %d, page differs in %zu bytes\n", run + 1, i, static_cast<long long>(flow.promptClock()), cyc, flow.startLevel(), rank, d);
            }
        }
        if (!ok) ++bad;
    }
    std::printf("    %u key lists of the hidden menu, %zu key waits compared (cycle, text page without the prompt character, rank $54E1): %zu runs differ, %zu pages differ of %zu\n", count, waits, bad, pageDiffs, pagesCompared);
    CHECK(bad == 0);
}

// The editing keys of the line input, from the monitor ROM listing ($FD35-$FD8E; the oracle's stand-in does not model them well): Ctrl-X prints a backslash, goes to a new line
// and prints the prompt character there; Backspace at the start of the line does the same without the backslash; Ctrl-U takes the character under the cursor.
TEST(hidden_menu_line_editing_follows_the_monitor_rom) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPackH(pack));
    auto open = [&](core::AttractFlow& flow) {
        CHECK(flow.start(pack));
        flow.advance(100'000);
        flow.keyDown(core::kKeySpace);
        while (flow.mode() == core::AttractFlow::Mode::Attract) flow.advance(10'000);
        flow.advance(30'000'000);
        flow.keyDown(core::kKeyCtrlV);
        flow.advance(30'000'000);
        flow.keyDown(core::kKeyCtrlC);
        flow.advance(30'000'000);
        CHECK(flow.mode() == core::AttractFlow::Mode::RankOrCartoon && flow.textReady());
    };
    auto type = [&](core::AttractFlow& flow, std::uint8_t k) {
        flow.keyDown(k);
        flow.advance(1'000'000);   // (a key goes into the latch only when the previous prompt is complete: run its cycles out)
    };
    {
        core::AttractFlow f;
        open(f);
        CHECK(f.cursorCol() == 17 && f.cursorRow() == 0);
        type(f, 0xB9);
        type(f, core::kKeyCtrlX);
        const auto& t = f.text();
        CHECK(t.byte(17) == 0xB9 && t.byte(18) == 0xDC);                                    // "9"
        CHECK(t.byte(core::TextScreen::rowOffset(1)) == 0xDD && f.cursorCol() == 1 && f.cursorRow() == 1);   // the prompt character at the start of the next line
        type(f, 0xB4);
        CHECK(t.byte(core::TextScreen::rowOffset(1) + 1) == 0xB4);
        CHECK(f.lineBuffer()[0] == 0xB4);                                                    // the line starts again at index 0
        type(f, core::kKeyReturn);
        CHECK(f.mode() == core::AttractFlow::Mode::RankNumber);                              // not a C: the rank prompt
    }
    {
        core::AttractFlow f;
        open(f);
        type(f, core::kKeyBackspace);                                                         // at the start of the line: the cursor moves left, then a new line with the prompt
        CHECK(f.cursorCol() == 1 && f.cursorRow() == 1);
        CHECK(f.text().byte(core::TextScreen::rowOffset(1)) == 0xDD);
        type(f, 0xE3);                                                                        // 'c' typed in lower case: upper case in the buffer and on the screen
        CHECK(f.lineBuffer()[0] == 0xC3 && f.text().byte(core::TextScreen::rowOffset(1) + 1) == 0xC3);
        type(f, core::kKeyReturn);
        CHECK(f.mode() == core::AttractFlow::Mode::CartoonNumber);                            // the first letter C decides
        // Esc takes the next key away
        type(f, core::kKeyEsc);
        type(f, 0xB1);
        CHECK(f.cursorCol() == 22);
        // Ctrl-U copies the character under the cursor (a space here)
        type(f, 0x95);
        CHECK(f.lineBuffer()[0] == 0xA0 && f.cursorCol() == 23);
    }
    {
        // two digits "00" give rank 0 (stored: below 22), "99" is rejected, a single "0" is rejected
        core::AttractFlow f;
        open(f);
        type(f, 0xC1);
        type(f, core::kKeyReturn);
        type(f, 0xB0);
        type(f, 0xB0);
        type(f, core::kKeyReturn);
        CHECK(f.mode() == core::AttractFlow::Mode::Players && f.startLevel() == 0);
    }
}
