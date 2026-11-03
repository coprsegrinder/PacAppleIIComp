#include "sound.h"

namespace pacman::core {

namespace {

struct Entry {
    SoundId id;
    SoundEngine engine;
    std::uint16_t address;
    std::size_t maxBytes;   // the longest the list may be, including its terminator (the manifest range)
};

// Addresses from docs/SOUND.md. The death steps are listed in the order of the pointer table at $7C8F (index 1..6).
constexpr Entry kEntries[] = {
    {SoundId::DotEaten, SoundEngine::A, 0x7BE3, 3},
    {SoundId::GhostEaten, SoundEngine::A, 0x7BED, 88},
    {SoundId::Energizer, SoundEngine::A, 0x7C4C, 5},
    {SoundId::Fruit, SoundEngine::A, 0x7C68, 26},
    {SoundId::Death1, SoundEngine::A, 0x7CE4, 12},
    {SoundId::Death2, SoundEngine::A, 0x7CD7, 13},
    {SoundId::Death3, SoundEngine::A, 0x7CCA, 13},
    {SoundId::Death4, SoundEngine::A, 0x7CBD, 13},
    {SoundId::Death5, SoundEngine::A, 0x7CB0, 13},
    {SoundId::Death6, SoundEngine::A, 0x7C9D, 19},
    {SoundId::Jingle, SoundEngine::B, 0x7D3F, 60},
};

}  // namespace

bool SoundData::load(const datapack::Pack& pack) {
    for (const Entry& e : kEntries) {
        const auto data = pack.find(e.address, e.maxBytes);
        if (data.empty()) return false;
        SoundList& l = lists_[static_cast<std::size_t>(e.id)];
        l.engine = e.engine;
        l.address = e.address;
        l.bytes.clear();
        if (e.engine == SoundEngine::A) {
            std::size_t i = 0;
            while (i < data.size() && data[i] != 0) l.bytes.push_back(data[i++]);
            if (i >= data.size()) return false;           // no terminator inside the range
        } else {
            std::size_t i = 0;
            while (i + 1 < data.size() && data[i] != 0) {
                l.bytes.push_back(data[i]);
                l.bytes.push_back(data[i + 1]);
                i += 2;
            }
            if (i >= data.size() || data[i] != 0) return false;
        }
    }
    lists_[static_cast<std::size_t>(SoundId::BonusLife)] = SoundList{SoundEngine::C, 0, {}};
    // the pointer table $7C8F: word k (k = 1..6) must point at the list of death step k
    const auto table = pack.find(0x7C8F, 14);
    if (table.empty()) return false;
    for (std::size_t k = 1; k <= 6; ++k) {
        const std::uint16_t w = static_cast<std::uint16_t>(table[2 * k] | table[2 * k + 1] << 8);
        if (w != list(static_cast<SoundId>(static_cast<std::size_t>(SoundId::Death1) + k - 1)).address) return false;
    }
    return true;
}

std::vector<std::uint8_t> SoundData::allBytes() const {
    std::vector<std::uint8_t> out;
    for (const SoundList& l : lists_) out.insert(out.end(), l.bytes.begin(), l.bytes.end());
    return out;
}

}  // namespace pacman::core
