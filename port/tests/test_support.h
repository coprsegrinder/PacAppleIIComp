// Shared helpers for tests: a synthetic DOS 3.3 disk with one game file (no original data).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../src/verify/dos33.h"
#include "../src/verify/sha256.h"
#include "../src/verify/verify.h"

namespace testsupport {

using namespace pacman;

inline void put(std::vector<std::uint8_t>& img, std::size_t t, std::size_t s, std::initializer_list<int> data, std::size_t off = 0) {
    std::size_t o = (t * 16 + s) * 256 + off;
    for (const int b : data) img[o++] = static_cast<std::uint8_t>(b);
}

struct DiskOptions {
    std::string name = "PAC-MAN";
    int type = 4;
    unsigned address = 0x1000;
    int volume = 254;
    int dosMarker = 0;
};

// Synthetic DOS 3.3 disk: VTOC, one catalog sector, one file whose track/sector list is at T3/S0, data from T5/S0.
inline std::vector<std::uint8_t> makeDisk(const std::vector<std::uint8_t>& body, const DiskOptions& o = {}) {
    std::vector<std::uint8_t> img(dos33::kImageSize, 0);
    put(img, 17, 0, {0, 17, 15, 3});
    img[(17 * 16) * 256 + 0x06] = static_cast<std::uint8_t>(o.volume);
    img[(17 * 16) * 256 + 0x34] = 35;
    img[(17 * 16) * 256 + 0x35] = 16;
    put(img, 0, 5, {o.dosMarker});
    std::vector<std::uint8_t> data = {static_cast<std::uint8_t>(o.address & 0xFF), static_cast<std::uint8_t>(o.address >> 8),
                                      static_cast<std::uint8_t>(body.size() & 0xFF), static_cast<std::uint8_t>(body.size() >> 8)};
    data.insert(data.end(), body.begin(), body.end());
    const std::size_t nsec = (data.size() + 255) / 256;
    std::size_t entry = (17 * 16 + 15) * 256 + 11;
    img[entry + 0] = 3;
    img[entry + 1] = 0;
    img[entry + 2] = static_cast<std::uint8_t>(o.type);
    for (std::size_t i = 0; i < 30; ++i) {
        const char c = i < o.name.size() ? o.name[i] : ' ';
        img[entry + 3 + i] = static_cast<std::uint8_t>(c | 0x80);
    }
    img[entry + 33] = static_cast<std::uint8_t>(nsec & 0xFF);
    for (std::size_t i = 0; i < nsec; ++i) {
        const std::size_t t = 5 + i / 16;
        const std::size_t s = i % 16;
        img[(3 * 16 + 0) * 256 + 12 + 2 * i] = static_cast<std::uint8_t>(t);
        img[(3 * 16 + 0) * 256 + 13 + 2 * i] = static_cast<std::uint8_t>(s);
        for (std::size_t k = 0; k < 256 && i * 256 + k < data.size(); ++k) img[(t * 16 + s) * 256 + k] = data[i * 256 + k];
    }
    return img;
}

inline std::vector<std::uint8_t> testBody() {
    std::vector<std::uint8_t> body(1000);
    for (std::size_t i = 0; i < body.size(); ++i) body[i] = static_cast<std::uint8_t>((i * 7) & 0xFF);
    return body;
}

inline verify::Expected expectedFor(const std::vector<std::uint8_t>& body) {
    return {verify::toHexUpper(verify::sha256(makeDisk(body))), "PAC-MAN", 4, 0x1000, static_cast<std::uint16_t>(body.size()),
            verify::toHexUpper(verify::sha256(body))};
}


}  // namespace testsupport
