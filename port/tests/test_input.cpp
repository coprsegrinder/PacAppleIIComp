// Tests of the input poll (src/core/input): synthetic cases for every branch of $6F5E / $6F77 / $6FD0 as read from the disassembly, and a per-tick
// comparison with the oracle recordings (needs --ticks): at every tick of scenarios A-D the keys of that tick are pushed, the poll runs on the
// recorded state, and the requested direction $63 must equal the oracle's value at the start of the next tick wherever the poll is the only writer.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../src/core/input.h"
#include "test_framework.h"

using namespace pacman::core;

namespace {
GameState game() {
    GameState s;
    s.demo = 0;
    s.requestedDirection() = 9;  // a value no direction uses, so "unchanged" is visible
    return s;
}
GameState demo() {
    GameState s;
    s.demo = 1;
    s.requestedDirection() = 9;
    return s;
}
}  // namespace

TEST(input_direction_keys_and_defaults) {
    KeyConfig k;
    const std::pair<int, int> table[] = {{0xC1, 1}, {0x95, 2}, {0x88, 3}, {0xDA, 0}};
    for (auto [code, dir] : table) {
        InputPoller p;
        GameState s = game();
        p.pushKey(static_cast<std::uint8_t>(code));
        const auto r = p.poll(s, k, {});
        CHECK(r.directionSet && s.requestedDirection() == dir && !r.paused);
    }
    InputPoller p;
    GameState s = game();
    p.pushKey('Q');  // an unknown key: nothing, but it is consumed
    CHECK(!p.poll(s, k, {}).directionSet);
    CHECK(s.requestedDirection() == 9 && p.queued() == 0);
    CHECK(!p.poll(s, k, {}).directionSet);   // no key at all
    p.pushKey(0x41);                         // bit 7 is set automatically, as the original keyboard does
    CHECK(p.poll(s, k, {}).directionSet && s.requestedDirection() == 1);
}

TEST(input_one_key_per_poll_in_order) {
    InputPoller p;
    GameState s = game();
    KeyConfig k;
    p.pushKey(0xC1);
    p.pushKey(0xDA);
    p.poll(s, k, {});
    CHECK(s.requestedDirection() == 1 && p.queued() == 1);
    p.poll(s, k, {});
    CHECK(s.requestedDirection() == 0 && p.queued() == 0);
    for (int i = 0; i < 200; ++i) p.pushKey(0xC1);
    CHECK(p.queued() == 64);  // bounded
}

TEST(input_custom_keys_and_duplicate_order) {
    KeyConfig k;
    k.up = 'I' | 0x80;
    k.down = 'K' | 0x80;
    k.left = 'J' | 0x80;
    k.right = 'L' | 0x80;
    InputPoller p;
    GameState s = game();
    p.pushKey('J');
    p.poll(s, k, {});
    CHECK(s.requestedDirection() == 3);
    p.pushKey(0xC1);  // the old default is now nothing
    s.requestedDirection() = 9;
    p.poll(s, k, {});
    CHECK(s.requestedDirection() == 9);
    KeyConfig dup;      // two directions on one key: the order of the compares in the original wins (up, right, left, down)
    dup.up = dup.down = 0xC1;
    p.pushKey(0xC1);
    p.poll(s, dup, {});
    CHECK(s.requestedDirection() == 1);
}

TEST(input_ctrl_s_toggles_sound_and_does_not_set_a_direction) {
    InputPoller p;
    GameState s = game();
    KeyConfig k;
    CHECK(p.soundOn());
    p.pushKey(0x93);
    const auto r = p.poll(s, k, {});
    CHECK(r.soundToggled && !r.directionSet && !p.soundOn() && s.requestedDirection() == 9);
    p.pushKey(0x93);
    p.poll(s, k, {});
    CHECK(p.soundOn());
}

TEST(input_escape_pauses_until_a_key_and_that_key_still_counts) {
    InputPoller p;
    GameState s = game();
    KeyConfig k;
    p.pushKey(0x9B);
    auto r = p.poll(s, k, {});
    CHECK(r.paused && p.paused());                 // no second key yet: the original is stuck in its wait loop
    r = p.poll(s, k, {});
    CHECK(r.paused && s.requestedDirection() == 9);
    p.pushKey(0xDA);                               // Z ends the pause AND is read as 'down' (A is reloaded in the wait loop at $6F9A)
    r = p.poll(s, k, {});
    CHECK(!r.paused && !p.paused() && r.directionSet && s.requestedDirection() == 0);
    // Esc followed by a key that is already waiting: no pause tick at all
    InputPoller q;
    GameState t = game();
    q.pushKey(0x9B);
    q.pushKey(0x95);
    r = q.poll(t, k, {});
    CHECK(!r.paused && t.requestedDirection() == 2);
    // the key that ends the pause is not checked for Ctrl-S / Esc again: Esc then Ctrl-S changes nothing
    InputPoller u;
    GameState v = game();
    u.pushKey(0x9B);
    u.pushKey(0x93);
    r = u.poll(v, k, {});
    CHECK(!r.paused && !r.soundToggled && u.soundOn() && v.requestedDirection() == 9);
}

