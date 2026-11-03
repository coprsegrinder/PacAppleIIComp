// TASK-032-1 (M7): the note lists of the original's sounds, read from the verified data pack (docs/SOUND.md section 3).
//   Engine A lists ($7BBC): one period byte P per note, ended by 0. Engine B list ($7D0D, the intro jingle): pairs (P, n), ended by P = 0.
//   Engine C ($7C51, the bonus-life sound) has no list: 80 plain tones of period $20.
// The list ADDRESS is kept because the original's timing depends on it: a list read that crosses a page boundary costs one cycle more (docs/SOUND.md section 2).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../datapack/datapack.h"

namespace pacman::core {

enum class SoundEngine : std::uint8_t { A, B, C };

enum class SoundId : std::uint8_t {
    DotEaten,      // $7BDC, list $7BE3
    GhostEaten,    // $7BE6, list $7BED
    Energizer,     // $7C45, list $7C4C
    Fruit,         // $7C61, list $7C68 (fruit eaten)
    Death1,        // the six steps of the death animation: pointer table $7C8F, index $0A = 1..6 (lists $7CE4 ... $7C9D)
    Death2,
    Death3,
    Death4,
    Death5,
    Death6,        // also the intermission sound ($7CF0)
    BonusLife,     // $7C51, engine C
    Jingle,        // $7D38, engine B, list $7D3F
    Count
};

struct SoundList {
    SoundEngine engine = SoundEngine::A;
    std::uint16_t address = 0;           // where the list lives in the original (0 for engine C)
    std::vector<std::uint8_t> bytes;     // without the terminator: engine A = periods; engine B = P, n, P, n ...
    std::size_t notes() const noexcept { return engine == SoundEngine::B ? bytes.size() / 2 : bytes.size(); }
};

class SoundData {
public:
    // False if the pack lacks a list or a list has no terminator inside its range, or the death pointer table disagrees with the lists.
    bool load(const datapack::Pack& pack);
    const SoundList& list(SoundId id) const noexcept { return lists_[static_cast<std::size_t>(id)]; }
    // The concatenated list bytes in SoundId order (engine C has none): used by the tests to compare against a hash.
    std::vector<std::uint8_t> allBytes() const;
private:
    std::array<SoundList, static_cast<std::size_t>(SoundId::Count)> lists_{};
};

}  // namespace pacman::core
