#pragma once

/*
Example usage:
CD_INFO("swap chain image count = {}", imageCount);
formatting: https://fmt.dev/dev/syntax.html
*/

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

class Log {
public:
	static void Init();
	inline static std::shared_ptr<spdlog::logger>& Log::GetPBLogger() { return s_PBLogger; }
private:
	static std::shared_ptr<spdlog::logger> s_PBLogger;
};

// log macros
#define CD_TRACE(...)	Log::GetPBLogger()->trace(__VA_ARGS__)
#define CD_INFO(...)	Log::GetPBLogger()->info(__VA_ARGS__)
#define CD_WARN(...)	Log::GetPBLogger()->warn(__VA_ARGS__)
#define CD_ERROR(...)	Log::GetPBLogger()->error(__VA_ARGS__)
#define CD_FATAL(...)	Log::GetPBLogger()->fatal(__VA_ARGS__)