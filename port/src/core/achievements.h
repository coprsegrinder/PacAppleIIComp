// TASK-034-ACHIEVEMENTS (modernization): the 22 achievements of the RetroAchievements set for "Pac-Man (Apple II)" (game 13932; the set's authors are named on the Achievements page), checked
// by the port from what the game does. This is a read-only observer: it is given facts about every tick (what was eaten, the score, the level, deaths, ...) and says which achievements were
// reached. It never touches the game's state. The texts are the set's, with the typos corrected and "goblins" written as ghosts. The achievements live in the port only: nothing is sent to
// RetroAchievements and no account is involved.
//
// How the set's wording is read (the page gives only the words, not the checks of the set; these readings are the port's):
//   * "maze N" = level N (the number of the level the game counts: $55);  "the first maze" = level 1;  "a game" = one game from its first level to GAME OVER.
//   * the fruit of a level is by level: cherry 1, strawberry 2, peach 3-4, apple 5-6, lemon 7-8, pineapple 9-10, bell 11-12, key 13 and up (checked against the game's own table in the tests);
//     "eat 2 X in maze N or M" = two of them in one of those mazes.
//   * a ghost "streak": the ghosts eaten during one energizer; four of them is the 1600 point bonus. "In a row with the same power pellet timer" = the four of one energizer.
//   * "turning around (180 degrees)" = Pac-Man's step goes the opposite way of his last step. Steps across the tunnel do not count.
//   * "3 minutes" = 3 x 60 seconds of the game's own clock (the cycles of the ticks), restarted by a pellet, an energizer, a ghost, a fruit or a death.
//   * "one session" = since the program started.
//   * the demo (attract game) counts for nothing.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace pacman::core {

inline constexpr int kAchievementCount = 22;

struct AchievementDef {
    const char* title;
    const char* description;
    int points;
};

// In the order of the set. (190 points in all.)
inline constexpr std::array<AchievementDef, kAchievementCount> kAchievementDefs = {{
    {"Coffee Break!", "Watch the classic scene after beating maze 2", 5},
    {"Another Break", "Watch another classic scene after beating maze 5", 5},
    {"The Third Break", "Watch yet another classic scene after beating maze 9", 10},
    {"Extra Chances", "Earn 10,000 points and receive an extra turn", 5},
    {"I'm not the Tax Man", "Reach a score of 20,000", 5},
    {"Pacman Expert", "Reach a score of 50,000", 10},
    {"Cherry Chomper", "Consume two cherries in the first maze", 5},
    {"Strawberry Slurper", "Eat 2 strawberries in maze 2", 5},
    {"Peach Picker", "Eat 2 peaches in maze 3 or 4", 5},
    {"2 Apples... on the Apple II!", "Eat 2 apples in maze 5 or 6", 5},
    {"Lemon Lounger", "Eat 2 lemons in maze 7 or 8", 5},
    {"Pineapple Pincher", "Eat a pineapple(?) in maze 9 or 10", 5},
    {"Bell Bagger", "Eat a bell in maze 11 or 12", 10},
    {"Finally the Key!", "Eat a key in maze 13 or higher", 10},
    {"A Healthy Diet", "Eat one of every fruit (Cherry, Strawberry, Peach, Apple, Lemon, Pineapple) in one single game", 10},
    {"3 Minute Starvation", "Survive for 3 minutes without eating any pellets, ghosts, fruit bonuses, or dying", 10},
    {"Spooky Treats!", "Eat 4 ghosts in a row with the same power pellet timer (1600 point bonus)", 5},
    {"Pac Perfectionist", "Beat any maze while receiving a 1600 point ghost streak twice, and 2 fruit bonuses, no deaths, same maze", 10},
    {"Ghost Slayer!", "Slay 50 ghosts (one session, game overs allowed)", 5},
    {"We Move Forwards", "Beat mazes 1 through 3 without turning around (180 degrees) and without dying", 25},
    {"Can't Turn Back", "Beat mazes 4 and 5 without turning around (180 degrees) and without dying", 25},
    {"I Pac in Peace", "Beat the first 5 mazes without eating a single ghost", 10},
}};

// the indices, named
enum Achievement : int {
    kCoffeeBreak, kAnotherBreak, kThirdBreak, kExtraChances, kTaxMan, kExpert, kCherry, kStrawberry, kPeach, kApples, kLemon, kPineapple, kBell, kKey, kHealthyDiet, kStarvation, kSpooky,
    kPerfectionist, kGhostSlayer, kForwards, kCantTurnBack, kPeace
};

// The fruit of a level: 0 cherry, 1 strawberry, 2 peach, 3 apple, 4 lemon, 5 pineapple, 6 bell, 7 key.
int fruitOfLevel(int level) noexcept;

// What the program tells about one tick of a real game (not the demo). `level` is the level the tick was played in (before a level change), `score` the player's score after the tick, `cycles` what
// the tick cost on the game's clock.
struct TickFacts {
    int player = 1;
    int level = 1;
    std::uint32_t score = 0;
    bool acted = false;           // Pac-Man took a step in this tick
    int pacX = 0, pacY = 0;       // where he stands after the tick
    bool pellet = false;          // a dot was eaten
    bool energizer = false;       // an energizer was eaten
    int ghostsEaten = 0;          // ghosts eaten in this tick
    bool frightened = false;      // the ghosts are blue after the tick
    bool fruitEaten = false;
    bool bonusLife = false;       // an extra life was awarded
    bool caught = false;          // Pac-Man lost a life
    bool levelComplete = false;   // the last dot was eaten
    std::int64_t cycles = 0;
};

class AchievementTracker {
public:
    // A game starts (any number of players, the first level `startLevel`: the achievements of the first mazes only count when it is 1, of mazes 4-5 when it is 4 or less).
    void newGame(int players, int startLevel);
    // The unlocked achievements (bit i = kAchievementDefs[i]); the program sets them from its saved progress. onTick and onCartoonDone only report what is new.
    std::uint32_t unlocked() const noexcept { return unlocked_; }
    void setUnlocked(std::uint32_t mask) noexcept { unlocked_ = mask; }
    // Returns the achievements this tick unlocked.
    std::vector<int> onTick(const TickFacts& f);
    // The intermission after a level was watched to its end (1-3).
    std::vector<int> onCartoonDone(int cartoon);
    int ghostsThisSession() const noexcept { return sessionGhosts_; }

private:
    struct Run {   // what one player's game has done so far
        int lastDx = 0, lastDy = 0;
        int lastX = -1, lastY = -1;
        bool started = false;
        bool clean123 = false, clean45 = false, noGhosts15 = false;   // still possible
        std::uint32_t fruitKinds = 0;
        int mazeFruit = 0, mazeStreaks = 0, streak = 0;
        bool mazeDied = false;
        std::int64_t quietCycles = 0;
    };
    void unlock(int i, std::vector<int>& out);
    std::uint32_t unlocked_ = 0;
    std::array<Run, 6> runs_{};
    int startLevel_ = 1;
    int sessionGhosts_ = 0;
};

}  // namespace pacman::core
