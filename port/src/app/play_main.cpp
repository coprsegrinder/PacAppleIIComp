// TASK-029h: the first playable program: Pac-Man, the maze, the dots, the score, lives, the blinks and the bonus fruit run on the C++ game core and are drawn with
// the verified hi-res routines into a GameWindow, now WITH THE GHOSTS (M5: all four move, are frightened by the energizers, can be eaten and can catch Pac-Man).
// The level change equals the oracle (TASK-031-3); cartoons 1-3 play after levels 2, 5, 9, 15, ... (031-6b, 031-7); the sounds play (032-5); there is no READY pause and no death animation yet.
//
// Usage: pacman_play --pack <data.pack> [--scale N] [--seconds N] [--ticks N --dump <page.bin>] [--level N] [--items K]
//   --scale N      whole-number window scale (default 3)
//   --seconds N    close the window by itself after N seconds (for tests)
//   --mute         start with the sound off (F9 toggles it)
//   --display M    borderless | windowed (the saved choice otherwise; the default is borderless)
//   --settings F   use this settings file instead of %LOCALAPPDATA%\PacmanAppleIIPort\settings.ini
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
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "audio.h"
#include "pausegate.h"
#include "pacing.h"
#include "precisesleep.h"
#include "settings.h"
#include "../core/attractflow.h"
#include "../core/achievementfacts.h"
#include "../core/achievements.h"
#include "../core/cartoon.h"
#include "../core/textscreen.h"
#include "../core/sound.h"
#include "../core/squarewave.h"
#include "../core/cycleclock.h"
#include "../core/gamedriver.h"
#include "../datapack/datapack.h"
#include "../verify/verify.h"
#include "../video/monochrome.h"
#include "../video/screens.h"
#include <commdlg.h>
#include "achievementbook.h"
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


// ---- the game data without a command line (the release: the program and the player's own disk image are all there is) ----
// The program needs the game's data (the "data pack": every byte of the original that is not code, built from the disk image). It is built ONCE from the player's own disk image, which has to be the
// exact original one (the program refuses any other), and kept in %LOCALAPPDATA%\PacmanAppleIIPort\game.pmpk (the environment variable PACMAN_DATA names another file); the disk image is not needed
// again. Looked for, in this order: the kept data pack, `game.pmpk` next to the program, a `.dsk` next to the program (or in the folder above it, or in the working folder), then the player is asked.
std::wstring keptPackPath() {
    wchar_t env[MAX_PATH * 2] = {};
    if (GetEnvironmentVariableW(L"PACMAN_DATA", env, MAX_PATH * 2) > 0) return env;
    wchar_t base[MAX_PATH * 2] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH * 2) == 0) return L"";
    return std::wstring(base) + L"\\PacmanAppleIIPort\\game.pmpk";
}

void makeParentFolders(const std::wstring& file) {
    for (std::size_t i = 0; i < file.size(); ++i)
        if (file[i] == L'\\' || file[i] == L'/') CreateDirectoryW(file.substr(0, i).c_str(), nullptr);
}

