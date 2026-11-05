// TASK-034-ACHIEVEMENTS: the player's progress in the achievement set (core/achievements.h): which are unlocked, kept in %LOCALAPPDATA%\PacmanAppleIIPort\achievements.ini (the environment variable
// PACMAN_ACHIEVEMENTS names another file), and the pop-ups that announce a new one. Thread-safe: the game thread unlocks, the window thread draws and the menu reads.
#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace pacman::app {

class AchievementBook {
public:
    static constexpr std::int64_t kToastMs = 5000;   // how long a pop-up stays
    explicit AchievementBook(std::wstring path) : path_(std::move(path)) {}
    static std::wstring defaultPath();
    const std::wstring& path() const noexcept { return path_; }
    void load();                                   // never fails: a missing or damaged file = nothing unlocked
    bool unlock(int index);                        // true when it is new (saved at once, a pop-up is queued)
    void reset();                                  // forgets all progress (saved)
    std::uint32_t mask() const;
    int count() const;
    int points() const;
    // The pop-up to show at `nowMs` (any steady clock in milliseconds): the index and how long it has been shown, or nothing. Pop-ups follow each other.
    std::optional<int> toast(std::int64_t nowMs, std::int64_t* ageMs = nullptr);
    bool toastWaiting() const;                     // a pop-up is on the screen or waiting for its turn (the window keeps repainting)
    static std::string format(std::uint32_t mask);
    static std::uint32_t parse(const std::string& text);

private:
    bool save() const;
    std::wstring path_;
    mutable std::mutex m_;
    std::uint32_t mask_ = 0;
    std::deque<int> pending_;
    int current_ = -1;
    std::int64_t shownAt_ = 0;
};

}  // namespace pacman::app
