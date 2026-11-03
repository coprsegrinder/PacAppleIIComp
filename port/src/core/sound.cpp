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

constexpr std::int64_t kGhostEatenFreeze = 179'070;   // measured on scenario D (3 ghosts eaten, one value): the clicks span 178,263

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

std::int64_t soundFreezeCycles(SoundId id) noexcept {
    switch (id) {
        case SoundId::DotEaten: return 2'932;
        case SoundId::GhostEaten: return kGhostEatenFreeze;
        case SoundId::Energizer: return 8'990;
        case SoundId::Fruit: return 89'807;
        case SoundId::Death1: return 5'004;
        case SoundId::Death2: return 30'785;
        case SoundId::Death3: return 27'617;
        case SoundId::Death4: return 21'425;
        case SoundId::Death5: return 18'041;
        case SoundId::Death6: return 17'351;
        case SoundId::BonusLife: return 97'770;
        case SoundId::Jingle: return 5'153'446;
        default: return 0;
    }
}

std::vector<std::uint32_t> clickTimes(const SoundList& list) {
    std::vector<std::uint32_t> out;
    std::uint32_t t = 0;
    if (list.engine == SoundEngine::C) {
        for (std::uint32_t k = 0; k < 80; ++k) {
            if (k != 0) t += 9 * 32 + 37;
            for (int i = 0; i < 4; ++i) {
                if (i != 0) t += 9 * 32 + 11;
                out.push_back(t);
            }
        }
    } else if (list.engine == SoundEngine::A) {
        for (std::uint32_t k = 0; k < list.bytes.size(); ++k) {
            const std::uint32_t p = list.bytes[k];
            if (k != 0) t += 9 * p + 44 + (((list.address & 255u) + k > 255u) ? 1u : 0u);
            for (int i = 0; i < 4; ++i) {
                if (i != 0) t += 9 * p + 11;
                out.push_back(t);
            }
        }
    } else {
        for (std::uint32_t k = 0; 2 * k + 1 < list.bytes.size(); ++k) {
            const std::uint32_t p = list.bytes[2 * k], n = list.bytes[2 * k + 1];
            if (k != 0) t += 31 * p + 30'624;
            for (std::uint32_t i = 0; i < n; ++i) {
                if (i != 0) t += 31 * p + 11;
                out.push_back(t);
            }
        }
    }
    return out;
}

std::vector<std::uint8_t> SoundData::allBytes() const {
    std::vector<std::uint8_t> out;
    for (const SoundList& l : lists_) out.insert(out.end(), l.bytes.begin(), l.bytes.end());
    return out;
}

}  // namespace pacman::core
