// TASK-033-4e: the loop of the attract mode and its prompts against the oracle (tools/capture_prompts.py: the original with a Space pressed at 15 different times, then a digit,
// then Space or Return; tools/export_prompts.py packs the runs). Needs --image and --prompts.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/attractflow.h"
#include "../src/core/textscreen.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;

namespace {

std::vector<std::uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool loadPackFlow(datapack::Pack& pack) {
    const auto built = datapack::buildPack(readAll(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    if (!built.ok) return false;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    if (!loaded.ok) return false;
    pack = loaded.pack;
    return true;
}

}  // namespace

TEST(flow_space_prompts_and_return_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().prompts.empty()) {
        std::printf("    SKIPPED (needs --image and --prompts)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPackFlow(pack));
    const auto f = readAll(testfw::config().prompts);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "PRMP", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    const std::size_t rec = 32 + 2048;
    CHECK(f.size() == 8 + static_cast<std::size_t>(count) * rec);
    std::size_t bad = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t v[8];
        const std::uint8_t* p = f.data() + 8 + static_cast<std::size_t>(i) * rec;
        std::memcpy(v, p, 32);
        const std::uint8_t* page2 = p + 32 + 1024;
        const std::uint32_t offset = v[0], digit = v[1], lastReturn = v[2], turn = v[3], poll = v[4], wait1 = v[5], wait2 = v[6], end = v[7];
        core::AttractFlow flow;
        CHECK(flow.start(pack));
        flow.advance(offset);
        flow.keyDown(core::kKeySpace);
        std::int64_t guard = 0;
        while (flow.mode() == core::AttractFlow::Mode::Attract && guard++ < 4000) flow.advance(50'000);
        // the tick cost model makes the demo's poll times drift by up to ~2,500 cycles inside the demo (its error reaches about 1,300 at one place) (tests/test_attract.cpp); everything else is exact
        const std::int64_t tol = (offset >= 23'000'000 && turn == 0) ? 2'500 : 0;
        const std::int64_t d = flow.spaceTakenAt() - static_cast<std::int64_t>(poll);
        bool ok = flow.mode() == core::AttractFlow::Mode::Players && flow.spaceTakenInTurn() == turn && std::llabs(d) <= tol;
        if (!ok) std::printf("    run %u (Space at %u): taken in turn %zu at %lld, oracle turn %u at %u (diff %lld)\n", i + 1, offset, flow.spaceTakenInTurn(), static_cast<long long>(flow.spaceTakenAt()), turn, poll, static_cast<long long>(d));
        ok = ok && flow.playersWaitAt() - flow.spaceTakenAt() == static_cast<std::int64_t>(wait1) - static_cast<std::int64_t>(poll);
        // the digit is already in the latch when the first prompt is complete
        flow.keyDown(static_cast<std::uint8_t>(0xB0 + digit));
        flow.advance(core::AttractFlow::kTakenToPlayersWait + 100);
        ok = ok && flow.mode() == core::AttractFlow::Mode::PressReturn;
        ok = ok && flow.pressWaitAt() - flow.spaceTakenAt() == static_cast<std::int64_t>(wait2) - static_cast<std::int64_t>(poll);
        std::size_t diff2 = 0;
        for (std::size_t b = 0; b < 1024; ++b) diff2 += flow.text().byte(b) != page2[b];
        if (diff2 != 0) std::printf("    run %u: the press-return page differs in %zu bytes\n", i + 1, diff2);
        ok = ok && diff2 == 0;
        flow.keyDown(lastReturn ? core::kKeyReturn : core::kKeySpace);
        flow.advance(core::AttractFlow::kDigitToPressWait + 10);
        const bool back = lastReturn == 0;
        ok = ok && (back ? flow.mode() == core::AttractFlow::Mode::Attract : flow.mode() == core::AttractFlow::Mode::Game);
        ok = ok && flow.players() == static_cast<int>(digit);
        // the original's cycle of $43D2 is 19 after the key wait; the turn's own $52BB follows 11 cycles later
        const std::int64_t endExpect = back ? static_cast<std::int64_t>(end) : static_cast<std::int64_t>(end);
        ok = ok && flow.endAt() - flow.spaceTakenAt() == endExpect - static_cast<std::int64_t>(poll);
        if (!ok) {
            ++bad;
            std::printf("    run %u (Space at %u, digit %u, %s): mismatch\n", i + 1, offset, digit, lastReturn ? "Return" : "Space");
        }
    }
    std::printf("    %u runs of the original (Space pressed at 15 different places of the loop): %zu differ\n", count, bad);
    CHECK(bad == 0);
}

TEST(flow_first_prompt_page_equals_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().prompts.empty()) {
        std::printf("    SKIPPED (needs --image and --prompts)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPackFlow(pack));
    const auto f = readAll(testfw::config().prompts);
    if (f.size() <= 8) return;
    const std::uint8_t* p = f.data() + 8;
    core::AttractFlow flow;
    CHECK(flow.start(pack));
    flow.advance(100'000);
    flow.keyDown(core::kKeySpace);
    while (flow.mode() == core::AttractFlow::Mode::Attract) flow.advance(10'000);
    flow.advance(core::AttractFlow::kTakenToPlayersWait + 100);
    CHECK(flow.textReady());
    std::size_t diff = 0;
    for (std::size_t b = 0; b < 1024; ++b) diff += flow.text().byte(b) != p[32 + b];
    std::printf("    the first prompt page: %zu of 1,024 bytes differ from the oracle's text page\n", diff);
    CHECK(diff == 0);
}

// The rest of the key poll $6F77 (read from the code, not run against the oracle: V for the instruction semantics, the cycle counts of the keys other than Space are not compared):
// Ctrl-S toggles the sound switch, Esc waits for the next key (which is then tested like any key: Space leads to the prompt), every other key is thrown away without effect.
TEST(flow_other_keys_and_the_loop) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPackFlow(pack));
    core::AttractFlow flow;
    CHECK(flow.start(pack));
    flow.advance(5'000'000);
    flow.keyDown(0xD8);                       // 'X'
    flow.advance(300'000);                    // past the next poll
    CHECK(flow.mode() == core::AttractFlow::Mode::Attract);
    CHECK(flow.soundOn());
    flow.keyDown(core::kKeyCtrlS);
    flow.advance(300'000);
    CHECK(!flow.soundOn());
    flow.keyDown(core::kKeyEsc);
    flow.advance(300'000);
    CHECK(flow.paused());
    const std::int64_t frozen = flow.turn().now();
    flow.advance(5'000'000);
    CHECK(flow.turn().now() == frozen);       // nothing moves while the original waits for a key
    flow.keyDown(0xD8);
    flow.advance(1'000);
    CHECK(!flow.paused() && flow.mode() == core::AttractFlow::Mode::Attract);
    flow.advance(300'000);
    flow.keyDown(core::kKeyEsc);
    flow.advance(300'000);
    CHECK(flow.paused());
    flow.keyDown(core::kKeySpace);
    flow.advance(1'000);
    CHECK(flow.mode() == core::AttractFlow::Mode::Players);
    // other keys at the prompts do nothing, Ctrl-V is not ported yet, digits 6-9 and 0 are not accepted
    flow.advance(core::AttractFlow::kTakenToPlayersWait + 10);
    CHECK(flow.textReady());
    for (std::uint8_t k : {std::uint8_t{0xB0}, std::uint8_t{0xB6}, std::uint8_t{0xB9}, std::uint8_t{0xC1}, std::uint8_t{0x8D}, std::uint8_t{0xA0}}) {
        flow.keyDown(k);
        flow.advance(10);
        CHECK(flow.mode() == core::AttractFlow::Mode::Players);
    }
    flow.keyDown(0xB4);
    flow.advance(10);
    CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn && flow.players() == 4);
    flow.advance(core::AttractFlow::kDigitToPressWait);
    flow.keyDown(0xB2);                       // a digit here does nothing
    flow.advance(10);
    CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn);
    // the loop: without any key the turns follow each other
    core::AttractFlow loop;
    CHECK(loop.start(pack));
    for (int i = 0; i < 700; ++i) loop.advance(100'000);
    CHECK(loop.turnIndex() == 1);
    CHECK(loop.mode() == core::AttractFlow::Mode::Attract);
    // Return starts a game; after GAME OVER the loop starts again with a new turn
    core::AttractFlow g;
    CHECK(g.start(pack));
    g.advance(200'000);
    g.keyDown(core::kKeySpace);
    g.advance(200'000);
    g.advance(core::AttractFlow::kTakenToPlayersWait);
    g.keyDown(0xB2);
    g.advance(10);
    g.advance(core::AttractFlow::kDigitToPressWait);
    g.keyDown(core::kKeyReturn);
    g.advance(10);
    CHECK(g.mode() == core::AttractFlow::Mode::Game && g.players() == 2);
    g.returnFromGame();
    CHECK(g.mode() == core::AttractFlow::Mode::Attract && g.turn().now() == 0 && g.turnIndex() == 1);
}
