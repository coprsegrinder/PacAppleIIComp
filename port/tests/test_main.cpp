// Usage: pacman_tests [--drawticks <list>] [--image <game.dsk>] [--vectors <hires_vectors.bin>] [--reference <other DOS 3.3 disk>]
// Tests that need the real disk images are skipped (and reported as skipped) when the paths are not given.
#include <cstdio>
#include <cstring>

#include "test_framework.h"

int main(int argc, char** argv) {
    const char* only = nullptr;   // --only <text>: run just the tests whose name contains <text>
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
        } else if (std::strcmp(argv[i], "--only") == 0 && i + 1 < argc) {
            only = argv[++i];
        } else if (std::strcmp(argv[i], "--titleevents") == 0 && i + 1 < argc) {
            testfw::config().titleEvents = argv[++i];
        } else if (std::strcmp(argv[i], "--chaseevents") == 0 && i + 1 < argc) {
            testfw::config().chaseEvents = argv[++i];
        } else if (std::strcmp(argv[i], "--setupevents") == 0 && i + 1 < argc) {
            testfw::config().setupEvents = argv[++i];
        } else if (std::strcmp(argv[i], "--endevents") == 0 && i + 1 < argc) {
            testfw::config().endEvents = argv[++i];
        } else if (std::strcmp(argv[i], "--demopages") == 0 && i + 1 < argc) {
            testfw::config().demoPages = argv[++i];
        } else if (std::strcmp(argv[i], "--demoafter") == 0 && i + 1 < argc) {
            testfw::config().demoAfter = argv[++i];
        } else if (std::strcmp(argv[i], "--playerpages") == 0 && i + 1 < argc) {
            testfw::config().playerPages = argv[++i];
        } else if (std::strcmp(argv[i], "--playerticks") == 0 && i + 1 < argc) {
            testfw::config().playerTicks = argv[++i];
        } else if (std::strcmp(argv[i], "--boot") == 0 && i + 1 < argc) {
            testfw::config().boot = argv[++i];
        } else if (std::strcmp(argv[i], "--hidden") == 0 && i + 1 < argc) {
            testfw::config().hidden = argv[++i];
        } else if (std::strcmp(argv[i], "--prompts") == 0 && i + 1 < argc) {
            testfw::config().prompts = argv[++i];
        } else if (std::strcmp(argv[i], "--charrom") == 0 && i + 1 < argc) {
            testfw::config().charRom = argv[++i];
        } else if (std::strcmp(argv[i], "--democycles") == 0 && i + 1 < argc) {
            testfw::config().demoCycles = argv[++i];
        } else if (std::strcmp(argv[i], "--titlesnaps") == 0 && i + 1 < argc) {
            testfw::config().titleSnaps = argv[++i];
        } else if (std::strcmp(argv[i], "--deaths") == 0 && i + 1 < argc) {
            testfw::config().deaths = argv[++i];
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
    std::size_t ran = 0;
    for (const auto& test : testfw::registry()) {
        if (only != nullptr && std::strstr(test.name, only) == nullptr) continue;
        ++ran;
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
