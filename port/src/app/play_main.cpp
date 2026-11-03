// TASK-029h: the first playable program: Pac-Man, the maze, the dots, the score, lives, the blinks and the bonus fruit run on the C++ game core and are drawn with
// the verified hi-res routines into a GameWindow, now WITH THE GHOSTS (M5: all four move, are frightened by the energizers, can be eaten and can catch Pac-Man).
// The level change equals the oracle (TASK-031-3); cartoons 1-3 play after levels 2, 5, 9, 15, ... (031-6b, 031-7); the sounds play (032-5); there is no READY pause and no death animation yet.
//
// Usage: pacman_play --pack <data.pack> [--scale N] [--seconds N] [--ticks N --dump <page.bin>] [--level N] [--items K]
//   --scale N      whole-number window scale (default 3)
//   --seconds N    close the window by itself after N seconds (for tests)
//   --sound-test   play every sound once (to listen to the speaker model) and exit
//   --level N      test aid: start at level N (1-255)
//   --items K      test aid: only K dots/energizers left to eat (1-248), so a level end comes quickly
//   --ticks N      run N ticks at once WITHOUT a window (no keys), write the final hi-res page (8,192 bytes) to --dump and exit
// Keys: arrow-style keys of the original (A = up, Z = down, Ctrl-H = left, Ctrl-U = right; the arrow keys of the PC keyboard are mapped to them), Esc = pause,
// Ctrl-S = sound on/off, F11 / Alt+Enter = full screen, Ctrl+Plus/Minus = scale, F9 = mute. Closing the window quits.
#include <windows.h>

#include <array>
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

