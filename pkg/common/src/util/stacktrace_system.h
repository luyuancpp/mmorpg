#pragma once
#include <boost/stacktrace.hpp>
#include <sstream>
#include <string>
#include <algorithm>
#include <muduo/base/Logging.h>

constexpr size_t kMaxEntries = 10;

inline std::string GetStackTraceAsString()
{
	// 获取堆栈跟踪
	boost::stacktrace::stacktrace stack = boost::stacktrace::stacktrace();

	// 使用 std::ostringstream 来格式化堆栈信息
	std::ostringstream oss;
	oss << stack;

	// 返回格式化后的堆栈信息
	return oss.str();
}


// 返回当前堆栈信息，最多返回 maxEntries 条堆栈信息
inline std::string GetCurrentStackTraceAsString(size_t maxEntries) {
    // 获取当前堆栈
    boost::stacktrace::stacktrace stack = boost::stacktrace::stacktrace();

    // 获取堆栈的总条目数量
    size_t stackSize = stack.size();

    // 确定要打印的堆栈条目数（最多 maxEntries 条）
    size_t numToPrint = std::min(stackSize, maxEntries);

    // 用于存储堆栈信息的字符串流
    std::ostringstream oss;
    oss << "Current Stack Trace (last " << numToPrint << " entries):" << std::endl;

    // 将堆栈信息添加到字符串流
    for (size_t i = stackSize - numToPrint; i < stackSize; ++i) {
        oss << stack[i] << std::endl;
    }

    // 返回构建好的堆栈信息字符串
    return oss.str();
}

// 打印当前堆栈，最多打印 maxEntries 条堆栈
inline void PrintCurrentStackTrace(size_t maxEntries) {
    LOG_ERROR << GetCurrentStackTraceAsString(maxEntries);
}

inline void PrintDefaultStackTrace() {
    PrintCurrentStackTrace(kMaxEntries);
}

// 打印当前堆栈，传入自定义条数
inline void PrintStackTraceWithLimit(size_t entries) {
    PrintCurrentStackTrace(entries);  // 打印指定条数的堆栈
}
