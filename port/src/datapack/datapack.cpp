#include "datapack.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "../verify/sha256.h"

namespace pacman::datapack {
namespace {

// File layout (little endian), see docs/DATAPACK_SPEC.md:
//   0   8   magic "PMPACK01"
//   8   4   format version (1)
//   12  4   manifest version (1)
//   16  32  SHA-256 of the program body the pack was built from
//   48  4   blob count N
//   52  N * 44   blob table: u32 address, u32 length, u32 file offset, 32-byte SHA-256 of the blob bytes
//   ... blob bytes
//   end-32  32  SHA-256 of everything before it
constexpr char kMagic[8] = {'P', 'M', 'P', 'A', 'C', 'K', '0', '1'};
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint32_t kManifestVersion = 1;
constexpr std::size_t kHeaderSize = 52;
constexpr std::size_t kEntrySize = 44;
constexpr std::size_t kHashSize = 32;
constexpr std::uint32_t kMaxBlobs = 4096;

void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

std::uint32_t getU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::array<std::uint8_t, 32> hexToBytes(const std::string& hex) {
    std::array<std::uint8_t, 32> out{};
    auto nibble = [](char c) { return static_cast<std::uint8_t>(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10); };
    for (std::size_t i = 0; i < 32 && i * 2 + 1 < hex.size(); ++i) {
        out[i] = static_cast<std::uint8_t>((nibble(hex[i * 2]) << 4) | nibble(hex[i * 2 + 1]));
    }
    return out;
}

LoadResult refuse(const std::string& why) {
    LoadResult r;
    r.reason = why;
    return r;
}

}  // namespace

std::span<const std::uint8_t> Pack::find(std::uint16_t address, std::size_t length) const {
    for (const auto& blob : blobs_) {
        if (address >= blob.address && static_cast<std::size_t>(address) + length <= static_cast<std::size_t>(blob.address) + blob.bytes.size()) {
            return std::span<const std::uint8_t>(blob.bytes).subspan(address - blob.address, length);
        }
    }
    return {};
}

BuildResult buildPack(std::span<const std::uint8_t> image, const verify::Expected& expected, std::span<const Range> manifest) {
    BuildResult result;
    const auto verdict = verify::verify(image, expected);
    if (verdict.status != verify::Status::ExactImage && verdict.status != verify::Status::GameProgram) {
        result.reason = std::string("the disk image is not verified (") + verify::statusName(verdict.status) + ": " + verdict.reason + ")";
        return result;
    }
    const auto body = verify::programBody(image, expected);
    if (!body) {
        result.reason = "the game program could not be read from the disk image";
        return result;
    }
    // Manifest sanity: inside the program, non-empty, ascending and non-overlapping.
    std::uint32_t previousEnd = kProgramOrigin;
    for (const auto& r : manifest) {
        const std::uint32_t end = static_cast<std::uint32_t>(r.address) + r.length;
        if (r.length == 0 || r.address < previousEnd || end > static_cast<std::uint32_t>(kProgramOrigin) + body->size()) {
            result.reason = "the data manifest is invalid";
            return result;
        }
        previousEnd = end;
    }
    const auto bodyHash = verify::sha256(*body);
    std::vector<std::uint8_t> out(kMagic, kMagic + 8);
    putU32(out, kFormatVersion);
    putU32(out, kManifestVersion);
    out.insert(out.end(), bodyHash.begin(), bodyHash.end());
    putU32(out, static_cast<std::uint32_t>(manifest.size()));
    std::size_t offset = kHeaderSize + manifest.size() * kEntrySize;
    for (const auto& r : manifest) {
        const std::span<const std::uint8_t> bytes(body->data() + (r.address - kProgramOrigin), r.length);
        const auto h = verify::sha256(bytes);
        putU32(out, r.address);
        putU32(out, r.length);
        putU32(out, static_cast<std::uint32_t>(offset));
        out.insert(out.end(), h.begin(), h.end());
        offset += r.length;
    }
    for (const auto& r : manifest) out.insert(out.end(), body->begin() + (r.address - kProgramOrigin), body->begin() + (r.address - kProgramOrigin) + r.length);
    const auto trailer = verify::sha256(out);
    out.insert(out.end(), trailer.begin(), trailer.end());
    result.ok = true;
    result.bytes = std::move(out);
    return result;
}

LoadResult loadPack(std::span<const std::uint8_t> bytes, const verify::Expected& expected) {
    if (bytes.size() < kHeaderSize + kHashSize) return refuse("the data pack is too small");
    if (std::memcmp(bytes.data(), kMagic, 8) != 0) return refuse("not a data pack (wrong signature)");
    if (getU32(bytes.data() + 8) != kFormatVersion) return refuse("the data pack has an unsupported format version");
    if (getU32(bytes.data() + 12) != kManifestVersion) return refuse("the data pack was built with a different manifest version");
    // Whole-file integrity first: everything below can then rely on the content being what the builder wrote.
    const auto trailer = verify::sha256(bytes.first(bytes.size() - kHashSize));
    if (!std::equal(trailer.begin(), trailer.end(), bytes.end() - static_cast<std::ptrdiff_t>(kHashSize))) {
        return refuse("the data pack is damaged (whole-file hash does not match)");
    }
    const auto expectedBody = hexToBytes(expected.bodySha256);
    if (!std::equal(expectedBody.begin(), expectedBody.end(), bytes.begin() + 16)) {
        return refuse("the data pack was not built from the expected game program");
    }
    const std::uint32_t count = getU32(bytes.data() + 48);
    if (count > kMaxBlobs || kHeaderSize + static_cast<std::size_t>(count) * kEntrySize + kHashSize > bytes.size()) {
        return refuse("the data pack table is invalid");
    }
    const std::size_t dataStart = kHeaderSize + static_cast<std::size_t>(count) * kEntrySize;
    const std::size_t dataEnd = bytes.size() - kHashSize;
    LoadResult result;
    std::uint32_t previousEnd = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* e = bytes.data() + kHeaderSize + static_cast<std::size_t>(i) * kEntrySize;
        const std::uint32_t address = getU32(e);
        const std::uint32_t length = getU32(e + 4);
        const std::uint32_t offset = getU32(e + 8);
        if (length == 0 || address < kProgramOrigin || address < previousEnd || static_cast<std::uint64_t>(address) + length > 0x10000 ||
            offset < dataStart || static_cast<std::uint64_t>(offset) + length > dataEnd) {
            return refuse("the data pack table is invalid");
        }
        previousEnd = address + length;
        const std::span<const std::uint8_t> blobBytes = bytes.subspan(offset, length);
        const auto h = verify::sha256(blobBytes);
        if (!std::equal(h.begin(), h.end(), e + 12)) return refuse("the data pack is damaged (a block does not match its hash)");
        Blob blob;
        blob.address = static_cast<std::uint16_t>(address);
        blob.bytes.assign(blobBytes.begin(), blobBytes.end());
        result.pack.blobs_.push_back(std::move(blob));
    }
    result.pack.programSha256_ = expected.bodySha256;
    result.ok = true;
    return result;
}

}  // namespace pacman::datapack
