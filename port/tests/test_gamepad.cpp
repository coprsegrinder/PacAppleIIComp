// TASK-034 Controls: the controller reports (DualShock 4, DualSense, XInput) turned into PadState, and the direction the original's key poll gets from them. The reports are made up
// byte by byte from the layouts of the devices; the real devices are tried by the user (the program cannot be tested against hardware that is not plugged in).
#include <array>
#include <cstdint>
#include <vector>

#include "../src/app/gamepad.h"
#include "flow_common.h"
#include "test_framework.h"

using namespace pacman;
using app::PadKind;
using app::PadState;

namespace {

// a DualShock 4 report: id, then x y rx ry buttons0 buttons1 buttons2 (USB layout; the Bluetooth full report has two more bytes after the id)
std::vector<std::uint8_t> ds4(std::uint8_t id, std::uint8_t lx, std::uint8_t ly, std::uint8_t b0, std::uint8_t b1) {
    std::vector<std::uint8_t> r(id == 0x11 ? 78 : 64, 0);
    r[0] = id;
    const std::size_t base = id == 0x11 ? 3 : 1;
    r[base] = lx;
    r[base + 1] = ly;
    r[base + 2] = 128;
    r[base + 3] = 128;
    r[base + 4] = b0;
    r[base + 5] = b1;
    return r;
}

std::vector<std::uint8_t> dualsense(std::uint8_t id, bool bt, std::uint8_t lx, std::uint8_t ly, std::uint8_t b0, std::uint8_t b1) {
    std::vector<std::uint8_t> r(id == 0x31 ? 78 : (bt ? 78 : 64), 0);
    r[0] = id;
    const std::size_t base = id == 0x31 ? 2 : 1;
    r[base] = lx;
    r[base + 1] = ly;
    r[base + 2] = 128;
    r[base + 3] = 128;
    const std::size_t at = id == 0x31 ? 9 : (bt ? 5 : 8);
    r[at] = b0;
    r[at + 1] = b1;
    return r;
}

constexpr std::uint8_t kHatNone = 8, kCross = 0x20, kCircle = 0x40, kSquare = 0x10, kTriangle = 0x80, kOptions = 0x20;

}  // namespace

TEST(dualshock4_reports_give_directions_confirm_back_and_menu_over_usb_and_bluetooth) {
    for (std::uint8_t id : {std::uint8_t{0x01}, std::uint8_t{0x11}}) {
        PadState s;
        CHECK(app::parseDualShock4(ds4(id, 128, 128, kHatNone, 0).data(), ds4(id, 128, 128, kHatNone, 0).size(), s));
        CHECK(s.kind == PadKind::DualShock4 && !s.anyInput && !s.up && !s.confirm && !s.menu && !app::padDirection(s));
        const auto press = [&](std::uint8_t b0, std::uint8_t b1, std::uint8_t lx = 128, std::uint8_t ly = 128) {
            const auto r = ds4(id, lx, ly, b0, b1);
            PadState t;
            CHECK(app::parseDualShock4(r.data(), r.size(), t));
            return t;
        };
        CHECK(press(0, 0).up && app::padDirection(press(0, 0)) == 1u);       // hat 0 = up
        CHECK(app::padDirection(press(2, 0)) == 2u);                          // east = right
        CHECK(app::padDirection(press(4, 0)) == 0u);                          // south = down
        CHECK(app::padDirection(press(6, 0)) == 3u);                          // west = left
        CHECK(app::padDirection(press(1, 0)) == 2u);                          // north-east: the horizontal direction on the D-pad
        CHECK(app::padDirection(press(7, 0)) == 3u);                          // north-west
        CHECK(press(kHatNone | kCross, 0).confirm && !press(kHatNone | kCross, 0).back);
        CHECK(press(kHatNone | kCircle, 0).back);
        CHECK(!press(kHatNone | kSquare | kTriangle, 0).confirm && press(kHatNone | kSquare, 0).anyInput);
        CHECK(press(kHatNone, kOptions).menu);
        CHECK(!press(kHatNone, 0x10).menu);                                   // share is not the menu
        CHECK(app::padDirection(press(kHatNone, 0, 0, 128)) == 3u);           // the left stick pushed left
        CHECK(app::padDirection(press(kHatNone, 0, 128, 255)) == 0u);         // down (the stick's y grows downwards)
        CHECK(app::padDirection(press(kHatNone, 0, 128, 0)) == 1u);
        CHECK(app::padDirection(press(kHatNone, 0, 160, 128)) == std::nullopt || app::padDirection(press(kHatNone, 0, 160, 128)) == 2u);   // 32 of 127: inside the dead zone or just out
        CHECK(!app::padDirection(press(kHatNone, 0, 140, 135)));             // a stick near the middle gives nothing
        CHECK(app::padDirection(press(kHatNone, 0, 255, 200)) == 2u);         // the stronger axis wins on the stick
        CHECK(app::padDirection(press(4, 0, 0, 128)) == 0u);                  // the D-pad wins over the stick
    }
    PadState s;
    const std::uint8_t other[4] = {0x05, 1, 2, 3};
    CHECK(!app::parseDualShock4(other, 4, s) && !app::parseDualShock4(nullptr, 0, s));
    const std::uint8_t shortReport[4] = {0x01, 1, 2, 3};
    CHECK(!app::parseDualShock4(shortReport, 4, s));
}

