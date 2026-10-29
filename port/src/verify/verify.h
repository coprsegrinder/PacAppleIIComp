// The disk-image verification rule (docs/VERIFIER_SPEC.md, confirmed by the user 2026-10-29).
// C++ twin of tools/verify_image.py; the two must give the same answers.
#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace pacman::verify {

enum class Status {
    ExactImage,   // whole-image SHA-256 matches the reference image
    GameProgram,  // the PAC-MAN program body matches; the rest of the disk differs
    NotVerified,  // readable, but not this game
    Unreadable    // not a supported container (only 140K DOS-order images are supported)
};

struct Result {
    Status status;
    std::string reason;  // plain English, shown to the user
};

struct Expected {
    std::string imageSha256;   // upper-case hex of the whole image
    std::string fileName;      // catalog name of the game program
    std::uint8_t fileType;     // 4 = DOS 3.3 binary
    std::uint16_t loadAddress;
    std::uint16_t length;
    std::string bodySha256;    // upper-case hex of the program body (after the 4-byte header)
};

// The values of the image supplied by the user. Only hashes: no original data is embedded here.
const Expected& referenceExpected();

Result verify(std::span<const std::uint8_t> image, const Expected& expected);
inline Result verify(std::span<const std::uint8_t> image) { return verify(image, referenceExpected()); }

// "exact-image", "game-program", "not-verified", "unreadable" (same words as the Python tool).
const char* statusName(Status status);

}  // namespace pacman::verify
