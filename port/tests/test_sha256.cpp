#include <cstdint>
#include <string>
#include <vector>

#include "../src/verify/sha256.h"
#include "test_framework.h"

using pacman::verify::sha256;
using pacman::verify::toHexUpper;

namespace {

std::string hashOf(const std::string& text) {
    const std::vector<std::uint8_t> bytes(text.begin(), text.end());
    return toHexUpper(sha256(bytes));
}

}  // namespace

// Expected values computed with Python's hashlib on 2026-10-29 (and equal to the published FIPS 180-4 examples).
TEST(sha256_standard_vectors) {
    CHECK(hashOf("") == "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
    CHECK(hashOf("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    CHECK(hashOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");
}

TEST(sha256_padding_boundaries) {
    // 55, 56 and 64 bytes straddle the "length fits in this block" boundary; 119/120 the second one.
    CHECK(hashOf(std::string(55, 'x')) == "D5E285683CD4EFC02D021A5C62014694958901005D6F71E89E0989FAC77E4072");
    CHECK(hashOf(std::string(56, 'x')) == "04C26261370EE7541549D16DEE320C723E3FD14671E66A099AFE0A377C16888E");
    CHECK(hashOf(std::string(64, 'x')) == "7CE100971F64E7001E8FE5A51973ECDFE1CED42BEFE7EE8D5FD6219506B5393C");
    CHECK(hashOf(std::string(119, 'y')) == "0EE964660D4956E34132B7B0F5BDC15FD0D365E26186AC9FD97A090D8D5E5508");
    CHECK(hashOf(std::string(120, 'y')) == "93DD18DA6780C736E1A176724E4AFB13B035014CE414D9C2675599E3124E41FB");
}

TEST(sha256_million_a_and_binary_pattern) {
    CHECK(hashOf(std::string(1000000, 'a')) == "CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0");
    std::vector<std::uint8_t> pattern(1000);
    for (std::size_t i = 0; i < pattern.size(); ++i) pattern[i] = static_cast<std::uint8_t>((i * 7) & 0xFF);
    CHECK(toHexUpper(sha256(pattern)) == "89F4FF56A25DD1DB06A4CE6033603775D705FB96F30F8693733FEF602A1CA532");
}

TEST(hex_upper_formats_bytes) {
    const std::uint8_t bytes[] = {0x00, 0x0F, 0xA5, 0xFF};
    CHECK(toHexUpper(bytes) == "000FA5FF");
}
