// TASK-034 Achievements: the 22 definitions and the rules that unlock them (src/core/achievements.h), driven with made-up facts about ticks.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <filesystem>
#include <fstream>

#include "../src/app/achievementbook.h"
#include "../src/app/pausemenu.h"
#include "../src/core/achievementfacts.h"
#include "../src/core/achievements.h"
#include "../src/core/gamedriver.h"
#include "flow_common.h"
#include "test_framework.h"

using namespace pacman;
using core::AchievementTracker;
using core::TickFacts;

namespace {

bool has(const std::vector<int>& v, int i) {
    for (int x : v)
        if (x == i) return true;
    return false;
}

TickFacts tick(int level, std::int64_t cycles = 5000) {
    TickFacts f;
    f.level = level;
    f.cycles = cycles;
    return f;
}

// Pac-Man's steps: `n` steps from x along the row, `dir` +1 right / -1 left
std::vector<int> walk(AchievementTracker& t, int level, int fromX, int dir, int n) {
    std::vector<int> all;
    for (int i = 0; i < n; ++i) {
        TickFacts f = tick(level);
        f.acted = true;
        f.pacX = fromX + dir * 2 * i;
        f.pacY = 100;
        for (int u : t.onTick(f)) all.push_back(u);
    }
    return all;
}

}  // namespace

TEST(the_achievement_set_has_22_achievements_worth_190_points_with_the_wording_corrected) {
    CHECK(core::kAchievementDefs.size() == 22);
    int points = 0;
    std::set<std::string> titles;
    for (const auto& d : core::kAchievementDefs) {
        points += d.points;
        titles.insert(d.title);
        const std::string text = std::string(d.title) + " " + d.description;
        CHECK(text.find("recieve") == std::string::npos && text.find("turing") == std::string::npos && text.find("oblin") == std::string::npos);
        CHECK(std::strlen(d.title) > 0 && std::strlen(d.title) <= 30 && std::strlen(d.description) > 0);
    }
    CHECK(points == 190 && titles.size() == 22);
    CHECK(std::string(core::kAchievementDefs[core::kExtraChances].description).find("receive an extra turn") != std::string::npos);
    CHECK(std::string(core::kAchievementDefs[core::kSpooky].description).find("ghosts") != std::string::npos);
    CHECK(core::kAchievementDefs[core::kForwards].points == 25 && core::kAchievementDefs[core::kCantTurnBack].points == 25);
}

