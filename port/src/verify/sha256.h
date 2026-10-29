// SHA-256 (FIPS 180-4), self-contained. Used only to verify the user's disk image.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace pacman::verify {

using Sha256Digest = std::array<std::uint8_t, 32>;

Sha256Digest sha256(std::span<const std::uint8_t> data);

// Upper-case hexadecimal text of any byte sequence (64 characters for a digest).
std::string toHexUpper(std::span<const std::uint8_t> bytes);

}  // namespace pacman::verify