// Builds the data pack from a disk image file and keeps it. Returns false (with the reason) when the file is not the expected disk image.
bool buildPackFromDisk(const std::filesystem::path& disk, datapack::LoadResult& loaded, std::string& reason) {
    std::ifstream f(disk, std::ios::binary);
    if (!f) {
        reason = "the file cannot be read";
        return false;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto built = datapack::buildPack(image, verify::referenceExpected(), datapack::dataManifest());
    if (!built.ok) {
        reason = built.reason;
        return false;
    }
    loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    if (!loaded.ok) {
        reason = loaded.reason;
        return false;
    }
    const std::wstring kept = keptPackPath();
    if (!kept.empty()) {   // (kept for next time; when it cannot be written the program still runs, and asks again next time)
        makeParentFolders(kept);
        const std::wstring tmp = kept + L".tmp";
        {
            std::ofstream out(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(built.bytes.data()), static_cast<std::streamsize>(built.bytes.size()));
        }
        if (!MoveFileExW(tmp.c_str(), kept.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) DeleteFileW(tmp.c_str());
    }
    return true;
}

bool silentLoadPack(const std::filesystem::path& file, datapack::LoadResult& loaded) {
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return false;
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    loaded = datapack::loadPack(bytes, verify::referenceExpected());
    return loaded.ok;
}

std::wstring chooseDiskFile() {
    wchar_t file[MAX_PATH * 2] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Disk images (*.dsk;*.do)\0*.dsk;*.do\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrTitle = L"Pac-Man (Apple II port): choose your disk image of the game";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&ofn) ? std::wstring(file) : std::wstring();
}

// `interactive`: ask the player (message boxes, a file dialog) when nothing is found; false: just fail (the tests).
bool resolveGameData(bool interactive, datapack::LoadResult& loaded) {
    namespace fs = std::filesystem;
    if (silentLoadPack(keptPackPath(), loaded)) return true;
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    const fs::path exeDir = fs::path(buf).parent_path();
    if (silentLoadPack(exeDir / L"game.pmpk", loaded)) return true;
    std::vector<fs::path> folders{exeDir, exeDir.parent_path(), fs::current_path()};
    for (const fs::path& dir : folders) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            const std::wstring ext = entry.path().extension().wstring();
            if (_wcsicmp(ext.c_str(), L".dsk") != 0 && _wcsicmp(ext.c_str(), L".do") != 0) continue;
            std::string reason;
            if (buildPackFromDisk(entry.path(), loaded, reason)) return true;   // a disk image that is not the game's is skipped
        }
    }
    if (!interactive) return false;
    std::wstring message = L"To play, this program needs your own copy of the game's disk image (the 1983 Atari release of Pac-Man for the Apple II, a .dsk file). It is not part of this program.\n\n"
                           L"Put the file next to this program, or choose it now. The program reads it once and keeps what it needs; the disk image is not needed again.";
    for (;;) {
        if (MessageBoxW(nullptr, message.c_str(), L"Pac-Man (Apple II port)", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) return false;
        const std::wstring chosen = chooseDiskFile();
        if (chosen.empty()) continue;
        std::string reason;
        if (buildPackFromDisk(chosen, loaded, reason)) return true;
        message = L"This file is not the disk image the program expects, so it cannot be used.\n\n(" + std::wstring(reason.begin(), reason.end()) + L")\n\nThe image has to be the exact original disk of the game. Choose another file?";
    }
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
        audio.play(core::renderSpeaker(clicks, core::soundFreezeCycles(it.id), 44100));
        while (audio.busy()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
    }
    return 0;
}

// `cartoons`: the live window plays the intermission after the level end; the batch mode (--ticks, no window) goes straight to the next level.
bool gAidEndless = false;    // --aid-endless (tests): every cell Pac-Man reaches has a dot (so that eating can be measured without end)
bool gAidWander = false;     // --aid-wander (tests): Pac-Man wanders by himself (no energizer)
bool gAidEnergizer = false;   // --aid-energizer (tests): Pac-Man wanders by himself and eats an energizer about 4 s into the game, to look at what happens then

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

// TASK-032-5: the game's sounds. Every sound is rendered once (the original's click times through the speaker model, TASK-033-S); a sound the core reports is queued behind what is still playing.
// The audio can be switched off by Ctrl-S (the original's own switch, in the core) or F9 (the window); the game still stands still for the sound (the original's engines
// busy-wait on the cassette output when Ctrl-S has silenced them).
struct SoundPlayer {
    app::AudioOutput audio;
    std::array<core::Pcm, static_cast<std::size_t>(core::SoundId::Count)> pcm;
    core::Pcm gapBetweenDeathSteps;   // 137,052 cycles of silence: measured between the six death steps (entry to entry minus the freeze of the earlier step)
    core::SoundData data;
    bool ready = false;

