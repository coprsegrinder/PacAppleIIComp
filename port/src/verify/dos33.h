// Read-only helpers for 140K Apple II DOS 3.3 disk images in DOS sector order (.dsk / .do).
// Never writes. Every access is bounds-checked: a damaged image yields an empty result, never a crash.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pacman::dos33 {

inline constexpr std::size_t kTracks = 35;
inline constexpr std::size_t kSectorsPerTrack = 16;
inline constexpr std::size_t kSectorSize = 256;
inline constexpr std::size_t kImageSize = kTracks * kSectorsPerTrack * kSectorSize;  // 143,360 bytes

struct CatalogEntry {
    std::string name;            // high bits stripped, trailing spaces removed
    std::uint8_t type = 0;       // file type without the "locked" bit (4 = binary)
    std::uint8_t listTrack = 0;  // first track/sector list sector of the file
    std::uint8_t listSector = 0;
};

// One 256-byte sector, or an empty span when track/sector are outside the disk or the image is too small.
std::span<const std::uint8_t> sector(std::span<const std::uint8_t> image, std::size_t track, std::size_t sectorNo);

// True when the volume table of contents (track 17, sector 0) has the shape of a 35-track, 16-sector DOS 3.3 disk.
bool looksLikeDos33(std::span<const std::uint8_t> image);

// All active catalog entries (deleted and unused slots skipped), or nullopt when the catalog chain is damaged.
std::optional<std::vector<CatalogEntry>> readCatalog(std::span<const std::uint8_t> image);

// The full sector contents of a file (whole sectors, as stored), or nullopt when its allocation lists are damaged.
std::optional<std::vector<std::uint8_t>> readFile(std::span<const std::uint8_t> image, const CatalogEntry& entry);

// The track/sector pairs holding a file's data, in file order, or nullopt when damaged.
std::optional<std::vector<std::pair<std::uint8_t, std::uint8_t>>> dataSectors(std::span<const std::uint8_t> image,
                                                                               const CatalogEntry& entry);

}  // namespace pacman::dos33
