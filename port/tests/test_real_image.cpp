// Checks against the real disk images, given with --image / --reference (kept outside the repository; skipped when absent).
// Expected answers are the ones the Python tool gave on 2026-10-29 (docs/VERIFIER_SPEC.md).
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "../src/datapack/datapack.h"
#include "../src/verify/dos33.h"
#include "../src/verify/sha256.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using verify::Status;

namespace {

std::optional<std::vector<std::uint8_t>> load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(real_image_exact_and_variants) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (no --image given)\n");
        return;
    }
    const auto img = load(testfw::config().image);
    CHECK_MSG(img.has_value(), "cannot read the image");
    if (!img) return;
    CHECK(verify::verify(*img).status == Status::ExactImage);

    auto boot = *img;  // altered DOS boot-track byte (T0/S5): the program is untouched
    boot[(0 * 16 + 5) * 256 + 10] ^= 0xFF;
    CHECK(verify::verify(boot).status == Status::GameProgram);

    auto volume = *img;  // volume number 254 -> 1 in the VTOC
    volume[(17 * 16) * 256 + 6] = 1;
    CHECK(verify::verify(volume).status == Status::GameProgram);

    // flip one byte inside the 4th data sector of PAC-MAN
    const auto catalog = dos33::readCatalog(*img);
    CHECK(catalog.has_value());
    if (!catalog) return;
    for (const auto& e : *catalog) {
        if (e.name != "PAC-MAN") continue;
        const auto sectors = dos33::dataSectors(*img, e);
        CHECK(sectors.has_value() && sectors->size() > 3);
        if (!sectors || sectors->size() <= 3) return;
        auto program = *img;
        const auto [t, s] = (*sectors)[3];
        program[(static_cast<std::size_t>(t) * 16 + s) * 256 + 20] ^= 1;
        const auto r = verify::verify(program);
        CHECK(r.status == Status::NotVerified);
        CHECK(r.reason == "game program bytes differ from the expected program");
    }
    // the original file bytes were only read, never changed
    CHECK(verify::verify(*img).status == Status::ExactImage);
}

TEST(real_reference_disk_is_not_verified) {
    if (testfw::config().reference.empty()) {
        std::printf("    SKIPPED (no --reference given)\n");
        return;
    }
    const auto img = load(testfw::config().reference);
    CHECK_MSG(img.has_value(), "cannot read the reference disk");
    if (!img) return;
    const auto r = verify::verify(*img);
    CHECK(r.status == Status::NotVerified);
    CHECK(r.reason == "no file named PAC-MAN in the catalog");
}

// The independent value was computed with Python (tools/make_manifest.py) on 2026-10-29 from the extracted program and the same
// 60 address ranges: SHA-256 of the data blobs concatenated in address order. It is a hash of derived data, not data.
TEST(real_data_pack_matches_the_independent_extraction) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (no --image given)\n");
        return;
    }
    const auto img = load(testfw::config().image);
    CHECK_MSG(img.has_value(), "cannot read the image");
    if (!img) return;
    const auto built = datapack::buildPack(*img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    CHECK(loaded.pack.blobs().size() == 60);
    std::size_t total = 0;
    std::vector<std::uint8_t> all;
    for (const auto& b : loaded.pack.blobs()) {
        total += b.bytes.size();
        all.insert(all.end(), b.bytes.begin(), b.bytes.end());
    }
    CHECK(total == 16680);
    CHECK(verify::toHexUpper(verify::sha256(all)) == "4754D38A095A3F0CAE1AF0C4924F04B32359BC04D53B7901E57D5C4D12A84F46");

    // a disk with other DOS tracks gives the identical pack; a changed program byte gives no pack at all
    auto boot = *img;
    boot[(0 * 16 + 5) * 256 + 10] ^= 0xFF;
    const auto fromBoot = datapack::buildPack(boot, verify::referenceExpected(), datapack::dataManifest());
    CHECK(fromBoot.ok);
    CHECK(fromBoot.bytes == built.bytes);

    const auto catalog = dos33::readCatalog(*img);
    CHECK(catalog.has_value());
    if (!catalog) return;
    for (const auto& e : *catalog) {
        if (e.name != "PAC-MAN") continue;
        const auto sectors = dos33::dataSectors(*img, e);
        CHECK(sectors.has_value() && sectors->size() > 3);
        if (!sectors || sectors->size() <= 3) return;
        auto program = *img;
        const auto [t, s] = (*sectors)[3];
        program[(static_cast<std::size_t>(t) * 16 + s) * 256 + 20] ^= 1;
        CHECK(!datapack::buildPack(program, verify::referenceExpected(), datapack::dataManifest()).ok);
    }

    // the pack must be refused when it is checked against a different expected program
    auto other = verify::referenceExpected();
    other.bodySha256[0] = other.bodySha256[0] == 'A' ? 'B' : 'A';
    CHECK(!datapack::loadPack(built.bytes, other).ok);
}
