// Whole-sequence replay: the complete ordered list of drawing calls of one original cartoon (captured by tools/capture_script.py: parameters only) is
// replayed on a fresh page by the C++ hires layer, and the final page must have the SHA-256 the original produced. Needs --image and
// --script <scratch/cartoon1_script.bin>; skipped otherwise. The script file is local only (it is derived from original data).
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/datapack/datapack.h"
#include "../src/hires/hires.h"
#include "../src/hires/replay.h"
#include "../src/verify/sha256.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;

namespace {
std::vector<std::uint8_t> readAll(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}
}  // namespace

TEST(script_replay_reaches_the_original_final_page) {
    if (testfw::config().image.empty() || testfw::config().script.empty()) {
        std::printf("    SKIPPED (needs --image and --script)\n");
        return;
    }
    const auto built = datapack::buildPack(readAll(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    const auto& pack = loaded.pack;
    std::vector<std::string> paths;
    for (std::size_t pos = 0; pos <= testfw::config().script.size();) {  // --script a.bin,b.bin,...
        const std::size_t comma = testfw::config().script.find(',', pos);
        const std::size_t end = comma == std::string::npos ? testfw::config().script.size() : comma;
        paths.push_back(testfw::config().script.substr(pos, end - pos));
        pos = end + 1;
    }
    for (const auto& path : paths) {
    std::printf("    script %s\n", path.c_str());
    const auto f = readAll(path);
    hires::Page page;
    hires::ReplayStats stats;
    CHECK(hires::replayScript(pack, f, page, stats));
    const auto& used = stats.used;
    const int unusable = stats.unusable, lastUnusable = stats.lastUnusable, lastClear = stats.lastClear;
    std::printf("    %u calls replayed; unusable %d; clear %d, image %d, masked clear %d, big sprite %d\n", stats.calls, unusable, used[10], used[1], used[3], used[8]);
    // a call whose data is not in the pack (e.g. the working buffer at $7E70, used by the attract demo) is harmless only if the page is cleared after it
    CHECK_MSG(lastUnusable < lastClear, "an unusable call comes after the last page clear");
    CHECK(used[1] + used[3] + used[8] + used[9] > 100);  // a real sequence of image draws, clears and sprites, not an empty script
    const auto digest = verify::sha256(page.bytes());
    CHECK(std::memcmp(digest.data(), stats.recordedSha256.data(), 32) == 0);
    }
}

// ---- TASK-031-6: cartoon 1 as game logic (no recorded script drives the picture) ----
#include "../src/core/cartoon.h"

TEST(cartoon_schedule_matches_the_original) {
    // docs/CARTOONS.md section 1: cartoon 1 after 2, 15, 28, 41; cartoon 2 after 5, 18, 31; cartoon 3 after 9, 22, 35; nothing after the others (also 10-13)
    CHECK(pacman::core::cartoonAfterLevel(2) == 1 && pacman::core::cartoonAfterLevel(15) == 1 && pacman::core::cartoonAfterLevel(28) == 1 && pacman::core::cartoonAfterLevel(41) == 1);
    CHECK(pacman::core::cartoonAfterLevel(5) == 2 && pacman::core::cartoonAfterLevel(18) == 2 && pacman::core::cartoonAfterLevel(31) == 2);
    CHECK(pacman::core::cartoonAfterLevel(9) == 3 && pacman::core::cartoonAfterLevel(22) == 3 && pacman::core::cartoonAfterLevel(35) == 3);
    for (unsigned lv : {1u, 3u, 4u, 6u, 7u, 8u, 10u, 11u, 12u, 13u, 14u, 16u, 255u}) CHECK(pacman::core::cartoonAfterLevel(lv) == 0 || lv == 255);
}

TEST(cartoon1_model_draws_the_original_final_page) {
    if (testfw::config().image.empty() || testfw::config().script.empty()) {
        std::printf("    SKIPPED (needs --image and --script)\n");
        return;
    }
    const auto built = datapack::buildPack(readAll(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    const auto script = pacman::core::cartoon1Script(loaded.pack);
    CHECK_MSG(!script.empty(), "frame tables $96E8 / $9E5C missing from the pack");
    if (script.empty()) return;
    hires::Page page;
    hires::ReplayStats stats;
    CHECK(hires::replayScript(loaded.pack, script, page, stats));
    // golden call counts (tests/golden/cartoon_hashes.json, cartoon 1): 1 page clear, 242 masked clears, 239 image draws, 79 big sprites (+ the hi-res switch, not a drawing call)
    CHECK(stats.calls == 1 + 242 + 239 + 79);
    std::printf("    cartoon 1 model: %u calls, unusable %d (last %d), clear %d, image %d, masked clear %d, big sprite %d\n", stats.calls, stats.unusable, stats.lastUnusable, stats.used[10], stats.used[1], stats.used[3], stats.used[8]);
    CHECK(stats.used[10] == 1 && stats.used[3] == 242 && stats.used[1] == 239 && stats.used[8] == 79);
    CHECK(stats.unusable == 0);
    // the page equals the page the original's own cartoon leaves behind (the file is the original's final page, recorded; its SHA-256 is the golden final_page_sha256)
    const auto digest = verify::sha256(page.bytes());
    const std::string golden = "0d846a292b97926ab8495a7e055777e1d1c5ac939ffccbdf49a3d6bb337b90fe";
    std::string hex;
    for (auto b : digest) { char c[3]; std::snprintf(c, sizeof c, "%02x", b); hex += c; }
    CHECK_MSG(hex == golden, "final page of cartoon 1 differs: " + hex);
}

TEST(cartoon1_plan_timing_and_player) {
    if (testfw::config().image.empty() || testfw::config().script.empty()) {
        std::printf("    SKIPPED (needs --image and --script)\n");
        return;
    }
    const auto built = datapack::buildPack(readAll(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    using namespace pacman::core;
    const CartoonPlan plan = cartoon1Plan(loaded.pack);
    CHECK(!plan.script.empty());
    CHECK(plan.passOfCall.size() == 561);
    bool monotone = true;
    for (std::size_t i = 1; i < plan.passOfCall.size(); ++i) monotone = monotone && plan.passOfCall[i] >= plan.passOfCall[i - 1];
    CHECK(monotone);
    CHECK(plan.passes > 800 && plan.cycles == kCartoon1Cycles);
    // nothing but the page clear at the start; everything at the end; in between a steady rise
    CHECK(cartoonCallsDue(plan, 0) >= 1 && cartoonCallsDue(plan, 0) < 5);
    CHECK(cartoonCallsDue(plan, kCartoon1Cycles) == 561);
    CHECK(cartoonCallsDue(plan, -5) == 0);
    std::size_t last = 0;
    bool rising = true;
    for (std::int64_t c = 0; c <= kCartoon1Cycles; c += kCartoon1Cycles / 200) {
        const std::size_t n = cartoonCallsDue(plan, c);
        rising = rising && n >= last;
        last = n;
    }
    CHECK(rising);
    // the player: starts for cartoon 1 only, ends after the duration, and its last picture is the original's final page
    CartoonPlayer player;
    CHECK(!player.start(loaded.pack, 0) && !player.active());
    CHECK(!player.start(loaded.pack, 2) && !player.active());   // cartoons 2 and 3: not rebuilt yet (031-7)
    CHECK(player.start(loaded.pack, 1) && player.active());
    player.advance(kCartoon1Cycles - 1);
    CHECK(player.active());
    hires::Page page;
    player.render(loaded.pack, page);
    player.advance(1);
    CHECK(!player.active());
    // after the end, the last render shows the complete picture
    CartoonPlayer again;
    CHECK(again.start(loaded.pack, 1));
    again.advance(kCartoon1Cycles - 1);
    again.advance(0);
    hires::Page nearEnd;
    again.render(loaded.pack, nearEnd);
    hires::Page whole;
    hires::ReplayStats stats;
    CHECK(hires::replayScript(loaded.pack, plan.script, whole, stats));
    // one cycle before the end only the last call or two may be missing: the pages differ in a few bytes at most
    std::size_t diff = 0;
    for (std::size_t k = 0; k < hires::kPageBytes; ++k) diff += nearEnd.byte(k) != whole.byte(k);
    CHECK_MSG(diff < 200, "page one cycle before the end differs in too many bytes");
}
