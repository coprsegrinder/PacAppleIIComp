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
    const auto f = readAll(testfw::config().script);
    CHECK_MSG(f.size() >= 40 && std::memcmp(f.data(), "CSCR", 4) == 0, "bad script file");
    if (f.size() < 40) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    const std::size_t rec = 1 + 8 * 4;
    CHECK(f.size() == 40 + count * rec);

    hires::Page page;
    std::array<int, 11> used{};
    int unusable = 0, lastUnusable = -1, lastClear = -1;
    for (std::uint32_t i = 0; i < count && 40 + (i + 1) * rec <= f.size(); ++i) {
        const std::uint8_t* r = f.data() + 40 + i * rec;
        const int type = r[0];
        std::array<std::int32_t, 8> p{};
        std::memcpy(p.data(), r + 1, 32);
        const auto addr = [&](std::int32_t a, std::size_t n) { return pack.find(static_cast<std::uint16_t>(a), n); };
        bool ran = true;
        switch (type) {
            case 10: page.clear(); break;
            case 1: {
                const auto d = addr(p[0], hires::kSevenCopies3x10);
                if (d.empty()) { ran = false; break; }
                hires::drawImage(page, p[1], p[2], d, p[3] != 0 && p[4] != 0);
                break;
            }
            case 2: {
                const auto d = addr(p[0], static_cast<std::size_t>(p[3]) * static_cast<std::size_t>(p[4]));
                if (d.empty()) { ran = false; break; }
                hires::blitOr(page, p[1], p[2], p[3], p[4], d);
                break;
            }
            case 3: {
                const auto m = addr(p[0], hires::kMaskTableBytes);
                if (m.empty()) { ran = false; break; }
                hires::maskedClear(page, p[1], p[2], m);
                break;
            }
            case 4: {
                const auto t = addr(0x6992, 12);
                if (t.empty()) { ran = false; break; }
                hires::boxClear(page, t[static_cast<std::size_t>(p[0]) * 2], t[static_cast<std::size_t>(p[0]) * 2 + 1]);
                break;
            }
            case 5: {
                const auto t = addr(0x61A8, hires::kDotTableBytes);
                if (t.empty()) { ran = false; break; }
                hires::smallDot(page, p[0], p[1], t);
                break;
            }
            case 6: {
                const auto t = addr(p[0], hires::kTileTableBytes);
                if (t.empty()) { ran = false; break; }
                hires::tile(page, p[1], p[2], t);
                break;
            }
            case 7: {
                const auto d = addr(p[0], 36);
                if (d.empty()) { ran = false; break; }
                hires::blitMergePalette(page, p[1], p[2], 3, 12, d);
                break;
            }
            case 8: {
                const auto d = addr(p[0], hires::kBigSpriteBytes);
                if (d.empty()) { ran = false; break; }
                hires::bigSprite(page, p[1], p[2], d);
                break;
            }
            case 9: {
                const auto d = addr(p[0], hires::kImage3x10);
                if (d.empty()) { ran = false; break; }
                hires::shiftedImage(page, p[1], p[2], d, p[3] != 0 && p[4] != 0);
                break;
            }
            default: ran = false;
        }
        if (ran) ++used[static_cast<std::size_t>(type)];
        else {
            ++unusable;
            lastUnusable = static_cast<int>(i);
        }
        if (type == 10) lastClear = static_cast<int>(i);
    }
    std::printf("    %u calls replayed; unusable %d; clear %d, image %d, masked clear %d, big sprite %d\n", count, unusable, used[10], used[1], used[3], used[8]);
    // a call whose data is not in the pack (e.g. the working buffer at $7E70, used by the attract demo) is harmless only if the page is cleared after it
    CHECK_MSG(lastUnusable < lastClear, "an unusable call comes after the last page clear");
    CHECK(used[3] > 100 && used[1] > 100 && used[8] > 10);
    const auto digest = verify::sha256(page.bytes());
    CHECK(std::memcmp(digest.data(), f.data() + 8, 32) == 0);
}
