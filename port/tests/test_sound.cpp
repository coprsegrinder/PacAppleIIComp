// TASK-032-1: the sound note lists come out of the verified data pack unchanged. Needs --image (the pack is built from it and verified).
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/sound.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/sha256.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;

namespace {
std::vector<std::uint8_t> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
}  // namespace

TEST(sound_lists_come_from_the_pack_unchanged) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    const auto built = datapack::buildPack(readFile(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    core::SoundData sd;
    CHECK(sd.load(loaded.pack));
    using core::SoundId;
    // note counts read from the original (tools: see docs/SOUND.md section 3)
    CHECK(sd.list(SoundId::DotEaten).notes() == 2);
    CHECK(sd.list(SoundId::GhostEaten).notes() == 87);
    CHECK(sd.list(SoundId::Energizer).notes() == 4);
    CHECK(sd.list(SoundId::Fruit).notes() == 25);
    CHECK(sd.list(SoundId::Death1).notes() == 11 && sd.list(SoundId::Death2).notes() == 12 && sd.list(SoundId::Death3).notes() == 12);
    CHECK(sd.list(SoundId::Death4).notes() == 12 && sd.list(SoundId::Death5).notes() == 12 && sd.list(SoundId::Death6).notes() == 18);
    CHECK(sd.list(SoundId::Jingle).notes() == 29 && sd.list(SoundId::Jingle).engine == core::SoundEngine::B);
    CHECK(sd.list(SoundId::BonusLife).engine == core::SoundEngine::C && sd.list(SoundId::BonusLife).bytes.empty());
    // all list bytes together: SHA-256 of the concatenation (a hash only; the lists themselves are not stored in the tests)
    const auto digest = verify::sha256(sd.allBytes());
    std::string hex;
    for (auto b : digest) {
        char c[3];
        std::snprintf(c, sizeof c, "%02x", b);
        hex += c;
    }
    CHECK_MSG(hex == "088424d9dc45f022041478870c0569852724b174ddacb0f77b8d58dbda282c61", "sound lists differ from the original: " + hex);
}
