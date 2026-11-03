// TASK-029h: the integration of the verified pieces into ONE game tick, in the order of the original main loop ($530F):
//   $7861  Pac-Man: input poll, step timer, junction logic, step, eating          (inputs/steptimer/junction/pacstep/eating)
//   $86EA  the ghosts: their steps (ghost logic = M5, supplied by a GhostSource) and, per ghost 4..1, the contact test (near delay, eaten, caught)
//   $6A7C  the bonus fruit (M6: not rebuilt yet; the caller may supply the bytes it owns)
//   $570C  the blinks: 1UP label ($E5/$E6) and the energizer phase ($5E/$5F)
// A caught Pac-Man ends the tick with the restarted life (lifestart), a completed maze ends it with `levelComplete` set (the next level's set-up is M6).
#pragma once

#include <array>
#include <cstdint>

#include "../datapack/datapack.h"
#include "eating.h"
#include "gamestate.h"
#include "input.h"
#include "junction.h"
#include "lifestart.h"
#include "ghostmode.h"
#include "lives.h"
#include "sound.h"
#include "steptimer.h"

namespace pacman::core {

// Moves the ghosts for this tick (the original's $86EA loop up to the contact tests). M5 implements it; the tests feed the oracle's ghost records.
class GhostSource {
public:
    virtual ~GhostSource() = default;
    // Returns which ghosts acted in this tick (their step timer ran out): only those run the contact test of $8B34.
    virtual std::array<bool, 4> step(GameState& state, bool energizerEaten) = 0;
};

class NoGhosts : public GhostSource {
public:
    std::array<bool, 4> step(GameState&, bool) override { return {}; }
};

struct TickEvents {
    bool paused = false;          // Esc: nothing happened
    bool acted = false;           // Pac-Man's step timer ran out
    bool ate = false, energizer = false;
    bool levelComplete = false;   // the last item was eaten: the tick ends here, the next level's set-up is not done by this class
    bool outsideMap = false;      // a poked position with stale cell counters (the original ate from a copy behind the map; not modelled)
    bool caught = false;          // Pac-Man was caught: the state is the restarted life (or game over: lives == 0)
    bool gameOver = false;        // the last life was lost
    bool playerSwitched = false;  // Pac-Man was caught and another player's turn begins (his bytes, his maze); gameOver is only set when NO player has a life left
    bool demoOver = false;        // the attract game's Pac-Man was caught: the demo ends (the state is the moment of the catch)
    std::array<bool, 4> ghostEaten{};
    std::vector<SoundId> sounds;  // the sounds the original starts in this tick, in order (none in the demo); each freezes the game for soundFreezeCycles()
};

class GameCore {
public:
    bool init(const datapack::Pack& pack);                    // loads the tables; false if the pack lacks them
    // State of the first tick of a game for `players` (1-5) players, all starting at level `rank` ($54D4: every player's bytes and maze are saved; player 1 plays first). TASK-033-6.
    bool newGame(int players = 1, int rank = 1);
    bool newDemo();                                           // state of the first tick of the attract game ($E8 = 1, no lives, $79 = 0, the leftovers of the title in the first ghost queue)
    GameState& state() noexcept { return state_; }
    const GameState& state() const noexcept { return state_; }
    InputPoller& input() noexcept { return poller_; }
    KeyConfig& keys() noexcept { return keys_; }
    PortInput& port() noexcept { return port_; }
    int player() const noexcept { return player_; }
    int players() const noexcept { return players_; }
    // The score (BCD, 24 bits) of player `p` as it is kept for him while another player plays (his $52-$54); for the current player the state holds it.
    std::uint8_t savedLives(int p) const noexcept { return stores_[static_cast<std::size_t>(p)].save.bytes[6]; }   // $58 of the saved player
    std::uint32_t savedScore(int p) const noexcept { const auto& b = stores_[static_cast<std::size_t>(p)].save.bytes; return b[0] | (b[1] << 8) | (b[2] << 16); }
    std::uint8_t blink1UpCounter() const noexcept { return e5_; }
    std::uint8_t blink1UpPhase() const noexcept { return e6_; }
    std::uint8_t fruitState() const noexcept { return e7_; }      // $E7: 0 none, 1 on the screen, 2 its points are shown

    // One tick with the ghosts moved by the caller's GhostSource (the tests feed the oracle's records) ...
    TickEvents tick(GhostSource& ghosts);
    // ... and one tick with the ghost logic of this class (M5): the mode timer, the energizer's effect, the ghosts' turns, their contact with Pac-Man.
    TickEvents tick();
    GhostShared& ghostShared() noexcept { return ghostShared_; }
    const GameState& caughtState() const noexcept { return caughtState_; }   // the state at the moment of the last catch (before the new life was set up)
    GhostQueues& ghostQueues() noexcept { return queues_; }
    // The level change (TASK-031-3): level + 1, the pristine maze, the counters of a fresh level, the score and the lives kept, then the start of a life.
    // VERIFIED against the oracle: the resulting logical state equals the first tick of the new level on 7 level ends (levels 1, 2, 4, 5, 9, 20; test_gamecore).
    // Not covered: the ghost move queues and the fruit/blink bytes outside the logical state, the cartoon, the flash and the screen rebuild (drawing is in tickdraw).
    void startNextLevel();
    // For recordings whose position was poked without the cell counters: the original ate an item from a copy of the map; the delay it causes is applied here.
    // Only the delay counter F3 is set (the caller already holds the oracle's $59).
    void itemEatenOutsideTheMap() {
        const std::uint8_t keep = state_.pacStepCounter();
        timer_.itemEaten(state_, steps_);
        state_.pacStepCounter() = keep;
    }

private:
    TickEvents tickImpl(GhostSource* fed);
    bool contactTest(int ghostIndex, TickEvents& ev);   // $8B34 and what follows for one ghost; true if Pac-Man was caught (the tick ends)
    void blinks();
    void fruitEatenCheck(TickEvents& ev);
    void fruit();
    const datapack::Pack* pack_ = nullptr;
    GameState state_;
    InputPoller poller_;
    KeyConfig keys_;
    PortInput port_;
    JunctionGraph graph_;
    StepTables steps_;
    StartTables start_;
    StepTimer timer_;
    int player_ = 1;                     // $50: the player who plays now
    int players_ = 1;                    // $51
    struct PlayerStore {
        PlayerSave save;                 // $0300 + 8 x player ($53BE / $53CF)
        std::array<std::uint8_t, 896> map{};   // the player's maze ($5394 / $5360)
    };
    std::array<PlayerStore, 6> stores_{};
    std::uint8_t e5_ = 0, e6_ = 0;       // $E5 / $E6: the 1UP blink counter and phase (outside the logical state)
    GhostShared ghostShared_;
    GameState caughtState_;
    GhostQueues queues_{};
    GhostTables ghostTables_;
    GhostDecisionData ghostData_;
    GhostModeTables ghostModes_;
    std::uint8_t e7_ = 0;                // $E7: the bonus fruit state (outside the logical state)
    std::uint16_t demoPointer_ = DemoStream::kStart;
    std::array<std::uint8_t, 16> energizerBlink_{}, oneUpBlink_{}, fruitPoints_{};   // $5733, $5903, $6CD4
    std::array<std::uint8_t, 27 * 4> fruitTable_{};                                   // $6AFE: image address and lifetime per level
};

}  // namespace pacman::core
