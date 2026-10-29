// pacman_verify: checks a disk image against the project's verification rule. Read-only.
// Usage: pacman_verify <image.dsk>      exit code 0 = verified, 1 = not verified, 2 = unreadable
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "../verify/verify.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: pacman_verify <image.dsk>\n");
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::printf("unreadable: cannot open %s\n", argv[1]);
        return 2;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto result = pacman::verify::verify(image);
    std::printf("%s: %s\n", pacman::verify::statusName(result.status), result.reason.c_str());
    using pacman::verify::Status;
    if (result.status == Status::ExactImage || result.status == Status::GameProgram) return 0;
    return result.status == Status::Unreadable ? 2 : 1;
}
