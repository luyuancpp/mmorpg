#pragma once
#include <boost/stacktrace.hpp>
#include <sstream>

std::string GetStackTraceAsString()
{
	// 获取堆栈跟踪
	boost::stacktrace::stacktrace stack = boost::stacktrace::stacktrace();

	// 使用 std::ostringstream 来格式化堆栈信息
	std::ostringstream oss;
	oss << stack;

	// 返回格式化后的堆栈信息
	return oss.str();
}