TEST(dualsense_reports_give_directions_confirm_back_and_menu_over_usb_and_bluetooth) {
    struct Kind { std::uint8_t id; bool bt; };
    for (const Kind k : {Kind{0x01, false}, Kind{0x01, true}, Kind{0x31, true}}) {
        const auto press = [&](std::uint8_t b0, std::uint8_t b1, std::uint8_t lx = 128, std::uint8_t ly = 128) {
            const auto r = dualsense(k.id, k.bt, lx, ly, b0, b1);
            PadState t;
            CHECK(app::parseDualSense(r.data(), r.size(), k.bt, t));
            return t;
        };
        CHECK(press(kHatNone, 0).kind == PadKind::DualSense && !press(kHatNone, 0).anyInput);
        CHECK(app::padDirection(press(0, 0)) == 1u && app::padDirection(press(2, 0)) == 2u && app::padDirection(press(4, 0)) == 0u && app::padDirection(press(6, 0)) == 3u);
        CHECK(press(kHatNone | kCross, 0).confirm && press(kHatNone | kCircle, 0).back && press(kHatNone, kOptions).menu);
        CHECK(!press(kHatNone | kTriangle, 0).confirm && press(kHatNone | kTriangle, 0).anyInput);
        CHECK(app::padDirection(press(kHatNone, 0, 0, 128)) == 3u && app::padDirection(press(kHatNone, 0, 255, 128)) == 2u);
    }
    PadState s;
    const std::uint8_t other[4] = {0x02, 1, 2, 3};
    CHECK(!app::parseDualSense(other, 4, false, s));
}

TEST(xinput_state_gives_directions_confirm_back_and_menu) {
    PadState s;
    CHECK(app::parseXInput(0, 0, 0, s) && s.kind == PadKind::XInput && !s.anyInput && !app::padDirection(s));
    CHECK(app::parseXInput(0x0001, 0, 0, s) && app::padDirection(s) == 1u);   // D-pad up
    CHECK(app::parseXInput(0x0002, 0, 0, s) && app::padDirection(s) == 0u);
    CHECK(app::parseXInput(0x0004, 0, 0, s) && app::padDirection(s) == 3u);
    CHECK(app::parseXInput(0x0008, 0, 0, s) && app::padDirection(s) == 2u);
    CHECK(app::parseXInput(0x0005, 0, 0, s) && app::padDirection(s) == 3u);   // up + left: the horizontal one
    CHECK(app::parseXInput(0x1000, 0, 0, s) && s.confirm && !s.back && s.anyInput);   // A
    CHECK(app::parseXInput(0x2000, 0, 0, s) && s.back && !s.confirm);                  // B
    CHECK(app::parseXInput(0x0010, 0, 0, s) && s.menu);                                // Start
    CHECK(app::parseXInput(0x0020, 0, 0, s) && !s.menu);                               // Back/View is not the menu button
    CHECK(app::parseXInput(0, -30000, 0, s) && app::padDirection(s) == 3u);           // the left stick left
    CHECK(app::parseXInput(0, 30000, 0, s) && app::padDirection(s) == 2u);
    CHECK(app::parseXInput(0, 0, 30000, s) && app::padDirection(s) == 1u);            // XInput: up is positive
    CHECK(app::parseXInput(0, 0, -30000, s) && app::padDirection(s) == 0u);
    CHECK(app::parseXInput(0, 3000, 2000, s) && !app::padDirection(s) && !s.anyInput);   // inside the dead zone
    CHECK(app::parseXInput(0, 30000, -32000, s) && app::padDirection(s) == 0u);       // the stronger axis
}

