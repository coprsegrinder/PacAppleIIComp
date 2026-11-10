// TASK-034-CONTROLS (modernization): game controllers. Three kinds are supported: XInput controllers (Xbox 360 / One / Series and everything that presents itself as one), the Sony DualShock 4
// and the Sony DualSense (wired or over Bluetooth, read as raw HID reports). The reports are turned into one PadState (directions from the D-pad or the left stick, "confirm", "back" and
// "menu" buttons) by pure functions that the tests drive with made-up reports; GamepadHub is the part that talks to Windows (it finds the devices by itself, also when one is plugged in or
// pulled out while the program runs, and reads them on its own thread). Nothing here touches the game: the program reads PadState and turns it into the original's own input (a joystick
// direction for the key poll, Apple II keys at the menus).
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "lightbar.h"
#include "vibration.h"

namespace pacman::app {

enum class PadKind : std::uint8_t { None, XInput, DualShock4, DualSense };

inline const char* padKindName(PadKind k) noexcept {
    switch (k) {
    case PadKind::XInput: return "XBOX CONTROLLER";
    case PadKind::DualShock4: return "DUALSHOCK 4";
    case PadKind::DualSense: return "DUALSENSE";
    case PadKind::None: break;
    }
    return "NONE";
}

// What the program asks of a controller. The buttons are by meaning, not by position: "confirm" is the bottom face button (A on an Xbox pad, the cross on a Sony pad), "back" the right face
// button (B, the circle), "menu" the menu button (Start, Options).
struct PadState {
    PadKind kind = PadKind::None;
    bool up = false, down = false, left = false, right = false;   // a direction held on the D-pad or the left stick (the menus use these)
    bool dpadUp = false, dpadDown = false, dpadLeft = false, dpadRight = false;   // the D-pad alone (it wins over the stick in padDirection())
    bool confirm = false, back = false, menu = false;
    bool anyInput = false;                                         // any button or a stick/D-pad out of its middle (the device in use)
    int stickX = 0, stickY = 0;                                    // the left stick, -127..127 (right / down positive)
    bool operator==(const PadState&) const = default;
};

// The stick as the original's paddle routines see it (TASK-034-PADDLE): the two paddles of the game port, 0 = left / up, 255 = right / down, 127 = middle. The D-pad pushes its axis
// to the end; otherwise the left stick's position is used as it is (the original's thresholds make the dead zone: counts 28 to 67 of 99 are the middle).
struct PaddlePosition {
    std::uint8_t x = 127, y = 127;
};
PaddlePosition padPaddles(const PadState& s) noexcept;

// The direction code of the original's key poll for the state: 0 = down, 1 = up, 2 = right, 3 = left (docs/CONTROLS.md); nothing when no direction is held. The D-pad wins over the stick; a
// diagonal gives the horizontal direction on the D-pad and the stronger axis on the stick.
std::optional<std::uint8_t> padDirection(const PadState& s) noexcept;

// The report parsers. `data` is the report as the device gives it, with the report id as the first byte. Returns false when the report is not an input report of the device.
//   DualShock 4: id 0x01 (USB, and the short report of Bluetooth) and id 0x11 (Bluetooth, full)
//   DualSense:   id 0x01 (USB, 64 bytes; over Bluetooth the short report, `bluetooth` = true) and id 0x31 (Bluetooth, full)
bool parseDualShock4(const std::uint8_t* data, std::size_t size, PadState& out) noexcept;
bool parseDualSense(const std::uint8_t* data, std::size_t size, bool bluetooth, PadState& out) noexcept;
// XInput: the fields of XINPUT_GAMEPAD
bool parseXInput(std::uint16_t buttons, std::int16_t thumbLX, std::int16_t thumbLY, PadState& out) noexcept;

// Vibration. CRC-32 (the usual one: polynomial 0xEDB88320, as zlib) and the output reports that start the motors of the Sony pads. `length` is the output report length that Windows tells for
// the device (DualShock 4: 32 on USB, 78 on Bluetooth; DualSense: 64 on USB, 78 on Bluetooth); the reports are zero-filled to it. The Bluetooth reports end with a CRC-32 over 0xA2 and the bytes before it.
std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept;
std::vector<std::uint8_t> buildDualShock4Rumble(std::size_t length, bool bluetooth, std::uint8_t strong, std::uint8_t weak);
std::vector<std::uint8_t> buildDualSenseRumble(std::size_t length, bool bluetooth, std::uint8_t strong, std::uint8_t weak, std::uint8_t sequence);

// The light (TASK-034-LIGHTBAR). One output report carries the motors, the light bar and the player LEDs, so every report the program writes carries ALL it is controlling: the motors as they stand
// and the light as it stands (a report with only one of them could blank the other on some firmware). `valid` flags tell the pad which parts of the report to apply.
struct PadOutput {
    std::uint8_t strong = 0, weak = 0;   // the motors
    bool light = false;                  // set the light bar to `rgb`
    Rgb rgb;
    bool ledControl = false;             // DualSense: set the five player LEDs to `leds`
    std::uint8_t leds = 0;
    bool lightSetup = false;             // DualSense, the first report of a take-over: the pad's own start-up light fade is cancelled
    bool release = false;                // DualSense: give the LEDs back to the pad (its own colour returns)
};
std::vector<std::uint8_t> buildDualShock4Output(std::size_t length, bool bluetooth, const PadOutput& o);
std::vector<std::uint8_t> buildDualSenseOutput(std::size_t length, bool bluetooth, const PadOutput& o, std::uint8_t sequence);
inline constexpr Rgb kDualShock4OwnLight{0, 0, 64};   // the colour a DualShock 4 shows for the first player: what it is given back (it has no "release")

// The controllers the program knows of and what the newest input came from. Thread-safe: the reading thread writes, any thread reads.
class GamepadHub {
public:
    GamepadHub();
    ~GamepadHub();
    GamepadHub(const GamepadHub&) = delete;
    GamepadHub& operator=(const GamepadHub&) = delete;
    void start();                       // opens the devices and starts the reading thread (no-op when running)
    void stop();
    PadState state() const;             // the state of the controller in use (the one that gave input last; else the first found); kind None when there is none
    PadKind connected() const;          // the kind of the controller in use (None: no controller)
    bool has(PadKind k) const;          // a controller of this kind is plugged in
    std::uint32_t changeCount() const;  // grows when a controller is plugged in or pulled out (the boot menu re-draws its words)

