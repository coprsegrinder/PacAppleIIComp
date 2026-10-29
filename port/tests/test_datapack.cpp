// Data pack builder/loader on synthetic disks (no original data).
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "../src/datapack/datapack.h"
#include "../src/verify/sha256.h"
#include "test_framework.h"
#include "test_support.h"

using namespace pacman;
using testsupport::DiskOptions;
using testsupport::expectedFor;
using testsupport::makeDisk;
using testsupport::testBody;

namespace {

const datapack::Range kManifest[] = {{0x4010, 50}, {0x4100, 200}, {0x4200, 8}};

datapack::BuildResult build(const std::vector<std::uint8_t>& body, const DiskOptions& options = {}) {
    return datapack::buildPack(makeDisk(body, options), expectedFor(body), kManifest);
}

// After editing a pack's bytes, recompute the whole-file hash so that only the intended check can reject it.
void reseal(std::vector<std::uint8_t>& pack) {
    const auto h = verify::sha256(std::span<const std::uint8_t>(pack).first(pack.size() - 32));
    std::copy(h.begin(), h.end(), pack.end() - 32);
}

void putU32(std::vector<std::uint8_t>& v, std::size_t at, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) v[at + i] = static_cast<std::uint8_t>(x >> (8 * i));
}

}  // namespace

TEST(pack_roundtrip_returns_exactly_the_manifest_bytes) {
    const auto body = testBody();
    const auto built = build(body);
    CHECK_MSG(built.ok, built.reason);
    const auto loaded = datapack::loadPack(built.bytes, expectedFor(body));
    CHECK_MSG(loaded.ok, loaded.reason);
    CHECK(loaded.pack.blobs().size() == 3);
    for (const auto& r : kManifest) {
        const auto got = loaded.pack.find(r.address, r.length);
        CHECK(got.size() == r.length);
        CHECK(std::equal(got.begin(), got.end(), body.begin() + (r.address - 0x4000)));
    }
    // partial reads inside a blob work; reads leaving a blob, or in code (outside every blob), do not
    CHECK(loaded.pack.find(0x4105, 10).size() == 10);
    CHECK(loaded.pack.find(0x4100, 201).empty());
    CHECK(loaded.pack.find(0x4000, 4).empty());
    CHECK(loaded.pack.find(0x4059, 4).empty());
    CHECK(loaded.pack.programSha256() == expectedFor(body).bodySha256);
}

TEST(pack_does_not_contain_the_code_bytes_or_the_disk) {
    const auto body = testBody();
    const auto built = build(body);
    CHECK(built.ok);
    // size = header + table + data + trailer: nothing else is stored (no DOS tracks, no unlisted program bytes)
    CHECK(built.bytes.size() == 52 + 3 * 44 + (50 + 200 + 8) + 32);
}

TEST(pack_is_identical_for_a_disk_with_other_dos_tracks) {
    const auto body = testBody();
    DiskOptions other;
    other.volume = 1;
    other.dosMarker = 0x99;
    const auto a = build(body);
    const auto b = build(body, other);
    CHECK(a.ok && b.ok);
    CHECK(a.bytes == b.bytes);
}

TEST(pack_build_refuses_an_unverified_image) {
    const auto body = testBody();
    auto other = body;
    other[300] ^= 1;
    const auto r = datapack::buildPack(makeDisk(other), expectedFor(body), kManifest);
    CHECK(!r.ok);
    CHECK(r.reason.find("not verified") != std::string::npos);
    CHECK(r.bytes.empty());
    const auto tiny = datapack::buildPack(std::vector<std::uint8_t>(10, 0), expectedFor(body), kManifest);
    CHECK(!tiny.ok);
}

TEST(pack_build_refuses_an_invalid_manifest) {
    const auto body = testBody();
    const auto disk = makeDisk(body);
    const auto expected = expectedFor(body);
    const datapack::Range overlapping[] = {{0x4010, 50}, {0x4030, 10}};
    const datapack::Range empty[] = {{0x4010, 0}};
    const datapack::Range outside[] = {{0x4000 + 990, 50}};
    const datapack::Range backwards[] = {{0x4100, 4}, {0x4010, 4}};
    CHECK(!datapack::buildPack(disk, expected, overlapping).ok);
    CHECK(!datapack::buildPack(disk, expected, empty).ok);
    CHECK(!datapack::buildPack(disk, expected, outside).ok);
    CHECK(!datapack::buildPack(disk, expected, backwards).ok);
}

TEST(pack_loader_refuses_every_single_byte_change) {
    const auto body = testBody();
    const auto built = build(body);
    CHECK(built.ok);
    const auto expected = expectedFor(body);
    int accepted = 0;
    for (std::size_t i = 0; i < built.bytes.size(); ++i) {
        auto damaged = built.bytes;
        damaged[i] ^= 0x01;
        if (datapack::loadPack(damaged, expected).ok) ++accepted;
    }
    CHECK_MSG(accepted == 0, std::to_string(accepted) + " single-byte changes were accepted");
}

TEST(pack_loader_refuses_every_truncation_and_extension) {
    const auto body = testBody();
    const auto built = build(body);
    const auto expected = expectedFor(body);
    int accepted = 0;
    for (std::size_t n = 0; n < built.bytes.size(); ++n) {
        if (datapack::loadPack(std::span<const std::uint8_t>(built.bytes).first(n), expected).ok) ++accepted;
    }
    auto longer = built.bytes;
    longer.push_back(0);
    if (datapack::loadPack(longer, expected).ok) ++accepted;
    CHECK_MSG(accepted == 0, std::to_string(accepted) + " damaged lengths were accepted");
}

TEST(pack_loader_refuses_a_pack_built_from_another_program) {
    const auto body = testBody();
    const auto built = build(body);
    auto otherBody = body;
    otherBody[0] ^= 0xFF;
    const auto r = datapack::loadPack(built.bytes, expectedFor(otherBody));
    CHECK(!r.ok);
    CHECK(r.reason == "the data pack was not built from the expected game program");
}

TEST(pack_loader_refuses_inconsistent_tables_even_when_all_hashes_are_valid) {
    const auto body = testBody();
    const auto built = build(body);
    const auto expected = expectedFor(body);
    const std::size_t entry0 = 52;
    const std::size_t entry1 = 52 + 44;
    {   // blob below the program origin
        auto p = built.bytes;
        putU32(p, entry0, 0x3000);
        reseal(p);
        CHECK(!datapack::loadPack(p, expected).ok);
    }
    {   // overlapping blobs
        auto p = built.bytes;
        putU32(p, entry1, 0x4020);
        reseal(p);
        CHECK(!datapack::loadPack(p, expected).ok);
    }
    {   // zero length
        auto p = built.bytes;
        putU32(p, entry0 + 4, 0);
        reseal(p);
        CHECK(!datapack::loadPack(p, expected).ok);
    }
    {   // data offset outside the data area
        auto p = built.bytes;
        putU32(p, entry0 + 8, 10);
        reseal(p);
        CHECK(!datapack::loadPack(p, expected).ok);
    }
    {   // absurd blob count
        auto p = built.bytes;
        putU32(p, 48, 0xFFFFFFFF);
        reseal(p);
        CHECK(!datapack::loadPack(p, expected).ok);
    }
}

TEST(pack_find_on_an_empty_pack_is_empty) {
    datapack::Pack pack;
    CHECK(pack.find(0x4000, 1).empty());
    CHECK(pack.blobs().empty());
}
