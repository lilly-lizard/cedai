#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

class Log {
public:
	static void Init();
	inline static std::shared_ptr<spdlog::logger>& GetCDLogger() { return s_CDLogger; }
private:
	static std::shared_ptr<spdlog::logger> s_CDLogger;
};

// log macros
#define CD_TRACE(...)	Log::GetCDLogger()->trace(__VA_ARGS__)
#define CD_INFO(...)	Log::GetCDLogger()->info(__VA_ARGS__)
#define CD_WARN(...)	Log::GetCDLogger()->warn(__VA_ARGS__)
#define CD_ERROR(...)	Log::GetCDLogger()->error(__VA_ARGS__)
#define CD_FATAL(...)	Log::GetCDLogger()->fatal(__VA_ARGS__)