TEST(the_fruit_of_a_level_follows_the_games_own_fruit_table) {
    CHECK(core::fruitOfLevel(1) == 0 && core::fruitOfLevel(2) == 1 && core::fruitOfLevel(3) == 2 && core::fruitOfLevel(4) == 2 && core::fruitOfLevel(5) == 3 && core::fruitOfLevel(6) == 3);
    CHECK(core::fruitOfLevel(7) == 4 && core::fruitOfLevel(8) == 4 && core::fruitOfLevel(9) == 5 && core::fruitOfLevel(10) == 5 && core::fruitOfLevel(11) == 6 && core::fruitOfLevel(12) == 6);
    CHECK(core::fruitOfLevel(13) == 7 && core::fruitOfLevel(21) == 7 && core::fruitOfLevel(99) == 7);
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED the table (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    const auto t = pack.find(0x6AFE, 27 * 4);   // the image address and lifetime of the bonus fruit, per level (index = level)
    CHECK(t.size() == 27u * 4u);
    if (t.size() < 27 * 4) return;
    int groups = 1;
    for (int level = 1; level < 22; ++level) {   // (level 23 has an image of its own in the original's table, a quirk: the set says "maze 13 or higher" for the key)
        const bool sameImage = t[static_cast<std::size_t>(level) * 4] == t[static_cast<std::size_t>(level + 1) * 4] && t[static_cast<std::size_t>(level) * 4 + 1] == t[static_cast<std::size_t>(level + 1) * 4 + 1];
        CHECK(sameImage == (core::fruitOfLevel(level) == core::fruitOfLevel(level + 1)));   // the fruit changes at the levels the set names
        groups += !sameImage;
    }
    CHECK(groups == 8);   // cherry, strawberry, peach, apple, lemon, pineapple, bell, key
    std::printf("    the table has %d fruit groups over levels 1-22\n", groups);
}

TEST(scores_extra_turn_and_the_intermissions_unlock_what_they_name) {
    AchievementTracker t;
    t.newGame(1, 1);
    TickFacts f = tick(1);
    f.score = 19990;
    CHECK(t.onTick(f).empty());
    f.score = 20000;
    CHECK(has(t.onTick(f), core::kTaxMan) && !(t.unlocked() & (1u << core::kExpert)));
    f.score = 50010;
    CHECK(has(t.onTick(f), core::kExpert) && t.onTick(f).empty());   // (only once)
    f = tick(1);
    f.bonusLife = true;
    CHECK(has(t.onTick(f), core::kExtraChances));
    CHECK(has(t.onCartoonDone(1), core::kCoffeeBreak) && has(t.onCartoonDone(2), core::kAnotherBreak) && has(t.onCartoonDone(3), core::kThirdBreak) && t.onCartoonDone(1).empty() && t.onCartoonDone(4).empty());
    // a fresh tracker for the saved progress
    AchievementTracker u;
    u.setUnlocked(t.unlocked());
    f = tick(1);
    f.bonusLife = true;
    CHECK(u.onTick(f).empty());
}

TEST(fruit_achievements_count_the_fruit_of_a_maze_and_of_a_game) {
    AchievementTracker t;
    t.newGame(1, 1);
    const auto eat = [&](int level) {
        TickFacts f = tick(level);
        f.fruitEaten = true;
        return t.onTick(f);
    };
    const auto done = [&](int level) {
        TickFacts f = tick(level);
        f.levelComplete = true;
        return t.onTick(f);
    };
    CHECK(eat(1).empty());
    CHECK(has(eat(1), core::kCherry));
    done(1);
    CHECK(eat(2).empty());
    done(2);                                  // one strawberry in maze 2 is not enough: the count is per maze
    CHECK(!(t.unlocked() & (1u << core::kStrawberry)));
    CHECK(eat(2).empty() && has(eat(2), core::kStrawberry));   // (a repeat of maze 2: two more)
    done(2);
    eat(3);
    CHECK(has(eat(3), core::kPeach));
    done(3);
    eat(4);
    done(4);                                  // one in each of 3 and 4 is not the set's "2 peaches in maze 3 or 4"
    eat(5);
    CHECK(has(eat(5), core::kApples));
    done(5);
    eat(7);
    CHECK(has(eat(7), core::kLemon));
    done(7);
    CHECK(!(t.unlocked() & (1u << core::kHealthyDiet)));   // (no pineapple yet)
    CHECK(has(eat(9), core::kPineapple));     // one is enough
    CHECK(t.unlocked() & (1u << core::kHealthyDiet));        // ... and now every fruit of the game has been eaten
    done(9);
    CHECK(has(eat(11), core::kBell));
    CHECK(has(eat(13), core::kKey) && eat(14).empty());
    // one of every fruit in one game: cherry, strawberry, peach, apple, lemon, pineapple
    AchievementTracker g;
    g.newGame(1, 1);
    for (int level : {1, 2, 3, 5, 7}) {
        TickFacts f = tick(level);
        f.fruitEaten = true;
        CHECK(!has(g.onTick(f), core::kHealthyDiet));
    }
    TickFacts last = tick(9);
    last.fruitEaten = true;
    CHECK(has(g.onTick(last), core::kHealthyDiet));
    // a new game starts the collection again
    g.setUnlocked(0);
    g.newGame(1, 1);
    for (int level : {1, 2, 3, 5, 7}) {
        TickFacts f = tick(level);
        f.fruitEaten = true;
        g.onTick(f);
    }
    CHECK(!has(g.onTick(tick(7)), core::kHealthyDiet));
}

TEST(ghost_achievements_streak_of_four_slayer_and_peace) {
    AchievementTracker t;
    t.newGame(1, 1);
    const auto energizer = [&] {
        TickFacts f = tick(1);
        f.energizer = true;
        f.frightened = true;
        return t.onTick(f);
    };
    const auto eatGhost = [&](bool frightenedAfter = true) {
        TickFacts f = tick(1);
        f.ghostsEaten = 1;
        f.frightened = frightenedAfter;
        return t.onTick(f);
    };
    energizer();
    eatGhost();
    eatGhost();
    eatGhost();
    CHECK(has(eatGhost(), core::kSpooky));
    // four ghosts from two energizers are not a streak
    AchievementTracker u;
    u.newGame(1, 1);
    const auto ue = [&] {
        TickFacts f = tick(1);
        f.energizer = true;
        f.frightened = true;
        u.onTick(f);
    };
    const auto ug = [&](bool fr) {
        TickFacts f = tick(1);
        f.ghostsEaten = 1;
        f.frightened = fr;
        return u.onTick(f);
    };
    ue();
    ug(true);
    ug(true);
    ue();
    ug(true);
    CHECK(!has(ug(true), core::kSpooky));
    // the timer ending between two ghosts also breaks the streak
    AchievementTracker v;
    v.newGame(1, 1);
    TickFacts e = tick(1);
    e.energizer = true;
    e.frightened = true;
    v.onTick(e);
    for (int i = 0; i < 3; ++i) {
        TickFacts f = tick(1);
        f.ghostsEaten = 1;
        f.frightened = i < 2;
        v.onTick(f);
    }
    TickFacts f4 = tick(1);
    f4.ghostsEaten = 1;
    f4.frightened = true;
    CHECK(!has(v.onTick(f4), core::kSpooky));
    // 50 ghosts in a session, through game overs
    AchievementTracker s;
    s.newGame(1, 1);
    for (int i = 0; i < 49; ++i) {
        TickFacts f = tick(3);
        f.ghostsEaten = 1;
        f.energizer = true;   // (every one its own energizer)
        f.frightened = true;
        CHECK(!has(s.onTick(f), core::kGhostSlayer));
        if (i == 20) s.newGame(1, 1);   // a new game: the session counter goes on
    }
    TickFacts f50 = tick(3);
    f50.ghostsEaten = 1;
    f50.frightened = true;
    CHECK(has(s.onTick(f50), core::kGhostSlayer) && s.ghostsThisSession() == 50);
    // five mazes without a ghost
    AchievementTracker p;
    p.newGame(1, 1);
    for (int level = 1; level <= 4; ++level) {
        TickFacts f = tick(level);
        f.levelComplete = true;
        CHECK(!has(p.onTick(f), core::kPeace));
    }
    TickFacts f5 = tick(5);
    f5.levelComplete = true;
    CHECK(has(p.onTick(f5), core::kPeace));
    // a ghost in maze 3 spoils it
    AchievementTracker q;
    q.newGame(1, 1);
    TickFacts g3 = tick(3);
    g3.ghostsEaten = 1;
    q.onTick(g3);
    for (int level = 1; level <= 5; ++level) {
        TickFacts f = tick(level);
        f.levelComplete = true;
        CHECK(!has(q.onTick(f), core::kPeace));
    }
    // a game that starts at maze 3 cannot earn it
    AchievementTracker r;
    r.newGame(1, 3);
    for (int level = 3; level <= 5; ++level) {
        TickFacts f = tick(level);
        f.levelComplete = true;
        CHECK(!has(r.onTick(f), core::kPeace) && !has(r.onTick(f), core::kForwards));
    }
}

TEST(never_turning_round_through_the_first_mazes_and_the_fourth_and_fifth) {
    const auto done = [](AchievementTracker& t, int level) {
        TickFacts f = tick(level);
        f.levelComplete = true;
        return t.onTick(f);
    };
    // a clean run
    {
        AchievementTracker t;
        t.newGame(1, 1);
        for (int level = 1; level <= 3; ++level) {
            walk(t, level, 40, 1, 20);       // right
            walk(t, level, 80, 1, 5);
            const auto turn = [&] {          // a turn to a side is not a 180: down
                TickFacts f = tick(level);
                f.acted = true;
                f.pacX = 120;
                f.pacY = 110;
                t.onTick(f);
            };
            turn();
            const auto u = done(t, level);
            CHECK(has(u, core::kForwards) == (level == 3));
        }
        for (int level = 4; level <= 5; ++level) {
            walk(t, level, 40, 1, 10);
            const auto u = done(t, level);
            CHECK(has(u, core::kCantTurnBack) == (level == 5));
        }
    }
    // a turn round in maze 2 spoils the first run only
    {
        AchievementTracker t;
        t.newGame(1, 1);
        done(t, 1);
        walk(t, 2, 40, 1, 6);
        walk(t, 2, 52, -1, 6);   // back the way he came
        done(t, 2);
        CHECK(!has(done(t, 3), core::kForwards));
        walk(t, 4, 40, 1, 10);
        done(t, 4);
        CHECK(has(done(t, 5), core::kCantTurnBack));
    }
    // a death in maze 4 spoils the second run
    {
        AchievementTracker t;
        t.newGame(1, 1);
        done(t, 1);
        done(t, 2);
        CHECK(has(done(t, 3), core::kForwards));
        TickFacts d = tick(4);
        d.caught = true;
        t.onTick(d);
        done(t, 4);
        CHECK(!has(done(t, 5), core::kCantTurnBack));
    }
    // the tunnel is not a turn: he goes left through it, the step across is far
    {
        AchievementTracker t;
        t.newGame(1, 1);
        walk(t, 1, 10, -1, 5);      // x 10, 8, 6, 4, 2
        TickFacts wrap = tick(1);
        wrap.acted = true;
        wrap.pacX = 250;            // out at the other side
        wrap.pacY = 100;
        t.onTick(wrap);
        walk(t, 1, 248, -1, 5);
        done(t, 1);
        done(t, 2);
        CHECK(has(done(t, 3), core::kForwards));
    }
    // starting at maze 4: the fourth and fifth can be earned, the first three cannot
    {
        AchievementTracker t;
        t.newGame(1, 4);
        done(t, 4);
        CHECK(has(done(t, 5), core::kCantTurnBack));
    }
}

TEST(three_quiet_minutes_and_the_perfect_maze) {
    // three minutes of the game's clock without eating anything or dying
    {
        AchievementTracker t;
        t.newGame(1, 1);
        const std::int64_t perTick = 5000, total = 3LL * 60 * 1'022'727;
        std::int64_t at = 0;
        bool got = false;
        while (at < total + 2 * perTick && !got) {
            at += perTick;
            got = has(t.onTick(tick(1, perTick)), core::kStarvation);
            if (got) CHECK(at >= total && at < total + 2 * perTick);
        }
        CHECK(got);
    }
    // a pellet restarts the count
    {
        AchievementTracker t;
        t.newGame(1, 1);
        const std::int64_t total = 3LL * 60 * 1'022'727;
        for (std::int64_t at = 0; at < total - 100'000; at += 5000) CHECK(!has(t.onTick(tick(1)), core::kStarvation));
        TickFacts eat = tick(1);
        eat.pellet = true;
        t.onTick(eat);
        for (std::int64_t at = 0; at < total - 100'000; at += 5000) CHECK(!has(t.onTick(tick(1)), core::kStarvation));
    }
    // two ghost streaks and two fruits in a maze without a death
    const auto streakOf4 = [](AchievementTracker& t, int level) {
        TickFacts e = tick(level);
        e.energizer = true;
        e.frightened = true;
        t.onTick(e);
        for (int i = 0; i < 4; ++i) {
            TickFacts f = tick(level);
            f.ghostsEaten = 1;
            f.frightened = true;
            t.onTick(f);
        }
    };
    const auto fruit = [](AchievementTracker& t, int level) {
        TickFacts f = tick(level);
        f.fruitEaten = true;
        t.onTick(f);
    };
    const auto done = [](AchievementTracker& t, int level) {
        TickFacts f = tick(level);
        f.levelComplete = true;
        return t.onTick(f);
    };
    {
        AchievementTracker t;
        t.newGame(1, 1);
        streakOf4(t, 6);
        fruit(t, 6);
        streakOf4(t, 6);
        fruit(t, 6);
        CHECK(has(done(t, 6), core::kPerfectionist));
    }
    {   // a death spoils it, one streak or one fruit short too
        AchievementTracker t;
        t.newGame(1, 1);
        streakOf4(t, 6);
        fruit(t, 6);
        streakOf4(t, 6);
        fruit(t, 6);
        TickFacts d = tick(6);
        d.caught = true;
        t.onTick(d);
        CHECK(!has(done(t, 6), core::kPerfectionist));
        streakOf4(t, 7);
        fruit(t, 7);
        fruit(t, 7);
        CHECK(!has(done(t, 7), core::kPerfectionist));
        streakOf4(t, 8);
        streakOf4(t, 8);
        fruit(t, 8);
        CHECK(!has(done(t, 8), core::kPerfectionist));
        // and the counts of one maze do not carry into the next
        streakOf4(t, 9);
        fruit(t, 9);
        done(t, 9);
        streakOf4(t, 10);
        fruit(t, 10);
        CHECK(!has(done(t, 10), core::kPerfectionist));
    }
}

TEST(every_title_and_description_fits_the_achievements_page) {
    for (const auto& d : core::kAchievementDefs) {
        CHECK(std::strlen(d.title) <= 30);                                              // the list shows the title in 30 characters
        const auto lines = app::PauseMenu::wrapUpper(d.description, 36);
        CHECK(!lines.empty() && lines.size() <= 3);                                     // the description gets three lines
        for (const auto& l : lines) CHECK(l.size() <= 36);
    }
    const auto lines = app::PauseMenu::wrapUpper("Eat 4 ghosts in a row", 10);
    CHECK(lines.size() == 3 && lines[0] == "EAT 4" && lines[1] == "GHOSTS IN" && lines[2] == "A ROW");
}

TEST(the_achievement_book_keeps_progress_in_a_file_and_times_the_pop_ups) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "pacman_ach_test";
    std::filesystem::remove_all(dir);
    const std::wstring file = (dir / "achievements.ini").wstring();
    {
        app::AchievementBook book(file);
        book.load();
        CHECK(book.mask() == 0 && book.count() == 0 && book.points() == 0);
        CHECK(book.unlock(core::kTaxMan) && !book.unlock(core::kTaxMan) && book.unlock(core::kStarvation) && !book.unlock(-1) && !book.unlock(22));
        CHECK(book.count() == 2 && book.points() == 5 + 10);
        CHECK(std::filesystem::exists(file));
    }
    {   // a new run reads it back
        app::AchievementBook book(file);
        book.load();
        CHECK(book.mask() == ((1u << core::kTaxMan) | (1u << core::kStarvation)) && book.count() == 2);
        // the pop-ups follow each other, each stays 5 seconds
        std::int64_t age = 0;
        CHECK(!book.toast(1000, &age));
        CHECK(book.unlock(core::kPeace) && book.unlock(core::kExpert) && book.toastWaiting());
        CHECK(book.toast(2000, &age) == core::kPeace && age == 0);
        CHECK(book.toast(5000, &age) == core::kPeace && age == 3000);
        CHECK(book.toast(7000, &age) == core::kExpert && age == 0);   // the next one, 5 s after the first was shown
        CHECK(!book.toast(12000, &age) && !book.toastWaiting());
        book.reset();
        CHECK(book.mask() == 0 && !book.toastWaiting());
    }
    {   // and the reset was kept
        app::AchievementBook book(file);
        book.load();
        CHECK(book.mask() == 0);
    }
    // damaged files read as nothing unlocked; stray bits are dropped
    CHECK(app::AchievementBook::parse("") == 0 && app::AchievementBook::parse("unlocked=zz\n") == 0 && app::AchievementBook::parse("unlocked=123456789\n") == 0);
    CHECK(app::AchievementBook::parse("version=1\nunlocked=3FFFFF\r\n") == (1u << 22) - 1 && app::AchievementBook::parse("unlocked=FFFFFFFF\n") == (1u << 22) - 1);
    CHECK(app::AchievementBook::parse(app::AchievementBook::format(0x2A5B3)) == 0x2A5B3);
    std::filesystem::remove_all(dir);
}

TEST(the_facts_of_real_ticks_unlock_cherry_chomper_and_the_score_ones_in_a_real_game) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    core::GameDriver drv;
    CHECK(drv.init(pack));
    CHECK(drv.newGame(1, 1));
    AchievementTracker tracker;
    tracker.newGame(1, 1);
    std::vector<int> unlocked;
    int fruits = 0;
    bool sawFruitFact = false, sawPelletFact = false;
    for (int t = 0; t < 4000 && fruits < 2; ++t) {
        core::GameState& st = drv.core().state();
        const int level = st.level(), player = drv.core().player();
        const std::uint32_t before = st.score();
        if (t % 40 == 0) st.requestedDirection() = static_cast<std::uint8_t>((t / 40) % 4);
        if (drv.core().fruitState() != 1 && t > 100 && t % 150 == 0) st.zp(0x57) = 0;   // the next bonus fruit starts
        if (drv.core().fruitState() == 1) {                                               // ... and Pac-Man is put on it
            st.pacX() = 0x8C;
            st.pacY() = 0x6B;
        }
        const core::TickEvents ev = drv.tick();
        if (ev.caught || ev.levelComplete) {
            drv.newGame(1, 1);
            tracker.newGame(1, 1);
            continue;
        }
        const core::TickFacts f = core::tickFacts(drv, ev, level, player, before, 5000);
        sawPelletFact = sawPelletFact || f.pellet;
        if (f.fruitEaten) {
            sawFruitFact = true;
            ++fruits;
            CHECK(f.level == 1 && f.player == 1);
        }
        for (int i : tracker.onTick(f)) unlocked.push_back(i);
    }
    std::printf("    in the real game: %d fruits eaten, pellets seen: %d, unlocked: %zu\n", fruits, sawPelletFact ? 1 : 0, unlocked.size());
    CHECK(sawFruitFact && fruits == 2);
    CHECK(has(unlocked, core::kCherry));   // two cherries in the first maze
}
