#pragma once

#include <muduo/base/Logging.h>
#include <google/protobuf/message.h>

#include "core/utils/debug/stacktrace_system.h"

inline uint32_t PrintStackAndReturnError(const uint32_t error_code) {
    LOG_ERROR << "Stack trace (error code: " << error_code << "):";
    LOG_ERROR << GetCurrentStackTraceAsString(kMaxEntries);
    return error_code;
}

inline uint32_t PrintStackAndReturnError(const uint32_t error_code, size_t maxEntries) {
    LOG_ERROR << "Stack trace (error code: " << error_code << "):";
    LOG_ERROR << GetCurrentStackTraceAsString(maxEntries);
    return error_code;
}

// Unified error-context dump for the rejected-RPC path (todo.md #70 + #125).
//
// Whenever a handler / RPC stub returns a non-zero error code, the caller
// usually has the originating request message in hand. Calling this helper
// at the rejection site logs everything an on-call needs to triage the bug
// without going back to the client to ask "what did you send":
//
//   - the error code (numeric, mapped via error_tip table downstream)
//   - the protobuf message type name (so the on-call knows which RPC fired)
//   - the request payload (DebugString — short-form key=value, multi-line)
//   - the current stack trace
//
// `tag` is a short label (function name / handler name) that prefixes the
// log line — pick something greppable, e.g. "AssignGate", "EnterScene".
//
// Cost: DebugString allocates and serializes the entire request. Acceptable
// because this is the error path; we are already returning failure, so an
// extra few microseconds of log work does not affect happy-path latency.
inline uint32_t LogErrorContextAndReturn(const char* tag,
                                         uint32_t error_code,
                                         const ::google::protobuf::Message& request) {
    const auto* descriptor = request.GetDescriptor();
    // protobuf >=27 returns absl::string_view from full_name(); muduo
    // LogStream doesn't accept that. Materialize to std::string.
    const std::string proto_name = descriptor
        ? std::string(descriptor->full_name())
        : std::string("<unknown>");
    LOG_ERROR << "[" << (tag ? tag : "?") << "] rejecting RPC: error_code=" << error_code
              << " proto=" << proto_name
              << " request={ " << request.ShortDebugString() << " }";
    LOG_ERROR << GetCurrentStackTraceAsString(kMaxEntries);
    return error_code;
}

// Variant for sites without the request message (e.g. a malformed packet
// where parsing already failed, so we have no proto to dump). Same log
// shape minus the request body.
inline uint32_t LogErrorContextAndReturn(const char* tag, uint32_t error_code) {
    LOG_ERROR << "[" << (tag ? tag : "?") << "] rejecting RPC: error_code=" << error_code
              << " (no request payload available)";
    LOG_ERROR << GetCurrentStackTraceAsString(kMaxEntries);
    return error_code;
}