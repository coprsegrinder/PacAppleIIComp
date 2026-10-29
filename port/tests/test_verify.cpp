// Mirrors tests/test_verify_image.py (synthetic disks, no original data).
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "../src/verify/dos33.h"
#include "../src/verify/sha256.h"
#include "../src/verify/verify.h"
#include "test_framework.h"
#include "test_support.h"

using namespace pacman;
using verify::Status;
using testsupport::DiskOptions;
using testsupport::expectedFor;
using testsupport::makeDisk;
using testsupport::put;
using testsupport::testBody;

TEST(verify_exact_image) {
    const auto body = testBody();
    CHECK(verify::verify(makeDisk(body), expectedFor(body)).status == Status::ExactImage);
}

TEST(verify_different_dos_tracks_and_volume_still_verify_the_program) {
    const auto body = testBody();
    DiskOptions o;
    o.volume = 1;
    o.dosMarker = 0x99;
    CHECK(verify::verify(makeDisk(body, o), expectedFor(body)).status == Status::GameProgram);
}

TEST(verify_one_changed_program_byte_fails) {
    const auto body = testBody();
    auto other = body;
    other[500] ^= 1;
    CHECK(verify::verify(makeDisk(other), expectedFor(body)).status == Status::NotVerified);
}

TEST(verify_wrong_name_type_or_load_address_fail) {
    const auto body = testBody();
    DiskOptions a;
    a.name = "OTHER";
    DiskOptions b;
    b.type = 2;
    DiskOptions c;
    c.address = 0x2000;
    CHECK(verify::verify(makeDisk(body, a), expectedFor(body)).status == Status::NotVerified);
    CHECK(verify::verify(makeDisk(body, b), expectedFor(body)).status == Status::NotVerified);
    CHECK(verify::verify(makeDisk(body, c), expectedFor(body)).status == Status::NotVerified);
}

TEST(verify_wrong_size_and_non_dos_images) {
    const auto body = testBody();
    CHECK(verify::verify(std::vector<std::uint8_t>(100, 0), expectedFor(body)).status == Status::Unreadable);
    CHECK(verify::verify(std::vector<std::uint8_t>(dos33::kImageSize, 0), expectedFor(body)).status == Status::NotVerified);
}

TEST(verify_damaged_catalog_does_not_crash) {
    const auto body = testBody();
    auto img = makeDisk(body);
    put(img, 3, 0, {0, 0, 0, 0, 0}, 0);
    put(img, 17, 15, {99, 99, 99}, 0);  // next catalog sector points outside the disk
    const auto r = verify::verify(img, expectedFor(body));
    CHECK(r.status == Status::NotVerified);
    CHECK(r.reason == "catalog or file allocation is damaged");
}

TEST(verify_file_list_pointing_outside_the_disk_is_damaged) {
    const auto body = testBody();
    auto img = makeDisk(body);
    put(img, 3, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 200, 3});  // first data sector: track 200
    CHECK(verify::verify(img, expectedFor(body)).status == Status::NotVerified);
}

TEST(verify_reason_text_matches_the_python_tool) {
    const auto body = testBody();
    CHECK(verify::verify(makeDisk(body), expectedFor(body)).reason == "whole-image SHA-256 matches");
    DiskOptions o;
    o.name = "OTHER";
    CHECK(verify::verify(makeDisk(body, o), expectedFor(body)).reason == "no file named PAC-MAN in the catalog");
    DiskOptions p;
    p.address = 0x2000;
    CHECK(verify::verify(makeDisk(body, p), expectedFor(body)).reason ==
          "header says load $2000 length $03E8, expected $1000 / $03E8");
}

TEST(dos33_catalog_names_types_and_deleted_entries) {
    const auto body = testBody();
    auto img = makeDisk(body);
    // second slot: a deleted file (track byte 0xFF) must be skipped
    put(img, 17, 15, {0xFF, 0, 4}, 11 + 35);
    const auto catalog = dos33::readCatalog(img);
    CHECK(catalog.has_value());
    CHECK(catalog && catalog->size() == 1);
    CHECK(catalog && (*catalog)[0].name == "PAC-MAN");
    CHECK(catalog && (*catalog)[0].type == 4);
    CHECK(dos33::looksLikeDos33(img));
}

TEST(dos33_sector_bounds) {
    const std::vector<std::uint8_t> img(dos33::kImageSize, 0);
    CHECK(dos33::sector(img, 34, 15).size() == 256);
    CHECK(dos33::sector(img, 35, 0).empty());
    CHECK(dos33::sector(img, 0, 16).empty());
    CHECK(dos33::sector(std::span<const std::uint8_t>(img).first(1000), 17, 0).empty());
}