TEST(the_gamepad_hub_reports_the_controller_in_use_and_none_without_one) {
    app::GamepadHub hub;
    CHECK(hub.connected() == PadKind::None && !hub.has(PadKind::XInput) && !hub.state().anyInput);
    PadState s;
    app::parseXInput(0x1000, 0, 0, s);
    hub.injectForTests(s, true);
    CHECK(hub.connected() == PadKind::XInput && hub.has(PadKind::XInput) && !hub.has(PadKind::DualSense) && hub.state().confirm);
    hub.injectForTests(s, false);
    CHECK(hub.connected() == PadKind::None);
    CHECK(hub.changeCount() >= 2);
}

namespace {
bool screenHas(const core::AttractFlow& flow, const char* text) {   // the words on the text page (a row is contiguous in the page's memory)
    const std::string t(text);
    for (std::size_t at = 0; at + t.size() <= 1024; ++at) {
        bool all = true;
        for (std::size_t i = 0; i < t.size() && all; ++i) all = flow.text().byte(at + i) == static_cast<std::uint8_t>(t[i] | 0x80);
        if (all) return true;
    }
    return false;
}
}  // namespace

TEST(the_first_menu_asks_for_space_and_names_the_controller_by_its_confirm_button) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    // the game's own words stay when the controls section is not asked for (the oracle tests rely on it)
    {
        core::AttractFlow flow;
        CHECK(flow.startBoot(pack, flowtest::romPath()));
        flowtest::settle(flow);
        CHECK(screenHas(flow, "1. KEYBOARD") && screenHas(flow, "2. APPLE JOYSTICK") && !screenHas(flow, "SPACE. KEYBOARD"));
    }
    // a PlayStation pad: X; an Xbox pad: A; none: a note
    for (const char letter : {'X', 'A', '\0'}) {
        core::AttractFlow flow;
        flow.setBootMenu(true, letter);
        CHECK(flow.startBoot(pack, flowtest::romPath()));
        flowtest::settle(flow);
        CHECK(screenHas(flow, "PACMAN CAN BE PLAYED WITH") && screenHas(flow, "SPACE. KEYBOARD") && !screenHas(flow, "1. KEYBOARD") && !screenHas(flow, "APPLE JOYSTICK"));
        if (letter) {
            const std::string line = std::string(1, letter) + ". CONTROLLER";
            CHECK(screenHas(flow, line.c_str()) && screenHas(flow, (std::string("(SPACE OR ") + letter + ")").c_str()));
        } else {
            CHECK(screenHas(flow, "NO CONTROLLER FOUND") && !screenHas(flow, ". CONTROLLER"));
        }
    }
    // the words follow a controller that is plugged in while the menu is on the screen
    {
        core::AttractFlow flow;
        flow.setBootMenu(true, '\0');
        CHECK(flow.startBoot(pack, flowtest::romPath()));
        flowtest::settle(flow);
        CHECK(screenHas(flow, "NO CONTROLLER FOUND"));
        flow.setBootMenu(true, 'A');
        flowtest::settle(flow);
        CHECK(screenHas(flow, "A. CONTROLLER") && !screenHas(flow, "NO CONTROLLER FOUND") && flow.mode() == core::AttractFlow::Mode::BootMode);
    }
    // Space chooses the keyboard (the next question is the keyboard's); the digit the program sends for the controller chooses the joystick mode (the first game starts as the original's)
    {
        core::AttractFlow flow;
        flow.setBootMenu(true, 'X');
        CHECK(flow.startBoot(pack, flowtest::romPath()));
        flowtest::settle(flow);
        flow.keyDown(core::kKeySpace);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::BootKeyboard && flow.inputDevice() == 0);
    }
    {
        core::AttractFlow flow;
        flow.setBootMenu(true, 'X');
        CHECK(flow.startBoot(pack, flowtest::romPath()));
        flowtest::settle(flow);
        flow.keyDown(0xB2);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::Attract && flow.inputDevice() == 1);
    }
    // another key does nothing (the menu waits)
    {
        core::AttractFlow flow;
        flow.setBootMenu(true, 'X');
        CHECK(flow.startBoot(pack, flowtest::romPath()));
        flowtest::settle(flow);
        flow.keyDown(0xC5);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::BootMode);
    }
}

