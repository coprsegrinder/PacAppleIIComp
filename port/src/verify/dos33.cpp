#include "dos33.h"

#include <set>
#include <utility>

namespace pacman::dos33 {

std::span<const std::uint8_t> sector(std::span<const std::uint8_t> image, std::size_t track, std::size_t sectorNo) {
    if (track >= kTracks || sectorNo >= kSectorsPerTrack) return {};
    const std::size_t offset = (track * kSectorsPerTrack + sectorNo) * kSectorSize;
    if (offset + kSectorSize > image.size()) return {};
    return image.subspan(offset, kSectorSize);
}

bool looksLikeDos33(std::span<const std::uint8_t> image) {
    const auto vtoc = sector(image, 17, 0);
    if (vtoc.empty()) return false;
    return vtoc[1] == 17 && (vtoc[3] == 2 || vtoc[3] == 3) && vtoc[0x35] == 16 && vtoc[0x34] == 35;
}

std::optional<std::vector<CatalogEntry>> readCatalog(std::span<const std::uint8_t> image) {
    const auto vtoc = sector(image, 17, 0);
    if (vtoc.empty()) return std::nullopt;
    std::vector<CatalogEntry> entries;
    std::set<std::pair<std::uint8_t, std::uint8_t>> seen;
    std::uint8_t track = vtoc[1];
    std::uint8_t sectorNo = vtoc[2];
    while (track != 0 && !seen.contains({track, sectorNo})) {
        seen.insert({track, sectorNo});
        const auto cat = sector(image, track, sectorNo);
        if (cat.empty()) return std::nullopt;
        for (std::size_t i = 0; i < 7; ++i) {
            const auto e = cat.subspan(11 + i * 35, 35);
            if (e[0] == 0 || e[0] == 0xFF) continue;  // 0 = unused slot, 0xFF = deleted file
            CatalogEntry entry;
            for (std::size_t k = 3; k < 33; ++k) entry.name.push_back(static_cast<char>(e[k] & 0x7F));
            while (!entry.name.empty() && entry.name.back() == ' ') entry.name.pop_back();
            entry.type = e[2] & 0x7F;
            entry.listTrack = e[0];
            entry.listSector = e[1];
            entries.push_back(std::move(entry));
        }
        track = cat[1];
        sectorNo = cat[2];
    }
    return entries;
}

std::optional<std::vector<std::pair<std::uint8_t, std::uint8_t>>> dataSectors(std::span<const std::uint8_t> image,
                                                                               const CatalogEntry& entry) {
    std::vector<std::pair<std::uint8_t, std::uint8_t>> out;
    std::set<std::pair<std::uint8_t, std::uint8_t>> seen;
    std::uint8_t track = entry.listTrack;
    std::uint8_t sectorNo = entry.listSector;
    while (track != 0 && !seen.contains({track, sectorNo})) {
        seen.insert({track, sectorNo});
        const auto list = sector(image, track, sectorNo);
        if (list.empty()) return std::nullopt;
        for (std::size_t k = 12; k < 256; k += 2) {
            if (list[k] != 0 || list[k + 1] != 0) out.emplace_back(list[k], list[k + 1]);
        }
        track = list[1];
        sectorNo = list[2];
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> readFile(std::span<const std::uint8_t> image, const CatalogEntry& entry) {
    const auto sectors = dataSectors(image, entry);
    if (!sectors) return std::nullopt;
    std::vector<std::uint8_t> data;
    data.reserve(sectors->size() * kSectorSize);
    for (const auto& [t, s] : *sectors) {
        const auto sec = sector(image, t, s);
        if (sec.empty()) return std::nullopt;
        data.insert(data.end(), sec.begin(), sec.end());
    }
    return data;
}

}  // namespace pacman::dos33