    // Vibration: plays the pattern on the controller in use (the one that gave input last) on the reading thread; a new pattern replaces the one that plays; an empty one stops it. The motors are
    // also stopped when the pad is pulled out of use, and when the hub stops.
    void rumble(const std::vector<RumbleStep>& steps);
    void stopRumble();

    // The light bar and the player LEDs (lightbar.h): the scene is fed in by the program (the setting and the colour mode, the game's moment, the pause menu, the events) and the reading thread
    // keeps the controller in use (a DualShock 4 or a DualSense; others are left alone) at lightFor(scene, now). The pad's own light is given back when the setting is Off, when another pad
    // comes into use, and when the hub stops.
    void lightConfig(LightbarLevel level, bool mono);
    void lightGame(bool playing, bool frightened, std::uint16_t timer, int lives);
    void lightPaused(bool paused);
    void lightCue(LightCue cue, int ghost);
    const std::vector<LightOut>& lightLogForTests() const;   // with injectForTests: every different output, in order

    // tests and the program's other inputs
    const std::vector<std::vector<RumbleStep>>& rumbleLogForTests() const;   // with injectForTests: the patterns asked for, in order
    void injectForTests(const PadState& s, bool present);   // replaces the devices by this one state (start() must not have been called)

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

}  // namespace pacman::app
