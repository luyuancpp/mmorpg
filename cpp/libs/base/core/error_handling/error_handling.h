#pragma once

#include <muduo/base/Logging.h>

#include "core/utils/stacktrace_system.h"

// 打印堆栈并返回错误码的函数
inline uint32_t PrintStackAndReturnError(const uint32_t error_code) {
    // 打印堆栈信息
    LOG_ERROR << "Stack trace (error code: " << error_code << "):";
    LOG_ERROR << GetCurrentStackTraceAsString(kMaxEntries);  // 获取并打印最近 20 条堆栈信息

    // 返回传入的错误码
    return error_code;
}

inline uint32_t PrintStackAndReturnError(const uint32_t error_code, size_t maxEntries) {
    LOG_ERROR << "Stack trace (error code: " << error_code << "):";
    LOG_ERROR << GetCurrentStackTraceAsString(maxEntries);  // 获取并打印最近 maxEntries 条堆栈信息
    return error_code;
}