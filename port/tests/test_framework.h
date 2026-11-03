// Minimal test framework (no external dependencies). Each TEST registers itself; test_main.cpp runs them.
#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    const char* name;
    void (*run)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures() {
    static int count = 0;
    return count;
}

// Paths to real disk images given on the command line (--image, --reference). Empty = not given: tests that need them skip.
struct Config {
    std::string image;
    std::string reference;
    std::string pages;  // --pages <pages_A.bin>: oracle hi-res pages every 10th tick (tools/record_tick_calls.py, local only)
    std::string tickcalls;  // --tickcalls <tick_calls_A.txt>: per-tick drawing calls of the oracle (tools/record_tick_calls.py, local only)
    std::string page0;  // --page0 <page_tick0.bin>: the oracle hi-res page at the first tick of a game (tools/record_page0.py, local only)
    std::string demo;   // --demo <ticks_DEMO.bin>: attract-demo recording (tools/record_special.py demo; demo_stream.bin must lie next to it)
    std::string titleEvents;  // --titleevents <title_events.bin>: the oracle's routine entries of one attract title turn (tools/export_title_events.py, local only)
    std::string titleSnaps;   // --titlesnaps <title_snaps.bin>: screen memory of the oracle every 50,000 cycles of that turn (tools/capture_title_snaps.py, local only)
    std::string deaths;  // --deaths <death_timing.bin>: measured landmarks of 220 deaths (tools/export_deaths.py, local only)
    std::string clicks;  // --clicks <sound_clicks.bin>: the original's speaker click times of the 12 sounds (tools/export_clicks.py, local only)
    std::string decide;  // --decide <decide_vectors.bin>: ghost decision routine vectors (tools/record_decide_vectors.py, local only)
    std::string drawticks;  // --drawticks a.bin,k.bin,...: recordings for the drawing test (scenarios with call logs and pokes); defaults to --ticks
    std::string ticks;  // --ticks a.bin,b.bin: per-tick oracle recordings (tools/record_ticks.py, local only)
    std::string script;  // --script <cartoonN_script.bin>: ordered drawing calls of an original cartoon (local only)
    std::string dots;  // --dots <dots_vectors.bin>: dot-pass runs captured from the original (local only)
    std::string vectors;  // --vectors <hires_vectors.bin>: drawing calls captured from the original (local only)
};
inline Config& config() {
    static Config c;
    return c;
}

struct Registrar {
    Registrar(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

inline void fail(const char* file, int line, const char* expr, const std::string& note) {
    ++failures();
    std::printf("    FAIL %s:%d: %s%s%s\n", file, line, expr, note.empty() ? "" : "  -- ", note.c_str());
}

}  // namespace testfw

#define TEST(name)                                         \
    static void name();                                    \
    static testfw::Registrar registrar_##name(#name, name); \
    static void name()

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) testfw::fail(__FILE__, __LINE__, #cond, "");    \
    } while (0)

#define CHECK_MSG(cond, note)                                         \
    do {                                                              \
        if (!(cond)) testfw::fail(__FILE__, __LINE__, #cond, (note)); \
    } while (0)
