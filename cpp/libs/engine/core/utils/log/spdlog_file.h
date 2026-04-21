#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"   // support for loading levels from the environment variable
using LoggerPtr = std::shared_ptr<spdlog::logger>;
extern LoggerPtr g_log;

void InitSpdLog(const std::string& logger_name,
    const std::string& filename,
    bool truncate = false,
    const spdlog::file_event_handlers& event_handlers = {});