// TASK-029h: the first playable program: Pac-Man, the maze, the dots, the score, lives, the blinks and the bonus fruit run on the C++ game core and are drawn with
// the verified hi-res routines into a GameWindow, now WITH THE GHOSTS (M5: all four move, are frightened by the energizers, can be eaten and can catch Pac-Man).
// The level change equals the oracle (TASK-031-3); cartoons 1-3 play after levels 2, 5, 9, 15, ... (031-6b, 031-7); there is no sound, no READY pause and no death animation yet.
//
// Usage: pacman_play --pack <data.pack> [--scale N] [--seconds N] [--ticks N --dump <page.bin>] [--level N] [--items K]
//   --scale N      whole-number window scale (default 3)
//   --seconds N    close the window by itself after N seconds (for tests)
//   --level N      test aid: start at level N (1-255)
//   --items K      test aid: only K dots/energizers left to eat (1-248), so a level end comes quickly
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

#include "../core/cartoon.h"
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
// `cartoons`: the live window plays the intermission after the level end; the batch mode (--ticks, no window) goes straight to the next level.
core::TickEvents liveTick(core::GameDriver& drv, core::CartoonPlayer* cartoons = nullptr, const datapack::Pack* pack = nullptr) {
    const unsigned levelBefore = drv.core().state().level();
    const core::TickEvents ev = drv.tick();
    if (ev.levelComplete) {
        // the cartoon (levels with level mod 13 = 2, 5, 9: cartoons 1, 2, 3) is shown BEFORE the next level starts (docs/CARTOONS.md)
        const bool playing = cartoons != nullptr && pack != nullptr && cartoons->start(*pack, core::cartoonAfterLevel(levelBefore));
        if (!playing) drv.nextLevel();
    }
    return ev;
}

void gameThread(core::GameDriver& drv, const datapack::Pack& pack, app::GameWindow& window, std::atomic<bool>& quit, int seconds) {
    core::CartoonPlayer cartoon;
    hires::Page cartoonPage;
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
        budget += clock.cyclesDue(now);
        if (cartoon.active()) {   // an intermission: the original does not read the keyboard meanwhile, a latched key is thrown away at the next level's start (docs/CARTOONS.md 7.1)
            while (window.popKey()) {}
            cartoon.advance(budget);
            budget = 0;
            if (cartoon.active()) {
                cartoon.render(pack, cartoonPage);
                window.setPage(cartoonPage);
            } else {
                drv.nextLevel();
                window.setPage(drv.page());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        while (const auto key = window.popKey()) drv.core().input().pushKey(*key);
        bool changed = false;
        while (budget >= core::kTypicalTickCycles) {
            liveTick(drv, &cartoon, &pack);
            if (cartoon.active()) {   // the level ended with a cartoon: leave the tick loop, the cartoon starts at 0 cycles
                budget = 0;
                break;
            }
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
    int scale = 3, seconds = 0, startLevel = 0, startItems = 0;
    long ticks = -1;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--pack" && i + 1 < argc) pack = argv[++i];
        else if (a == L"--scale" && i + 1 < argc) scale = _wtoi(argv[++i]);
        else if (a == L"--seconds" && i + 1 < argc) seconds = _wtoi(argv[++i]);
        else if (a == L"--ticks" && i + 1 < argc) ticks = _wtol(argv[++i]);
        else if (a == L"--dump" && i + 1 < argc) dump = argv[++i];
        else if (a == L"--level" && i + 1 < argc) startLevel = _wtoi(argv[++i]);
        else if (a == L"--items" && i + 1 < argc) startItems = _wtoi(argv[++i]);
        else return 64;
    }
    datapack::LoadResult loaded;
    if (pack.empty() || !loadPackFile(pack, loaded)) return 3;
    core::GameDriver drv;
    if (!drv.init(loaded.pack) || !drv.newGame()) {
        std::puts("the data pack lacks tables the game core needs");
        return 3;
    }
    // Test aids, NOT part of the faithful game: start at a chosen level, and/or with only a few dots left to eat so that a level end (and the cartoon after level 2, 5, 9, ...) is quick to reach.
    if (startLevel >= 1 && startLevel <= 255) drv.core().state().zp(0x55) = static_cast<std::uint8_t>(startLevel);
    if (startItems >= 1 && startItems <= 248) drv.core().state().zp(0x56) = static_cast<std::uint8_t>(startItems);
    if (startLevel > 0 || startItems > 0) drv.rebuildScreen();
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
    std::thread worker(gameThread, std::ref(drv), std::cref(loaded.pack), std::ref(window), std::ref(quit), seconds);
    const int code = window.run();
    quit = true;
    worker.join();
    return code;
}
