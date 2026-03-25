#pragma once

#include <muduo/base/Logging.h>

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