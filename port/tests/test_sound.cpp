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

// TASK-032-2: the click times of all 12 sounds, generated from the pack's lists and the engine formulas, equal the original's (measured on the oracle, cycle for cycle).
TEST(sound_click_times_equal_the_originals_for_all_12_sounds) {
    if (testfw::config().image.empty() || testfw::config().clicks.empty()) {
        std::printf("    SKIPPED (needs --image and --clicks)\n");
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
    const auto f = readFile(testfw::config().clicks);
    CHECK(f.size() > 8 && f[0] == 'C' && f[1] == 'L' && f[2] == 'K' && f[3] == '1');
    if (f.size() <= 8) return;
    auto u32 = [&](std::size_t pos) { return static_cast<std::uint32_t>(f[pos] | f[pos + 1] << 8 | f[pos + 2] << 16 | static_cast<std::uint32_t>(f[pos + 3]) << 24); };
    CHECK(u32(4) == 12);
    // file order (tools/sound_model.SOUNDS): dot, ghost, energizer, fruit, death 6..1, bonus life, jingle
    const core::SoundId order[12] = {core::SoundId::DotEaten, core::SoundId::GhostEaten, core::SoundId::Energizer, core::SoundId::Fruit, core::SoundId::Death6, core::SoundId::Death5,
                                     core::SoundId::Death4, core::SoundId::Death3, core::SoundId::Death2, core::SoundId::Death1, core::SoundId::BonusLife, core::SoundId::Jingle};
    std::size_t pos = 8, totalClicks = 0, wrong = 0;
    for (int s = 0; s < 12; ++s) {
        const std::uint32_t n = u32(pos);
        pos += 4;
        const auto mine = core::clickTimes(sd.list(order[s]));
        bool same = mine.size() == n;
        for (std::uint32_t i = 0; i < n && same; ++i) same = mine[i] == u32(pos + 4 * i);
        if (!same) {
            ++wrong;
            std::size_t firstBad = 0;
            while (firstBad < mine.size() && firstBad < n && mine[firstBad] == u32(pos + 4 * firstBad)) ++firstBad;
            std::printf("    sound %d: %zu clicks (oracle %u), first difference at click %zu (mine %u, oracle %u)\n", s, mine.size(), n, firstBad,
                        firstBad < mine.size() ? mine[firstBad] : 0u, firstBad < n ? u32(pos + 4 * firstBad) : 0u);
        }
        totalClicks += n;
        pos += 4 * n;
    }
    std::printf("    12 sounds, %zu clicks compared, %zu sounds differ\n", totalClicks, wrong);
    CHECK(wrong == 0 && totalClicks == 3312);   // 8 + 348 + 16 + 100 + (72 + 4 x 48 + 44) + 320 + 2212, the counts of tools/export_clicks.py
}

// ---- TASK-032-4: the speaker as a square wave (pure renderer; the Windows device is only a smoke test by ear) ----
#include "../src/core/squarewave.h"

TEST(squarewave_edges_sit_at_the_click_times) {
    // clicks at 0, 1000 and 2000 cycles at 44,100 Hz: edges at samples 0, floor(1000 x 44100 / 1022727) = 43 and 86
    const std::vector<std::uint32_t> clicks = {0, 1000, 2000};
    const core::Pcm p = core::renderClicks(clicks, 5000, 44100, 5000);
    CHECK(p.sampleRate == 44100);
    CHECK(p.samples.size() == static_cast<std::size_t>(5000LL * 44100 / core::kCpuHz) + 1);
    CHECK(p.samples[0] == 5000 && p.samples[42] == 5000);
    CHECK(p.samples[43] == -5000 && p.samples[85] == -5000);
    CHECK(p.samples[86] == 5000);
    // the level is held for 1 ms (44 samples) after the last edge, then released to silence
    CHECK(p.samples[86 + 44] == 5000 && p.samples[86 + 45] == 0 && p.samples.back() == 0);
    // no clicks: silence of the requested length; a delayed first click: silence before it
    const core::Pcm quiet = core::renderClicks({}, 1000, 44100);
    CHECK(!quiet.samples.empty() && quiet.samples[0] == 0 && quiet.samples.back() == 0);
    const core::Pcm late = core::renderClicks({10000, 11000}, 20000, 44100, 3000);
    CHECK(late.samples[0] == 0 && late.samples[429] == 0 && late.samples[431] == 3000);   // 10000 x 44100 / 1022727 = 431.2
}

TEST(sound_all_sounds_render_with_the_right_length) {
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
    for (int i = 0; i < static_cast<int>(core::SoundId::Count); ++i) {
        const auto id = static_cast<core::SoundId>(i);
        const auto clicks = core::clickTimes(sd.list(id));
        const std::int64_t freeze = core::soundFreezeCycles(id);
        CHECK(!clicks.empty() && freeze >= static_cast<std::int64_t>(clicks.back()));   // the game stands still at least as long as the clicks last
        const core::Pcm p = core::renderClicks(clicks, freeze, 44100);
        CHECK(p.samples.size() == static_cast<std::size_t>(freeze * 44100 / core::kCpuHz) + 1);
        std::size_t loud = 0;
        for (auto v : p.samples) loud += v != 0;
        CHECK(loud > 10);
    }
    // the jingle lasts 5.04 s of audio
    const core::Pcm jingle = core::renderClicks(core::clickTimes(sd.list(core::SoundId::Jingle)), core::soundFreezeCycles(core::SoundId::Jingle), 44100);
    CHECK(jingle.samples.size() > 5.0 * 44100 && jingle.samples.size() < 5.1 * 44100);
}
