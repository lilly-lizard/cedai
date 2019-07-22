#include "Log.hpp"

/*
Example usage:
CD_INFO("swap chain image count = {}", imageCount);
formatting: https://fmt.dev/dev/syntax.html
https://github.com/fmtlib/fmt
*/

#include "spdlog/sinks/stdout_color_sinks.h"

std::shared_ptr<spdlog::logger> Log::s_PBLogger;

void Log::Init() {
	spdlog::set_pattern("%^[%T] %n: %v%$");
	s_PBLogger = spdlog::stdout_color_mt("CEDAI");
	s_PBLogger->set_level(spdlog::level::trace);
}