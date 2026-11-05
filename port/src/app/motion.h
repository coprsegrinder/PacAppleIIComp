// TASK-034-11 (modernization): smooth motion for the modern FPS modes (30, 60, 120, 144, Unlocked). The game moves Pac-Man and the ghosts in steps (Pac-Man about 20 steps a second, a ghost
// 10 to 50), and the original shows each step as one jump; the pictures it makes change only 50 to 90 times a second, so a higher FPS setting had nothing new to show (found by
// investigation, docs/MODERNIZATION.md Section 11). Here the steps are joined: each actor (Pac-Man, the four ghosts) glides from where a step put it to where the next step puts it, over
// the time between the two, and the window draws it at that in-between place. The game's state and timing are untouched (nothing is predicted or invented: the actor is shown on the
// straight line between two places the game really had); the cost is a delay of one step (the window shows the game about 50 to 90 ms behind, the time the glide needs to be known).
// Pure (no Windows calls); tests/test_gamewindow.cpp.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pacman::app {

inline constexpr int kActorCount = 5;                         // 0 = Pac-Man, 1..4 = the ghosts
inline constexpr float kSnapDistance = 24.0f;                 // a step longer than this (in game pixels) is a jump (the tunnel, a new life): no glide
inline constexpr std::int64_t kMaxGlideNs = 140'000'000;      // an actor that stood still for longer starts gliding this long before its next step, not at the old one
inline constexpr std::int64_t kMinDelayNs = 20'000'000, kMaxDelayNs = 150'000'000, kDefaultDelayNs = 60'000'000;

struct ActorStep {
    std::int64_t tNs = 0;      // when the game made the step (its own clock)
    float x = 0, y = 0;        // where it stands from then on (game pixels)
    std::uint64_t seq = 0;     // which published picture holds the step (the sprite is taken from it)
};

struct ActorPlace {
    float x = 0, y = 0;
    std::uint64_t seq = 0;     // the step the sprite belongs to (the latest one at or before the asked time)
    bool valid = false;
};

class ActorTrack {
public:
    void clear() { steps_.clear(); }
    bool empty() const noexcept { return steps_.empty(); }
    // A step: appended when the place differs from the last one. A jump (further than kSnapDistance) starts a new line: nothing glides across it.
    void add(std::int64_t tNs, float x, float y, std::uint64_t seq) {
        if (!steps_.empty() && steps_.back().x == x && steps_.back().y == y) return;
        steps_.push_back(ActorStep{tNs, x, y, seq});
        if (steps_.size() > 64) steps_.erase(steps_.begin(), steps_.begin() + 32);
    }
    // Where the actor is shown at time `sigma`: on the straight line between the step at or before it and the next one, reached at the next step's time (the glide takes at most
    // kMaxGlideNs); before the first step: the first; after the last known one: the last.
    ActorPlace at(std::int64_t sigmaNs) const {
        ActorPlace out;
        if (steps_.empty()) return out;
        out.valid = true;
        std::size_t k = 0;
        while (k + 1 < steps_.size() && steps_[k + 1].tNs <= sigmaNs) ++k;   // (the latest step at or before sigma)
        const ActorStep& a = steps_[k];
        out.x = a.x;
        out.y = a.y;
        out.seq = a.seq;
        if (sigmaNs < a.tNs || k + 1 >= steps_.size()) return out;   // before the first step, or after the last: stand there
        const ActorStep& b = steps_[k + 1];
        if (std::fabs(b.x - a.x) > kSnapDistance || std::fabs(b.y - a.y) > kSnapDistance) return out;   // a jump: the actor stands at its old place until the step
        const std::int64_t start = (std::max)(a.tNs, b.tNs - kMaxGlideNs);
        if (sigmaNs <= start) return out;
        const double f = std::clamp(static_cast<double>(sigmaNs - start) / static_cast<double>(b.tNs - start), 0.0, 1.0);
        out.x = static_cast<float>(a.x + (b.x - a.x) * f);
        out.y = static_cast<float>(a.y + (b.y - a.y) * f);
        return out;
    }
    // How far behind the game the actor has to be shown so that the step it glides to is known: the longest of its last three step intervals, a little more (clamped).
    std::int64_t delayNs() const {
        if (steps_.size() < 2) return kDefaultDelayNs;
        std::int64_t longest = 0;
        for (std::size_t i = steps_.size() - 1, n = 0; i > 0 && n < 3; --i, ++n) longest = (std::max)(longest, steps_[i].tNs - steps_[i - 1].tNs);
        return std::clamp(longest + longest / 8 + 4'000'000, kMinDelayNs, kMaxDelayNs);
    }
    // Steps older than `olderThanNs` that are no longer needed (the last one before it is kept).
    void trim(std::int64_t olderThanNs) {
        while (steps_.size() > 2 && steps_[1].tNs < olderThanNs) steps_.erase(steps_.begin());
    }
    std::size_t size() const noexcept { return steps_.size(); }

private:
    std::vector<ActorStep> steps_;
};

}  // namespace pacman::app
