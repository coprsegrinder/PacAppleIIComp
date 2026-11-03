// TASK-033-4b: the screens of the attract mode, rebuilt from the original program ($52BB, $5260, $43D2-$44E5, $42D0 and the drawing routines they call).
// A screen is a TIMED SCRIPT: every drawing the original does, with the cycle (counted from the entry of $52BB) at which it happens. The cycle counts are not measured data:
// they are summed up instruction by instruction while the script is built (page-crossing and branch-crossing cycles included), the same way the original's routines would
// spend them; tests/test_attract.cpp compares every routine entry of one whole title turn (11,549 events) with the oracle: all equal to the cycle.
// The pictures are made with the verified hires routines (tickdraw.h `executeCall`), the text with the glyph table of the data pack.
#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "../datapack/datapack.h"
#include "../hires/hires.h"
#include "tickdraw.h"

namespace pacman::core {

enum class ScreenOp : std::uint8_t {
    LoresFill,        // lo-res page 2 gets the value `a` in every byte ($52BB: $DD)
    ShowHires,        // the hi-res page 1 is displayed ($6F2F)
    WipeStart,        // the page clear $6F46 starts: the page turns black byte by byte, 8,192 bytes in 90,575 cycles
    Draw,             // one drawing call (tickdraw.h DrawCall)
};

struct ScreenStep {
    std::int64_t at = 0;
    ScreenOp op = ScreenOp::Draw;
    DrawCall call;
    int a = 0;
};

// One routine entry of the original, for the tests (kinds as in tools/export_title_events.py: 1 clear, 2 logo line, 3 blit row, 4 key poll, 5 delay step, 6 image, 7 dot, 8 tile)
struct TraceEvent {
    std::int64_t at = 0;
    int kind = 0;
    std::array<int, 5> p{};
};

struct ScreenScript {
    std::vector<ScreenStep> steps;
    std::int64_t length = 0;            // the cycle at which the next part of the attract loop starts
    std::vector<TraceEvent> trace;      // filled only when asked for
    std::vector<std::pair<std::int64_t, std::int64_t>> busy;   // [start, end) of every drawing routine of the original: inside such a window the original's picture is half drawn
};

// The title turn: from the entry of $52BB (lo-res flash, the logo, the typed "Character / Nickname" screen) to the first call of the chase. False if the pack lacks data.
bool buildTitleScript(const datapack::Pack& pack, ScreenScript& out, bool withTrace);

// The title turn and the chase that follows it (the two chase loops and the four ghost-eaten animations), up to the start of the demo game at cycle 22,486,410.
bool buildAttractScript(const datapack::Pack& pack, ScreenScript& out, bool withTrace);

// Plays a script. `advance` moves time forward and applies every step that has become due. The hi-res page is exact at every cycle apart from the drawing
// calls themselves, which take effect at the cycle they start (the original needs up to ~800 cycles to finish a glyph).
class ScreenPlayer {
public:
    void start(const ScreenScript* script, const datapack::Pack* pack);
    void advance(std::int64_t cycles);
    std::int64_t now() const noexcept { return now_; }
    bool finished() const noexcept { return script_ == nullptr || now_ >= script_->length; }
    bool hiresVisible() const noexcept { return hiresShown_; }
    const hires::Page& hires() const noexcept { return page_; }   // with the page clear applied up to `now`
    std::uint8_t loresByte(std::size_t offset) const noexcept { return offset < lores_.size() ? lores_[offset] : std::uint8_t{0}; }

private:
    void applyWipeUpTo(std::int64_t t);
    const ScreenScript* script_ = nullptr;
    const datapack::Pack* pack_ = nullptr;
    std::size_t next_ = 0;
    std::int64_t now_ = 0;
    hires::Page page_;
    std::array<std::uint8_t, 1024> lores_{};
    bool hiresShown_ = false;
    std::int64_t wipeAt_ = -1;
    std::size_t wiped_ = 0;
};

}  // namespace pacman::core
