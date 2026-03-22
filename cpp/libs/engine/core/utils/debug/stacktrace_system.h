#pragma once
#include <boost/stacktrace.hpp>
#include <sstream>
#include <string>
#include <algorithm>
#include <muduo/base/Logging.h>

constexpr size_t kMaxEntries = 10;

inline std::string GetStackTraceAsString()
{
	boost::stacktrace::stacktrace stack = boost::stacktrace::stacktrace();
	std::ostringstream oss;
	oss << stack;
	return oss.str();
}


// Return current stack trace, limited to maxEntries frames.
inline std::string GetCurrentStackTraceAsString(size_t maxEntries) {
    boost::stacktrace::stacktrace stack = boost::stacktrace::stacktrace();
    size_t stackSize = stack.size();
    size_t numToPrint = std::min(stackSize, maxEntries);

    std::ostringstream oss;
    oss << "Current Stack Trace (last " << numToPrint << " entries):" << std::endl;

    for (size_t i = stackSize - numToPrint; i < stackSize; ++i) {
        oss << stack[i] << std::endl;
    }

    return oss.str();
}

inline void PrintCurrentStackTrace(size_t maxEntries) {
    LOG_ERROR << GetCurrentStackTraceAsString(maxEntries);
}

inline void PrintDefaultStackTrace() {
    PrintCurrentStackTrace(kMaxEntries);
}

inline void PrintStackTraceWithLimit(size_t entries) {
    PrintCurrentStackTrace(entries);
}
