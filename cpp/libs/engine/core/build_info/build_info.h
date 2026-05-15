#pragma once

// Build version info (todo.md #273).
//
// Production debugging often hits the wall "we know which version was
// running but we can't go back to that exact build" — binary plus source
// snapshot in a known state. This header gives every node a consistent
// startup banner and a query API so triage can correlate a crash dump
// with the exact source tree.
//
// Granularity:
//   - `__DATE__` / `__TIME__`  always available (compiler builtins). They
//                              update on every recompile of THIS file.
//   - `BUILD_GIT_SHA`          optional macro, expected to be defined by
//                              the build system as a string literal (e.g.
//                              `-DBUILD_GIT_SHA="abc1234"`). Falls back to
//                              "unknown" if not set, so the header is
//                              usable in any build environment.
//   - `BUILD_BRANCH`           optional, same shape as BUILD_GIT_SHA.
//
// Why a header instead of a generated .cpp: keeping the macros at the
// translation-unit level guarantees that whoever last recompiled the
// node imprinted their own __DATE__/__TIME__. A separate generated .cpp
// can go stale silently if the build graph doesn't depend on it; the
// header pattern can't.

#include <cstdint>
#include <ctime>
#include <sstream>
#include <string>

#include <muduo/base/Logging.h>

#ifndef BUILD_GIT_SHA
#define BUILD_GIT_SHA "unknown"
#endif

#ifndef BUILD_BRANCH
#define BUILD_BRANCH "unknown"
#endif

namespace build_info {

inline const char* GitSha()
{
    return BUILD_GIT_SHA;
}

inline const char* Branch()
{
    return BUILD_BRANCH;
}

inline const char* CompileDate()
{
    return __DATE__;
}

inline const char* CompileTime()
{
    return __TIME__;
}

// One-line summary for log banners and /version-style diagnostic RPCs.
inline std::string AsString()
{
    std::ostringstream oss;
    oss << "git=" << GitSha()
        << " branch=" << Branch()
        << " compiled=" << CompileDate() << " " << CompileTime();
    return oss.str();
}

// Multi-line variant for the startup banner: scans well in `journalctl`
// output where 80-col wrapping otherwise mangles the one-line form.
inline std::string AsBanner()
{
    std::ostringstream oss;
    oss << "=== Build Info ===\n"
        << "  git_sha:       " << GitSha() << "\n"
        << "  branch:        " << Branch() << "\n"
        << "  compile_date:  " << CompileDate() << "\n"
        << "  compile_time:  " << CompileTime() << "\n"
        << "==================";
    return oss.str();
}

// Convenience: emit the banner via muduo logging at startup.
inline void LogStartupBanner()
{
    LOG_INFO << "\n" << AsBanner();
}

} // namespace build_info
