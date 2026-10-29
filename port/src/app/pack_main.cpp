// pacman_pack: builds or checks the local data pack.
//   pacman_pack build <image.dsk> <pack.pmpk>   verifies the image, then writes the pack (never touches the image)
//   pacman_pack info <pack.pmpk>                checks the pack against the expected program and prints a summary
// Exit codes: 0 ok, 1 refused (not verified / damaged / wrong pack), 2 usage or file error.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../datapack/datapack.h"

namespace {

bool readFile(const char* path, std::vector<std::uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    namespace dp = pacman::datapack;
    namespace vf = pacman::verify;
    if (argc == 4 && std::string(argv[1]) == "build") {
        std::vector<std::uint8_t> image;
        if (!readFile(argv[2], image)) {
            std::printf("cannot read %s\n", argv[2]);
            return 2;
        }
        const auto built = dp::buildPack(image, vf::referenceExpected(), dp::dataManifest());
        if (!built.ok) {
            std::printf("refused: %s\n", built.reason.c_str());
            return 1;
        }
        const std::filesystem::path target(argv[3]);
        std::filesystem::path temp = target;
        temp += ".tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(built.bytes.data()), static_cast<std::streamsize>(built.bytes.size()));
            if (!file) {
                std::printf("cannot write %s\n", temp.string().c_str());
                return 2;
            }
        }
        std::error_code ec;
        std::filesystem::rename(temp, target, ec);  // replaces an existing pack; a failed build never leaves a half-written pack
        if (ec) {
            std::printf("cannot create %s: %s\n", target.string().c_str(), ec.message().c_str());
            return 2;
        }
        std::printf("data pack written: %s (%zu bytes, %zu blocks)\n", argv[3], built.bytes.size(), dp::dataManifest().size());
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "info") {
        std::vector<std::uint8_t> bytes;
        if (!readFile(argv[2], bytes)) {
            std::printf("cannot read %s\n", argv[2]);
            return 2;
        }
        const auto loaded = dp::loadPack(bytes, vf::referenceExpected());
        if (!loaded.ok) {
            std::printf("refused: %s\n", loaded.reason.c_str());
            return 1;
        }
        std::size_t total = 0;
        for (const auto& b : loaded.pack.blobs()) total += b.bytes.size();
        std::printf("data pack ok: %zu blocks, %zu data bytes, built from program %s\n", loaded.pack.blobs().size(), total,
                    loaded.pack.programSha256().c_str());
        return 0;
    }
    std::fprintf(stderr, "usage: pacman_pack build <image.dsk> <pack.pmpk> | pacman_pack info <pack.pmpk>\n");
    return 2;
}
