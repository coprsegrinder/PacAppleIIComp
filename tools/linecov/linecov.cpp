// TASK-034-COVERAGE: a small line-coverage runner for native Windows programs built with MSVC and a PDB (development tool, not part of the game).
// Microsoft's dotnet-coverage could not do this (its native instrumentation is a Visual Studio component that is not in the NuGet package), so this tool does what such tools do:
// it starts the program under the debugger, puts an int3 on the first byte of every source line that the PDB lists (only files whose path contains the filter), removes each int3 when it
// is hit, and writes which lines were reached. A line is covered when any of its addresses ran (templates and inline functions have several).
// Usage: linecov.exe <filter> <out.tsv> <program> [arguments...]        e.g. linecov.exe \port\src\ scratch\linecov.tsv out\cov\pacman_tests.exe --image ...
// Output: one line per source line "file<TAB>line<TAB>hits" (hits 0 = never reached). The program's exit code is returned. Windows x64 only.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

namespace {

struct LineKey {
    std::string file;
    unsigned line;
    bool operator<(const LineKey& o) const { return file != o.file ? file < o.file : line < o.line; }
};

struct Context {
    std::string filter;
    std::map<LineKey, unsigned> hits;                          // every line, with its hit count
    std::unordered_map<DWORD64, std::pair<LineKey, BYTE>> bp;  // address -> line and the original byte
};

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

BOOL CALLBACK onLine(PSRCCODEINFO info, PVOID user) {
    auto* ctx = static_cast<Context*>(user);
    const std::string file = lower(info->FileName);
    if (file.find(ctx->filter) == std::string::npos || info->LineNumber == 0 || info->LineNumber >= 0xF00000) return TRUE;
    LineKey k{file, info->LineNumber};
    ctx->hits.emplace(k, 0u);
    ctx->bp.emplace(info->Address, std::make_pair(k, BYTE{0}));
    return TRUE;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: linecov <path-filter> <out.tsv> <program> [arguments...]\n");
        return 2;
    }
    Context ctx;
    ctx.filter = lower(argv[1]);
    const std::string outPath = argv[2];
    std::string cmd;
    for (int i = 3; i < argc; ++i) {
        if (i > 3) cmd += ' ';
        std::string a = argv[i];
        if (i == 3) std::replace(a.begin(), a.end(), '/', '\\');   // (CreateProcess does not take a program path with forward slashes)
        const bool quote = a.find_first_of(" \t") != std::string::npos || a.empty();
        cmd += quote ? "\"" + a + "\"" : a;
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, DEBUG_ONLY_THIS_PROCESS, nullptr, nullptr, &si, &pi)) {
        std::fprintf(stderr, "cannot start %s (error %lu)\n", cmd.c_str(), GetLastError());
        return 2;
    }
    HANDLE proc = pi.hProcess;
    DWORD exitCode = 1;
    bool symbols = false, loaderBreakSeen = false;
    DEBUG_EVENT ev{};
    for (bool running = true; running;) {
        if (!WaitForDebugEvent(&ev, INFINITE)) break;
        DWORD status = DBG_CONTINUE;
        switch (ev.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT: {
            const DWORD64 base = reinterpret_cast<DWORD64>(ev.u.CreateProcessInfo.lpBaseOfImage);
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_IGNORE_NT_SYMPATH);
            if (SymInitialize(proc, nullptr, FALSE) && SymLoadModuleEx(proc, ev.u.CreateProcessInfo.hFile, nullptr, nullptr, base, 0, nullptr, 0)) {
                symbols = SymEnumSourceLines(proc, base, nullptr, nullptr, 0, 0, onLine, &ctx) != FALSE;
                std::fprintf(stderr, "linecov: %zu lines, %zu addresses to watch%s\n", ctx.hits.size(), ctx.bp.size(), symbols ? "" : " (no line information!)");
                for (auto& [addr, entry] : ctx.bp) {
                    BYTE orig = 0;
                    SIZE_T n = 0;
                    if (ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(addr), &orig, 1, &n) && n == 1) {
                        entry.second = orig;
                        const BYTE int3 = 0xCC;
                        WriteProcessMemory(proc, reinterpret_cast<LPVOID>(addr), &int3, 1, &n);
                    }
                }
                FlushInstructionCache(proc, nullptr, 0);
            }
            if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
            break;
        }
        case EXCEPTION_DEBUG_EVENT: {
            const EXCEPTION_RECORD& er = ev.u.Exception.ExceptionRecord;
            if (er.ExceptionCode == EXCEPTION_BREAKPOINT) {
                const DWORD64 addr = reinterpret_cast<DWORD64>(er.ExceptionAddress);
                auto it = ctx.bp.find(addr);
                if (it != ctx.bp.end()) {
                    ++ctx.hits[it->second.first];
                    SIZE_T n = 0;
                    WriteProcessMemory(proc, reinterpret_cast<LPVOID>(addr), &it->second.second, 1, &n);
                    FlushInstructionCache(proc, nullptr, 0);
                    HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, ev.dwThreadId);
                    if (th) {
                        CONTEXT c{};
                        c.ContextFlags = CONTEXT_CONTROL;
                        if (GetThreadContext(th, &c)) {
                            c.Rip = addr;   // (back to the instruction the int3 replaced)
                            SetThreadContext(th, &c);
                        }
                        CloseHandle(th);
                    }
                    ctx.bp.erase(it);
                } else if (!loaderBreakSeen) {
                    loaderBreakSeen = true;   // the loader's own breakpoint
                } else {
                    status = DBG_EXCEPTION_NOT_HANDLED;
                }
            } else {
                status = ev.u.Exception.dwFirstChance ? DBG_EXCEPTION_NOT_HANDLED : DBG_EXCEPTION_NOT_HANDLED;
            }
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
            exitCode = ev.u.ExitProcess.dwExitCode;
            running = false;
            break;
        case LOAD_DLL_DEBUG_EVENT:
            if (ev.u.LoadDll.hFile) CloseHandle(ev.u.LoadDll.hFile);
            break;
        default: break;
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status);
    }
    if (symbols) SymCleanup(proc);
    CloseHandle(pi.hThread);
    CloseHandle(proc);
    FILE* f = std::fopen(outPath.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
        return 2;
    }
    for (const auto& [k, n] : ctx.hits) std::fprintf(f, "%s\t%u\t%u\n", k.file.c_str(), k.line, n);
    std::fclose(f);
    std::fprintf(stderr, "linecov: program exited with %lu, report in %s\n", exitCode, outPath.c_str());
    return static_cast<int>(exitCode);
}
