// TASK-033-4e / 4e3: the loop of the attract mode and its prompts against the oracle (tools/capture_prompts.py with A2ROM=1: the original with a Space pressed at 15 different
// times, then a digit, then Space or Return; the monitor ROM of the Apple II Plus runs in the oracle and in the port; tools/export_prompts.py packs the runs). Needs --image and
// --prompts and the ROM dump in "Apple II roms/".
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "flow_common.h"

using namespace pacman;
using flowtest::settle;

TEST(flow_space_prompts_and_return_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().prompts.empty()) {
        std::printf("    SKIPPED (needs --image and --prompts)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    const auto f = flowtest::readAll(testfw::config().prompts);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "PRMP", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    const std::size_t rec = 32 + 2048 + 512;
    CHECK(f.size() == 8 + static_cast<std::size_t>(count) * rec);
    std::size_t bad = 0, pagesCompared = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t v[8];
        const std::uint8_t* p = f.data() + 8 + static_cast<std::size_t>(i) * rec;
        std::memcpy(v, p, 32);
        const std::uint8_t* page1 = p + 32;
        const std::uint8_t* page2 = p + 32 + 1024;
        const std::uint8_t* zp1 = p + 32 + 2048;
        const std::uint8_t* zp2 = p + 32 + 2048 + 256;
        const std::uint32_t offset = v[0], digit = v[1], lastReturn = v[2], turn = v[3], poll = v[4], wait1 = v[5], wait2 = v[6], end = v[7];
        core::AttractFlow flow;
        CHECK(flow.start(pack, flowtest::romPath()));
        flow.advance(offset);
        flow.keyDown(core::kKeySpace);
        std::int64_t guard = 0;
        while (flow.mode() == core::AttractFlow::Mode::Attract && guard++ < 4000) flow.advance(50'000);
        // the tick cost model makes the demo's poll times drift by up to ~2,500 cycles inside the demo (tests/test_attract.cpp); everything else is exact
        const std::int64_t tol = (offset >= 23'000'000 && turn == 0) ? 2'500 : 0;
        const std::int64_t d = flow.spaceTakenAt() - static_cast<std::int64_t>(poll);
        bool ok = flow.mode() == core::AttractFlow::Mode::Players && flow.spaceTakenInTurn() == turn && std::llabs(d) <= tol;
        if (!ok) std::printf("    run %u (Space at %u): taken in turn %zu at %lld, oracle turn %u at %u (diff %lld)\n", i + 1, offset, flow.spaceTakenInTurn(), static_cast<long long>(flow.spaceTakenAt()), turn, poll, static_cast<long long>(d));
        // the first key wait: the cycle (counted from $6FAE), the text page with the flashing cursor, the zero page of the monitor and the random counter
        flow.advance(flow.busyCycles());
        const bool ready1 = flow.textReady() && flow.waitClock() == static_cast<std::int64_t>(wait1) - static_cast<std::int64_t>(poll);
        std::size_t dp = 0;
        for (std::size_t b = 0; b < 1024; ++b) dp += flow.text().byte(b) != page1[b];
        const std::size_t dz = flowtest::zeroPageDiff(flow.waitZeroPage(), zp1);
        ++pagesCompared;
        if (!ready1 || dp != 0 || dz != 0) std::printf("    run %u first wait: clock mine %lld oracle %lld, page %zu bytes, zero page %zu bytes differ\n", i + 1, static_cast<long long>(flow.waitClock()), static_cast<long long>(wait1) - static_cast<long long>(poll), dp, dz);
        ok = ok && ready1 && dp == 0 && dz == 0;
        // the digit, then the second key wait (the oracle presses it at once; the port may have looped in KEYIN for a while, which shifts its clock and random counter)
        const std::int64_t spun = flow.promptClock() - flow.waitClock();
        const int rndShift = flowtest::rndOf(flow.monitor()) - (zp1[0x4E] | (zp1[0x4F] << 8));
        flow.keyDown(static_cast<std::uint8_t>(0xB0 + digit));
        settle(flow);
        const bool ready2 = flow.mode() == core::AttractFlow::Mode::PressReturn && flow.textReady() && flow.waitClock() == static_cast<std::int64_t>(wait2) - static_cast<std::int64_t>(poll) + spun;
        dp = 0;
        for (std::size_t b = 0; b < 1024; ++b) dp += flow.text().byte(b) != page2[b];
        const std::size_t dz2 = flowtest::zeroPageDiff(flow.waitZeroPage(), zp2, rndShift) + (flow.waitZeroPage()[0x51] != zp2[0x51]);
        ++pagesCompared;
        if (!ready2 || dp != 0 || dz2 != 0) std::printf("    run %u second wait: clock mine %lld oracle %lld, page %zu bytes, zero page %zu bytes differ\n", i + 1, static_cast<long long>(flow.waitClock()), static_cast<long long>(wait2) - static_cast<long long>(poll) + static_cast<long long>(spun), dp, dz2);
        ok = ok && ready2 && dp == 0 && dz2 == 0;
        flow.keyDown(lastReturn ? core::kKeyReturn : core::kKeySpace);
        settle(flow);
        const bool back = lastReturn == 0;
        ok = ok && (back ? flow.mode() == core::AttractFlow::Mode::Attract : flow.mode() == core::AttractFlow::Mode::Game);
        ok = ok && flow.players() == static_cast<int>(digit);
        ok = ok && flow.endAt() == static_cast<std::int64_t>(end) - static_cast<std::int64_t>(poll) + spun;
        if (flow.endAt() != static_cast<std::int64_t>(end) - static_cast<std::int64_t>(poll))
            std::printf("    run %u end: mine %lld oracle %lld\n", i + 1, static_cast<long long>(flow.endAt()), static_cast<long long>(end) - static_cast<long long>(poll));
        if (!ok) {
            ++bad;
            std::printf("    run %u (Space at %u, digit %u, %s): mismatch\n", i + 1, offset, digit, lastReturn ? "Return" : "Space");
        }
    }
    std::printf("    %u runs of the original (Space pressed at 15 different places of the loop; %zu key waits compared: cycle, text page, zero page, random counter): %zu differ\n", count, pagesCompared, bad);
    CHECK(bad == 0);
}

// The rest of the key poll $6F77 (read from the code, not run against the oracle: V for the instruction semantics, the cycle counts of the keys other than Space are not compared):
// Ctrl-S toggles the sound switch, Esc waits for the next key (which is then tested like any key: Space leads to the prompt), every other key is thrown away without effect.
TEST(flow_other_keys_and_the_loop) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    core::AttractFlow flow;
    CHECK(flow.start(pack, flowtest::romPath()));
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
    // other keys at the prompts do nothing, digits 6-9 and 0 are not accepted
    settle(flow);
    CHECK(flow.textReady());
    for (std::uint8_t k : {std::uint8_t{0xB0}, std::uint8_t{0xB6}, std::uint8_t{0xB9}, std::uint8_t{0xC1}, std::uint8_t{0x8D}, std::uint8_t{0xA0}}) {
        flow.keyDown(k);
        settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::Players);
    }
    flow.keyDown(0xB4);
    settle(flow);
    CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn && flow.players() == 4);
    flow.keyDown(0xB2);                       // a digit here does nothing
    settle(flow);
    CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn);
    // the loop: without any key the turns follow each other
    core::AttractFlow loop;
    CHECK(loop.start(pack, flowtest::romPath()));
    for (int i = 0; i < 700; ++i) loop.advance(100'000);
    CHECK(loop.turnIndex() == 1);
    CHECK(loop.mode() == core::AttractFlow::Mode::Attract);
    // Return starts a game; after GAME OVER the loop starts again with a new turn
    core::AttractFlow g;
    CHECK(g.start(pack, flowtest::romPath()));
    g.advance(200'000);
    g.keyDown(core::kKeySpace);
    g.advance(200'000);
    settle(g);
    g.keyDown(0xB2);
    settle(g);
    g.keyDown(core::kKeyReturn);
    settle(g);
    CHECK(g.mode() == core::AttractFlow::Mode::Game && g.players() == 2);
    g.returnFromGame();
    CHECK(g.mode() == core::AttractFlow::Mode::Attract && g.turn().now() == 0 && g.turnIndex() == 1);
    // the random counter of the monitor counts while a key is awaited: 1 per 15 cycles (INC $4E, BNE, BIT, BPL)
    core::AttractFlow w;
    CHECK(w.start(pack, flowtest::romPath()));
    w.advance(200'000);
    w.keyDown(core::kKeySpace);
    w.advance(200'000);
    settle(w);
    const int before = w.monitor().peek(0x4E) | (w.monitor().peek(0x4F) << 8);
    w.advance(15'000);
    const int after = w.monitor().peek(0x4E) | (w.monitor().peek(0x4F) << 8);
    CHECK(after - before >= 990 && after - before <= 1010);
    // the digit of the prompt stays in `$51`: after Space (back to the demo) the next turn shows that many players (TASK-033-6c); a game then leaves its memory to the demo
    core::AttractFlow s;
    CHECK(s.start(pack, flowtest::romPath()));
    CHECK(s.memory().players == 1);
    s.advance(200'000);
    s.keyDown(core::kKeySpace);
    s.advance(200'000);
    settle(s);
    s.keyDown(0xB3);
    settle(s);
    CHECK(s.memory().players == 3);
    s.keyDown(core::kKeySpace);
    settle(s);
    CHECK(s.mode() == core::AttractFlow::Mode::Attract && s.memory().players == 3 && s.turnIndex() == 1);
    core::AttractMemory left;
    left.players = 2;
    left.current = 2;
    left.live[0] = 0x40;
    left.live[1] = 0x03;
    left.saves[1][0] = 0x20;
    left.high = {0x40, 0x03, 0x00};
    left.e5 = 6;
    s.returnFromGame(left);
    CHECK(s.memory().players == 2 && s.memory().high[0] == 0x40 && s.memory().e5 == 6 && s.turnIndex() == 2);
}
