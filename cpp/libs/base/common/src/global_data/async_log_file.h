#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"   // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h"  // support for user defined types
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

using LoggerPtr = std::shared_ptr<spdlog::logger>;
extern LoggerPtr g_log;

void InitSpdLog(const std::string& logger_name,
    const std::string& filename,
    bool truncate = false,
    const spdlog::file_event_handlers& event_handlers = {});