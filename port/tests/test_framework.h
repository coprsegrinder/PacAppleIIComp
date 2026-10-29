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
