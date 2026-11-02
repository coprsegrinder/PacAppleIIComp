// Tests of contact, ghosts eaten, Pac-Man caught and the extra life (src/core/lives): synthetic cases for every branch, and comparisons with the oracle
// recordings (--ticks). Events are found in the recordings (a life lost, a ghost turned into eyes, the extra life) and the C++ effect must give the oracle's
// values at the next tick. Which ghost is touched and when is ghost logic (M5): the contact rule itself is checked as a NECESSARY condition with one step
// (2 units) of slack, because the original tests it inside the ghost's own step.
#include <cstdint>
#include <string>
#include <vector>

#include "../src/core/lives.h"
#include "test_framework.h"

using namespace pacman::core;

namespace {

GameState pacAt(int x, int y) {
    GameState s;
    s.pacX() = static_cast<std::uint8_t>(x);
    s.pacY() = static_cast<std::uint8_t>(y);
    return s;
}

GhostRecord ghostAt(int x, int y) {
    GhostRecord g;
    g.b[0] = static_cast<std::uint8_t>(x);
    g.b[1] = static_cast<std::uint8_t>(y);
    return g;
}

unsigned bcd(std::uint8_t v) { return (v >> 4) * 10u + (v & 15u); }

std::vector<std::string> split(const std::string& list) {
    std::vector<std::string> out;
    for (std::size_t pos = 0; pos <= list.size();) {
        const std::size_t comma = list.find(',', pos);
        const std::size_t end = comma == std::string::npos ? list.size() : comma;
        if (end > pos) out.push_back(list.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

}  // namespace

TEST(lives_contact_distances) {
    const GameState s = pacAt(100, 100);
    CHECK(ghostContact(s, ghostAt(100, 100)) == Contact::Touch);   // same spot
    CHECK(ghostContact(s, ghostAt(100, 95)) == Contact::Touch);    // 5 apart, same column
    CHECK(ghostContact(s, ghostAt(106, 100)) == Contact::Near);    // 6 apart, same row: not a touch
    CHECK(ghostContact(s, ghostAt(100, 109)) == Contact::Near);    // 9 apart
    CHECK(ghostContact(s, ghostAt(100, 110)) == Contact::None);    // 10 apart
    CHECK(ghostContact(s, ghostAt(101, 101)) == Contact::None);    // neither row nor column shared
    CHECK(ghostContact(s, ghostAt(94, 100)) == Contact::Near);     // 6 to the left
    CHECK(ghostContact(s, ghostAt(95, 100)) == Contact::Touch);
    CHECK(ghostContact(pacAt(0, 200), ghostAt(0, 255)) == Contact::None);   // no wrap-around
}

TEST(lives_ghost_eaten_scores_and_doubles) {
    GameState s;
    s.zp(0x68) = 2;
    s.zp(0x53) = 0x00;
    s.zp(0x58) = 0x83;
    s.ghosts[2].b[8] = 1;
    ghostEaten(s, 2);
    CHECK(s.ghosts[2].b[9] == 1 && s.ghosts[2].b[8] == 0);         // eyes
    CHECK(s.score() == 200 && s.zp(0x68) == 4);
    s.ghosts[0].b[8] = 1;
    ghostEaten(s, 0);
    CHECK(s.score() == 600 && s.zp(0x68) == 8);
    ghostEaten(s, 1);
    CHECK(s.score() == 1400 && s.zp(0x68) == 0x16);                // 8 doubles to 16 (BCD)
    ghostEaten(s, 3);
    CHECK(s.score() == 3000 && s.zp(0x68) == 0x32);                // 1,600 points; the step is now 32 (BCD)
    GameState carry;
    carry.zp(0x53) = 0x90;
    carry.zp(0x54) = 0x00;
    carry.zp(0x68) = 2;
    ghostEaten(carry, 0);
    CHECK(carry.zp(0x53) == 0x92 && carry.zp(0x54) == 0x00);       // 9,000 + 200 hundreds-wise: 0x90 + 2 = 0x92 (BCD hundreds)
    carry.zp(0x53) = 0x99;
    carry.zp(0x68) = 2;
    ghostEaten(carry, 0);
    CHECK(carry.zp(0x53) == 0x01 && carry.zp(0x54) == 0x01);       // carry into the ten-thousands byte
    GameState demo;
    demo.demo = 1;
    demo.zp(0x68) = 2;
    demo.ghosts[1].b[8] = 1;
    ghostEaten(demo, 1);
    CHECK(demo.ghosts[1].b[9] == 1 && demo.score() == 0 && demo.zp(0x68) == 2);   // the demo changes the ghost only
}

TEST(lives_caught_and_extra_life) {
    GameState s;
    s.zp(0x58) = 0x83;
    pacmanCaught(s);
    CHECK(s.zp(0x58) == 0x82);
    pacmanCaught(s);
    CHECK(s.zp(0x58) == 0x81);
    pacmanCaught(s);
    CHECK(s.zp(0x58) == 0);                                        // 0x80 has no lives in its low 7 bits: the byte becomes 0
    s.zp(0x58) = 0x03;                                             // flag already used
    pacmanCaught(s);
    CHECK(s.zp(0x58) == 0x02);
    s.zp(0x58) = 0x01;
    pacmanCaught(s);
    CHECK(s.zp(0x58) == 0);
    GameState b;
    b.zp(0x58) = 0x83;
    b.zp(0x54) = 0x00;
    awardBonusLife(b);
    CHECK(b.zp(0x58) == 0x83);                                     // below 10,000
    b.zp(0x54) = 0x01;
    awardBonusLife(b);
    CHECK(b.zp(0x58) == 0x04);                                     // 3 + 1 lives, flag gone
    awardBonusLife(b);
    CHECK(b.zp(0x58) == 0x04);                                     // only once
    GameState p0;
    p0.zp(0x58) = 0x83;
    p0.zp(0x54) = 0x05;
    awardBonusLife(p0, 0);
    CHECK(p0.zp(0x58) == 0x83);                                    // player number 0: no bonus
}

TEST(lives_against_the_oracle_recordings) {
    if (testfw::config().ticks.empty()) {
        std::printf("    SKIPPED (needs --ticks)\n");
        return;
    }
    std::size_t caught = 0, eaten = 0, bonus = 0, badCaught = 0, badEaten = 0, badBonus = 0, noContact = 0;
    for (const auto& path : split(testfw::config().ticks)) {
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
            if (tr.at(k, 0xE8) != 0) continue;
            GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            // ---- Pac-Man caught: low 7 bits of $58 go down by one ----
            if ((b.lives() & 0x7F) < (a.lives() & 0x7F) && !(b.lives() == 0 && a.lives() == 0)) {
                ++caught;
                GameState c = a;
                pacmanCaught(c);
                if (c.lives() != b.lives()) ++badCaught;
                bool near = false;   // some ghost near Pac-Man at the start of the tick (weak necessary condition: both sides step before the test)
                for (const auto& g : a.ghosts) {
                    const unsigned dx = a.pacX() > g.x() ? a.pacX() - g.x() : g.x() - a.pacX();
                    const unsigned dy = a.pacY() > g.y() ? a.pacY() - g.y() : g.y() - a.pacY();
                    // both Pac-Man and the ghost step 2 units before the test, so the start positions may be off by up to 4 in one coordinate
                    const unsigned lo = dx < dy ? dx : dy, hi = dx < dy ? dy : dx;
                    near = near || (lo <= 4 && hi <= 12);
                }
                if (!near) ++noContact;
            }
            // ---- a ghost turns into eyes: touched while frightened ----
            for (std::size_t g = 0; g < 4; ++g) {
                if (a.ghosts[g].b[9] == 0 && b.ghosts[g].b[9] == 1 && a.ghosts[g].b[8] != 0 && b.ghosts[g].b[8] == 0 && b.demo == 0) {
                    ++eaten;
                    GameState c = a;
                    const unsigned before = c.score();
                    ghostEaten(c, g);
                    const unsigned gain = c.score() - before;
                    const unsigned oracleGain = b.score() - before;
                    // the same tick may also eat a dot (10) or an energizer (50, which also resets the step $68 to 2)
                    const bool scoreOk = oracleGain == gain || oracleGain == gain + 10 || oracleGain == gain + 50;
                    const bool stepOk = b.zp(0x68) == c.zp(0x68) || (oracleGain == gain + 50 && b.zp(0x68) == 2);
                    if (!scoreOk || !stepOk || b.ghosts[g].b[9] != c.ghosts[g].b[9]) ++badEaten;
                }
            }
            // ---- the extra life: the score reached 10,000 while the flag in bit 7 of $58 was set ----
            if ((a.lives() & 0x80) != 0 && (b.lives() & 0x80) == 0 && (b.lives() & 0x7F) == (a.lives() & 0x7F) + 1) {
                ++bonus;
                GameState c = b;       // the score after this tick's scoring, the lives before
                c.zp(0x58) = a.lives();
                awardBonusLife(c);
                if (c.lives() != b.lives()) ++badBonus;
            }
        }
    }
    std::printf("    lives lost: %zu (wrong %zu, none within reach %zu); ghosts eaten: %zu (wrong %zu); extra lives: %zu (wrong %zu)\n", caught, badCaught, noContact, eaten, badEaten, bonus, badBonus);
    CHECK(badCaught == 0 && badEaten == 0 && badBonus == 0 && noContact == 0);
    CHECK(caught >= 8 && eaten >= 3);
}