    void init(const datapack::Pack& pack) {
        if (!data.load(pack) || !audio.open(44100)) return;
        for (std::size_t i = 0; i < pcm.size(); ++i) {
            const auto id = static_cast<core::SoundId>(i);
            if (id == core::SoundId::Count) continue;
            const auto clicks = core::clickTimes(data.list(id));
            if (!clicks.empty()) pcm[i] = core::renderSpeaker(clicks, core::soundFreezeCycles(id), 44100);
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
    void playJingle(int pollExtra) {   // joystick mode: each of the 29 polls of the jingle is the paddle timing, so the notes are further apart (sound.h)
        if (pollExtra == 0) {
            play(core::SoundId::Jingle);
            return;
        }
        if (!ready) return;
        const auto clicks = core::clickTimes(data.list(core::SoundId::Jingle), pollExtra);
        if (!clicks.empty()) audio.play(core::renderSpeaker(clicks, core::soundFreezeCycles(core::SoundId::Jingle, pollExtra), 44100));
    }
    void playNow(core::SoundId id) {   // no gap: the death animation schedules its steps itself
        if (ready && !pcm[static_cast<std::size_t>(id)].samples.empty()) audio.play(pcm[static_cast<std::size_t>(id)]);
    }
    void stop() {
        if (ready) audio.stop();
    }
    void pause() {
        if (ready) audio.pause();
    }
    void resume() {
        if (ready) audio.resume();
    }
};

// One game, from the intro jingle to the end of the GAME OVER wait. Returns true when the game is over (the program goes back to the attract loop), false when the window closed.
// The letter on the boot menu's controller line: the controller's confirm button (X on a PlayStation pad, A on an Xbox pad); 0 = no controller found.
char padLetter(const app::GamepadHub* pad) {
    if (!pad) return 0;
    switch (pad->connected()) {
    case app::PadKind::XInput: return 'A';
    case app::PadKind::DualShock4:
    case app::PadKind::DualSense: return 'X';
    case app::PadKind::None: break;
    }
    return 0;
}

// What a controller does in the screens before the game (the program's own keys: the original has no controller there): its confirm button is the key that goes on at that screen.
void padToFlow(core::AttractFlow& flow, app::GameWindow& window) {
    using Mode = core::AttractFlow::Mode;
    using Press = app::GameWindow::PadPress;
    while (const auto p = window.popPadPress()) {
        const Mode mode = flow.mode();
        if (mode == Mode::Players) {   // the number of players: left / right (or up / down) change it, the confirm button takes it
            if (*p == Press::Left || *p == Press::Up) flow.padPlayersStep(-1);
            else if (*p == Press::Right || *p == Press::Down) flow.padPlayersStep(1);
            else if (*p == Press::Confirm) flow.keyDown(static_cast<std::uint8_t>(0xB0 | flow.padPlayers()));
            continue;
        }
        if (mode == Mode::PressReturn && *p == Press::Back) {   // "<SPACE> OR (the back button): BACK TO DEMO"
            flow.keyDown(core::kKeySpace);
            continue;
        }
        if (*p != Press::Confirm) continue;
        switch (mode) {
        case Mode::BootMode: if (padLetter(window.gamepad()) != 0) flow.keyDown(0xB2); break;   // "2": the controller (the joystick mode of the original: its direction comes from the controller)
        case Mode::BootKeyboard: flow.keyDown(0xCE); break;                                      // N: the normal keys
        case Mode::Attract: flow.keyDown(core::kKeySpace); break;                                // Space: leave the demo for the players prompt
        case Mode::PressReturn: flow.keyDown(core::kKeyReturn); break;
        default: break;
        }
    }
}

bool runGame(core::GameDriver& drv, const datapack::Pack& pack, app::GameWindow& window, std::atomic<bool>& quit, int seconds, std::int64_t begin, core::AchievementTracker* ach = nullptr, app::AchievementBook* book = nullptr) {
    struct MenuAllowed {   // Esc opens the pause menu from the start of a game until it is over (and back in the attract loop it does nothing again)
        app::GameWindow& w;
        explicit MenuAllowed(app::GameWindow& win) : w(win) { w.setMenuAllowed(true); }
        ~MenuAllowed() { w.setMenuAllowed(false); }
    } menuAllowed(window);
    struct TickLog {   // for the real-program checks (--log): how many game ticks ran in how much real time, so that the game's speed can be compared between FPS settings
        app::GameWindow& w;
        std::int64_t firstNs = 0;   // the first tick (the jingle and READY come before it)
        long ticks = 0, pictureChanges = 0;
        std::int64_t costSum = 0, costMin = 1LL << 60, costMax = 0, freezeSum = 0;   // modelled cycles per tick (with the sound stand-stills), and the stand-stills alone
        long playCalls = 0;
        double playTotalMs = 0, playMaxMs = 0;   // time the game thread spent inside the sound calls (they must be quick: the game waits for them)
        std::int64_t secStartNs = 0;   // a line a second in the log: ticks, mean cost, picture changes, the frightened timer
        long secTicks = 0, secChanges = 0, energizers = 0, secPac = 0, secGhost = 0, secAte = 0, secActed = 0;
        std::array<int, 10> lastPos{};   // Pac-Man and the four ghosts' places at the last tick
        std::int64_t secCost = 0;
        std::array<long, 8> hist{};   // ticks by cost: <3.5k, <4k, <5k, <6k, <8k, <12k, <20k, more (cycles)
        std::array<std::int64_t, 8> histCycles{};
        long ateTicks = 0, noAteTicks = 0;
        std::int64_t ateCycles = 0, noAteCycles = 0;
        std::int64_t ordMin = 1LL << 60, ordMax = 0;   // the time of ticks without a long sequence or a sound stand-still: how even the pace is
        long slow = 0;                                                               // ticks that cost more than twice the typical tick   // ticks after which the picture differed from the one before
        std::vector<std::uint8_t> last;
        void note(const hires::Page& p) {
            const auto b = p.bytes();
            if (last.size() != b.size() || !std::equal(b.begin(), b.end(), last.begin())) {
                ++pictureChanges;
                ++secChanges;
                last.assign(b.begin(), b.end());
            }
        }
        ~TickLog() {
            const double s = firstNs == 0 ? 0.0 : static_cast<double>(core::nowNanoseconds() - firstNs) / 1e9;
            char buf[480];
            std::snprintf(buf, sizeof(buf), "game: ticks=%ld seconds=%.3f picture_changes=%ld sound_calls=%ld sound_call_total_ms=%.2f sound_call_max_ms=%.2f tick_cycles_min=%lld mean=%.0f max=%lld freeze_cycles_total=%lld slow_ticks=%ld ordinary_min=%lld ordinary_max=%lld", ticks, s, pictureChanges, playCalls, playTotalMs, playMaxMs, static_cast<long long>(costMin), ticks ? static_cast<double>(costSum) / static_cast<double>(ticks) : 0.0, static_cast<long long>(costMax), static_cast<long long>(freezeSum), slow, static_cast<long long>(ordMin), static_cast<long long>(ordMax));
            w.logLine(buf);
            static const char* const names[8] = {"<3.5k", "<4k", "<5k", "<6k", "<8k", "<12k", "<20k", ">=20k"};
            std::string h = "cost histogram (ticks / share of the game's time):";
            std::int64_t total = 0;
            for (auto c : histCycles) total += c;
            for (std::size_t i = 0; i < 8; ++i) {
                char b2[80];
                std::snprintf(b2, sizeof(b2), " %s %ld/%.0f%%", names[i], hist[i], total ? 100.0 * static_cast<double>(histCycles[i]) / static_cast<double>(total) : 0.0);
                h += b2;
            }
            w.logLine(h);
            char b3[200];
            std::snprintf(b3, sizeof(b3), "dot ticks: %ld, mean cost %.0f; other ticks: %ld, mean cost %.0f", ateTicks, ateTicks ? static_cast<double>(ateCycles) / static_cast<double>(ateTicks) : 0.0, noAteTicks, noAteTicks ? static_cast<double>(noAteCycles) / static_cast<double>(noAteTicks) : 0.0);
            w.logLine(b3);
        }
        void second(std::int64_t now, bool frightened) {
            if (secStartNs == 0) secStartNs = now;
            if (now - secStartNs < 1'000'000'000) return;
            char buf[200];
            std::snprintf(buf, sizeof(buf), "second: ticks=%ld mean_cost=%.0f picture_changes=%ld pacman_steps=%ld ghost_moves=%ld dots_eaten=%ld acted=%ld frightened=%d energizers_so_far=%ld", secTicks, secTicks ? static_cast<double>(secCost) / static_cast<double>(secTicks) : 0.0, secChanges, secPac, secGhost, secAte, secActed, frightened ? 1 : 0, energizers);
            w.logLine(buf);
            secStartNs = now;
            secTicks = secChanges = secPac = secGhost = secAte = secActed = 0;
            secCost = 0;
        }
    } tickLog{window};
    app::SmoothPacer pacer;
    std::int64_t tickClockNs = core::nowNanoseconds();   // when the game made its last tick, on its own clock (for the smooth motion)
    core::StepTables stepTables;   // Pac-Man's step countdown tables: the modern pacing keeps his real-time step the same whatever the ghosts do
    core::loadStepTables(pack, stepTables);
    SoundPlayer sounds;
    sounds.init(pack);
    core::CartoonPlayer cartoon;
    int achCartoon = 0;   // the intermission that plays (for the achievements: watching it to its end)
    hires::Page cartoonPage;
    core::CycleClock clock;
    clock.start(core::nowNanoseconds());
    std::int64_t budget = 0;
    app::PauseGate gate;   // the pause menu: nothing runs, nothing sounds, no time is earned while it is open
    gate.onPause = [&] { sounds.pause(); };
    gate.onResume = [&] { sounds.resume(); };
    drv.setPlayDeath(true);
    bool startWait = true, readyShown = false;
    std::size_t seqNext = 0;
    std::int64_t seqTime = 0;
    bool seqActive = false;
    window.setPage(drv.page());
    // the intro jingle starts a game (before the first tick, docs/SOUND.md section 6); the game stands still for its 5 s
    const int jingleExtra = drv.core().jinglePollExtra();   // (0 with the keyboard)
    if (drv.core().input().soundOn() && !window.muted()) sounds.playJingle(jingleExtra);
    budget = -(core::soundFreezeCycles(core::SoundId::Jingle, jingleExtra) + core::kReadyPauseCycles + drv.core().lifeStartCycles());   // the jingle, then the READY wait (033-2)
    while (!quit.load() && window.ok()) {
        const std::int64_t now = core::nowNanoseconds();
        if (seconds > 0 && now - begin > static_cast<std::int64_t>(seconds) * 1'000'000'000) {
            window.close();
            return false;
        }
        if (gate.update(window.menuOpen(), now, clock)) {
            app::preciseSleepMs(5);
            continue;
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
            app::preciseSleepMs(2);
            continue;
        }
        if (cartoon.active() && budget < 0) {   // the level-end pause before the intermission (the original spends kLevelEndCycles there, measured 033-1b)
            app::preciseSleepMs(2);
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
                if (ach && book) {
                    ach->setUnlocked(book->mask());
                    for (const int i : ach->onCartoonDone(achCartoon)) book->unlock(i);
                }
                drv.nextLevel();
                window.setPage(drv.page());
            }
            app::preciseSleepMs(2);
            continue;
        }
        while (const auto key = window.popKey()) drv.core().input().pushKey(*key);
        if (const app::GamepadHub* pad = window.gamepad()) {   // a controller gives its direction to the original's own input poll: as the joystick when the first menu chose it, else as the four switches of the game port
            const app::PadState padState = pad->state();
            const auto dir = app::padDirection(padState);
            core::PortInput& port = drv.core().port();
            const app::PaddlePosition paddles = app::padPaddles(padState);   // joystick mode: the original's own paddle routines ($701E, $7041) read these positions
            port.paddleX = paddles.x;
            port.paddleY = paddles.y;
            static constexpr std::uint8_t mask[4] = {1, 2, 4, 8};   // by direction code 0 down, 1 up, 2 right, 3 left: the switches of the table at $700E
            port.switches = dir ? mask[*dir] : 0;
        }
        bool changed = false;
        while (budget >= core::kTypicalTickCycles) {
            const int ghostEffect = static_cast<int>(core::StepTimer::reloadValue(drv.core().state(), stepTables)) - static_cast<int>(core::StepTimer::calmReloadValue(drv.core().state(), stepTables));   // (what the ghosts add to his countdown, as the reload in this tick sees it)
            const int achLevel = static_cast<int>(drv.core().state().level()), achPlayer = drv.core().player();   // (what the tick is played in: a level change or a change of player happens inside it)
            const std::uint32_t achScoreBefore = drv.core().state().score();
            const core::TickEvents ev = liveTick(drv, &cartoon, &pack);
            if (drv.core().state().demo == 0) {   // controller vibration for the game's events (not in the attract game)
                if (ev.gameOver) window.vibrate(app::RumbleEvent::GameOver);
                else if (ev.caught) window.vibrate(app::RumbleEvent::Caught);
                else if (ev.ghostEaten[0] || ev.ghostEaten[1] || ev.ghostEaten[2] || ev.ghostEaten[3]) window.vibrate(app::RumbleEvent::GhostEaten);
                else if (ev.levelComplete) window.vibrate(app::RumbleEvent::LevelComplete);
                else if (ev.energizer) window.vibrate(app::RumbleEvent::PowerPill);
            }
            if (ev.levelComplete && cartoon.active()) achCartoon = core::cartoonAfterLevel(static_cast<unsigned>(achLevel));
            if (++tickLog.ticks == 1) tickLog.firstNs = core::nowNanoseconds();
            std::int64_t soundFreeze = 0;
            const bool audible = drv.core().input().soundOn() && !window.muted();
            const bool modern = window.fpsForGameThread() != app::FpsMode::Native;   // the modern FPS modes keep the original's pace but not its hitches (pacing.h); Native is the original's timing exactly
            const bool dying = !drv.sequence().empty();   // the animation schedules the six death sounds itself
            for (const core::SoundId id : ev.sounds) {
                const bool deathStep = id >= core::SoundId::Death1 && id <= core::SoundId::Death6;
                if (dying && deathStep) continue;
                if (audible) {
                    const std::int64_t p0 = core::nowNanoseconds();
                    sounds.play(id);
                    const double ms = static_cast<double>(core::nowNanoseconds() - p0) / 1e6;
                    ++tickLog.playCalls;
                    tickLog.playTotalMs += ms;
                    tickLog.playMaxMs = (std::max)(tickLog.playMaxMs, ms);
                }
                soundFreeze += app::soundStandStill(modern, id);
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
            if (ev.acted) pacer.startStep(drv.core().state().pacStepCounter(), ghostEffect);   // (Pac-Man's countdown was loaded in this tick: his step is paced from it)
            const std::int64_t cost = app::tickCost(modern, pacer, drv.tickCycles(), drv.extraCycles(), soundFreeze);
            if (ach && book && drv.core().state().demo == 0) {   // the achievements: facts about this tick (the game's state is only read)
                const core::TickFacts f = core::tickFacts(drv, ev, achLevel, achPlayer, achScoreBefore, cost);
                ach->setUnlocked(book->mask());   // (the menu may have reset the progress)
                for (const int i : ach->onTick(f)) book->unlock(i);
            }
            {
                tickLog.costSum += cost;
                tickLog.costMin = (std::min)(tickLog.costMin, cost);
                tickLog.costMax = (std::max)(tickLog.costMax, cost);
                tickLog.freezeSum += soundFreeze;
                if (soundFreeze == 0 && drv.extraCycles() == 0) {
                    tickLog.ordMin = (std::min)(tickLog.ordMin, cost);
                    tickLog.ordMax = (std::max)(tickLog.ordMax, cost);
                }
                tickLog.slow += cost > 2 * core::kTypicalTickCycles;
                {
                    const std::size_t bin = cost < 3500 ? 0 : cost < 4000 ? 1 : cost < 5000 ? 2 : cost < 6000 ? 3 : cost < 8000 ? 4 : cost < 12000 ? 5 : cost < 20000 ? 6 : 7;
                    ++tickLog.hist[bin];
                    tickLog.histCycles[bin] += cost;
                    if (ev.ate && !ev.energizer) {
                        ++tickLog.ateTicks;
                        tickLog.ateCycles += cost;
                    } else {
                        ++tickLog.noAteTicks;
                        tickLog.noAteCycles += cost;
                    }
                }
            }
            budget -= cost;   // the tick's modelled cost (033-1c), or its smoothed pace in the modern modes (pacing.h)
            changed = true;
            tickLog.note(drv.page());
            ++tickLog.secTicks;
            tickLog.secCost += cost;
            tickLog.energizers += ev.energizer;
            tickLog.secAte += ev.ate;
            tickLog.secActed += ev.acted;
            {
                const core::GameState& st = drv.core().state();
                std::array<int, 10> pos{st.pacX(), st.pacY(), st.ghosts[0].x(), st.ghosts[0].y(), st.ghosts[1].x(), st.ghosts[1].y(), st.ghosts[2].x(), st.ghosts[2].y(), st.ghosts[3].x(), st.ghosts[3].y()};
                tickLog.secPac += pos[0] != tickLog.lastPos[0] || pos[1] != tickLog.lastPos[1];
                for (int g = 0; g < 4; ++g) tickLog.secGhost += pos[2 + 2 * g] != tickLog.lastPos[2 + 2 * g] || pos[3 + 2 * g] != tickLog.lastPos[3 + 2 * g];
                tickLog.lastPos = pos;
            }
            tickLog.second(core::nowNanoseconds(), drv.core().ghostShared().e4 != 0);
            if ((gAidEnergizer || gAidWander) && tickLog.ticks % 150 == 0) drv.core().state().requestedDirection() = static_cast<std::uint8_t>((tickLog.ticks / 150) % 4);   // test aid: Pac-Man wanders by himself
            if (gAidEnergizer && tickLog.ticks == 900) for (auto& c : drv.core().state().map) if (c == 1) c = 2;   // test aid: the next item he meets is an energizer
            if (gAidEndless) {
                core::GameState& st = drv.core().state();
                for (auto& c : st.map) if (c == 0) c = 1;
                st.zp(0x56) = 200;
            }
            if (gAidEnergizer && ev.energizer) for (auto& c : drv.core().state().map) if (c == 2) c = 1;
            {   // every tick is a picture the window may show (the window takes the newest one at its next frame); the smooth motion of the modern FPS modes also gets where the actors moved to
                std::array<app::GameWindow::FrameMove, 5> mv{};
                std::size_t n = 0;
                for (const auto& m : drv.lastMoves())
                    if (n < mv.size()) mv[n++] = app::GameWindow::FrameMove{m.actor, m.x, m.y, m.image, m.solo, true};
                window.setFrame(drv.page(), drv.backgroundPage(), std::span<const app::GameWindow::FrameMove>(mv.data(), n), drv.interpolatable(), tickClockNs);
                tickClockNs += cost * 1'000'000'000LL / core::kAppleCyclesPerSecond;   // (the game's own clock: smooth, and never far from the real one)
                const std::int64_t realNow = core::nowNanoseconds();
                if (tickClockNs > realNow + 40'000'000 || tickClockNs < realNow - 40'000'000) tickClockNs = realNow;
            }
            if (budget < -core::kDeathSequenceCycles * 2) budget = 0;
        }
        (void)changed;   // (every tick has already been published, with where its actors moved: window.setFrame)
        app::preciseSleepMs(2);
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
    std::vector<std::uint8_t> dots(static_cast<std::size_t>(video::kDotsW) * video::kDotsH);
    std::int64_t lastShown = 0, lastCartoonElapsed = 0;
    bool cartoonOn = false;
    int lastKind = -1;
    app::PauseGate gate;   // the pause menu stops the screens as well (the boot menus, the attract loop, the prompts)
    while (!quit.load() && window.ok()) {
        const std::int64_t now = core::nowNanoseconds();
        if (seconds > 0 && now - begin > static_cast<std::int64_t>(seconds) * 1'000'000'000) {
            window.close();
            return false;
        }
        if (gate.update(window.menuOpen(), now, clock)) {
            app::preciseSleepMs(5);
            continue;
        }
        flow.setBootMenu(true, padLetter(window.gamepad()));   // the first menu's words follow the controller that is plugged in
        padToFlow(flow, window);
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
                    video::bitmapDots(bitmap, dots);   // the dot positions for Native Black & White (text: two positions per dot)
                    window.setImage(argb, dots);
                    lastKind = 2;
                } else if (flow.mode() == core::AttractFlow::Mode::Attract) {
                    if (flow.turn().hiresVisible()) {
                        window.setPage(flow.turn().hires());
                        lastKind = 0;
                    } else {
                        std::array<std::uint8_t, 1024> lores{};
                        for (std::size_t i = 0; i < lores.size(); ++i) lores[i] = flow.turn().loresByte(i);
                        video::renderLoresArgb(lores, argb);
                        video::loresDots(lores, dots);   // lo-res colours as the 4-position patterns of their bits
                        window.setImage(argb, dots);
                        lastKind = 1;
                    }
                }
            }
        }
        app::preciseSleepMs(1);
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
    core::AchievementTracker tracker;   // lives as long as the program: the ghosts of a session are counted across games
    core::AttractFlow flow;
    flow.setBootMenu(true, padLetter(window.gamepad()));   // (before the first screen is made)
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
        tracker.newGame(flow.players(), level > 0 ? level : 1);
        if (first && opt.directGame && opt.startItems >= 1 && opt.startItems <= 248) {
            drv.core().state().zp(0x56) = static_cast<std::uint8_t>(opt.startItems);   // test aid: only a few items left to eat
            drv.rebuildScreen();
        }
        first = false;
        if (!runGame(drv, pack, window, quit, seconds, begin, &tracker, window.achievements())) return;
        flow.setSoundOn(drv.core().input().soundOn());
        flow.returnFromGame(drv.attractMemory(flow.memory()));
    }
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring pack, dump;
    int scale = 3, seconds = 0, startLevel = 0, startItems = 0;
    bool mute = false;
    bool soundTest = false, skipBoot = false;
    int toastAid = -1;
    std::wstring romsArg, settingsPath = app::SettingsStore::defaultPath(), displayArg;
    bool scaleGiven = false;
    std::wstring logArg;
    long ticks = -1;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--pack" && i + 1 < argc) pack = argv[++i];
        else if (a == L"--scale" && i + 1 < argc) {
            scale = _wtoi(argv[++i]);
            scaleGiven = true;
        }
        else if (a == L"--seconds" && i + 1 < argc) seconds = _wtoi(argv[++i]);
        else if (a == L"--ticks" && i + 1 < argc) ticks = _wtol(argv[++i]);
        else if (a == L"--dump" && i + 1 < argc) dump = argv[++i];
        else if (a == L"--sound-test") soundTest = true;
        else if (a == L"--level" && i + 1 < argc) startLevel = _wtoi(argv[++i]);
        else if (a == L"--items" && i + 1 < argc) startItems = _wtoi(argv[++i]);
        else if (a == L"--roms" && i + 1 < argc) romsArg = argv[++i];
        else if (a == L"--settings" && i + 1 < argc) settingsPath = argv[++i];   // another settings file (tests)
        else if (a == L"--log" && i + 1 < argc) logArg = argv[++i];   // a text file that gets a line for every display mode change (for tests and for reports)
        else if (a == L"--display" && i + 1 < argc) displayArg = argv[++i];     // fullscreen | borderless | windowed (written to the settings like a change in the menu)
        else if (a == L"--skip-boot") skipBoot = true;
        else if (a == L"--aid-energizer") gAidEnergizer = true;
        else if (a == L"--aid-toast" && i + 1 < argc) toastAid = _wtoi(argv[++i]);   // test aid: an achievement is unlocked at the start (to look at its pop-up; use PACMAN_ACHIEVEMENTS to keep your own progress out of it)
        else if (a == L"--aid-wander") gAidWander = true;
        else if (a == L"--aid-endless") { gAidEndless = true; gAidWander = true; }
        else if (a == L"--mute") mute = true;   // no sound (the same as F9 at the start); for running the real program in tests
        else return 64;
    }
    datapack::LoadResult loaded;
    if (pack.empty()) {
        if (!resolveGameData(ticks < 0 && !soundTest, loaded)) return 5;   // no game data and no disk image (the player was asked, or this is a test run)
    } else if (!loadPackFile(pack, loaded)) {
        return 3;
    }
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
        std::puts("cannot find the Apple II ROM files (the Autostart Monitor 341-0020 and the character ROM 341-0036): put them in a folder `Apple II roms` next to the program or name the folder with --roms");
        if (ticks < 0) MessageBoxW(nullptr, L"The two Apple II ROM files the program shows its text with are missing.\n\nThey belong in the folder \"Apple II roms\" next to this program (the release contains them).", L"Pac-Man (Apple II port)", MB_OK | MB_ICONERROR);
        return 6;
    }
    ProgramOptions opt;
    opt.skipBoot = skipBoot;
    opt.directGame = startLevel > 0 || startItems > 0;
    opt.startLevel = startLevel;
    opt.startItems = startItems;
    app::GameWindowOptions options;
    options.scale = scale;
    options.muted = mute;
    options.menuAllowed = false;   // the pause menu belongs to a running game (runGame allows it)
    app::SettingsStore settingsStore(settingsPath);   // outlives the window, which writes its changes into it
    app::Settings settings = settingsStore.load();
    if (scaleGiven) settings.hasWindowRect = false;   // an explicit --scale wins over the window size kept in the settings
    if (displayArg == L"fullscreen" || displayArg == L"borderless") settings.displayMode = app::DisplayMode::Borderless;
    else if (displayArg == L"windowed") settings.displayMode = app::DisplayMode::Windowed;
    app::GamepadHub pad;   // (outlives the window)
    app::AchievementBook achievementBook(app::AchievementBook::defaultPath());
    achievementBook.load();
    app::GameWindow window(options);
    if (!window.ok()) return 4;
    pad.start();
    window.setGamepad(&pad);
    window.setAchievements(&achievementBook);
    if (toastAid >= 0) achievementBook.unlock(toastAid);
    if (!logArg.empty()) window.setLogFile(logArg);
    window.attachSettings(&settingsStore, settings);
    core::CharacterRom menuFont;   // the pause menu is drawn with the Apple II character ROM
    if (menuFont.load(charRom)) window.setMenuFont(menuFont);
    std::atomic<bool> quit{false};
    std::thread worker(programThread, std::ref(drv), std::cref(loaded.pack), std::ref(window), std::ref(quit), seconds, std::cref(opt), std::cref(monitorRom), std::cref(charRom));
    const int code = window.run();
    quit = true;
    worker.join();
    return code;
}
