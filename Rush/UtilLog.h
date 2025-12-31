#pragma once

#include "Rush.h"

namespace Rush
{

void Platform_TerminateProcess(int status);

struct Log
{
	typedef void (*LogMessageCallback)(const char* msg);

	static void debug(const char* msg, ...);
	static void message(const char* msg, ...);
	static void warning(const char* msg, ...);
	static void error(const char* msg, ...);
	static void fatal(const char* msg, ...);

	static bool breakOnWarning;
	static bool breakOnError;

	static LogMessageCallback callbackDebug;
	static LogMessageCallback callbackMessage;
	static LogMessageCallback callbackWarning;
	static LogMessageCallback callbackError;
	static LogMessageCallback callbackFatal;

	static const char* prefixMessage;
	static const char* prefixWarning;
	static const char* prefixError;
	static const char* prefixFatal;
};

#ifdef __GNUC__
#define RUSH_LOG(text, ...) do { Rush::Log::message(text, ##__VA_ARGS__); } while (0)
#define RUSH_LOG_WARNING(text, ...) \
	do { Rush::Log::warning(text, ##__VA_ARGS__); if (Rush::Log::breakOnWarning) RUSH_BREAK; } while (0)
#define RUSH_LOG_ERROR(text, ...) \
	do { Rush::Log::error(text, ##__VA_ARGS__); if (Rush::Log::breakOnError) RUSH_BREAK; } while (0)
#define RUSH_LOG_FATAL(text, ...) \
	do { Rush::Log::fatal(text, ##__VA_ARGS__); if (Rush::Log::breakOnError) RUSH_BREAK; else Platform_TerminateProcess(0x80000003); } while (0)
#else
#define RUSH_LOG(text, ...) do { Rush::Log::message(text, __VA_ARGS__); } while (0)
#define RUSH_LOG_WARNING(text, ...) \
	do { Rush::Log::warning(text, __VA_ARGS__); if (Rush::Log::breakOnWarning) RUSH_BREAK; } while (0)
#define RUSH_LOG_ERROR(text, ...) \
	do { Rush::Log::error(text, __VA_ARGS__); if (Rush::Log::breakOnError) RUSH_BREAK; } while (0)
#define RUSH_LOG_FATAL(text, ...) \
	do { Rush::Log::fatal(text, __VA_ARGS__); if (Rush::Log::breakOnError) RUSH_BREAK; else Platform_TerminateProcess(0x80000003); } while (0)
#endif

#if (defined(RUSH_DEBUG) || defined(FORCE_ASSERTS) || defined(RUSH_FORCE_ASSERTS))
#define RUSH_ASSERT(v) do { if (!(v)) { RUSH_LOG_FATAL("Assert '" #v "' failed in '%s'.", RUSH_FUNCTION); } } while (0)
#ifdef __GNUC__
#define RUSH_ASSERT_MSG(v, msg, ...) \
	do { if (!(v)) { RUSH_LOG_FATAL("Assert '" #v "' failed in '%s'. " msg, RUSH_FUNCTION, ##__VA_ARGS__); } } while (0)
#else
#define RUSH_ASSERT_MSG(v, msg, ...) \
	do { if (!(v)) { RUSH_LOG_FATAL("Assert '" #v "' failed in '%s'. " ## msg, RUSH_FUNCTION, __VA_ARGS__); } } while (0)
#endif
#else
#define RUSH_ASSERT(v) do { RUSH_UNUSED(v); } while (0)
#define RUSH_ASSERT_MSG(v, msg, ...) do { RUSH_UNUSED(v); } while (0)
#endif

}
