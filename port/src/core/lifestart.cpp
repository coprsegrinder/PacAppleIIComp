#include "lifestart.h"

#include <algorithm>

namespace pacman::core {

bool loadStartTables(const datapack::Pack& pack, StartTables& out) {
    const auto all = pack.find(0x5446, 142);   // $5446-$54D3: Pac-Man's start, the ghost template, the three house tables
    if (all.empty()) return false;
    std::copy_n(all.begin(), 14, out.pacman.begin());
    std::copy_n(all.begin() + (0x5454 - 0x5446), 80, out.ghosts.begin());
    std::copy_n(all.begin() + (0x54A4 - 0x5446), 16, out.house2.begin());
    std::copy_n(all.begin() + (0x54B4 - 0x5446), 16, out.house3.begin());
    std::copy_n(all.begin() + (0x54C4 - 0x5446), 16, out.house4.begin());
    return true;
}

PlayerSave savePlayer(const GameState& s) noexcept {
    PlayerSave p;
    for (std::size_t i = 0; i < 8; ++i) p.bytes[i] = s.zeroPage[i];   // $52-$59
    return p;
}

void loadPlayer(GameState& s, const PlayerSave& p) noexcept {
    for (std::size_t i = 0; i < 8; ++i) s.zeroPage[i] = p.bytes[i];
}

void startOfLife(GameState& s, const StartTables& t, const PlayerSave& save) noexcept {
    for (std::size_t i = 0; i < 14; ++i) s.zeroPage[(0x60 - 0x52) + i] = t.pacman[i];   // $60-$6D
    for (std::size_t g = 0; g < 4; ++g)
        for (std::size_t i = 0; i < 20; ++i) s.ghosts[g].b[i] = t.ghosts[g * 20 + i];  // $84-$D3
    for (std::size_t a = 0x59; a <= 0x5F; ++a) s.zp(static_cast<std::uint16_t>(a)) = 0;   // $5402 loop
    loadPlayer(s, save);                                                                // $540B: JSR $53CF
    s.cornerPhase = 1;                                                                  // $E9 := 1
    const std::size_t idx = std::min<std::size_t>(s.level(), 15);                       // $6D1E (after the player was loaded)
    s.ghosts[1].b[18] = t.house2[idx];                                                  // $98 + $12
    s.ghosts[2].b[18] = t.house3[idx];                                                  // $AC + $12
    s.ghosts[3].b[18] = t.house4[idx];                                                  // $C0 + $12
}

// The start of a one-player game ($54D5 and $531E, then $53E6): the pristine map from the pack, the player's bytes from the new-game routine, the rest
// from startOfLife() and the pack tables. Only $78/$79 (leftovers of the ghost set-up in the working slot) remain literals taken from the oracle.
bool newGameState(const datapack::Pack& pack, GameState& out) {
    StartTables tables;
    const auto map = pack.find(0x65BA, 896);
    if (map.empty() || !loadStartTables(pack, tables)) return false;
    GameState s;
    std::copy(map.begin(), map.end(), s.map.begin());
    PlayerSave save;
    save.bytes = {0x00, 0x00, 0x00, 0x01, 0xF8, 0x40, 0x83, 0x01};   // score 0, level 1, items $F8, fruit counter $40, lives $83, step counter 1
    s.demo = 0;                                                     // $54DC
    startOfLife(s, tables, save);
    s.mode78 = 1;                                                   // oracle literals (see above)
    s.mode79 = 1;
    out = s;
    return true;
}

}  // namespace pacman::core
