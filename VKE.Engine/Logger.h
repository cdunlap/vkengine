#pragma once

#include <cstdarg>

namespace VKE {
	class Logger
	{
	public:
		static void Trace(const char* message, ...);
		static void Info(const char* message, ...);
		static void Warn(const char* message, ...);
		static void Error(const char* message, ...);
		static void Fatal(const char* message, ...);

	private:
		static void WriteLog(const char* prepend, const char* message, va_list args);
	};
}
