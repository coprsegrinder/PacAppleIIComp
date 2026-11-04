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

double rms(const core::Pcm& p, double fromMs, double toMs);

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
        // TASK-033-S: through the speaker model the sound keeps its place in time, is never clipped, and rings out after its last click
        const core::Pcm sp = core::renderSpeaker(clicks, freeze, 44100);
        CHECK(sp.nominal == p.samples.size() && sp.samples.size() > sp.nominal);
        std::int32_t peak = 0;
        for (auto v : sp.samples) peak = std::max<std::int32_t>(peak, std::abs(static_cast<std::int32_t>(v)));
        std::printf("    sound %d: peak %d, ring after the end %.0f rms (10-40 ms)\n", i, peak, rms(sp, static_cast<double>(sp.nominal) * 1000.0 / 44100 + 10.0, static_cast<double>(sp.nominal) * 1000.0 / 44100 + 40.0));
        CHECK(peak < 32767);
    }
    // the jingle lasts 5.04 s of audio
    const core::Pcm jingle = core::renderClicks(core::clickTimes(sd.list(core::SoundId::Jingle)), core::soundFreezeCycles(core::SoundId::Jingle), 44100);
    CHECK(jingle.samples.size() > 5.0 * 44100 && jingle.samples.size() < 5.1 * 44100);
}

// ---- TASK-033-S: the speaker model (a short burst becomes a low thump at the cone's resonance) ----
#include <cmath>

// 8 clicks 307 cycles apart: about the size of the dot-eaten sound (8 clicks in 2.4 ms)
std::vector<std::uint32_t> dotLikeClicks() {
    std::vector<std::uint32_t> c;
    for (std::uint32_t i = 0; i < 8; ++i) c.push_back(100 + i * 307);
    return c;
}
double rms(const core::Pcm& p, double fromMs, double toMs) {
    const std::size_t a = static_cast<std::size_t>(fromMs * p.sampleRate / 1000), b = std::min(p.samples.size(), static_cast<std::size_t>(toMs * p.sampleRate / 1000));
    double s = 0;
    for (std::size_t i = a; i < b; ++i) s += static_cast<double>(p.samples[i]) * p.samples[i];
    return b > a ? std::sqrt(s / static_cast<double>(b - a)) : 0.0;
}

TEST(speaker_keeps_the_nominal_length_and_adds_a_ring_out) {
    const auto clicks = dotLikeClicks();
    const core::Pcm raw = core::renderClicks(clicks, 2414, 44100);
    const core::Pcm sp = core::renderSpeaker(clicks, 2414, 44100);
    CHECK(sp.nominal == raw.samples.size());                                       // the sound occupies the same time as before
    CHECK(sp.samples.size() == sp.nominal + 80u * 44100u / 1000u);                 // plus the 80 ms ring-out
    const core::Pcm none = core::renderSpeaker({}, 1000, 44100);
    CHECK(none.samples.size() == none.nominal && none.samples[0] == 0);            // no clicks: silence, no tail
    for (auto v : sp.samples) CHECK(v > -32767 && v < 32767);                      // never clipped
}

TEST(speaker_burst_rings_at_the_resonance_and_decays) {
    const core::SpeakerModel m;                                                     // 120 Hz, Q 2.5
    const core::Pcm sp = core::renderSpeaker(dotLikeClicks(), 2414, 44100, 5000, m);
    const core::Pcm raw = core::renderClicks(dotLikeClicks(), 2414, 44100, 5000);
    // the raw wave is silent 5 ms after the burst; the speaker still rings
    CHECK(rms(raw, 8.0, 40.0) == 0.0);
    const double ring1 = rms(sp, 10.0, 18.3), ring2 = rms(sp, 25.0, 33.3);          // one period of 120 Hz each, 15 ms apart
    CHECK(ring1 > 100.0);
    const double expected = std::exp(-15.0e-3 * 3.14159265358979 * m.bodyHz / m.bodyQ);   // tau = Q / (pi f0)
    std::printf("    ring rms %.0f -> %.0f, ratio %.3f, expected %.3f\n", ring1, ring2, ring2 / ring1, expected);
    CHECK(ring2 / ring1 > expected * 0.5 && ring2 / ring1 < expected * 2.0);
    // the ring oscillates at about 120 Hz: count the zero crossings between 6 and 60 ms
    int crossings = 0;
    const std::size_t a = static_cast<std::size_t>(0.006 * 44100), b = static_cast<std::size_t>(0.060 * 44100);
    for (std::size_t i = a + 1; i < b; ++i) crossings += (sp.samples[i - 1] < 0) != (sp.samples[i] < 0);
    const double hz = crossings / 2.0 / (0.060 - 0.006);
    std::printf("    ring frequency %.0f Hz (%d crossings)\n", hz, crossings);
    CHECK(hz > 100.0 && hz < 140.0);
}

TEST(speaker_passes_a_long_tone_nearly_unchanged) {
    std::vector<std::uint32_t> tone;   // 1 kHz square wave for 40 ms (500 us per half period)
    for (std::uint32_t i = 0; i < 80; ++i) tone.push_back(i * 511);
    const core::Pcm raw = core::renderClicks(tone, 41'000, 44100, 5000);
    const core::Pcm sp = core::renderSpeaker(tone, 41'000, 44100, 5000);
    const double r = rms(sp, 10.0, 35.0) / rms(raw, 10.0, 35.0);
    std::printf("    1 kHz tone: speaker/raw level %.2f\n", r);
    CHECK(r > 0.6 && r < 1.2);
}
