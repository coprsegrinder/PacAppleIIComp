// The game window (TASK-028k, design in docs/WINDOW_DESIGN.md): shows the newest hi-res page at an integer scale (GDI), switches to full screen,
// changes scale at run time, turns Windows key events into Apple II keyboard codes, and handles the port's own hotkeys.
// Threading: setPage() and popKey() may be called from any thread (the emulation thread); everything else belongs to the thread that created the window.
#pragma once

#include <windows.h>

#include <cstdint>
#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../hires/hires.h"
#include "../video/monochrome.h"
#include "background.h"
#include "d3drenderer.h"
#include "display_layout.h"
#include "background_art.h"
#include "colourmode.h"
#include "crtfilter.h"
#include "achievementbook.h"
#include "fpsmode.h"
#include "gamepad.h"
#include "framed_layout.h"
#include "motion.h"
#include "glow.h"
#include "pausemenu.h"
#include "resolutions.h"
#include "settings.h"

namespace pacman::app {

struct GameWindowOptions {
    int scale = 3;         // whole-number scale 1..8 for the windowed mode
    bool visible = true;   // false: create hidden (used by tests)
    bool muted = false;    // start with the sound off (--mute: for tests that run the real program)
    bool menuAllowed = true;   // the pause menu can be opened; the program starts with false and allows it while a game is running
};

class GameWindow {
public:
    explicit GameWindow(const GameWindowOptions& options);
    ~GameWindow();
    GameWindow(const GameWindow&) = delete;
    GameWindow& operator=(const GameWindow&) = delete;

    bool ok() const noexcept { return hwnd_ != nullptr; }
    HWND hwnd() const noexcept { return hwnd_; }

    // --- any thread ---
    void setPage(const hires::Page& page);            // copies the 8 KB page under a lock; the window repaints at the next refresh tick
    void setImage(std::span<const std::uint32_t> argb);   // (a picture without dot data: in Native Black & White a dot with any light is lit)
    void setImage(std::span<const std::uint32_t> argb, std::span<const std::uint8_t> dots560x192);   // with the dot positions of the screen (src/video/monochrome.h) for Native Black & White   // 280 x 192 ARGB pixels (the lo-res page, the text page): shown instead of the hi-res page until the next setPage
    // The game's tick as a picture for the smooth motion (motion.h): the page, the page without the images of the actors, where the actors moved to in this tick, whether the page can be
    // taken apart like that (false: it was rebuilt or drawn by a sequence), and when the game made the tick (its own clock, nanoseconds on the performance counter). Shown like setPage().
    struct FrameMove {
        int actor, x, y;
        hires::Page image;   // the actor's image alone (which pixels are its own: the sprite is these and nothing else; empty = the pixels that differ from the background)
        hires::Page solo;    // the background with this actor's image alone on it (hasSolo): the sprite takes its fringe from it, so another actor standing near is not carried along
        bool hasSolo = false;
    };
    void setFrame(const hires::Page& page, const hires::Page& background, std::span<const FrameMove> moves, bool interpolatable, std::int64_t tNs);
    // Controllers (gamepad.h). The window reads the hub on a timer: the menu button opens and closes the pause menu, the D-pad / stick, confirm and back drive the menu while it is open, and
    // before the game (the boot menus, the attract loop, the prompts) the presses wait for the program in popPadPress(). The last device that gave input (keyboard or a controller) is
    // inputDevice(): 0 keyboard, 1 Xbox, 2 PlayStation, for the controls page.
    enum class PadPress : std::uint8_t { Confirm, Back, Up, Down, Left, Right };
    void setGamepad(GamepadHub* hub) noexcept { pad_ = hub; }
    GamepadHub* gamepad() const noexcept { return pad_; }
    // The achievements (core/achievements.h): the book holds the progress and the pop-ups; the window draws a pop-up over the picture and the menu's Achievements page reads the progress.
    void setAchievements(AchievementBook* book) noexcept { book_ = book; }
    AchievementBook* achievements() const noexcept { return book_; }
    std::optional<PadPress> popPadPress();
    int inputDevice() const noexcept { return inputDevice_.load(); }
    void padTickForTests() { pollPad(); }   // tests: one reading of the controller
    std::optional<std::uint8_t> popKey();             // next Apple II key code (7-bit ASCII with bit 7 set) or nothing

