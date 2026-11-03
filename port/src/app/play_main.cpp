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
#include "../core/attractflow.h"
#include "../core/cartoon.h"
#include "../core/textscreen.h"
#include "../core/sound.h"
#include "../core/squarewave.h"
#include "../core/cycleclock.h"
#include "../core/gamedriver.h"
#include "../datapack/datapack.h"
#include "../verify/verify.h"
#include "../video/screens.h"
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

// The ROM dumps of the Apple II Plus that the program runs (the Autostart Monitor) and shows text with (the character generator). They are files next to the program (the release has a folder
// `roms`); the game image is never needed. Searched: the folder named with --roms, `roms` and `Apple II roms` next to the program and in the folders above it, and in the working folder.
bool findRoms(const std::wstring& arg, std::string& monitorRom, std::string& charRom) {
    namespace fs = std::filesystem;
    const wchar_t* kMonitor = L"Apple II plus ROM Pages F8-FF - 341-0020 - Autostart Monitor.bin";
    const wchar_t* kChar = L"Apple II Character ROM - 341-0036.bin";
    std::vector<fs::path> dirs;
    if (!arg.empty()) dirs.emplace_back(arg);
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    fs::path exe = fs::path(buf).parent_path();
    for (int up = 0; up < 3; ++up) {
        dirs.push_back(exe / L"roms");
        dirs.push_back(exe / L"Apple II roms");
        exe = exe.parent_path();
    }
    dirs.emplace_back(L"roms");
    dirs.emplace_back(L"Apple II roms");
    for (const fs::path& d : dirs) {
        std::error_code ec;
        if (fs::exists(d / kMonitor, ec) && fs::exists(d / kChar, ec)) {
            monitorRom = (d / kMonitor).string();
            charRom = (d / kChar).string();
            return true;
        }
    }
    return false;
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
    void playNow(core::SoundId id) {   // no gap: the death animation schedules its steps itself
        if (ready && !pcm[static_cast<std::size_t>(id)].samples.empty()) audio.play(pcm[static_cast<std::size_t>(id)]);
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

// One game, from the intro jingle to the end of the GAME OVER wait. Returns true when the game is over (the program goes back to the attract loop), false when the window closed.
bool runGame(core::GameDriver& drv, const datapack::Pack& pack, app::GameWindow& window, std::atomic<bool>& quit, int seconds, std::int64_t begin) {
    SoundPlayer sounds;
    sounds.init(pack);
    core::CartoonPlayer cartoon;
    hires::Page cartoonPage;
    core::CycleClock clock;
    clock.start(core::nowNanoseconds());
    std::int64_t budget = 0;
    drv.setPlayDeath(true);
    bool startWait = true, readyShown = false;
    std::size_t seqNext = 0;
    std::int64_t seqTime = 0;
    bool seqActive = false;
    window.setPage(drv.page());
    // the intro jingle starts a game (before the first tick, docs/SOUND.md section 6); the game stands still for its 5 s
    if (drv.core().input().soundOn() && !window.muted()) sounds.play(core::SoundId::Jingle);
    budget = -(core::soundFreezeCycles(core::SoundId::Jingle) + core::kReadyPauseCycles);   // the jingle, then the READY wait (033-2)
    while (!quit.load() && window.ok()) {
        const std::int64_t now = core::nowNanoseconds();
        if (seconds > 0 && now - begin > static_cast<std::int64_t>(seconds) * 1'000'000'000) {
            window.close();
            return false;
        }
        budget += clock.cyclesDue(now);
        if (startWait) {   // the jingle, then READY on the screen for 3.05 s (033-2, ready_timing.py: READY is drawn after the jingle)
            if (!readyShown && budget >= -core::kReadyPauseCycles) {
                readyShown = true;
                drv.showReady();
                window.setPage(drv.page());
            }
            if (budget >= 0) {
                startWait = false;
                drv.rebuildScreen();
                window.setPage(drv.page());
            }
        }
        if (seqActive) {   // the death animation (033-3): steps at oracle times; keys stay queued, the original does not read them meanwhile
            seqTime += budget;
            budget = 0;
            while (seqNext < drv.sequence().size() && drv.sequence()[seqNext].at <= seqTime) {
                const core::SequenceStep& st = drv.sequence()[seqNext++];
                drv.applySequenceStep(st);
                if (st.deathSound > 0 && drv.core().input().soundOn() && !window.muted())
                    sounds.playNow(static_cast<core::SoundId>(static_cast<int>(core::SoundId::Death1) + st.deathSound - 1));
                window.setPage(drv.page());
            }
            if (seqNext >= drv.sequence().size()) {
                seqActive = false;
                if (drv.gameOverPending()) return true;   // GAME OVER and the wait after it are over: the attract loop starts again
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
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
            const bool dying = !drv.sequence().empty();   // the animation schedules the six death sounds itself
            for (const core::SoundId id : ev.sounds) {
                const bool deathStep = id >= core::SoundId::Death1 && id <= core::SoundId::Death6;
                if (dying && deathStep) continue;
                if (audible) sounds.play(id);
                soundFreeze += extraFreeze(id);
            }
            if (!audible) sounds.stop();   // Ctrl-S / F9 during a long sound cuts it
            if (dying) {   // the pacman was caught: play the animation, then the next life starts
                seqActive = true;
                seqNext = 0;
                seqTime = 0;
                budget = 0;
                break;
            }
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
    return false;
}

// The screens before and between the games: the boot menus, the attract loop, the prompts and the hidden menu (AttractFlow), shown with the lo-res page, the text page (the Apple II
// character ROM) or the hi-res page. Returns true when Return starts a game (flow.mode() == Game), false when the window closed.
bool runMenus(core::AttractFlow& flow, const core::CharacterRom& rom, const datapack::Pack& pack, app::GameWindow& window, std::atomic<bool>& quit, int seconds, std::int64_t begin) {
    core::CycleClock clock;
    clock.start(core::nowNanoseconds());
    core::CartoonPlayer cartoon;
    hires::Page cartoonPage;
    std::vector<std::uint8_t> bitmap;
    std::vector<std::uint32_t> argb(280 * 192);
    std::int64_t lastShown = 0, lastCartoonElapsed = 0;
    bool cartoonOn = false;
    int lastKind = -1;
    while (!quit.load() && window.ok()) {
        const std::int64_t now = core::nowNanoseconds();
        if (seconds > 0 && now - begin > static_cast<std::int64_t>(seconds) * 1'000'000'000) {
            window.close();
            return false;
        }
        if (const auto key = window.popKey()) flow.keyDown(*key);
        flow.advance(clock.cyclesDue(now));
        if (flow.mode() == core::AttractFlow::Mode::Game) return true;
        if (now - lastShown >= 8'000'000) {   // about 120 pictures a second at most
            lastShown = now;
            if (flow.mode() == core::AttractFlow::Mode::Cartoon) {
                if (!cartoonOn) {
                    cartoonOn = cartoon.start(pack, flow.cartoonNumber());
                    lastCartoonElapsed = 0;
                }
                if (cartoonOn) {
                    cartoon.advance(flow.cartoonElapsed() - lastCartoonElapsed);
                    lastCartoonElapsed = flow.cartoonElapsed();
                    cartoon.render(pack, cartoonPage);
                    window.setPage(cartoonPage);
                }
            } else {
                cartoonOn = false;
                if (flow.showsText()) {
                    const bool flash = ((now / 270'000'000) & 1) != 0;   // the cursor and flashing characters: about 1.9 changes a second (I: video hardware, not measured)
                    core::renderText(flow.text(), rom, flash, -1, -1, bitmap);
                    video::renderBitmapArgb(bitmap, argb);
                    window.setImage(argb);
                    lastKind = 2;
                } else if (flow.mode() == core::AttractFlow::Mode::Attract) {
                    if (flow.turn().hiresVisible()) {
                        window.setPage(flow.turn().hires());
                        lastKind = 0;
                    } else {
                        std::array<std::uint8_t, 1024> lores{};
                        for (std::size_t i = 0; i < lores.size(); ++i) lores[i] = flow.turn().loresByte(i);
                        video::renderLoresArgb(lores, argb);
                        window.setImage(argb);
                        lastKind = 1;
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    (void)lastKind;
    return false;
}

struct ProgramOptions {
    std::wstring roms;
    bool skipBoot = false;       // start in the attract loop as if "1" and "N" had been typed at the boot menus
    bool directGame = false;     // test aids (--level / --items): start the game at once
    int startLevel = 0, startItems = 0;
};

// The whole program: the boot menus, the attract loop with its prompts, the games, GAME OVER and back (TASK-033-4g).
void programThread(core::GameDriver& drv, const datapack::Pack& pack, app::GameWindow& window, std::atomic<bool>& quit, int seconds, const ProgramOptions& opt, const std::string& monitorRom, const std::string& charRom) {
    const std::int64_t begin = core::nowNanoseconds();
    core::CharacterRom rom;
    if (!rom.load(charRom)) {
        std::puts("cannot read the Apple II character ROM");
        window.close();
        return;
    }
    core::AttractFlow flow;
    const bool started = opt.skipBoot || opt.directGame ? flow.start(pack, monitorRom) : flow.startBoot(pack, monitorRom);
    if (!started) {
        std::puts("cannot read the Apple II monitor ROM");
        window.close();
        return;
    }
    bool first = true;
    for (;;) {
        if (!(first && opt.directGame)) {
            if (!runMenus(flow, rom, pack, window, quit, seconds, begin)) return;
        }
        // Return was pressed: a game starts with the choices of the menus: the number of players (1-5, they play in turn: TASK-033-6), the first level (the rank of the hidden menu), the keys
        int level = flow.startLevel();
        if (first && opt.directGame && opt.startLevel > 0) level = opt.startLevel;
        drv.newGame(flow.players(), level > 0 ? level : 1);
        drv.core().keys() = core::KeyConfig{flow.keyUp(), flow.keyRight(), flow.keyLeft(), flow.keyDown_()};
        drv.core().port().joystickMode = flow.inputDevice() != 0;
        drv.core().input().setSoundOn(flow.soundOn());
        if (first && opt.directGame && opt.startItems >= 1 && opt.startItems <= 248) {
            drv.core().state().zp(0x56) = static_cast<std::uint8_t>(opt.startItems);   // test aid: only a few items left to eat
            drv.rebuildScreen();
        }
        first = false;
        if (!runGame(drv, pack, window, quit, seconds, begin)) return;
        flow.setSoundOn(drv.core().input().soundOn());
        flow.returnFromGame();
    }
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring pack, dump;
    int scale = 3, seconds = 0, startLevel = 0, startItems = 0;
    bool soundTest = false, skipBoot = false;
    std::wstring romsArg;
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
        else if (a == L"--roms" && i + 1 < argc) romsArg = argv[++i];
        else if (a == L"--skip-boot") skipBoot = true;
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
    if (ticks >= 0) {   // headless: run N ticks, write the page
        // Test aids, NOT part of the faithful game: start at a chosen level, and/or with only a few dots left to eat so that a level end (and the cartoon after level 2, 5, 9, ...) is quick to reach.
        if (startLevel >= 1 && startLevel <= 255) drv.core().state().zp(0x55) = static_cast<std::uint8_t>(startLevel);
        if (startItems >= 1 && startItems <= 248) drv.core().state().zp(0x56) = static_cast<std::uint8_t>(startItems);
        if (startLevel > 0 || startItems > 0) drv.rebuildScreen();
        for (long t = 0; t < ticks; ++t) liveTick(drv);
        if (!dump.empty()) {
            std::ofstream out(std::filesystem::path(dump), std::ios::binary);
            for (std::size_t k = 0; k < hires::kPageBytes; ++k) out.put(static_cast<char>(drv.page().byte(k)));
        }
        return 0;
    }
    std::string monitorRom, charRom;
    if (!findRoms(romsArg, monitorRom, charRom)) {
        std::puts("cannot find the Apple II ROM files (the Autostart Monitor 341-0020 and the character ROM 341-0036): put them in a folder `roms` next to the program or name the folder with --roms");
        return 6;
    }
    ProgramOptions opt;
    opt.skipBoot = skipBoot;
    opt.directGame = startLevel > 0 || startItems > 0;
    opt.startLevel = startLevel;
    opt.startItems = startItems;
    app::GameWindowOptions options;
    options.scale = scale;
    app::GameWindow window(options);
    if (!window.ok()) return 4;
    std::atomic<bool> quit{false};
    std::thread worker(programThread, std::ref(drv), std::cref(loaded.pack), std::ref(window), std::ref(quit), seconds, std::cref(opt), std::cref(monitorRom), std::cref(charRom));
    const int code = window.run();
    quit = true;
    worker.join();
    return code;
}
