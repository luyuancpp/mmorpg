#include "async_log_file.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
LoggerPtr g_log;

void InitSpdLog(const std::string& logger_name, 
    const std::string& filename, 
    bool truncate /*= false*/, 
    const spdlog::file_event_handlers& event_handlers /*= {}*/)
{
	auto console = spdlog::stdout_color_mt("console");
	console->info("This should be in color.");

    spdlog::set_pattern("[%H:%M:%S %z] [%^%L%$] [thread %t] [source %s] [function %!] [line %#] %v");
    g_log =
        spdlog::rotating_logger_mt<spdlog::async_factory>(logger_name, filename, 1048576 * 5, 3);
    spdlog::flush_every(std::chrono::seconds(3));
}