TEST(the_players_and_press_return_prompts_use_the_words_and_buttons_of_the_plugged_in_controller) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    const auto toPlayers = [&](core::AttractFlow& flow, char letter) {
        CHECK(flow.start(pack, flowtest::romPath()));
        flow.setBootMenu(true, letter);
        flow.advance(300'000);
        flow.keyDown(core::kKeyEsc);
        flow.advance(300'000);
        flow.keyDown(core::kKeySpace);
        flow.advance(1'000);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::Players);
    };
    // no controller: the game's own words, nothing added
    {
        core::AttractFlow flow;
        toPlayers(flow, '\0');
        CHECK(screenHas(flow, "HOW MANY PLAYERS?") && !screenHas(flow, "PLAYER >") && !screenHas(flow, "CONTINUE"));
        flow.keyDown(0xB3);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn && screenHas(flow, "PRESS <RETURN> TO PLAY") && screenHas(flow, "OR <SPACE> TO GO BACK TO DEMO."));
    }
    // a PlayStation pad: the number is chosen with left / right and X, the second prompt names X and the circle
    {
        core::AttractFlow flow;
        toPlayers(flow, 'X');
        CHECK(screenHas(flow, "HOW MANY PLAYERS?") && screenHas(flow, "< 1 PLAYER >") && screenHas(flow, "LEFT / RIGHT: CHANGE THE NUMBER") && screenHas(flow, "X: CONTINUE"));
        flow.padPlayersStep(1);
        CHECK(flow.padPlayers() == 2 && screenHas(flow, "< 2 PLAYERS >") && !screenHas(flow, "< 1 PLAYER >") && flow.mode() == core::AttractFlow::Mode::Players);
        flow.padPlayersStep(-1);
        flow.padPlayersStep(-1);
        CHECK(flow.padPlayers() == 5 && screenHas(flow, "< 5 PLAYERS >"));   // round the end
        flow.padPlayersStep(1);
        CHECK(flow.padPlayers() == 1);
        flow.padPlayersStep(1);
        flow.padPlayersStep(1);                                               // 3
        flow.keyDown(static_cast<std::uint8_t>(0xB0 | flow.padPlayers()));
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn && flow.players() == 3);
        CHECK(screenHas(flow, "PRESS <RETURN> OR X TO PLAY") && screenHas(flow, "<SPACE> OR CIRCLE: BACK TO DEMO.") && !screenHas(flow, "PLAYERS >") && !screenHas(flow, "CONTINUE"));
        flow.keyDown(core::kKeyReturn);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::Game && flow.players() == 3);
    }
    // an Xbox pad: A and B; a keyboard digit still works; Space still goes back to the demo
    {
        core::AttractFlow flow;
        toPlayers(flow, 'A');
        CHECK(screenHas(flow, "A: CONTINUE"));
        flow.keyDown(0xB2);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn && flow.players() == 2 && screenHas(flow, "PRESS <RETURN> OR A TO PLAY") && screenHas(flow, "<SPACE> OR B: BACK TO DEMO."));
        flow.keyDown(core::kKeySpace);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::Attract);
    }
    // the words follow the controller while a prompt is shown: pulled out on the players screen, another kind plugged in on the second prompt
    {
        core::AttractFlow flow;
        toPlayers(flow, 'X');
        flow.setBootMenu(true, '\0');
        flowtest::settle(flow);
        CHECK(screenHas(flow, "HOW MANY PLAYERS?") && !screenHas(flow, "PLAYER >") && flow.mode() == core::AttractFlow::Mode::Players);
        flow.setBootMenu(true, 'A');
        flowtest::settle(flow);
        CHECK(screenHas(flow, "< 1 PLAYER >"));
        flow.keyDown(0xB1);
        flowtest::settle(flow);
        CHECK(flow.mode() == core::AttractFlow::Mode::PressReturn && screenHas(flow, "OR A TO PLAY"));
        flow.setBootMenu(true, 'X');
        flowtest::settle(flow);
        CHECK(screenHas(flow, "OR X TO PLAY") && !screenHas(flow, "OR A TO PLAY") && screenHas(flow, "CIRCLE") && flow.mode() == core::AttractFlow::Mode::PressReturn);
        flow.setBootMenu(true, '\0');
        flowtest::settle(flow);
        CHECK(screenHas(flow, "PRESS <RETURN> TO PLAY") && !screenHas(flow, "CIRCLE") && flow.mode() == core::AttractFlow::Mode::PressReturn);
    }
}