#include "audio.h"
#include "../core/cartoon.h"
#include "../core/sound.h"
#include "../core/squarewave.h"
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
// --sound-test: plays every sound of the game once through the speaker model (the way it will sound in the game), with a short pause between them, and exits.
int playSoundTest(const datapack::Pack& pack) {
    core::SoundData sounds;
    if (!sounds.load(pack)) {
        std::puts("the data pack lacks the sound lists");
        return 3;
    }
    app::AudioOutput audio;
    if (!audio.open(44100)) {
        std::puts("no audio device");
        return 5;
    }
    struct Item { core::SoundId id; const char* name; };
    const Item items[] = {{core::SoundId::DotEaten, "dot"}, {core::SoundId::Energizer, "energizer"}, {core::SoundId::Fruit, "fruit"}, {core::SoundId::GhostEaten, "ghost eaten"},
                          {core::SoundId::Death6, "death step 6"}, {core::SoundId::Death5, "death step 5"}, {core::SoundId::Death4, "death step 4"}, {core::SoundId::Death3, "death step 3"},
                          {core::SoundId::Death2, "death step 2"}, {core::SoundId::Death1, "death step 1"}, {core::SoundId::BonusLife, "bonus life"}, {core::SoundId::Jingle, "intro jingle (5 s)"}};
    for (const Item& it : items) {
        std::printf("playing: %s\n", it.name);
        std::fflush(stdout);
        const auto clicks = core::clickTimes(sounds.list(it.id));
        audio.play(core::renderClicks(clicks, core::soundFreezeCycles(it.id), 44100));
        while (audio.busy()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
    }
    return 0;
}

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

// TASK-032-5: the game's sounds. Every sound is rendered once (square wave from the original's click times); a sound the core reports is queued behind what is still playing.
// The audio can be switched off by Ctrl-S (the original's own switch, in the core) or F9 (the window); the game still stands still for the sound (the original's engines
// busy-wait on the cassette output when Ctrl-S has silenced them).
struct SoundPlayer {
    app::AudioOutput audio;
    std::array<core::Pcm, static_cast<std::size_t>(core::SoundId::Count)> pcm;
    core::Pcm gapBetweenDeathSteps;   // 137,052 cycles of silence: measured between the six death steps (entry to entry minus the freeze of the earlier step)
    bool ready = false;

    void init(const datapack::Pack& pack) {
        core::SoundData data;
        if (!data.load(pack) || !audio.open(44100)) return;
        for (std::size_t i = 0; i < pcm.size(); ++i) {
            const auto id = static_cast<core::SoundId>(i);
            if (id == core::SoundId::Count) continue;
            const auto clicks = core::clickTimes(data.list(id));
            if (!clicks.empty()) pcm[i] = core::renderClicks(clicks, core::soundFreezeCycles(id), 44100);
        }
        gapBetweenDeathSteps = core::renderClicks({}, 137'052, 44100);
        ready = true;
    }
    void play(core::SoundId id) {
        if (!ready || pcm[static_cast<std::size_t>(id)].samples.empty()) return;
        const bool laterDeathStep = id == core::SoundId::Death5 || id == core::SoundId::Death4 || id == core::SoundId::Death3 || id == core::SoundId::Death2 || id == core::SoundId::Death1;
        if (laterDeathStep) audio.play(gapBetweenDeathSteps);
        audio.play(pcm[static_cast<std::size_t>(id)]);
    }
    void stop() {
        if (ready) audio.stop();
    }
};

// How many cycles beyond the ordinary tick cost the game stands still for a sound. The dot sound is part of the measured mean tick cost (kTypicalTickCycles) and the six
// death steps are part of the measured death sequence (kDeathSequenceCycles); all other sounds are added (INFERRED approximation until the pacing step, M8).
std::int64_t extraFreeze(core::SoundId id) {
    switch (id) {
        case core::SoundId::DotEaten:
        case core::SoundId::Death1:
        case core::SoundId::Death2:
        case core::SoundId::Death3:
        case core::SoundId::Death4:
        case core::SoundId::Death5:
        case core::SoundId::Death6:
            return 0;
        default:
            return core::soundFreezeCycles(id);
    }
}

void gameThread(core::GameDriver& drv, const datapack::Pack& pack, app::GameWindow& window, std::atomic<bool>& quit, int seconds) {
    SoundPlayer sounds;
    sounds.init(pack);
    core::CartoonPlayer cartoon;
    hires::Page cartoonPage;
    core::CycleClock clock;
    clock.start(core::nowNanoseconds());
    std::int64_t budget = 0;
    const std::int64_t begin = core::nowNanoseconds();
    window.setPage(drv.page());
    // the intro jingle starts a game (before the first tick, docs/SOUND.md section 6); the game stands still for its 5 s
    if (drv.core().input().soundOn() && !window.muted()) sounds.play(core::SoundId::Jingle);
    budget = -(core::soundFreezeCycles(core::SoundId::Jingle) + core::kReadyPauseCycles);   // the jingle, then the READY wait (033-2)
    while (!quit.load() && window.ok()) {
        const std::int64_t now = core::nowNanoseconds();
        if (seconds > 0 && now - begin > static_cast<std::int64_t>(seconds) * 1'000'000'000) {
            window.close();
            break;
        }
        budget += clock.cyclesDue(now);
        if (cartoon.active() && budget < 0) {   // the level-end pause before the intermission (the original spends kLevelEndCycles there, measured 033-1b)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
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
            const core::TickEvents ev = liveTick(drv, &cartoon, &pack);
            std::int64_t soundFreeze = 0;
            const bool audible = drv.core().input().soundOn() && !window.muted();
            for (const core::SoundId id : ev.sounds) {
                if (audible) sounds.play(id);
                soundFreeze += extraFreeze(id);
            }
            if (!audible) sounds.stop();   // Ctrl-S / F9 during a long sound cuts it
            if (cartoon.active()) {   // the level ended with a cartoon: leave the tick loop; the level-end pause comes first, then the cartoon starts
                budget = -core::kLevelEndCycles;
                window.setPage(drv.page());
                break;
            }
            budget -= drv.tickCycles() + drv.extraCycles() + soundFreeze;   // the tick's modelled cost (033-1c), not a flat mean
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
    bool soundTest = false;
    long ticks = -1;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--pack" && i + 1 < argc) pack = argv[++i];
        else if (a == L"--scale" && i + 1 < argc) scale = _wtoi(argv[++i]);
        else if (a == L"--seconds" && i + 1 < argc) seconds = _wtoi(argv[++i]);
        else if (a == L"--ticks" && i + 1 < argc) ticks = _wtol(argv[++i]);
        else if (a == L"--dump" && i + 1 < argc) dump = argv[++i];
        else if (a == L"--sound-test") soundTest = true;
        else if (a == L"--level" && i + 1 < argc) startLevel = _wtoi(argv[++i]);
        else if (a == L"--items" && i + 1 < argc) startItems = _wtoi(argv[++i]);
        else return 64;
    }
    datapack::LoadResult loaded;
    if (pack.empty() || !loadPackFile(pack, loaded)) return 3;
    if (soundTest) return playSoundTest(loaded.pack);
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
