#include "ticktrace.h"

#include <cstring>
#include <fstream>
#include <iterator>

#include "../verify/sha256.h"

namespace pacman::core {

int stateOffsetOf(std::uint16_t a) noexcept {
    if (a >= 0x52 && a <= 0x6F) return a - 0x52;
    if (a == 0x78) return 30;
    if (a == 0x79) return 31;
    if (a == 0xE8) return 32;
    if (a == 0xE9) return 33;
    if (a >= 0x84 && a <= 0xD3) return static_cast<int>(kGhostRecordsAt) + (a - 0x84);
    if (a >= 0x623A && a <= 0x65B9) return static_cast<int>(kMapAt) + (a - 0x623A);
    return -1;
}

std::span<const std::uint8_t> TickTrace::state(std::size_t tick) const noexcept {
    if (tick >= ticks()) return {};
    return std::span<const std::uint8_t>(states_).subspan(tick * kStateBytes, kStateBytes);
}

std::uint8_t TickTrace::at(std::size_t tick, std::uint16_t address) const noexcept {
    const int off = stateOffsetOf(address);
    const auto s = state(tick);
    return off < 0 || s.empty() ? std::uint8_t{0} : s[static_cast<std::size_t>(off)];
}

TickTrace TickTrace::load(const std::string& path, std::string& reason) {
    TickTrace t;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        reason = "cannot read " + path;
        return t;
    }
    const std::vector<std::uint8_t> f((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (f.size() < 44 || std::memcmp(f.data(), "TICK", 4) != 0) {
        reason = "not a tick file";
        return t;
    }
    std::uint32_t count = 0, size = 0;
    std::memcpy(&count, f.data() + 4, 4);
    std::memcpy(&size, f.data() + 8, 4);
    if (size != kStateBytes || count == 0) {
        reason = "unexpected state size or empty trace";
        return t;
    }
    std::memcpy(t.golden_.data(), f.data() + 12, 32);
    std::size_t pos = 44;
    const std::size_t stateBytes = static_cast<std::size_t>(count) * size;
    if (pos + stateBytes + 4 > f.size()) {
        reason = "truncated (states)";
        return t;
    }
    t.states_.assign(f.begin() + static_cast<std::ptrdiff_t>(pos), f.begin() + static_cast<std::ptrdiff_t>(pos + stateBytes));
    pos += stateBytes;
    auto u32 = [&](std::uint32_t& v) {
        if (pos + 4 > f.size()) return false;
        std::memcpy(&v, f.data() + pos, 4);
        pos += 4;
        return true;
    };
    std::uint32_t n = 0;
    if (!u32(n) || pos + static_cast<std::size_t>(n) * 8 > f.size()) {
        reason = "truncated (inputs)";
        t.states_.clear();
        return t;
    }
    for (std::uint32_t i = 0; i < n; ++i) {
        TickInput in;
        u32(in.tick);
        u32(in.key);
        t.inputs_.push_back(in);
    }
    if (!u32(n) || pos + static_cast<std::size_t>(n) * 12 != f.size()) {
        reason = "truncated or oversized (pokes)";
        t.states_.clear();
        return t;
    }
    for (std::uint32_t i = 0; i < n; ++i) {
        TickPoke p;
        u32(p.tick);
        u32(p.address);
        u32(p.value);
        t.pokes_.push_back(p);
    }
    return t;
}

std::array<std::uint8_t, 32> TickTrace::chainHash() const {
    std::array<std::uint8_t, 32> h{};
    bool first = true;
    std::vector<std::uint8_t> buf;
    for (std::size_t t = 0; t < ticks(); ++t) {
        buf.clear();
        if (!first) buf.insert(buf.end(), h.begin(), h.end());
        const auto s = state(t);
        buf.insert(buf.end(), s.begin(), s.end());
        h = verify::sha256(buf);
        first = false;
    }
    return h;
}

}  // namespace pacman::core
