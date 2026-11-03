// Usage: pacman_tests [--drawticks <list>] [--image <game.dsk>] [--vectors <hires_vectors.bin>] [--reference <other DOS 3.3 disk>]
// Tests that need the real disk images are skipped (and reported as skipped) when the paths are not given.
#include <cstdio>
#include <cstring>

#include "test_framework.h"

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
            testfw::config().image = argv[++i];
        } else if (std::strcmp(argv[i], "--vectors") == 0 && i + 1 < argc) {
            testfw::config().vectors = argv[++i];
        } else if (std::strcmp(argv[i], "--pages") == 0 && i + 1 < argc) {
            testfw::config().pages = argv[++i];
        } else if (std::strcmp(argv[i], "--tickcalls") == 0 && i + 1 < argc) {
            testfw::config().tickcalls = argv[++i];
        } else if (std::strcmp(argv[i], "--page0") == 0 && i + 1 < argc) {
            testfw::config().page0 = argv[++i];
        } else if (std::strcmp(argv[i], "--demo") == 0 && i + 1 < argc) {
            testfw::config().demo = argv[++i];
        } else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            testfw::config().ticks = argv[++i];
        } else if (std::strcmp(argv[i], "--drawticks") == 0 && i + 1 < argc) {
            testfw::config().drawticks = argv[++i];
        } else if (std::strcmp(argv[i], "--clicks") == 0 && i + 1 < argc) {
            testfw::config().clicks = argv[++i];
        } else if (std::strcmp(argv[i], "--decide") == 0 && i + 1 < argc) {
            testfw::config().decide = argv[++i];
        } else if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            testfw::config().script = argv[++i];
        } else if (std::strcmp(argv[i], "--dots") == 0 && i + 1 < argc) {
            testfw::config().dots = argv[++i];
        } else if (std::strcmp(argv[i], "--reference") == 0 && i + 1 < argc) {
            testfw::config().reference = argv[++i];
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return 2;
        }
    }
    int failedTests = 0;
    for (const auto& test : testfw::registry()) {
        const int before = testfw::failures();
        std::printf("[ RUN  ] %s\n", test.name);
        test.run();
        if (testfw::failures() == before) {
            std::printf("[  OK  ] %s\n", test.name);
        } else {
            std::printf("[FAILED] %s\n", test.name);
            ++failedTests;
        }
    }
    std::printf("\n%zu tests, %d failed\n", testfw::registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
