#include "achievementbook.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../core/achievements.h"

namespace pacman::app {

namespace {
void makeDirectories(const std::wstring& file) {
    for (std::size_t i = 0; i < file.size(); ++i)
        if (file[i] == L'\\' || file[i] == L'/') CreateDirectoryW(file.substr(0, i).c_str(), nullptr);   // an existing folder is fine
}
constexpr std::uint32_t kAll = (1u << core::kAchievementCount) - 1;
}  // namespace

std::wstring AchievementBook::defaultPath() {
    wchar_t env[MAX_PATH * 2] = {};
    if (GetEnvironmentVariableW(L"PACMAN_ACHIEVEMENTS", env, MAX_PATH * 2) > 0) return env;
    wchar_t base[MAX_PATH * 2] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH * 2) == 0) return L"pacman_achievements.ini";
    return std::wstring(base) + L"\\PacmanAppleIIPort\\achievements.ini";
}

std::string AchievementBook::format(std::uint32_t mask) {
    std::ostringstream o;
    o << "# Pac-Man (Apple II port): achievements unlocked. Written by the program.\n";
    o << "version=1\n";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%06X", mask & kAll);
    o << "unlocked=" << buf << "\n";
    return o.str();
}

std::uint32_t AchievementBook::parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("unlocked=", 0) != 0) continue;
        const std::string v = line.substr(9);
        if (v.empty() || v.size() > 8) return 0;
        std::uint32_t mask = 0;
        for (char c : v) {
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else return 0;
            mask = (mask << 4) | static_cast<std::uint32_t>(d);
        }
        return mask & kAll;
    }
    return 0;
}

void AchievementBook::load() {
    std::ifstream f{std::filesystem::path(path_), std::ios::binary};
    std::uint32_t m = 0;
    if (f) {
        std::ostringstream all;
        all << f.rdbuf();
        m = parse(all.str());
    }
    std::lock_guard<std::mutex> lock(m_);
    mask_ = m;
}

bool AchievementBook::save() const {
    const std::wstring tmp = path_ + L".tmp";
    makeDirectories(path_);
    {
        std::ofstream f(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc);
        if (!f) return false;
        const std::string text = format(mask_);
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        f.flush();
        if (!f) {
            f.close();
            DeleteFileW(tmp.c_str());
            return false;
        }
    }
    if (!MoveFileExW(tmp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

bool AchievementBook::unlock(int index) {
    if (index < 0 || index >= core::kAchievementCount) return false;
    std::lock_guard<std::mutex> lock(m_);
    const std::uint32_t bit = 1u << index;
    if (mask_ & bit) return false;
    mask_ |= bit;
    pending_.push_back(index);
    save();
    return true;
}

void AchievementBook::reset() {
    std::lock_guard<std::mutex> lock(m_);
    mask_ = 0;
    pending_.clear();
    current_ = -1;
    save();
}

std::uint32_t AchievementBook::mask() const {
    std::lock_guard<std::mutex> lock(m_);
    return mask_;
}

int AchievementBook::count() const {
    std::lock_guard<std::mutex> lock(m_);
    int n = 0;
    for (int i = 0; i < core::kAchievementCount; ++i) n += (mask_ >> i) & 1;
    return n;
}

int AchievementBook::points() const {
    std::lock_guard<std::mutex> lock(m_);
    int p = 0;
    for (int i = 0; i < core::kAchievementCount; ++i)
        if ((mask_ >> i) & 1) p += core::kAchievementDefs[static_cast<std::size_t>(i)].points;
    return p;
}

bool AchievementBook::toastWaiting() const {
    std::lock_guard<std::mutex> lock(m_);
    return current_ >= 0 || !pending_.empty();
}

std::optional<int> AchievementBook::toast(std::int64_t nowMs, std::int64_t* ageMs) {
    std::lock_guard<std::mutex> lock(m_);
    if (current_ >= 0 && nowMs - shownAt_ >= kToastMs) current_ = -1;
    if (current_ < 0 && !pending_.empty()) {
        current_ = pending_.front();
        pending_.pop_front();
        shownAt_ = nowMs;
    }
    if (current_ < 0) return std::nullopt;
    if (ageMs) *ageMs = nowMs - shownAt_;
    return current_;
}

}  // namespace pacman::app
