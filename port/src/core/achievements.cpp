#include "achievements.h"

#include <cstdlib>

namespace pacman::core {

namespace {
constexpr std::int64_t kThreeMinutes = 3LL * 60 * 1'022'727;   // cycles of the game's clock
int sgn(int v) { return (v > 0) - (v < 0); }
}  // namespace

int fruitOfLevel(int level) noexcept {
    if (level <= 1) return 0;
    if (level == 2) return 1;
    if (level <= 4) return 2;
    if (level <= 6) return 3;
    if (level <= 8) return 4;
    if (level <= 10) return 5;
    if (level <= 12) return 6;
    return 7;
}

void AchievementTracker::unlock(int i, std::vector<int>& out) {
    const std::uint32_t bit = 1u << i;
    if (unlocked_ & bit) return;
    unlocked_ |= bit;
    out.push_back(i);
}

void AchievementTracker::newGame(int players, int startLevel) {
    startLevel_ = startLevel;
    for (Run& r : runs_) r = Run{};
    for (int p = 1; p <= players && p < static_cast<int>(runs_.size()); ++p) {
        Run& r = runs_[static_cast<std::size_t>(p)];
        r.clean123 = startLevel == 1;
        r.clean45 = startLevel == 4;   // from level 3's end on when the game starts earlier
        r.noGhosts15 = startLevel == 1;
    }
}

std::vector<int> AchievementTracker::onTick(const TickFacts& f) {
    std::vector<int> out;
    if (f.player < 1 || f.player >= static_cast<int>(runs_.size())) return out;
    Run& r = runs_[static_cast<std::size_t>(f.player)];

    // Pac-Man turning round: his step goes the opposite way of the step before
    if (f.acted) {
        if (r.lastX >= 0) {
            const int dx = f.pacX - r.lastX, dy = f.pacY - r.lastY;
            if ((dx != 0 || dy != 0) && std::abs(dx) <= 8 && std::abs(dy) <= 8) {   // (a longer way is the tunnel)
                const int sx = sgn(dx), sy = sgn(dy);
                if ((r.lastDx != 0 || r.lastDy != 0) && sx == -r.lastDx && sy == -r.lastDy) {
                    if (f.level <= 3) r.clean123 = false;
                    if (f.level >= 4 && f.level <= 5) r.clean45 = false;
                }
                r.lastDx = sx;
                r.lastDy = sy;
            }
        }
        r.lastX = f.pacX;
        r.lastY = f.pacY;
    }

    // the quiet minutes
    r.quietCycles += f.cycles;
    if (f.pellet || f.energizer || f.ghostsEaten > 0 || f.fruitEaten || f.caught) r.quietCycles = 0;
    if (r.quietCycles >= kThreeMinutes) unlock(kStarvation, out);

    // ghosts
    if (f.ghostsEaten > 0) {
        sessionGhosts_ += f.ghostsEaten;
        if (f.level <= 5) r.noGhosts15 = false;
    }
    if (sessionGhosts_ >= 50) unlock(kGhostSlayer, out);
    if (f.energizer) r.streak = 0;
    r.streak += f.ghostsEaten;
    if (r.streak >= 4) {   // the four of one energizer: the 1600 points
        unlock(kSpooky, out);
        ++r.mazeStreaks;
        r.streak = 0;
    }
    if (!f.frightened) r.streak = 0;

    // fruit
    if (f.fruitEaten) {
        const int kind = fruitOfLevel(f.level);
        if (kind < 6) r.fruitKinds |= 1u << kind;
        ++r.mazeFruit;
        if (f.level == 1 && r.mazeFruit >= 2) unlock(kCherry, out);
        if (f.level == 2 && r.mazeFruit >= 2) unlock(kStrawberry, out);
        if (f.level >= 3 && f.level <= 4 && r.mazeFruit >= 2) unlock(kPeach, out);
        if (f.level >= 5 && f.level <= 6 && r.mazeFruit >= 2) unlock(kApples, out);
        if (f.level >= 7 && f.level <= 8 && r.mazeFruit >= 2) unlock(kLemon, out);
        if (f.level >= 9 && f.level <= 10) unlock(kPineapple, out);
        if (f.level >= 11 && f.level <= 12) unlock(kBell, out);
        if (f.level >= 13) unlock(kKey, out);
        if ((r.fruitKinds & 0x3F) == 0x3F) unlock(kHealthyDiet, out);
    }

    if (f.bonusLife) unlock(kExtraChances, out);
    if (f.score >= 20000) unlock(kTaxMan, out);
    if (f.score >= 50000) unlock(kExpert, out);

    if (f.caught) {
        r.mazeDied = true;
        if (f.level <= 3) r.clean123 = false;
        if (f.level >= 4 && f.level <= 5) r.clean45 = false;
        r.streak = 0;
        r.lastDx = r.lastDy = 0;
        r.lastX = -1;
    }

    if (f.levelComplete) {
        if (r.mazeStreaks >= 2 && r.mazeFruit >= 2 && !r.mazeDied) unlock(kPerfectionist, out);
        if (f.level == 3 && r.clean123) unlock(kForwards, out);
        if (f.level == 3 && startLevel_ <= 3) r.clean45 = true;   // maze 4 begins
        if (f.level == 5 && r.clean45) unlock(kCantTurnBack, out);
        if (f.level == 5 && r.noGhosts15) unlock(kPeace, out);
        r.mazeFruit = r.mazeStreaks = r.streak = 0;
        r.mazeDied = false;
        r.lastDx = r.lastDy = 0;
        r.lastX = -1;
        r.quietCycles = 0;
    }
    return out;
}

std::vector<int> AchievementTracker::onCartoonDone(int cartoon) {
    std::vector<int> out;
    if (cartoon >= 1 && cartoon <= 3) unlock(kCoffeeBreak + (cartoon - 1), out);
    return out;
}

}  // namespace pacman::core