    // --- window thread ---
    int scale() const noexcept { return scale_; }
    bool fullscreen() const noexcept { return fullscreen_; }
    bool muted() const noexcept { return muted_; }
    int effectiveScale() const noexcept;               // the scale actually drawn (largest whole number that fits when in full screen)
    bool setScale(int scale);                          // 1..8; applies at once in windowed mode, remembered in full screen
    // Windowed mode (TASK-034-2): the window can be resized freely; the picture keeps its 280:192 shape, whole-number scale, centred, bars around it (display_layout.h).
    // setBorders(false) removes the title bar and frame; the window is still resizable at its edges and can be moved by dragging the picture. The picture does not move or change size.
    // The pause menu (TASK-034-3): Esc opens it (the original Esc pause is replaced), the keyboard and the mouse drive it, the game threads stand still while menuOpen() is true.
    bool menuOpen() const noexcept { return menuOpen_.load(); }   // any thread
    void setMenuAllowed(bool on) noexcept { menuAllowed_ = on; }   // any thread: false = Esc does nothing (boot menus, attract loop, prompts); the program allows the menu from the start of a game to its end
    void setMenuFont(const core::CharacterRom& rom) { font_ = rom; artKey_.width = 0; }   // before the window is used: the Apple II character ROM the menu is drawn with
    bool borders() const noexcept { return borders_; }
    void setBorders(bool on);
    // Display mode (TASK-034-4): Windowed, or Borderless Fullscreen (a borderless window over the monitor at its own resolution).
    // Neither touches the game or its clock.
    DisplayMode displayMode() const noexcept { return mode_; }
    void setDisplayMode(DisplayMode m);
    // Resolution (TASK-034-5): in Windowed mode the size of the picture area (the display resolution the game is shown at); Borderless Fullscreen always uses the desktop's own.
    // A size the display cannot show (its window would be larger than the monitor's work area) is refused and nothing changes.
    struct RenderInfo {
        int displayW = 0, displayH = 0;   // what the picture is shown at: the client area = the Direct3D back buffer
        int gameW = kGameW, gameH = kGameH;   // what the game draws: always the Apple II's 280 x 192
        Viewport viewport;                // where the game picture sits inside the display
    };
    RenderInfo renderInfo() const;
    bool resolutionFits(int w, int h) const;      // windowed: a window with this picture area fits the monitor's work area
    bool setResolution(int w, int h);             // false: Borderless Fullscreen is active (native resolution), or the size does not fit
    void cycleResolution(int dir);                // the next / previous listed size that fits; from a custom size the first / last that fits
    void logLine(const std::string& text);   // any thread: appends a line to the log file (when there is one)
    void setLogFile(const std::wstring& path) { logPath_ = path; }   // appends a line for every display mode change (the mode and the picture area; for tests and reports)
    // Settings: starts with these (display mode, frame, window position and size), and from then on writes every change of them to the store (atomic file); the window's last
    // windowed place is written when the window closes.
    void attachSettings(SettingsStore* store, const Settings& initial);
    const Settings& settings();                         // the settings as they are now
    void setFullscreen(bool on);                       // never touches the game: only the window style, size and the swap chain change
    // The Apple II desktop's window can be moved and resized with the mouse: drag its title bar, its frame or the picture (with the pause menu closed); drag the grow box in its lower right
    // corner to resize it (whole-number scales); the zoom box in its title bar fits it to the largest size and centres it.
    const FrameState& desktopState() const noexcept { return desk_; }
    void setDesktopState(const FrameState& st);   // programmatic (settings, tests)
    // Colour mode (TASK-034-8): Original Colour, or Native Black & White (the picture as a monochrome Apple II monitor showed it: green phosphor, 560 dot positions per line with the half-dot shift,
    // a slight softness). Only the game picture changes; the menu overlay follows (in the phosphor's green).
    ColourMode colourMode() const noexcept { return colour_; }
    void setColourMode(ColourMode m);
    void cycleColourMode(int dir) { setColourMode(nextColourMode(colour_, dir)); }
    // CRT filter (TASK-034-9, crtfilter.h): nine independent options with several degrees each, all off by default (then the picture is exactly the program's own). Applied to the whole
    // 280 x 192 picture including the pause menu (so a change can be seen while it is made); never touches the game, the clock or the sound.
    const CrtFilter& crt() const noexcept { return crt_; }
    void setCrt(const CrtFilter& f);
    void stepCrtOption(CrtOption o, int dir);
    void stepCrtPreset(int dir) { setCrt(crtPresetFilter(crtNextPreset(crtPresetOf(crt_), dir))); }
    // FPS (TASK-034-11): how often the window shows the game's pictures: Native (the Apple II's 60.05 pictures a second), 30, 60, 120, 144, or Unlocked (a picture whenever the game has a new
    // one). The game, its clock and the sound run on their own thread and never depend on it. Only effective while run() is the message loop.
    // Vibration (vibration.h): the level is kept in the settings; vibrate() starts the pattern of a game event on the controller in use (any thread; nothing when Off or no controller).
    VibrationLevel vibration() const noexcept { return vibrationAtomic_.load(); }
    void setVibration(VibrationLevel l);   // (also gives a short test pulse)
    void cycleVibration(int dir) { setVibration(nextVibration(vibrationAtomic_.load(), dir)); }
    void vibrate(RumbleEvent e);
    FpsMode fps() const noexcept { return fps_; }
    FpsMode fpsForGameThread() const noexcept { return fpsAtomic_.load(); }   // any thread
    void setFps(FpsMode m);
    void cycleFps(int dir) { setFps(nextFpsMode(fps_, dir)); }
    struct FrameStats {   // the pictures shown by the paced loop since resetFrameStats()
        unsigned long long frames = 0;
        double meanMs = 0, maxMs = 0, deviationMs = 0;   // between two pictures (gaps over 250 ms, a still picture, are not counted)
        double drawMs = 0, presentMs = 0;                // mean time inside the drawing calls and inside Present
    };
    FrameStats frameStats() const;
    void resetFrameStats();
    unsigned long long framesShown() const noexcept { return frames_.load(); }
    unsigned long long distinctFrames() const noexcept { return distinct_.load(); }   // pictures shown that differed from the one before (what the eye sees change)
    std::size_t smoothSpriteCount() const noexcept { return spriteDraws_.size(); }
    bool smoothMotion() const noexcept { return smoothActive_; }                      // the last picture was drawn with the actors at in-between places
    void setInterpolationClockForTests(std::int64_t ns) noexcept { testClockNs_ = ns; }   // tests: the time the pictures are made for (0 = the real time)
    void setBackground(Background b);                  // what is drawn around the picture (window thread); written to the settings
    void cycleBackground(int dir);
    Background background() const noexcept { return background_; }
    // Port hotkeys; returns true if the key was one of them (and so must not reach the game). ctrl/alt: modifier state.
    bool handleHotkey(unsigned vk, bool ctrl, bool alt);
    int run();                                         // message loop until the window closes; returns the exit code
    void close();
    unsigned refreshHz() const noexcept { return refreshHz_; }
    bool usesDirect3d() const noexcept { return d3d_ != nullptr; }   // false: the GDI fallback draws (no usable Direct3D 11 device, or PACMAN_GDI=1)
    bool captureFrame(std::vector<std::uint32_t>& argb, int& w, int& h);   // tests: draws the current picture with the active renderer and returns the client area (Direct3D only)

private:
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void pushKey(std::uint8_t code);
    void pollPad();                       // the timer: the controller's edges and repeats (window thread)
    void applyWindowedSize();
    LONG windowedStyle() const;                        // the style of the windowed mode for the current borders_ setting (without WS_VISIBLE)
    RECT outerRect(int clientX, int clientY, int clientW, int clientH) const;   // the window rectangle that has this client area, in the current windowed style
    LRESULT hitTest(LPARAM lp);                        // borderless windowed mode: resize edges, and the picture moves the window
    void applyFullscreenWindow(bool on);               // the borderless window over the monitor (what Borderless Fullscreen is, and the base of Fullscreen)
    void rememberWindowRect();                         // windowed mode: where the picture area is now (kept for the settings file)
    void saveSettings();
    void fitToMonitor();                               // full screen: cover the monitor the window is on (again after a display change)
    UINT dpi() const;
    void paint();
    void menuAction(PauseMenu::Action a);
    void menuChanged();                                // repaint, and tell the other threads
    bool menuKey(unsigned vk);                         // true if the menu used the key
    bool pictureCoords(LPARAM lp, int& px, int& py) const;   // client position -> picture pixel; false outside the picture
    struct Snap {
        std::uint64_t seq = 0;
        std::int64_t tNs = 0;
        hires::Page page, bg;
        std::vector<FrameMove> moves;
        bool interp = false;
    };
    struct SpriteImg {
        std::uint64_t seq = 0;
        bool valid = false;
        int w = 0, h = 0;
        float relX = 0, relY = 0;   // where its top left is, from the place of the step (game pixels)
        std::vector<std::uint32_t> argb;
        std::vector<std::uint8_t> onWall;           // 1 = a pixel of the background (a wall, a dot) that the sprite recolours (the Apple II's colour fringe): drawn only where the background has something at the sprite's place of the moment
        std::vector<std::uint32_t> drawArgb;        // argb for the moment (the recoloured pixels that have nothing under them removed)
    };
    struct Conv {   // a published picture and its background as colour (or monochrome) pictures
        std::uint64_t seq = 0;
        bool mono = false, valid = false;
        std::vector<std::uint32_t> comp, bg;
    };
    bool buildSmooth(std::int64_t nowNs);              // the background and the sprites of the smooth motion for this time; false: draw the picture as it is
    const Conv& converted(const Snap& s, bool mono);
    void extractSprite(int actor, const Snap& s, const FrameMove& m, bool mono);
    bool renderFrame();                                // Direct3D: draws the newest picture and presents it
    void pacedFrame(bool everySlot);                   // one step of the paced loop
    void currentPixels(std::uint32_t* out, double glowDtSeconds);   // the 280 x 192 picture to show (page or image), marks it as painted

