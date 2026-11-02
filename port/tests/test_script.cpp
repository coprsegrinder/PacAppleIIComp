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
