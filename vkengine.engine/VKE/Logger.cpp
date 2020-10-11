#include "Logger.h"
#include <cstdio>
#include <string>
#include <iostream>

#include "assert.h"

namespace VKE
{
	void Logger::WriteLog(const char* prepend, const char* message, va_list args)
	{
		vprintf((std::string(prepend) + message + "\n").c_str(), args);
	}

	void Logger::Trace(const char * message, ...)
	{
		va_list args;
		va_start(args, message);
		WriteLog("[TRACE]: ", message, args);
		va_end(args);
	}
	
	void Logger::Info(const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		WriteLog("[INFO]: ", message, args);
		va_end(args);
	}

	void Logger::Warn(const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		WriteLog("[WARN]: ", message, args);
		va_end(args);
	}

	void Logger::Error(const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		WriteLog("[ERROR]: ", message, args);
		va_end(args);
	}

	void Logger::Fatal(const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		WriteLog("[FATAL]: ", message, args);
		va_end(args);

		ASSERT(false);
	}

}
