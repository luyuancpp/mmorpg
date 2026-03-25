#pragma once

#include <muduo/base/Logging.h>
#include "../utils/debug/stacktrace_system.h"

// ─── MAKE_ERROR / MAKE_ERROR_MSG ──────────────────────────────────────
//
// Generic error-value factory with automatic diagnostics.
// Works with any return type (uint32_t, Guid, NodeId, enum, ...).
// Always used with an explicit `return`:
//
//   return MAKE_ERROR(kSkillInvalidTargetId);
//   return MAKE_ERROR_MSG(kSkillCooldownNotReady,
//              "skill_id=" << skillTable->id()
//              << " caster=" << entt::to_integral(casterEntity));
//
// LOG_ERROR already records __FILE__:__LINE__, so we don't repeat it.
// The macro only adds: error code name, optional context, and stack trace.
//
// Extension:
//   Add hooks (metrics, analytics) in the macro body without
//   changing any call site.
// ──────────────────────────────────────────────────────────────────────

inline constexpr size_t kErrorReturnStackDepth = 5;

// Basic: log error code name + stack trace.
// LOG_ERROR provides file:line automatically.
#define MAKE_ERROR(error_code) \
    [&]() { \
        LOG_ERROR << #error_code << "(" << (error_code) << ")"; \
        PrintCurrentStackTrace(kErrorReturnStackDepth); \
        return (error_code); \
    }()

// With additional context params (stream expression).
#define MAKE_ERROR_MSG(error_code, msg_stream) \
    [&]() { \
        LOG_ERROR << #error_code << "(" << (error_code) << ")" \
                  << " | " << msg_stream; \
        PrintCurrentStackTrace(kErrorReturnStackDepth); \
        return (error_code); \
    }()

