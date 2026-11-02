// TASK-029h: the first playable program: Pac-Man, the maze, the dots, the score, lives, the blinks and the bonus fruit run on the C++ game core and are drawn with
// the verified hi-res routines into a GameWindow, now WITH THE GHOSTS (M5: all four move, are frightened by the energizers, can be eaten and can catch Pac-Man).
// The level change is an unverified stand-in (level + 1, fresh maze); there is no sound, no READY pause and no death animation yet.
//
// Usage: pacman_play --pack <data.pack> [--scale N] [--seconds N] [--ticks N --dump <page.bin>]
//   --scale N      whole-number window scale (default 3)
//   --seconds N    close the window by itself after N seconds (for tests)
//   --ticks N      run N ticks at once WITHOUT a window (no keys), write the final hi-res page (8,192 bytes) to --dump and exit
// Keys: arrow-style keys of the original (A = up, Z = down, Ctrl-H = left, Ctrl-U = right; the arrow keys of the PC keyboard are mapped to them), Esc = pause,
// Ctrl-S = sound on/off (no sound yet), F11 / Alt+Enter = full screen, Ctrl+Plus/Minus = scale, F9 = mute. Closing the window quits.
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "../core/cycleclock.h"
#include "../core/gamedriver.h"
#include "../datapack/datapack.h"
#include "../verify/verify.h"
#include "gamewindow.h"

namespace {

using namespace pacman;

bool loadPackFile(const std::wstring& packPath, datapack::LoadResult& loaded) {
    std::ifstream file(std::filesystem::path(packPath), std::ios::binary);
    if (!file) {
        std::puts("cannot read the data pack");
        return false;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    loaded = datapack::loadPack(bytes, verify::referenceExpected());
    if (!loaded.ok) {
        std::puts(("data pack refused: " + loaded.reason).c_str());
        return false;
    }
    return true;
}

// One tick with the consequences the live program draws: a completed maze starts the (unverified) next level.
core::TickEvents liveTick(core::GameDriver& drv) {
    const core::TickEvents ev = drv.tick();
    if (ev.levelComplete) drv.nextLevelUnverified();
    return ev;
}

void gameThread(core::GameDriver& drv, app::GameWindow& window, std::atomic<bool>& quit, int seconds) {
    core::CycleClock clock;
    clock.start(core::nowNanoseconds());
    std::int64_t budget = 0;
    const std::int64_t begin = core::nowNanoseconds();
    window.setPage(drv.page());
    while (!quit.load() && window.ok()) {
        const std::int64_t now = core::nowNanoseconds();
        if (seconds > 0 && now - begin > static_cast<std::int64_t>(seconds) * 1'000'000'000) {
            window.close();
            break;
        }
        while (const auto key = window.popKey()) drv.core().input().pushKey(*key);
        budget += clock.cyclesDue(now);
        bool changed = false;
        while (budget >= core::kTypicalTickCycles) {
            liveTick(drv);
            budget -= core::kTypicalTickCycles + drv.extraCycles();
            changed = true;
            if (budget < -core::kDeathSequenceCycles * 2) budget = 0;
        }
        if (changed) window.setPage(drv.page());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring pack, dump;
    int scale = 3, seconds = 0;
    long ticks = -1;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--pack" && i + 1 < argc) pack = argv[++i];
        else if (a == L"--scale" && i + 1 < argc) scale = _wtoi(argv[++i]);
        else if (a == L"--seconds" && i + 1 < argc) seconds = _wtoi(argv[++i]);
        else if (a == L"--ticks" && i + 1 < argc) ticks = _wtol(argv[++i]);
        else if (a == L"--dump" && i + 1 < argc) dump = argv[++i];
        else return 64;
    }
    datapack::LoadResult loaded;
    if (pack.empty() || !loadPackFile(pack, loaded)) return 3;
    core::GameDriver drv;
    if (!drv.init(loaded.pack) || !drv.newGame()) {
        std::puts("the data pack lacks tables the game core needs");
        return 3;
    }
    if (ticks >= 0) {   // headless: run N ticks, write the page
        for (long t = 0; t < ticks; ++t) liveTick(drv);
        if (!dump.empty()) {
            std::ofstream out(std::filesystem::path(dump), std::ios::binary);
            for (std::size_t k = 0; k < hires::kPageBytes; ++k) out.put(static_cast<char>(drv.page().byte(k)));
        }
        return 0;
    }
    app::GameWindowOptions options;
    options.scale = scale;
    app::GameWindow window(options);
    if (!window.ok()) return 4;
    std::atomic<bool> quit{false};
    std::thread worker(gameThread, std::ref(drv), std::ref(window), std::ref(quit), seconds);
    const int code = window.run();
    quit = true;
    worker.join();
    return code;
}