TEST(vibration_crc32_and_the_output_reports_of_the_sony_pads) {
    const std::uint8_t digits[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(app::crc32(digits, 9) == 0xCBF43926u);   // the standard check value of CRC-32
    // DualShock 4, USB: report 5, motors flag, right (light) at 4, left (heavy) at 5
    auto r = app::buildDualShock4Rumble(32, false, 200, 100);
    CHECK(r.size() == 32 && r[0] == 0x05 && r[1] == 0x01 && r[4] == 100 && r[5] == 200);
    // DualShock 4, Bluetooth: report 0x11, 78 bytes, motors at 6 / 7, CRC-32 of 0xA2 + the first 74 bytes at the end
    r = app::buildDualShock4Rumble(78, true, 200, 100);
    CHECK(r.size() == 78 && r[0] == 0x11 && r[1] == 0xC4 && r[3] == 0x01 && r[6] == 100 && r[7] == 200);
    {
        std::vector<std::uint8_t> seeded{0xA2};
        seeded.insert(seeded.end(), r.begin(), r.begin() + 74);
        const std::uint32_t c = app::crc32(seeded.data(), seeded.size());
        CHECK(r[74] == (c & 0xFF) && r[75] == ((c >> 8) & 0xFF) && r[76] == ((c >> 16) & 0xFF) && r[77] == ((c >> 24) & 0xFF));
    }
    // DualSense, USB: report 2, flags 3, motors at 3 (right) / 4 (left), flag2 at 39
    r = app::buildDualSenseRumble(64, false, 250, 50, 0);
    CHECK(r.size() == 64 && r[0] == 0x02 && r[1] == 0x03 && r[3] == 50 && r[4] == 250 && r[39] == 0x04);
    // DualSense, Bluetooth: report 0x31, sequence in the high nibble of byte 1, tag 0x10, motors at 5 / 6, flag2 at 41, CRC at the end
    r = app::buildDualSenseRumble(78, true, 250, 50, 5);
    CHECK(r.size() == 78 && r[0] == 0x31 && r[1] == 0x50 && r[2] == 0x10 && r[3] == 0x03 && r[5] == 50 && r[6] == 250 && r[41] == 0x04);
    {
        std::vector<std::uint8_t> seeded{0xA2};
        seeded.insert(seeded.end(), r.begin(), r.begin() + 74);
        const std::uint32_t c = app::crc32(seeded.data(), seeded.size());
        CHECK(r[74] == (c & 0xFF) && r[77] == ((c >> 24) & 0xFF));
    }
    // the length Windows tells is kept (zero-filled); zero motors give zero bytes
    CHECK(app::buildDualSenseRumble(64, false, 0, 0, 0).size() == 64 && app::buildDualShock4Rumble(32, false, 0, 0).size() == 32);
    r = app::buildDualShock4Rumble(32, false, 0, 0);
    CHECK(r[4] == 0 && r[5] == 0);
}
