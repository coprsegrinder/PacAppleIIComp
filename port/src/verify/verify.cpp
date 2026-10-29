#include "verify.h"

#include <cstdio>

#include "dos33.h"
#include "sha256.h"

namespace pacman::verify {

const Expected& referenceExpected() {
    static const Expected expected{
        "F9E19375B15852B91776776A95755DD3F57E2B20A517AA99ED8603413E0FE805",
        "PAC-MAN",
        4,
        0x1000,
        0x6A00,
        "EFF5B2635EDF0E3B6A227BEE0A477EB697AAEBC29EBC57089167DF5F10EE182C",
    };
    return expected;
}

const char* statusName(Status status) {
    switch (status) {
        case Status::ExactImage: return "exact-image";
        case Status::GameProgram: return "game-program";
        case Status::NotVerified: return "not-verified";
        case Status::Unreadable: return "unreadable";
    }
    return "unreadable";
}

namespace {

std::string format(const char* fmt, unsigned a, unsigned b, unsigned c, unsigned d) {
    char buffer[160];
    std::snprintf(buffer, sizeof buffer, fmt, a, b, c, d);
    return buffer;
}

}  // namespace

Result verify(std::span<const std::uint8_t> image, const Expected& expected) {
    if (image.size() != dos33::kImageSize) {
        return {Status::Unreadable, "size is " + std::to_string(image.size()) + " bytes, expected " +
                                        std::to_string(dos33::kImageSize) +
                                        " (only 140K DOS-order .dsk/.do images are supported)"};
    }
    if (toHexUpper(sha256(image)) == expected.imageSha256) {
        return {Status::ExactImage, "whole-image SHA-256 matches"};
    }
    if (!dos33::looksLikeDos33(image)) {
        return {Status::NotVerified, "not a DOS 3.3 140K image in DOS sector order"};
    }
    const auto catalog = dos33::readCatalog(image);
    if (!catalog) return {Status::NotVerified, "catalog or file allocation is damaged"};
    const dos33::CatalogEntry* entry = nullptr;
    for (const auto& e : *catalog) {
        if (e.name == expected.fileName) {
            entry = &e;
            break;
        }
    }
    if (entry == nullptr) return {Status::NotVerified, "no file named " + expected.fileName + " in the catalog"};
    if (entry->type != expected.fileType) {
        return {Status::NotVerified, "file " + expected.fileName + " is not a binary file"};
    }
    const auto file = dos33::readFile(image, *entry);
    if (!file || file->size() < 4) return {Status::NotVerified, "catalog or file allocation is damaged"};
    const unsigned address = (*file)[0] | ((*file)[1] << 8);
    const unsigned length = (*file)[2] | ((*file)[3] << 8);
    const std::size_t available = file->size() - 4;
    const std::size_t bodySize = length < available ? length : available;  // Python slices the same way
    if (address != expected.loadAddress || length != expected.length || bodySize != length) {
        return {Status::NotVerified, format("header says load $%04X length $%04X, expected $%04X / $%04X", address,
                                            length, expected.loadAddress, expected.length)};
    }
    const std::span<const std::uint8_t> body(file->data() + 4, bodySize);
    if (toHexUpper(sha256(body)) != expected.bodySha256) {
        return {Status::NotVerified, "game program bytes differ from the expected program"};
    }
    return {Status::GameProgram,
            "game program SHA-256 matches; the rest of the disk (DOS tracks, volume number, free space) differs "
            "from the reference image"};
}

}  // namespace pacman::verify