    HWND hwnd_ = nullptr;
    int scale_ = 3;
    bool fullscreen_ = false;                 // the window covers its monitor (Borderless Fullscreen and Fullscreen)
    DisplayMode mode_ = DisplayMode::Windowed;
    DisplayMode wantedMode_ = DisplayMode::Windowed;          // what is written to the settings (what the user chose; see setDisplayMode)
    std::wstring logPath_;
    void logMode(const char* what);
    SettingsStore* store_ = nullptr;
    Settings settings_;
    bool borders_ = true;               // windowed mode only: title bar and frame
    bool scalePending_ = false;         // setScale() was called in full screen: applies when leaving it
    Background background_ = Background::Black;
    ColourMode colour_ = ColourMode::Original;
    CrtFilter crt_, crtDraw_;
    FpsMode fps_ = FpsMode::Native;
    std::atomic<VibrationLevel> vibrationAtomic_{VibrationLevel::Medium};
    std::atomic<FpsMode> fpsAtomic_{FpsMode::Native};   // the same as fps_, for the game's thread (it chooses the timing of the ticks by it, pacing.h)
    bool paced_ = false;                      // run() is the message loop: pictures are paced by fps_ instead of the window timer
    bool inModalLoop_ = false;                // the user is dragging or sizing the window (Windows runs its own loop then)
    std::atomic<unsigned long long> frames_{0};
    std::int64_t lastFrameNs_ = 0;
    std::deque<std::shared_ptr<const Snap>> shared_;      // guarded by mutex_: the pictures the game published (newest last)
    std::uint64_t nextSeq_ = 1;
    std::deque<std::shared_ptr<const Snap>> local_;       // window thread: the same, kept for as long as the smooth motion needs them
    std::uint64_t seenSeq_ = 0, trackedSeq_ = 0;
    std::array<ActorTrack, kActorCount> tracks_;
    std::array<SpriteImg, kActorCount> sprites_;
    std::array<Conv, 3> convs_;
    int convNext_ = 0;
    std::vector<std::uint32_t> smoothBg_;                 // the background picture of the smooth motion (colour 280 x 192, or monochrome 560 x 192)
    std::vector<D3dRenderer::SpriteDraw> spriteDraws_;
    std::uint64_t smoothBgSeq_ = 0;
    bool smoothBgMono_ = false;
    bool smoothActive_ = false;
    std::int64_t testClockNs_ = 0;
    std::atomic<unsigned long long> distinct_{0};
    std::uint64_t lastSignature_ = 0;
    unsigned long lastVersionCounted_ = 0;
    std::uint64_t lastPixelsHash_ = 0;
    unsigned long long smoothFrames_ = 0;
    double sumMs_ = 0, sumSqMs_ = 0, maxMs_ = 0;
    unsigned long long intervals_ = 0;
    double drawSumMs_ = 0, presentSumMs_ = 0;
    std::vector<std::uint32_t> trailPrev_, trailPrevMono_;   // the afterglow: the last pictures shown
    bool glowStale_ = true;                                   // the ambient glow was not kept up to date (another background): start it from the current picture
    bool afterglowBusy_ = false;                              // a trail is still fading: keep repainting
    std::vector<std::uint8_t> monoDots_;       // the dot positions of the screen shown by setImage (when the program gave them), guarded by mutex_
    bool monoDotsValid_ = false;
    std::vector<std::uint32_t> mono560_;       // the monochrome picture of the last currentPixels() call (Native Black & White only)
    FrameState desk_;                         // the desktop window's chosen scale and place
    DeskHit dragKind_ = DeskHit::None;        // a drag or a resize in progress (the mouse is captured)
    POINT dragFrom_{0, 0};
    Viewport dragStart_;
    bool deskMouse(UINT msg, WPARAM wp, LPARAM lp);   // the mouse over the desktop window; true if it was used
    GlowField glow_;                          // the picture behind the picture of the ambient glow, smoothed over time
    Viewport viewportFor(int clientW, int clientH) const { return computeViewportFor(background_, clientW, clientH, desk_); }   // the picture's place: framed backgrounds need room for their frame
    struct ArtKey {
        Background kind = Background::Black;
        int width = 0, height = 0, x = 0, y = 0, scale = 0;
        bool font = false;
        bool mono = false;
        bool operator==(const ArtKey&) const = default;
    };
    ArtKey artKey_;                           // what the cached art image was drawn for
    std::vector<std::uint32_t> art_;          // the picture of the CRT bezel / the desktop for the present window size
    std::uint64_t artVersion_ = 0;
    D3dRenderer::BackgroundFrame backgroundFrame(int clientW, int clientH, const Viewport& vp);   // everything the renderer needs for the bars; draws the art again only when something changed
    std::int64_t lastPaintNs_ = 0;
    PauseMenu menu_;
    core::CharacterRom font_;
    std::atomic<bool> menuOpen_{false};
    std::atomic<bool> menuAllowed_{true};
    GamepadHub* pad_ = nullptr;
    AchievementBook* book_ = nullptr;
    PadState prevPad_;
    std::atomic<int> inputDevice_{0};
    std::deque<PadPress> padQueue_;                // (guarded by mutex_)
    std::int64_t padRepeatAtMs_[4] = {0, 0, 0, 0};   // the menu's key repeat for up / down / left / right
    bool muted_ = false;
    unsigned refreshHz_ = 60;
    std::unique_ptr<D3dRenderer> d3d_;
    WINDOWPLACEMENT saved_{};
    POINT savedClientOrigin_{};         // windowed client area before full screen (screen coordinates) and its size
    SIZE savedClientSize_{};
    bool savedZoomed_ = false;
    LONG savedStyle_ = 0;

    std::mutex logMutex_;
    std::mutex mutex_;                  // guards page_, pageVersion_, keys_
    hires::Page page_;
    std::vector<std::uint32_t> image_;
    bool useImage_ = false;
    unsigned long pageVersion_ = 1;
    std::deque<std::uint8_t> keys_;
    unsigned long paintedVersion_ = 0;  // window thread only
};

}  // namespace pacman::app
