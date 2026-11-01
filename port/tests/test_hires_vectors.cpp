// Routine-level tests of the hi-res layer against REAL drawing calls of the original program (captured on the oracle by tools/capture_hires_vectors.py).
// Needs --image (to build the data pack the images come from) and --vectors <scratch/hires_vectors.bin>; skipped otherwise.
// The vector file holds pages drawn from original data: it stays local and is never committed.
// Each vector = the page before a call + its parameters; the C++ routine must produce exactly the page after the call (all 8,192 bytes).
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "../src/datapack/datapack.h"
#include "../src/hires/hires.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using hires::Page;

namespace {

struct Vector {
    int type = 0;
    std::array<std::int32_t, 8> p{};
    std::vector<std::uint8_t> before, after;
};

std::optional<std::vector<std::uint8_t>> loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::vector<Vector> parse(const std::vector<std::uint8_t>& f) {
    std::vector<Vector> out;
    if (f.size() < 8 || std::memcmp(f.data(), "HIRV", 4) != 0) return out;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    std::size_t pos = 8;
    const std::size_t rec = 1 + 8 * 4 + 2 * hires::kPageBytes;
    for (std::uint32_t i = 0; i < count && pos + rec <= f.size(); ++i) {
        Vector v;
        v.type = f[pos];
        std::memcpy(v.p.data(), f.data() + pos + 1, 32);
        v.before.assign(f.begin() + static_cast<std::ptrdiff_t>(pos + 33), f.begin() + static_cast<std::ptrdiff_t>(pos + 33 + hires::kPageBytes));
        v.after.assign(f.begin() + static_cast<std::ptrdiff_t>(pos + 33 + hires::kPageBytes), f.begin() + static_cast<std::ptrdiff_t>(pos + rec));
        out.push_back(std::move(v));
        pos += rec;
    }
    return out;
}

Page pageFrom(const std::vector<std::uint8_t>& bytes) {
    Page p;
    for (std::size_t i = 0; i < bytes.size(); ++i) p.set(i, bytes[i]);
    return p;
}

bool same(const Page& p, const std::vector<std::uint8_t>& bytes) {
    return std::memcmp(p.bytes().data(), bytes.data(), hires::kPageBytes) == 0;
}

}  // namespace

TEST(hires_real_routine_vectors) {
    if (testfw::config().image.empty() || testfw::config().vectors.empty()) {
        std::printf("    SKIPPED (needs --image and --vectors)\n");
        return;
    }
    const auto img = loadFile(testfw::config().image);
    const auto file = loadFile(testfw::config().vectors);
    CHECK_MSG(img.has_value() && file.has_value(), "cannot read the image or the vector file");
    if (!img || !file) return;
    const auto built = datapack::buildPack(*img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    const auto& pack = loaded.pack;
    const auto vectors = parse(*file);
    CHECK(vectors.size() > 1000);

    std::array<int, 10> tested{}, skipped{}, failed{};
    for (const auto& v : vectors) {
        Page page = pageFrom(v.before);
        const auto addr = [&](std::int32_t a, std::size_t n) { return pack.find(static_cast<std::uint16_t>(a), n); };
        bool ran = true;
        switch (v.type) {
            case 1: {  // dispatcher: 3 x 10 image, 7 copies; base, x, y, $1D, $1E
                const auto data = addr(v.p[0], hires::kSevenCopies3x10);
                if (data.empty()) { ran = false; break; }
                hires::drawImage(page, v.p[1], v.p[2], data, v.p[3] != 0 && v.p[4] != 0);
                break;
            }
            case 2: {  // direct OR blit: data address, column, row, width, rows
                const auto data = addr(v.p[0], static_cast<std::size_t>(v.p[3]) * static_cast<std::size_t>(v.p[4]));
                if (data.empty()) { ran = false; break; }
                hires::blitOr(page, v.p[1], v.p[2], v.p[3], v.p[4], data);
                break;
            }
            case 3: {  // masked clear: mask table, x, y
                const auto mask = addr(v.p[0], hires::kMaskTableBytes);
                if (mask.empty()) { ran = false; break; }
                hires::maskedClear(page, v.p[1], v.p[2], mask);
                break;
            }
            case 4: {  // box clear: position from the table $6992 indexed by $50
                const auto t = addr(0x6992, 12);
                if (t.empty()) { ran = false; break; }
                hires::boxClear(page, t[static_cast<std::size_t>(v.p[0]) * 2], t[static_cast<std::size_t>(v.p[0]) * 2 + 1]);
                break;
            }
            case 5: {  // small dot: x, y; table $61A8
                const auto t = addr(0x61A8, hires::kDotTableBytes);
                if (t.empty()) { ran = false; break; }
                hires::smallDot(page, v.p[0], v.p[1], t);
                break;
            }
            case 6: {  // tile: table, x, y
                const auto t = addr(v.p[0], hires::kTileTableBytes);
                if (t.empty()) { ran = false; break; }
                hires::tile(page, v.p[1], v.p[2], t);
                break;
            }
            case 7: {  // 12 x 3 merge image: data, column, row
                const auto d = addr(v.p[0], 36);
                if (d.empty()) { ran = false; break; }
                hires::blitMergePalette(page, v.p[1], v.p[2], 3, 12, d);
                break;
            }
            case 8: {  // big sprite: frame, x, y
                const auto d = addr(v.p[0], hires::kBigSpriteBytes);
                if (d.empty()) { ran = false; break; }
                hires::bigSprite(page, v.p[1], v.p[2], d);
                break;
            }
            case 9: {  // shifted image: pointer, x, y, $1D, $1E
                const auto d = addr(v.p[0], hires::kImage3x10);
                if (d.empty()) { ran = false; break; }
                hires::shiftedImage(page, v.p[1], v.p[2], d, v.p[3] != 0 && v.p[4] != 0);
                break;
            }
            default:
                ran = false;
        }
        if (!ran) {
            ++skipped[static_cast<std::size_t>(v.type)];
            continue;
        }
        ++tested[static_cast<std::size_t>(v.type)];
        if (!same(page, v.after)) {
            ++failed[static_cast<std::size_t>(v.type)];
            if (failed[static_cast<std::size_t>(v.type)] <= 2) {
                std::printf("    type %d vector differs: p = %d %d %d %d %d\n", v.type, v.p[0], v.p[1], v.p[2], v.p[3], v.p[4]);
            }
        }
    }
    for (int t = 1; t <= 9; ++t) {
        std::printf("    type %d: tested %d, skipped (data not in the pack, e.g. the working buffer at $7E70) %d, failed %d\n", t, tested[static_cast<std::size_t>(t)], skipped[static_cast<std::size_t>(t)], failed[static_cast<std::size_t>(t)]);
        CHECK(failed[static_cast<std::size_t>(t)] == 0);
    }
    // every kind must have been exercised on real calls
    for (int t : {1, 2, 3, 4, 5, 6, 7, 8, 9}) CHECK_MSG(tested[static_cast<std::size_t>(t)] > 0, "no vector of this type could be tested");
}