TEST(input_demo_ignores_directions_and_space_leaves) {
    InputPoller p;
    GameState s = demo();
    KeyConfig k;
    p.pushKey(0xC1);
    auto r = p.poll(s, k, {});
    CHECK(!r.directionSet && s.requestedDirection() == 9 && !r.leaveToPlayersPrompt);
    p.pushKey(0xA0);
    r = p.poll(s, k, {});
    CHECK(r.leaveToPlayersPrompt);
    // switches are ignored in the demo as well
    PortInput port;
    port.switches = 1;
    CHECK(!p.poll(s, k, port).directionSet);
    // Space in a real game is just an unknown key
    GameState g = game();
    p.pushKey(0xA0);
    r = p.poll(g, k, {});
    CHECK(!r.leaveToPlayersPrompt && !r.directionSet);
}

TEST(input_game_port_switches_and_joystick_mode) {
    InputPoller p;
    GameState s = game();
    KeyConfig k;
    const std::pair<int, int> masks[] = {{1, 0}, {2, 1}, {4, 2}, {8, 3}};
    for (auto [mask, dir] : masks) {
        s.requestedDirection() = 9;
        PortInput port;
        port.switches = static_cast<std::uint8_t>(mask);
        CHECK(p.poll(s, k, port).directionSet && s.requestedDirection() == dir);
    }
    for (int mask : {0, 3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15}) {  // everything else: nothing
        s.requestedDirection() = 9;
        PortInput port;
        port.switches = static_cast<std::uint8_t>(mask);
        CHECK(!p.poll(s, k, port).directionSet && s.requestedDirection() == 9);
    }
    PortInput both;                                  // stage 2 runs after stage 1 and wins
    both.switches = 4;
    p.pushKey(0xC1);
    p.poll(s, k, both);
    CHECK(s.requestedDirection() == 2);
    PortInput joy;                                   // joystick mode: the (not rebuilt) analog result is used instead of the switches
    joy.joystickMode = true;
    joy.switches = 1;
    joy.joystickDirection = static_cast<std::uint8_t>(3);
    p.poll(s, k, joy);
    CHECK(s.requestedDirection() == 3);
    joy.joystickDirection.reset();
    s.requestedDirection() = 9;
    CHECK(!p.poll(s, k, joy).directionSet);
}

TEST(input_against_the_oracle_recordings) {
    if (testfw::config().ticks.empty()) {
        std::printf("    SKIPPED (needs --ticks)\n");
        return;
    }
    std::size_t totalTicks = 0, polledKeys = 0, compared = 0;
    for (std::size_t pos = 0; pos <= testfw::config().ticks.size();) {
        const std::size_t comma = testfw::config().ticks.find(',', pos);
        const std::size_t end = comma == std::string::npos ? testfw::config().ticks.size() : comma;
        const std::string path = testfw::config().ticks.substr(pos, end - pos);
        pos = end + 1;
        std::string why;
        const auto t = TickTrace::load(path, why);
        CHECK_MSG(t.ok(), path + ": " + why);
        if (!t.ok()) continue;
        std::multimap<std::uint32_t, std::uint32_t> keysAt;
        for (const auto& in : t.inputs()) keysAt.emplace(in.tick, in.key);
        InputPoller poller;
        KeyConfig keys;
        std::size_t mismatches = 0, firstBad = 0, otherWriter = 0;
        std::multimap<std::uint32_t, TickPoke> pokesAt;
        for (const auto& p : t.pokes()) pokesAt.emplace(p.tick, p);
        for (std::size_t k = 0; k + 1 < t.ticks(); ++k) {
            if (t.at(k, 0xE8) != 0) continue;  // the golden ticks all have the demo off; kept as a guard
            const auto range = keysAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = range.first; it != range.second; ++it) {
                poller.pushKey(static_cast<std::uint8_t>(it->second));
                ++polledKeys;
            }
            GameState s = fromOracleLayout(t.state(k));
            const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));   // the scenario's pokes of this tick (zero page only) come before the tick runs
            for (auto it = pr.first; it != pr.second; ++it)
                if (it->second.address >= 0x52 && it->second.address <= 0x6F) s.zp(static_cast<std::uint16_t>(it->second.address)) = static_cast<std::uint8_t>(it->second.value);
            const std::uint8_t before = s.requestedDirection();
            poller.poll(s, keys, {});
            // Where the poll changes $63 the oracle must show exactly that value at the next tick. Where the poll leaves $63 alone but the oracle
            // changed it, another routine (Pac-Man's movement, TASK-029d) wrote it: the recordings show that this always sets $63 to the new
            // direction $62 at the same time, which is checked here so that the exemption cannot hide an input bug.
            const std::uint8_t next = t.at(k + 1, 0x63);
            if (s.requestedDirection() != before) {
                ++compared;
                if (s.requestedDirection() != next) {
                    if (mismatches++ == 0) firstBad = k;
                }
            } else if (next != before) {
                ++otherWriter;
                if (next != t.at(k + 1, 0x62)) {
                    if (mismatches++ == 0) firstBad = k;
                }
            } else {
                ++compared;  // nothing changed on either side
            }
        }
        totalTicks += t.ticks();
        std::printf("    %s: %zu ticks where the movement code (not the poll) wrote $63; mismatches %zu%s\n", path.c_str(), otherWriter, mismatches, mismatches ?(" (first at tick " + std::to_string(firstBad) + ")").c_str() : "");
        CHECK(mismatches == 0);
    }
    std::printf("    %zu ticks, %zu keys pushed, %zu decisions compared\n", totalTicks, polledKeys, compared);
    CHECK(compared > 5000);
}
