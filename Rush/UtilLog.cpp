#include "UtilLog.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef RUSH_PLATFORM_WINDOWS
#include <windows.h>
#endif // RUSH_PLATFORM_WINDOWS

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(                                                                                                       \
    disable : 4996) // 'vsnprintf': This function or variable may be unsafe. Consider using vsnprintf_s instead.
#endif              //_MSC_VER

#ifdef __EMSCRIPTEN__
#undef stderr
#define stderr stdout
#endif

namespace Rush
{
bool Log::breakOnError   = IsDebuggerPresent();
bool Log::breakOnWarning = false;

Log::LogMessageCallback Log::callbackDebug   = nullptr;
Log::LogMessageCallback Log::callbackMessage = nullptr;
Log::LogMessageCallback Log::callbackWarning = nullptr;
Log::LogMessageCallback Log::callbackError   = nullptr;
Log::LogMessageCallback Log::callbackFatal   = nullptr;

const char* Log::prefixMessage = "";
const char* Log::prefixWarning = "Warning: ";
const char* Log::prefixError   = "Error: ";
const char* Log::prefixFatal   = "Fatal: ";

void Log::debug(const char* msg, ...)
{
	if (callbackDebug)
	{
		char    str[1024];
		va_list varargs;
		va_start(varargs, msg);
		vsnprintf(str, sizeof(str), msg, varargs);
		va_end(varargs);
		callbackDebug(str);
	}
	else
	{
		va_list varargs;
		va_start(varargs, msg);

#if defined(_MSC_VER)
		char str[1024];
		vsprintf_s(str, msg, varargs);
		OutputDebugStringA(str);
#else  //_MSC_VER
		vfprintf(stdout, msg, varargs);
		fflush(stdout);
#endif //_MSC_VER

		va_end(varargs);
	}
}

void Log::message(const char* msg, ...)
{
	if (callbackMessage)
	{
		char    str[1024];
		va_list varargs;
		va_start(varargs, msg);
		vsnprintf(str, sizeof(str), msg, varargs);
		va_end(varargs);
		callbackMessage(str);
	}
	else
	{
		va_list varargs;
		va_start(varargs, msg);
		fputs(prefixMessage, stdout);
		vfprintf(stdout, msg, varargs);
		fprintf(stdout, "\n");
		va_end(varargs);
		fflush(stdout);
	}
}

void Log::warning(const char* msg, ...)
{
	if (callbackWarning)
	{
		char    str[1024];
		va_list varargs;
		va_start(varargs, msg);
		vsnprintf(str, sizeof(str), msg, varargs);
		va_end(varargs);
		callbackWarning(str);
	}
	else
	{
#ifdef RUSH_PLATFORM_WINDOWS
		auto                       h = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(h, &csbi);
		SetConsoleTextAttribute(h, 0x06);
#endif

		va_list varargs;
		va_start(varargs, msg);
		fputs(prefixWarning, stderr);
		vfprintf(stderr, msg, varargs);
		fprintf(stderr, "\n");
		va_end(varargs);
		fflush(stderr);

#ifdef RUSH_PLATFORM_WINDOWS
		SetConsoleTextAttribute(h, csbi.wAttributes);
#endif
	}
}

void Log::error(const char* msg, ...)
{
	if (callbackError)
	{
		char    str[1024];
		va_list varargs;
		va_start(varargs, msg);
		vsnprintf(str, sizeof(str), msg, varargs);
		va_end(varargs);
		callbackError(str);
	}
	else
	{
#ifdef RUSH_PLATFORM_WINDOWS
		auto                       h = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(h, &csbi);
		SetConsoleTextAttribute(h, 0x0C);
#endif

		va_list varargs;
		va_start(varargs, msg);
		fputs(prefixError, stderr);
		vfprintf(stderr, msg, varargs);
		fprintf(stderr, "\n");
		va_end(varargs);
		fflush(stderr);

#ifdef RUSH_PLATFORM_WINDOWS
		SetConsoleTextAttribute(h, csbi.wAttributes);
#endif
	}
}

void Log::fatal(const char* msg, ...)
{
	if (callbackFatal)
	{
		char    str[1024];
		va_list varargs;
		va_start(varargs, msg);
		vsnprintf(str, sizeof(str), msg, varargs);
		va_end(varargs);
		callbackFatal(str);
	}
	else
	{
#ifdef RUSH_PLATFORM_WINDOWS
		auto                       h = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(h, &csbi);
		SetConsoleTextAttribute(h, 0x04);
#endif

		va_list varargs;
		va_start(varargs, msg);
		fputs(prefixFatal, stderr);
		vfprintf(stderr, msg, varargs);
		fprintf(stderr, "\n");
		va_end(varargs);
		fflush(stderr);

#ifdef RUSH_PLATFORM_WINDOWS
		SetConsoleTextAttribute(h, csbi.wAttributes);
#endif
	}
}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER
